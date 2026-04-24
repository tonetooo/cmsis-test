# FreeRTOS + CMSIS-RTOS v2 Migration Plan
**Project:** HERMES-A1 (AWTAS - Autonomous Wireless Triaxial Acquisition System)  
**Target:** STM32F446RET6  
**Date:** April 2026

---

## Executive Summary

Migrate from bare-metal polling architecture to FreeRTOS-based multitasking with CMSIS-RTOS v2 API. This enables:
- **Concurrent operations:** Sensor acquisition + modem upload simultaneously
- **Event-driven design:** Interrupts wake tasks instead of setting flags
- **Real-time guarantees:** Priority-based scheduling for 10ms sensor cycle
- **Resource safety:** Mutex protection for shared SD card access

---

## Current Architecture Analysis

### Wake-on-Motion (Current)
```
┌─ ADXL_INT1 Pin (Active High)
│     │
│     ├─ EXTI9_5_IRQn
│     │
│     └─ HAL_GPIO_EXTI_Callback()
│            ↓
│        g_event_pending = 1  (volatile uint8_t)
│            ↓
│        Main Loop polls in AUTO_STATE_IDLE_LOW_POWER
│            ↓
│        START ACQUISITION (blocking loop for 60s)
│            │
│            ├─ Acquire data every 10ms with HAL_Delay(10)
│            ├─ Check settling condition
│            └─ Write to SD card (blocking)
│            ↓
│        MOVE TO UPLOAD STATE
│            ↓
│        Modem_UploadFile() (blocking, up to 5+ minutes)
```

**Problems:**
- Single flag can miss rapid triggers
- Acquisition blocks modem operations
- HAL_Delay() blocks entire CPU
- No concurrent file writes during upload
- Hard to add new tasks (CLI, config check)

---

## Target Architecture: Task-Based Model

```
┌─ ADXL_INT1 Pin (Active High)
│     │
│     ├─ EXTI9_5_IRQn (Priority 5)
│     │
│     └─ HAL_GPIO_EXTI_Callback()
│            ↓
│        osEventFlagsSet(sensor_event_flags, EVT_MOTION_DETECTED)
│            ↓
│        FreeRTOS wakes sensor_task (HIGH priority)
│            │
│     ┌──────┴──────┬──────────────┬─────────────┐
│     │             │              │             │
│     ↓             ↓              ↓             ↓
│  sensor_task   modem_task   file_task   control_task
│  (Prio=3)     (Prio=4)      (Prio=5)     (Prio=6)
│  HIGH         MEDIUM        LOW          LOWEST
│     │             │              │             │
│     ├─ Read        ├─ Wait for   ├─ SD card   ├─ CLI menu
│     │  ADXL355    │  data queue  │  writes    ├─ Manual FSM
│     │  every 10ms │  from sensor │  (mutex)   ├─ Config check
│     │             │              │            └─ State mgmt
│     ├─ Push to    ├─ Upload to  │
│     │  osMessage   │  Google     │
│     │  Queue       │  Drive      │
│     │              │  (3+ min)   │
│     │              │             │
│     └──→ File_task ← osEventFlagsSet(upload_done)
```

### Key Improvements
1. **Sensor task** runs independently at 10ms + 50ms safety margin
2. **Modem task** uploads in background without blocking acquisition
3. **File task** writes queue to SD with mutex protection
4. **Control task** handles CLI in lowest priority

---

## Task Specification

### Task 1: sensor_task (Priority = HIGH/3)
**Purpose:** Continuous ADXL355 polling, event detection, data queuing

| Parameter | Value |
|-----------|-------|
| Priority | 3 (HIGH) |
| Stack Size | 512 bytes |
| Periodicity | 10ms (100 Hz polling) |
| Blocking Points | osMessageQueuePut() when full |

**State Machine:**
```
┌─ WAITING
│   ├─ osEventFlagsWait(sensor_event_flags, EVT_MOTION, osWaitAny, osWaitForever)
│   └─ On trigger: Read FIFO, start acquisition timer
│
└─ ACQUIRING
    ├─ Loop: Read ADXL355 every 10ms
    ├─ Calculate magnitude (XY plane)
    ├─ Check settling (silent < threshold for 3s)
    ├─ Check earthquake (> 2G rejection)
    ├─ Push ADXL355_Data_t to osMessageQueue
    ├─ On settle: Set EVT_ACQSTN_DONE flag
    └─ Return to WAITING
```

**Data Flow:**
```
ADXL355
   ↓
sensor_task (every 10ms)
   ├─ ADXL355_Read_Data(&d)
   ├─ Calculate mag = sqrt(dx² + dy²)
   ├─ osMessageQueuePut(sensor_queue, &d, 0, 0)  // Non-blocking
   └─ If settling detected: osEventFlagsSet(EVT_ACQSTN_DONE)
```

---

### Task 2: modem_task (Priority = MEDIUM/4)
**Purpose:** File uploads, queue management, remote config check

| Parameter | Value |
|-----------|-------|
| Priority | 4 (MEDIUM) |
| Stack Size | 1KB |
| Event Triggers | EVT_ACQSTN_DONE, EVT_TIMER_CHECK_CONFIG |
| Blocking Points | Modem_UploadFile() for file duration |

**State Machine:**
```
┌─ IDLE
│   ├─ Wait: osEventFlagsWait(modem_event_flags, EVT_ACQSTN_DONE | EVT_CFG_CHECK, ...)
│   └─ On event: Process queue or check config
│
├─ UPLOADING
│   ├─ Queue_Peek(oldest)
│   ├─ Modem_UploadFile(oldest)  // Can be interrupted by sensor_task motion
│   ├─ On success: Queue_Pop(), remove file
│   ├─ On sensor interrupt: Break immediately
│   └─ Return to IDLE
│
└─ CONFIG_CHECK
    ├─ Every 20 minutes: Modem_DownloadConfig()
    ├─ Apply_Remote_Config()
    └─ Return to IDLE
```

**Abort Logic:**
```
sensor_task triggers EVT_MOTION_DETECTED
   ↓
modem_task polling Modem_UploadFile()
   ├─ Check osEventFlagsGet(EVT_MOTION_DETECTED)
   ├─ If set: Call Modem_Abort() (send Ctrl+C)
   ├─ Break upload loop
   └─ Return to IDLE for sensor acquisition
```

---

### Task 3: file_task (Priority = LOW/5)
**Purpose:** Asynchronous SD card writes, file management with mutex

| Parameter | Value |
|-----------|-------|
| Priority | 5 (LOW) |
| Stack Size | 512 bytes |
| Mutex Protected | SD card access (shared with control_task) |
| Blocking Points | f_write(), f_sync() under mutex |

**State Machine:**
```
LOOP:
   ├─ osMutexAcquire(sd_mutex, osWaitForever)
   ├─ osMessageQueueGet(file_queue, &cmd, 0, osWaitForever)
   │   ├─ CMD_WRITE: f_write(fil, buffer, len, &bw)
   │   ├─ CMD_SYNC: f_sync(fil)
   │   └─ CMD_CLOSE: f_close(fil)
   ├─ osMutexRelease(sd_mutex)
   └─ Yield to scheduler
```

**Queue Messages:**
```c
typedef struct {
    uint8_t cmd;     // CMD_WRITE, CMD_SYNC, CMD_CLOSE
    void *buffer;
    size_t length;
    FIL *file;
} FileCmd_t;
```

---

### Task 4: control_task (Priority = LOWEST/6)
**Purpose:** CLI menu, manual mode FSM, state management

| Parameter | Value |
|-----------|-------|
| Priority | 6 (LOWEST) |
| Stack Size | 2KB (larger due to FSM + menu printing) |
| Event Triggers | osEventFlagsWait(control_event_flags, ...) |
| Blocking Points | HAL_UART_Receive() with timeout |

**State Machine:**
```
┌─ MODE_SELECTION
│   ├─ Print menu (Manual vs Auto)
│   ├─ HAL_UART_Receive(&rx_byte, 1, HAL_MAX_DELAY)
│   └─ On 'a' or '1': → AUTO_MODE
│
├─ MANUAL_MODE
│   ├─ Loop: Check UART for input (timeout=100ms)
│   ├─ 'm': Start monitoring (calls ADXL355_Read_Data in loop)
│   ├─ 'l': Start logging (non-blocking file writes via file_task)
│   ├─ 'i': Interrupt mode (arm wake-on-motion)
│   └─ 'q': Return to MODE_SELECTION
│
└─ AUTO_MODE
    ├─ osEventFlagsWait(sensor_event_flags, EVT_ACQSTN_DONE)
    ├─ Process upload queue via modem_task
    ├─ osEventFlagsWait(EVT_MOTION_DETECTED) for fast re-arm
    └─ On UART 'q': Return to MODE_SELECTION
```

---

## Synchronization Primitives

### osEventFlags: sensor_event_flags
```c
#define EVT_MOTION_DETECTED     (1 << 0)  // ADXL_INT1 triggered
#define EVT_ACQSTN_DONE        (1 << 1)  // Acquisition settling complete
#define EVT_UPLOAD_DONE        (1 << 2)  // File uploaded successfully
#define EVT_CFG_CHECK          (1 << 3)  // Config check timer fired
#define EVT_SENSOR_FIFO_READY  (1 << 4)  // ADXL FIFO watermark hit
```

**Usage:**
```c
// In interrupt handler:
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == ADXL_INT1_Pin) {
        osEventFlagsSet(sensor_event_flags, EVT_MOTION_DETECTED);
    }
}

// In sensor_task:
osEventFlagsWait(sensor_event_flags, EVT_MOTION_DETECTED, osWaitAny, osWaitForever);
```

### osMutex: sd_mutex
Protects all FAT FS operations (f_open, f_write, f_close, etc.)

```c
// In any task writing to SD:
osMutexAcquire(sd_mutex, osWaitForever);
f_write(&fil, buffer, len, &bw);
f_sync(&fil);
osMutexRelease(sd_mutex);
```

### osMessageQueue: sensor_queue
Ring buffer between sensor_task → modem_task for upload queueing

```c
typedef struct {
    uint32_t timestamp_ms;
    float x_g, y_g, z_g;
    float voltage, current, power;
} SensorReading_t;

// sensor_task pushes:
osMessageQueuePut(sensor_queue, &reading, 0, 0);  // Non-blocking

// file_task pops (optional buffering):
osMessageQueueGet(sensor_queue, &reading, 0, 10);  // 10ms timeout
```

---

## Interrupt Priority Configuration

**Key Setting:** `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 4`

| Interrupt | NVIC Priority | Description |
|-----------|---------------|-------------|
| ADXL_INT1 (EXTI9_5) | 5 | Below MAX_SYSCALL (safe osEventFlagsSet call) |
| UART1_IRQn (Modem) | 5 | Rx complete callback safe for osEventFlagsSet |
| UART2_IRQn (CLI) | 5 | Rx input for control_task |
| SPI1_IRQn (SD) | 6 | Can call osEventFlagsSet (future DMA) |
| SPI2_IRQn (ADXL) | 5 | Can call osEventFlagsSet (future DMA) |
| TIM1_IRQn (Timebase) | 6 | FreeRTOS SysTick replacement |
| Unused ISRs | 7+ | Non-RTOS interrupts |

**Formula:** NVIC_Priority = (BASEPri >> 4) where configLIBRARY_MAX_SYSCALL = bits[7:4]

---

## Stack Size Estimation

| Task | Call Depth | Local Vars | Stack Size | Notes |
|------|-----------|-----------|-----------|-------|
| sensor_task | shallow (3 levels) | ~150B | 512B | ADXL355_Read_Data is minimal |
| modem_task | deep (6+ levels) | ~400B | 1024B | Modem_UploadFile has many local vars |
| file_task | shallow (2 levels) | ~50B | 512B | Mostly messaging + mutex |
| control_task | medium (4 levels) | ~600B | 2048B | Menu sprintf, FSM state vars |
| FreeRTOS idle | - | - | 512B | Default, can be reduced |
| Total User Tasks | - | - | 4.5KB | Well within 192KB RAM budget |

**Remaining RAM:**
- Total: 192KB
- FreeRTOS heap: 10KB (configurable)
- Tasks & stacks: 4.5KB
- Available: ~177KB for buffers, queues, fatfs

---

## Phase-Based Implementation

### Phase 1: FreeRTOS Kernel Integration (1 day)
1. Download FreeRTOS v10.4.x LTS
2. Add sources to Makefile
3. Create FreeRTOSConfig.h with optimized settings
4. Update STM32CubeMX:
   - Change Timebase: SysTick → TIM1
   - Set NVIC priorities (see table above)
5. Create main RTOS init structure

### Phase 2: Basic Task Skeleton (1 day)
1. Create sensor_task stub (prints every 10ms)
2. Create modem_task stub (waits on flag)
3. Create file_task stub (queue handler)
4. Create control_task stub (UART input loop)
5. Update main() to spawn 4 tasks
6. Verify task switching with serial output

### Phase 3: Sensor Task Implementation (1 day)
1. Add ADXL355 data acquisition loop
2. Implement settling detection logic
3. Create sensor_queue with ADXL355_Data_t
4. Test 10ms periodicity with timing logs

### Phase 4: Modem Task + Event Signaling (1 day)
1. Implement EVT_MOTION_DETECTED in EXTI callback
2. Replace g_event_pending with osEventFlagsSet()
3. Add upload queue management
4. Test interrupt → task wake-up

### Phase 5: SD Card Mutex Protection (1 day)
1. Wrap all FAT FS calls in osMutexAcquire/Release
2. Create file_task command queue
3. Update logging in all tasks to use file_task
4. Test concurrent SD access (sensor logs + modem uploads)

### Phase 6: Control Task + Integration (1 day)
1. Migrate Run_Manual_Mode() to control_task
2. Replace HAL_Delay with osThreadYield
3. Update interrupt mode logic for task-aware design
4. Integrate monitoring/logging to use file_task

### Phase 7: Testing & Validation (2 days)
1. Unit tests: Each task individually
2. Integration test: All concurrent
3. Real-world scenario: Sensor trigger → acquisition → upload
4. Profiling: Task timing, stack usage, response latency

---

## Critical Code Changes

### 1. Wake-on-Motion Handler (stm32f4xx_it.c)
**Before:**
```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == ADXL_INT1_Pin) {
        g_event_pending = 1;  // Simple flag
    }
}
```

**After:**
```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == ADXL_INT1_Pin) {
        osEventFlagsSet(sensor_event_flags, EVT_MOTION_DETECTED);
        // FreeRTOS wakes sensor_task immediately (High priority)
    }
}
```

### 2. Main Entry Point (main.c)
**Before:**
```c
if (operation_mode == 2) {
    Run_Auto_Mode();  // Blocking infinite loop
}
```

**After:**
```c
// Create RTOS tasks
osThreadId_t sensor_tid = osThreadNew(sensor_task, NULL, &sensor_attr);
osThreadId_t modem_tid = osThreadNew(modem_task, NULL, &modem_attr);
osThreadId_t file_tid = osThreadNew(file_task, NULL, &file_attr);
osThreadId_t control_tid = osThreadNew(control_task, NULL, &control_attr);

// Start scheduler (never returns)
osKernelStart();
```

### 3. Sensor Task (new file: tasks/sensor_task.c)
```c
void sensor_task(void *arg) {
    ADXL355_Data_t data;
    uint32_t acq_start = 0;
    uint8_t acquiring = 0;
    
    while (1) {
        // Wait for motion trigger
        osEventFlagsWait(sensor_event_flags, EVT_MOTION_DETECTED, 
                         osWaitAny, osWaitForever);
        
        acquiring = 1;
        acq_start = osKernelGetTickCount();
        
        // 60-second acquisition window
        while ((osKernelGetTickCount() - acq_start) < 60000) {
            // Read sensor every 10ms
            osThreadFlagsWait(0, osWaitAll, 10);  // Precise 10ms delay
            ADXL355_Read_Data(&data);
            
            // Push to queue (non-blocking)
            osMessageQueuePut(sensor_queue, &data, 0, 0);
            
            // Check settling condition (silent for 3s)
            // ...settling logic...
            
            if (settling_complete) {
                osEventFlagsSet(sensor_event_flags, EVT_ACQSTN_DONE);
                break;
            }
        }
        
        acquiring = 0;
    }
}
```

### 4. Modem Task (new file: tasks/modem_task.c)
```c
void modem_task(void *arg) {
    char oldest[40];
    
    while (1) {
        // Wait for acquisition or config check
        uint32_t flags = osEventFlagsWait(sensor_event_flags,
                                          EVT_ACQSTN_DONE | EVT_CFG_CHECK,
                                          osWaitAny, osWaitForever);
        
        if (flags & EVT_CFG_CHECK) {
            // Check for remote config every 20 min
            Modem_DownloadConfig(cfg, sizeof(cfg));
            continue;
        }
        
        // Upload pending files
        while (Queue_Peek(oldest, sizeof(oldest)) == 0) {
            
            // Check if sensor triggered during upload
            if (osEventFlagsGet(sensor_event_flags) & EVT_MOTION_DETECTED) {
                Modem_Abort();  // Send Ctrl+C, break upload
                break;
            }
            
            if (Modem_UploadFile(oldest) == HAL_OK) {
                Queue_Pop(oldest, sizeof(oldest));
            } else {
                break;  // Retry next iteration
            }
        }
    }
}
```

---

## File Organization (Post-Migration)

```
Core/
├── Inc/
│   ├── FreeRTOSConfig.h         (NEW)
│   └── tasks.h                  (NEW) - task declarations
├── Src/
│   ├── main.c                   (REFACTORED)
│   ├── stm32f4xx_it.c           (MODIFIED)
│   └── tasks/
│       ├── sensor_task.c        (NEW)
│       ├── modem_task.c         (NEW)
│       ├── file_task.c          (NEW)
│       └── control_task.c       (NEW)
└── Lib/
    └── FreeRTOS/                (NEW - kernel sources)
        ├── include/
        └── portable/
            └── GCC/ARM_CM4F/

Makefile                         (MODIFIED - add FREERTOS paths)
```

---

## Validation Checklist

- [ ] Tasks compile without errors
- [ ] FreeRTOS kernel starts (osKernelStart() called)
- [ ] Serial debug shows task creation messages
- [ ] Sensor task fires every 10ms (verify with timing logs)
- [ ] Motion trigger wakes sensor task within 1ms
- [ ] Acquisition and upload happen concurrently
- [ ] SD card writes protected by mutex (no corruption)
- [ ] Control task accepts CLI input in lowest priority
- [ ] Configuration check fires every 20 minutes
- [ ] Upload can be interrupted by new motion trigger
- [ ] No stack overflow (monitor osThreadGetStackSpace)
- [ ] No deadlock (monitor task states with SEGGER SystemView)

---

## Next Actions

1. **Review & Approve:** Validate architecture with project team
2. **Prepare Environment:** Download FreeRTOS, backup current code
3. **Start Phase 1:** Create FreeRTOS kernel integration
4. **Document Decisions:** Record any deviations from this plan
5. **Testing:** Validate each phase before moving to next

---

**Document Version:** 1.0  
**Last Updated:** April 24, 2026  
**Status:** Ready for Implementation
