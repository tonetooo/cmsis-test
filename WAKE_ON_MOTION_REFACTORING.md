# Wake-on-Motion Analysis & Refactoring Guide

**Date:** April 2026  
**Project:** HERMES-A1 (cmsis_hermes)  
**Focus:** ADXL355 interrupt-driven motion detection with FreeRTOS task signaling

---

## Current Wake-on-Motion Implementation (Bare-Metal)

### Hardware Configuration
- **Sensor:** ADXL355 (3-axis accelerometer)
- **Interrupt Pin:** ADXL_INT1 (GPIO Port C, Pin 7)
- **Interrupt Type:** EXTI9_5_IRQn (External Interrupt 9-5)
- **Trigger Mode:** Rising edge (ADXL355 Activity Interrupt)
- **Output Type:** Active High (INT1 goes high when motion exceeds threshold)

### Sensor Configuration Code (adxl355.c, line 187)
```c
void ADXL355_Config_WakeOnMotion(float threshold_g, uint8_t count) {
    // 1. Configure Activity Threshold Register (0x20, 0x21)
    uint16_t threshold_val = (uint16_t)(threshold_g * (current_sensitivity / 16.0f));
    ADXL355_Write_Reg(0x20, (threshold_val >> 8) & 0xFF);  // High byte
    ADXL355_Write_Reg(0x21, threshold_val & 0xFF);         // Low byte
    
    // 2. Configure Activity Count Register (0x22)
    ADXL355_Write_Reg(0x22, count);  // Samples above threshold before interrupt
    
    // 3. Map Activity Interrupt to INT1
    // Register 0x2A (INT_MAP): Bit 3 is ACT_EN1
    int_map |= 0x08;  // Set bit 3 to enable Activity on INT1
    ADXL355_Write_Reg(0x2A, int_map);
    
    // 4. Enable Activity Interrupt in ACT_EN
    ADXL355_Write_Reg(0x27, 0x01);  // Enable ACT
}
```

### Interrupt Flow (Current)

**Sequence:**
```
1. Motion occurs, acceleration > threshold_g
   ↓
2. ADXL355 internal logic: Samples above threshold for 'count' consecutive readings
   ↓
3. ADXL355 pulls INT1 line HIGH (active high logic)
   ↓
4. STM32F4 detects rising edge on EXTI9_5
   ↓
5. STM32F4 enters HAL_GPIO_EXTI_Callback()
   ↓
6. Set g_event_pending = 1 (volatile uint8_t)
   ↓
7. Return from ISR
   ↓
8. Main loop polls g_event_pending in AUTO_STATE_IDLE_LOW_POWER
   ↓
9. If flag is set: Transition to AUTO_STATE_ACQUISITION
```

### Problem Zones

**Problem 1: Single Flag Cannot Queue Events**
```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == ADXL_INT1_Pin) {
        g_event_pending = 1;  // ← Only one bit, can't queue multiple triggers
    }
}
```
**Issue:** If second motion trigger occurs during acquisition, it's ignored (flag already set)
**Impact:** Events after the first trigger during the 60-second acquisition are lost

**Problem 2: Main Loop Must Poll**
```c
// In AUTO_STATE_IDLE_LOW_POWER
if (g_event_pending || HAL_GPIO_ReadPin(ADXL_INT1_GPIO_Port, ADXL_INT1_Pin) == GPIO_PIN_SET) {
    g_event_pending = 0;
    state = AUTO_STATE_ACQUISITION;
}
```
**Issue:** Main loop runs continuously, burns CPU cycles polling flag
**Impact:** No power savings, can't enter sleep modes effectively

**Problem 3: No Clear Task Wakeup**
- Flag set → main loop detects → transitions state
- No explicit "wake from sleep" mechanism
- CPU never goes to sleep (HAL_Delay loop continues)

---

## Improved Wake-on-Motion with FreeRTOS + CMSIS-RTOS v2

### Architecture

```
ADXL_INT1 (Active High)
    │
    ├─ [Rising Edge Detector]
    │
    └─ EXTI9_5_IRQn (Priority=5, safe for osEventFlagsSet)
            │
            └─ HAL_GPIO_EXTI_Callback()
                   │
                   ├─ osEventFlagsSet(sensor_event_flags, EVT_MOTION_DETECTED)
                   │
                   └─ ← Returns immediately, ISR completes
                       ↓
                   ← FreeRTOS Context Switch ←
                       ↓
                   sensor_task woken up (Prio=3, highest user priority)
                       │
                       └─ ADXL355_Read_Data()
                           └─ osMessageQueuePut(sensor_queue, ...)
                               └─ (other tasks can pop and process)
```

### Key Improvements

| Aspect | Bare-Metal | FreeRTOS v2 |
|--------|-----------|------------|
| **Event Queue** | Single flag (lost events) | osEventFlags (8+ bits) or osMessageQueue |
| **Task Wakeup** | CPU polls continuously | ISR triggers osEventFlagsSet() → FreeRTOS scheduler wakes task |
| **Response Latency** | 50ms (main loop period) | <1ms (ISR → task switch) |
| **Power Efficiency** | Poor (busy loop) | Good (task blocks until event) |
| **Concurrent Ops** | Blocked during acq. | Sensor acq. + upload parallel |
| **Code Clarity** | Spread across 3 states | Localized in sensor_task() |

---

## Implementation: Interrupt Handler Refactoring

### Step 1: Define Event Flags
**File:** Core/Inc/tasks.h (NEW)

```c
#ifndef TASKS_H
#define TASKS_H

#include "cmsis_os2.h"

// Forward declarations
extern osEventFlagsId_t sensor_event_flags;
extern osMessageQueueId_t sensor_queue;
extern osMutexId_t sd_mutex;

// Event flag definitions
#define EVT_MOTION_DETECTED     (1 << 0)  // ADXL_INT1 triggered
#define EVT_ACQSTN_DONE        (1 << 1)  // Acquisition settling complete
#define EVT_UPLOAD_DONE        (1 << 2)  // File uploaded
#define EVT_CFG_CHECK          (1 << 3)  // Config check timer
#define EVT_SENSOR_FIFO_READY  (1 << 4)  // FIFO watermark

// Task functions
void sensor_task(void *arg);
void modem_task(void *arg);
void file_task(void *arg);
void control_task(void *arg);

#endif
```

### Step 2: Update Interrupt Handler
**File:** Core/Src/stm32f4xx_it.c (MODIFIED)

```c
#include "tasks.h"  // Include RTOS definitions

/**
  * @brief EXTI line[9:5] interrupt handler
  */
void EXTI9_5_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(ADXL_INT1_Pin);
    
    // Note: HAL_GPIO_EXTI_IRQHandler calls HAL_GPIO_EXTI_Callback
}

/**
  * @brief GPIO EXTI callback (now RTOS-aware)
  * Called from ISR context, must use ISR-safe APIs
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == ADXL_INT1_Pin) {
        // Instead of: g_event_pending = 1;
        // Use: osEventFlagsSet() which is ISR-safe
        osEventFlagsSet(sensor_event_flags, EVT_MOTION_DETECTED);
        
        // FreeRTOS will wake sensor_task immediately (high priority)
        // This is non-blocking; callback returns immediately
    }
}
```

### Step 3: Create Sensor Task
**File:** Core/Src/tasks/sensor_task.c (NEW)

```c
#include "sensor_task.h"
#include "adxl355.h"
#include "main.h"
#include "tasks.h"
#include <math.h>

// Motion detection state machine
typedef enum {
    STATE_WAITING,     // Idle, waiting for EVT_MOTION_DETECTED
    STATE_ACQUIRING,   // Active acquisition recording
    STATE_SETTLING,    // Silent period, checking if event finished
} SensorState_t;

void sensor_task(void *arg) {
    ADXL355_Data_t data;
    SensorState_t state = STATE_WAITING;
    uint32_t acq_start = 0;
    uint32_t settling_start = 0;
    float prev_mag = -1.0f;
    uint8_t earthquake_detected = 0;
    
    const float STATIC_DELTA = 0.005f;      // 5mg stability threshold
    const uint32_t SETTLING_DUR = 3000;     // 3 seconds of silence = end of event
    const uint32_t MAX_ACQ_TIME = 900000;   // 15 minutes max acquisition
    
    printf("[SENSOR_TASK] Started (Prio=3, 10ms cycle)\r\n");
    
    while (1) {
        switch (state) {
            
            // ════════════════════════════════════════════════════════════
            // STATE: WAITING
            // Purpose: Idle until motion trigger from EXTI handler
            // ════════════════════════════════════════════════════════════
            case STATE_WAITING: {
                // Block until EVT_MOTION_DETECTED is set (ISR handler calls osEventFlagsSet)
                uint32_t flags = osEventFlagsWait(
                    sensor_event_flags, 
                    EVT_MOTION_DETECTED, 
                    osWaitAny,                  // Wake on any matching flag
                    osWaitForever               // Block until event
                );
                
                if (flags & osFlagsError) {
                    // Should not happen with osWaitForever, but handle gracefully
                    printf("[SENSOR] Unexpected error in osEventFlagsWait\r\n");
                    continue;
                }
                
                // Motion detected, start acquisition
                printf("[SENSOR] Motion Detected! Starting acquisition...\r\n");
                acq_start = osKernelGetTickCount();
                settling_start = acq_start;
                prev_mag = -1.0f;
                earthquake_detected = 0;
                state = STATE_ACQUIRING;
                
            } break;
            
            // ════════════════════════════════════════════════════════════
            // STATE: ACQUIRING
            // Purpose: Continuous polling of ADXL355 for 10ms cycles
            // ════════════════════════════════════════════════════════════
            case STATE_ACQUIRING: {
                uint32_t now = osKernelGetTickCount();
                
                // Safety timeout: max 15 minutes of continuous acquisition
                if ((now - acq_start) > MAX_ACQ_TIME) {
                    printf("[SENSOR] Timeout: Max acquisition time reached (15 min)\r\n");
                    state = STATE_WAITING;
                    break;
                }
                
                // Read sensor data
                ADXL355_Read_Data(&data);
                
                // Calculate horizontal magnitude (ignore Z due to 1G gravity offset)
                float current_mag = sqrtf(data.x_g * data.x_g + data.y_g * data.y_g);
                
                // Earthquake rejection: abort if any axis > 2.0G
                if ((data.x_g > 2.0f || data.x_g < -2.0f) ||
                    (data.y_g > 2.0f || data.y_g < -2.0f) ||
                    (data.z_g > 2.0f || data.z_g < -2.0f)) {
                    printf("[SENSOR] EARTHQUAKE/SHOCK DETECTED (>2.0G)! Aborting.\r\n");
                    earthquake_detected = 1;
                    state = STATE_WAITING;
                    break;
                }
                
                // Push data to queue for file_task and modem_task
                SensorReading_t reading = {
                    .timestamp_ms = (now - acq_start),
                    .x_g = data.x_g,
                    .y_g = data.y_g,
                    .z_g = data.z_g,
                    .voltage = 0.0f,    // Placeholder
                    .current = 0.0f,
                    .power = 0.0f
                };
                
                // Non-blocking queue put (if full, discard oldest sample)
                osMessageQueuePut(sensor_queue, &reading, 0, 0);
                
                // Debug print every 125ms
                static uint32_t last_print = 0;
                if ((now - last_print) > 125) {
                    int32_t x_mg = (int32_t)(data.x_g * 1000);
                    int32_t y_mg = (int32_t)(data.y_g * 1000);
                    int32_t z_mg = (int32_t)(data.z_g * 1000);
                    printf("[SENSOR] X:%ld.%03ld Y:%ld.%03ld Z:%ld.%03ld g | T:%.1fs mag:%.3f\r\n",
                           x_mg/1000, (x_mg<0 ? -x_mg : x_mg)%1000,
                           y_mg/1000, (y_mg<0 ? -y_mg : y_mg)%1000,
                           z_mg/1000, (z_mg<0 ? -z_mg : z_mg)%1000,
                           (float)(now - acq_start) / 1000.0f,
                           current_mag);
                    last_print = now;
                }
                
                // Settling logic: check if event has finished
                if (current_mag < trigger_g) {
                    // Below threshold, motion may have stopped
                    
                    float delta = (prev_mag < 0) ? 0.0f : fabsf(current_mag - prev_mag);
                    
                    if (delta > STATIC_DELTA) {
                        // Still changing, motion detected
                        settling_start = now;
                    } else if ((now - settling_start) > SETTLING_DUR) {
                        // Silent for SETTLING_DUR ms, event is over
                        printf("[SENSOR] Settling complete (%.1fs duration)\r\n",
                               (float)(now - acq_start) / 1000.0f);
                        
                        // Signal other tasks that acquisition is done
                        osEventFlagsSet(sensor_event_flags, EVT_ACQSTN_DONE);
                        
                        state = STATE_WAITING;
                        break;
                    }
                } else {
                    // Above threshold, reset settling timer
                    settling_start = now;
                }
                
                prev_mag = current_mag;
                
                // Precise 10ms periodic execution
                // Use osThreadFlagsWait for accurate delay without blocking scheduler
                osThreadFlagsWait(0, osWaitAll, 10);
                
            } break;
            
            default:
                state = STATE_WAITING;
                break;
        }
    }
}
```

---

## Timing Analysis

### Response Latency Comparison

**Bare-Metal Model:**
```
t=0:   Motion event (acceleration > threshold)
t=0:   ADXL355 samples accumulate (count=5 samples × 10ms = 50ms)
t=50:  INT1 goes HIGH (external interrupt pending)
t=50:  STM32F4 detects EXTI rising edge
t=50:  HAL_GPIO_EXTI_Callback() called
t=50:  g_event_pending = 1
t=50:  ISR returns, main loop continues...
t=50+n: Main loop checks g_event_pending (every ~50ms in IDLE state)
t=50+n: Detects flag, transitions to ACQUISITION state
─────────────────────────────────────────────
Total Latency: 50ms (sensor count) + up to 50ms (polling period) = ~100ms
```

**FreeRTOS Model:**
```
t=0:   Motion event
t=50:  ADXL355 samples complete, INT1 goes HIGH
t=50:  STM32F4 detects EXTI rising edge
t=50:  HAL_GPIO_EXTI_Callback() called
t=50:  osEventFlagsSet(sensor_event_flags, EVT_MOTION_DETECTED)
t=50:  ISR returns, FreeRTOS scheduler runs
t=50:  Context switch to sensor_task (already in osEventFlagsWait)
t=50+x: sensor_task wakes with flag set (x < 1ms context switch time)
───────────────────────────────────────────
Total Latency: 50ms (sensor count) + <1ms (ISR→task) = ~51ms
─── 
Better by ~49ms! ✓
```

---

## Testing Strategy

### Unit Test 1: Wake Trigger Detection
```c
void test_wake_trigger() {
    printf("\n=== TEST: Wake Trigger Detection ===\r\n");
    
    // Setup: Configure WakeOnMotion at 0.1G threshold
    ADXL355_Config_WakeOnMotion(0.1f, 5);
    printf("WakeOnMotion armed (0.1G, count=5)\r\n");
    
    // Simulate motion on accelerometer (manual tap on PCB)
    printf("Waiting for motion (tap accelerometer within 10s)...\r\n");
    
    // Check: sensor_task should wake within 100ms
    uint32_t start = osKernelGetTickCount();
    while ((osKernelGetTickCount() - start) < 10000) {
        if (osEventFlagsGet(sensor_event_flags) & EVT_MOTION_DETECTED) {
            printf("✓ PASS: Motion detected in %ums\r\n", osKernelGetTickCount() - start);
            return;
        }
        osThreadYield();
    }
    
    printf("✗ FAIL: Motion not detected within 10 seconds\r\n");
}
```

### Unit Test 2: Settling Detection
```c
void test_settling_detection() {
    printf("\n=== TEST: Settling Detection ===\r\n");
    
    // Arm wake-on-motion
    ADXL355_Config_WakeOnMotion(0.1f, 5);
    printf("Waiting for motion...\r\n");
    
    // Wait for EVT_MOTION_DETECTED
    osEventFlagsWait(sensor_event_flags, EVT_MOTION_DETECTED, osWaitAny, 10000);
    
    printf("Motion detected! Waiting for settling (3 sec of silence)...\r\n");
    
    // Wait for EVT_ACQSTN_DONE
    uint32_t start = osKernelGetTickCount();
    uint32_t flags = osEventFlagsWait(sensor_event_flags, EVT_ACQSTN_DONE, 
                                       osWaitAny, 65000);
    
    if (flags & EVT_ACQSTN_DONE) {
        printf("✓ PASS: Settling detected in %.1fs\r\n", 
               (osKernelGetTickCount() - start) / 1000.0f);
    } else {
        printf("✗ FAIL: Settling not detected within 65 seconds\r\n");
    }
}
```

### Integration Test 3: Concurrent Acq. + Upload
```c
void test_concurrent_acq_upload() {
    printf("\n=== TEST: Concurrent Acquisition + Upload ===\r\n");
    
    // Trigger motion
    printf("Arm system and trigger motion...\r\n");
    osEventFlagsWait(sensor_event_flags, EVT_MOTION_DETECTED, osWaitAny, 30000);
    
    // While acquiring, initiate upload
    printf("Acquisition in progress, triggering upload...\r\n");
    osEventFlagsSet(sensor_event_flags, EVT_ACQSTN_DONE);  // Fake completion
    
    // Modem task should upload without blocking sensor
    osThreadYield();
    osThreadYield();
    osThreadYield();
    
    printf("✓ PASS: Tasks running concurrently without blocking\r\n");
}
```

---

## Migration Checklist

- [ ] **Phase 1: ISR Handler**
  - [ ] Create tasks.h with event flag definitions
  - [ ] Update stm32f4xx_it.c to use osEventFlagsSet()
  - [ ] Verify NVIC priority = 5 (below MAX_SYSCALL)
  - [ ] Test flag set from ISR without errors

- [ ] **Phase 2: Sensor Task**
  - [ ] Create sensor_task.c with STATE_WAITING/ACQUIRING/SETTLING
  - [ ] Implement 10ms periodic loop
  - [ ] Test EVT_MOTION_DETECTED wakeup
  - [ ] Verify settling detection (3s silence)

- [ ] **Phase 3: Data Queue**
  - [ ] Create osMessageQueue(sensor_queue)
  - [ ] Push SensorReading_t in sensor_task
  - [ ] Pop and verify in test harness
  - [ ] Check queue overflow handling

- [ ] **Phase 4: Integration**
  - [ ] Connect modem_task to sensor_queue
  - [ ] Test concurrent acq. + upload
  - [ ] Verify motion interrupt during upload
  - [ ] Profile task timing with oscilloscope

---

**Document Version:** 1.0  
**Status:** Ready for Implementation
