# FreeRTOS Migration - Getting Started Guide

**Status:** Ready to begin  
**Estimated Time:** 2-3 weeks (working 2-3 hours daily)  
**Difficulty:** Intermediate (requires FreeRTOS concepts understanding)

---

## Week 1: Foundation Setup & Kernel Integration

### Day 1: Environment Preparation

#### Task 1a: Backup Current Code
```bash
# Create a clean backup branch
cd c:\Users\LindUser\Desktop\HERMES-A1\cmsis_hermes
git init  # if not already git
git add -A
git commit -m "Backup: Pre-FreeRTOS migration baseline"
git branch feature/freertos-migration
```

#### Task 1b: Download FreeRTOS Kernel
1. Go to https://www.freertos.org/
2. Download **FreeRTOS v10.4.x LTS** (Kernel only, not with libraries)
3. Extract to: `c:\Users\LindUser\Desktop\HERMES-A1\cmsis_hermes\Lib\FreeRTOS\`

**Expected Structure:**
```
Lib/FreeRTOS/
├── kernel/
│   ├── include/
│   │   ├── FreeRTOS.h
│   │   ├── task.h
│   │   ├── queue.h
│   │   ├── semphr.h
│   │   ├── event_groups.h
│   │   ├── timers.h
│   │   ├── croutine.h
│   │   ├── list.h
│   │   └── ...
│   └── portable/
│       ├── GCC/ARM_CM4F/
│       │   ├── port.c
│       │   └── portmacro.h
│       └── ...
└── ...
```

#### Task 1c: Add CMSIS-RTOS v2 Adapter
1. Download **CMSIS-FreeRTOS v2** from ARM Embedded Processor Libraries
   - https://github.com/ARM-software/CMSIS-FreeRTOS/releases
2. Extract `CMSIS-FreeRTOS/RTOS2/FreeRTOS/` to your project
3. Key files needed:
   - `cmsis_os2.h` (already in CMSIS, but verify)
   - `os_tick.h`
   - `freertos_os2.c`

**Verification:**
```c
// Should compile without errors:
#include "cmsis_os2.h"

osVersion_t v = osKernelGetInfo(NULL);  // Test CMSIS API
```

---

### Day 2-3: FreeRTOSConfig.h Creation

#### Task 2a: Create FreeRTOSConfig.h
**File:** `Core/Inc/FreeRTOSConfig.h`

```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ================================================================
   STM32F446 Specific Configuration
   ================================================================ */

// System Clock = 16MHz (HSI)
#define configCPU_CLOCK_HZ          ( 16000000UL )

// PortTICK_PERIOD_MS = 1000 / configTICK_RATE_HZ
#define configTICK_RATE_HZ          ( 1000 )        // 1ms tick

// Highest priority (0) reserved for FreeRTOS
// User priorities: 1-4 (lower number = higher priority)
#define configMAX_PRIORITIES        ( 5 )

// IMPORTANT: Interrupt safe syscall threshold
// ISRs with priority >= this value (lower number) can call FreeRTOS APIs
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  ( 4 )

// Task stack sizes (in words, not bytes; word = 4 bytes on Cortex-M4)
#define configMINIMAL_STACK_SIZE    ( 128 )         // Idle task
#define configTOTAL_HEAP_SIZE       ( 10 * 1024 )   // 10KB heap

// Task name/display
#define configMAX_TASK_NAME_LEN     ( 12 )

// 1 = Include full task state info
#define configUSE_TRACE_FACILITY    ( 1 )
#define configUSE_STATS_FORMATTING_FUNCTIONS  ( 1 )

// Kernel hooks
#define configUSE_TICK_HOOK         ( 0 )
#define configUSE_IDLE_HOOK         ( 0 )
#define configUSE_MALLOC_FAILED_HOOK ( 0 )

// Assert support
#define configASSERT( x ) \
    do { if( !(x) ) { printf("FreeRTOS Assert: %s:%d\r\n", __FILE__, __LINE__); while(1); } } while(0)

// Optional: Enable co-routines (not needed for this project)
#define configUSE_CO_ROUTINES       ( 0 )

// Optional: Timers
#define configUSE_TIMERS            ( 1 )
#define configTIMER_TASK_PRIORITY   ( 2 )           // Medium priority
#define configTIMER_QUEUE_LENGTH    ( 10 )
#define configTIMER_TASK_STACK_DEPTH ( 256 )        // 1KB

// Features
#define configUSE_MUTEXES           ( 1 )           // For SD mutex
#define configUSE_COUNTING_SEMAPHORES ( 1 )
#define configUSE_EVENT_GROUPS      ( 1 )           // For sensor_event_flags
#define configUSE_QUEUE_SETS        ( 0 )

// Message queues for sensor data
#define configQUEUE_REGISTRY_SIZE   ( 5 )

// IMPORTANT: Port-specific macro
#define configYIELD_FROM_ISR( xHigherPriorityTaskWoken ) \
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken )

// Stack overflow check (optional, helps debugging)
#define configCHECK_FOR_STACK_OVERFLOW ( 2 )

/* Task Creation Hooks */
#define configUSE_APPLICATION_TASK_TAG 0

/* CMSIS-RTOS v2 Integration */
#define configUSE_POSIX_ERRNO       ( 0 )
#define configUSE_PORT_OPTIMISED_TASK_SELECTION ( 0 )

/* Clock Configuration */
#define configSYSTICK_CLOCK_HZ      ( configCPU_CLOCK_HZ )

#endif /* FREERTOS_CONFIG_H */
```

#### Task 2b: Verify Header Includes
```c
// Core/Inc/main.h - Add at top
#include "cmsis_os2.h"  // CMSIS-RTOS v2 API
#include "FreeRTOSConfig.h"
```

---

### Day 4-5: Makefile & Build System Update

#### Task 3a: Update Makefile
**File:** `Debug/makefile` (or root makefile if using custom build)

Add FreeRTOS sources:
```makefile
# FreeRTOS Paths
FREERTOS_PATH = ../Lib/FreeRTOS/kernel
CMSIS_RTOS2_PATH = ../Lib/CMSIS-FreeRTOS/RTOS2/FreeRTOS

# FreeRTOS Sources
SOURCES += $(FREERTOS_PATH)/tasks.c
SOURCES += $(FREERTOS_PATH)/queue.c
SOURCES += $(FREERTOS_PATH)/list.c
SOURCES += $(FREERTOS_PATH)/timers.c
SOURCES += $(FREERTOS_PATH)/portable/GCC/ARM_CM4F/port.c
SOURCES += $(CMSIS_RTOS2_PATH)/freertos_os2.c

# FreeRTOS Includes
INCLUDES += -I$(FREERTOS_PATH)/include
INCLUDES += -I$(FREERTOS_PATH)/portable/GCC/ARM_CM4F
INCLUDES += -I$(CMSIS_RTOS2_PATH)

# Enable FreeRTOS compilation flags
CFLAGS += -DFREERTOS -DportUSING_MPU_WRAPPERS=0
```

#### Task 3b: Test Compilation
```bash
# Try to compile (will fail without tasks, but should load FreeRTOS)
cd Debug
make clean
make -j4

# Expected errors: undefined reference to tasks (we haven't created them yet)
# This is OK for now
```

---

### Day 6-7: STM32CubeMX Timebase Configuration

#### Task 4a: Change Timebase Source
1. Open **AWTAS_DEFINITIVE.ioc** in STM32CubeMX
2. Go to **System Core → SYS**
3. Find **Timebase Source**
   - Current: `SysTick`
   - Change to: `TIM1` (or TIM2, whichever is available)
4. Enable **TIM1** with:
   - Prescaler: Calculate for 1ms tick
   - Period: (APB2_CLK / Prescaler / 1000) - 1
   - Example: APB2=16MHz → Prescaler=16 → Period=999
5. **Save** the .ioc file
6. **Generate Code** → This regenerates stm32f4xx_hal_msp.c, system_stm32f4xx.c

#### Task 4b: Update NVIC Priorities
In STM32CubeMX → **System Core → NVIC**:

| Interrupt | Priority | Subpriority | Notes |
|-----------|----------|-------------|-------|
| EXTI9_5 (ADXL_INT1) | 5 | 0 | Safe for osEventFlagsSet() |
| USART1 (Modem) | 5 | 1 | Modem data reception |
| USART2 (CLI) | 5 | 2 | CLI input |
| TIM1_UP (Timebase) | 6 | 0 | FreeRTOS tick, lowest priority |

**Key Rule:** All ISRs calling CMSIS APIs must have priority > configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (4)

#### Task 4c: Generate Code & Verify
```bash
# After regenerating code:
cd ../Debug
make clean

# Should compile without errors related to SysTick
# (May have missing task references, still OK)
```

---

## Week 2: Core Task Structure

### Day 1-2: Create Basic Task Framework

#### Task 5a: Create Tasks Header
**File:** `Core/Inc/tasks.h` (NEW)

```c
#ifndef TASKS_H
#define TASKS_H

#include "cmsis_os2.h"
#include <stdint.h>

/* ================================================================
   Event Flags & Synchronization Objects
   ================================================================ */

extern osEventFlagsId_t sensor_event_flags;
extern osMessageQueueId_t sensor_queue;
extern osMutexId_t sd_mutex;

/* Event flag bit definitions */
#define EVT_MOTION_DETECTED     (1U << 0)   // ADXL_INT1 triggered
#define EVT_ACQSTN_DONE        (1U << 1)   // Acquisition settling complete
#define EVT_UPLOAD_DONE        (1U << 2)   // File upload success
#define EVT_CFG_CHECK          (1U << 3)   // Config check timer
#define EVT_SENSOR_FIFO_READY  (1U << 4)   // ADXL FIFO watermark

/* ================================================================
   Sensor Data Structure
   ================================================================ */

typedef struct {
    uint32_t timestamp_ms;      // Relative time since acq start
    float x_g;                  // X acceleration in g
    float y_g;                  // Y acceleration in g
    float z_g;                  // Z acceleration in g
    float voltage;              // Power measurement (placeholder)
    float current;
    float power;
} SensorReading_t;

/* ================================================================
   Task Functions
   ================================================================ */

void sensor_task(void *arg);
void modem_task(void *arg);
void file_task(void *arg);
void control_task(void *arg);

/* ================================================================
   RTOS Initialization
   ================================================================ */

void rtos_init(void);  // Call from main() to create all tasks

#endif /* TASKS_H */
```

#### Task 5b: Create RTOS Initialization
**File:** `Core/Src/rtos_init.c` (NEW)

```c
#include "rtos_init.h"
#include "tasks.h"
#include <stdio.h>

/* Global RTOS objects */
osEventFlagsId_t sensor_event_flags;
osMessageQueueId_t sensor_queue;
osMutexId_t sd_mutex;

void rtos_init(void) {
    printf("[RTOS] Initializing FreeRTOS v10.4 with CMSIS-RTOS v2...\r\n");
    
    /* Create Synchronization Objects */
    
    // Event flags for interrupt signaling
    const osEventFlagsAttr_t evt_attr = {
        .name = "sensor_events"
    };
    sensor_event_flags = osEventFlagsNew(&evt_attr);
    if (!sensor_event_flags) {
        printf("[RTOS] ERROR: Failed to create sensor_event_flags\r\n");
        return;
    }
    printf("[RTOS] Created sensor_event_flags\r\n");
    
    // Message queue for sensor readings
    const osMessageQueueAttr_t queue_attr = {
        .name = "sensor_queue"
    };
    sensor_queue = osMessageQueueNew(
        50,                      // 50 messages max
        sizeof(SensorReading_t),
        &queue_attr
    );
    if (!sensor_queue) {
        printf("[RTOS] ERROR: Failed to create sensor_queue\r\n");
        return;
    }
    printf("[RTOS] Created sensor_queue (50 x %lu bytes)\r\n", sizeof(SensorReading_t));
    
    // Mutex for SD card protection
    const osMutexAttr_t mutex_attr = {
        .name = "sd_mutex",
        .attr_bits = osMutexPrioInherit  // Priority inheritance to avoid priority inversion
    };
    sd_mutex = osMutexNew(&mutex_attr);
    if (!sd_mutex) {
        printf("[RTOS] ERROR: Failed to create sd_mutex\r\n");
        return;
    }
    printf("[RTOS] Created sd_mutex\r\n");
    
    /* Create Tasks */
    
    const osThreadAttr_t sensor_attr = {
        .name = "sensor_task",
        .priority = osPriorityHigh,     // Priority 3
        .stack_size = 512 * 4            // 512 words = 2KB
    };
    osThreadId_t sensor_tid = osThreadNew(sensor_task, NULL, &sensor_attr);
    if (!sensor_tid) {
        printf("[RTOS] ERROR: Failed to create sensor_task\r\n");
        return;
    }
    printf("[RTOS] Created sensor_task (Prio=HIGH/3)\r\n");
    
    const osThreadAttr_t modem_attr = {
        .name = "modem_task",
        .priority = osPriorityAboveNormal,  // Priority 4
        .stack_size = 1024 * 4               // 1KB words = 4KB
    };
    osThreadId_t modem_tid = osThreadNew(modem_task, NULL, &modem_attr);
    if (!modem_tid) {
        printf("[RTOS] ERROR: Failed to create modem_task\r\n");
        return;
    }
    printf("[RTOS] Created modem_task (Prio=ABOVE_NORMAL/4)\r\n");
    
    const osThreadAttr_t file_attr = {
        .name = "file_task",
        .priority = osPriorityNormal,    // Priority 5
        .stack_size = 512 * 4             // 512 words = 2KB
    };
    osThreadId_t file_tid = osThreadNew(file_task, NULL, &file_attr);
    if (!file_tid) {
        printf("[RTOS] ERROR: Failed to create file_task\r\n");
        return;
    }
    printf("[RTOS] Created file_task (Prio=NORMAL/5)\r\n");
    
    const osThreadAttr_t control_attr = {
        .name = "control_task",
        .priority = osPriorityBelowNormal,  // Priority 6
        .stack_size = 2048 * 4               // 2KB words = 8KB
    };
    osThreadId_t control_tid = osThreadNew(control_task, NULL, &control_attr);
    if (!control_tid) {
        printf("[RTOS] ERROR: Failed to create control_task\r\n");
        return;
    }
    printf("[RTOS] Created control_task (Prio=BELOW_NORMAL/6)\r\n");
    
    printf("[RTOS] All tasks created successfully!\r\n");
    printf("[RTOS] Starting kernel...\r\n");
    
    // Start the RTOS scheduler (this never returns)
    osKernelStart();
}
```

#### Task 5c: Create Stub Tasks
**File:** `Core/Src/tasks/sensor_task.c` (NEW)

```c
#include "tasks.h"
#include <stdio.h>

void sensor_task(void *arg) {
    printf("[SENSOR_TASK] Started\r\n");
    
    uint32_t count = 0;
    while (1) {
        // Wait for motion trigger
        uint32_t flags = osEventFlagsWait(
            sensor_event_flags,
            EVT_MOTION_DETECTED,
            osWaitAny,
            osWaitForever
        );
        
        if (flags & EVT_MOTION_DETECTED) {
            printf("[SENSOR] Motion detected! (count=%lu)\r\n", ++count);
            
            // Simulate acquisition for 5 seconds
            for (int i = 0; i < 50; i++) {
                SensorReading_t reading = {
                    .timestamp_ms = i * 100,
                    .x_g = 0.1f * (i % 10),
                    .y_g = 0.05f,
                    .z_g = 1.0f,
                    .voltage = 3.3f,
                    .current = 0.5f,
                    .power = 1.65f
                };
                osMessageQueuePut(sensor_queue, &reading, 0, 0);
                osThreadYield();
            }
            
            printf("[SENSOR] Settling complete, waiting for next motion...\r\n");
        }
    }
}
```

**File:** `Core/Src/tasks/modem_task.c` (NEW)

```c
#include "tasks.h"
#include <stdio.h>

void modem_task(void *arg) {
    printf("[MODEM_TASK] Started\r\n");
    
    while (1) {
        printf("[MODEM] Waiting for acquisition done...\r\n");
        osThreadYield();
    }
}
```

**File:** `Core/Src/tasks/file_task.c` (NEW)

```c
#include "tasks.h"
#include <stdio.h>

void file_task(void *arg) {
    printf("[FILE_TASK] Started\r\n");
    
    while (1) {
        SensorReading_t reading;
        uint32_t status = osMessageQueueGet(
            sensor_queue,
            &reading,
            0,
            1000  // 1 second timeout
        );
        
        if (status == osOK) {
            printf("[FILE] Got sensor reading: X=%.2f g\r\n", reading.x_g);
        }
    }
}
```

**File:** `Core/Src/tasks/control_task.c` (NEW)

```c
#include "tasks.h"
#include <stdio.h>

void control_task(void *arg) {
    printf("[CONTROL_TASK] Started\r\n");
    
    while (1) {
        printf("[CONTROL] Lowest priority task running...\r\n");
        osThreadYield();
    }
}
```

---

### Day 3-4: Update main() and Integrate

#### Task 6a: Update main.c
**File:** `Core/Src/main.c`

```c
// At top, after includes
#include "tasks.h"
#include "rtos_init.h"

// In main() function, replace the old infinite loop:

int main(void) {
    /* Standard STM32 Init */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART2_UART_Init();
    MX_SPI2_Init();
    MX_USART1_UART_Init();
    
    /* Initialize Peripherals */
    Modem_Init(&huart1);
    printf("\r\n--- HERMES-A1 INITIALIZING (FreeRTOS Edition) ---\r\n");
    Modem_PowerOn();
    if (ADXL355_Init(&hspi2)) {
        printf("[SENSOR] ADXL355 Initialized Successfully\r\n");
        ADXL355_LevelToZero();
    } else {
        printf("[SENSOR] ADXL355 Initialization Failed\r\n");
    }
    
    if (sd_mount() == 0) {
        fres = FR_OK;
        printf("[STORAGE] SD Card Mounted\r\n");
    } else {
        fres = FR_NOT_READY;
        printf("[STORAGE] SD Card Mount Failed\r\n");
    }
    
    /* Start FreeRTOS Scheduler */
    printf("[MAIN] Starting FreeRTOS kernel...\r\n");
    rtos_init();  // Creates tasks and starts scheduler
    
    // Never reaches here
    return 0;
}
```

#### Task 6b: Update Interrupt Handler
**File:** `Core/Src/stm32f4xx_it.c`

```c
// At top, add:
#include "tasks.h"

// Replace HAL_GPIO_EXTI_Callback:
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == ADXL_INT1_Pin) {
        // Signal sensor_task via event flags (ISR-safe)
        osEventFlagsSet(sensor_event_flags, EVT_MOTION_DETECTED);
    }
}
```

#### Task 6c: First Compilation Test
```bash
cd Debug
make clean
make -j4 2>&1 | head -50

# Expected output:
# - Successful compilation of all source files
# - FreeRTOS symbols resolved
# - New task objects linked
# - Size of binary increased by ~20KB (FreeRTOS)
```

---

### Day 5-7: Testing & Debugging

#### Task 7a: Test Basic Boot
```bash
# If using STM32CubeMX IDE (STM32CubeIDE):
# 1. Build project
# 2. Program STM32F446 with debugger
# 3. Open serial console (115200 baud)

# Expected output:
# [RTOS] Initializing FreeRTOS v10.4 with CMSIS-RTOS v2...
# [RTOS] Created sensor_event_flags
# [RTOS] Created sensor_queue
# [RTOS] Created sd_mutex
# [RTOS] Created sensor_task (Prio=HIGH/3)
# [RTOS] Created modem_task (Prio=ABOVE_NORMAL/4)
# [RTOS] Created file_task (Prio=NORMAL/5)
# [RTOS] Created control_task (Prio=BELOW_NORMAL/6)
# [RTOS] All tasks created successfully!
# [RTOS] Starting kernel...
# [SENSOR_TASK] Started
# [MODEM_TASK] Started
# [FILE_TASK] Started
# [CONTROL_TASK] Started
```

#### Task 7b: Test Manual Trigger (Desktop Test)
```c
// In control_task or via UART command handler:
printf("\r\nManual: Trigger motion event? (press 'T'): ");
uint8_t cmd = 0;
if (HAL_UART_Receive(&huart2, &cmd, 1, 1000) == HAL_OK && cmd == 'T') {
    printf("Triggering EVT_MOTION_DETECTED\r\n");
    osEventFlagsSet(sensor_event_flags, EVT_MOTION_DETECTED);
}
```

#### Task 7c: Monitor Task States
```bash
# Use FreeRTOS trace tools:
# - SEGGER SystemView (requires probe)
# - Print vTaskList() output periodically
# - Monitor stack space with osThreadGetStackSpace()

void print_task_stats(void) {
    osStatus_t status;
    
    printf("\r\n=== FreeRTOS Task Statistics ===\r\n");
    printf("Kernel Ticks: %lu\r\n", osKernelGetTickCount());
    printf("Kernel State: ");
    osKernelState_t state = osKernelGetState();
    printf((state == osKernelReady) ? "READY" :
           (state == osKernelRunning) ? "RUNNING" : "ERROR");
    printf("\r\n");
}
```

---

## Validation Checklist - Week 1 & 2

- [ ] FreeRTOS sources downloaded and integrated
- [ ] FreeRTOSConfig.h created with STM32F446 settings
- [ ] Makefile updated with FreeRTOS build paths
- [ ] STM32CubeMX: Timebase changed to TIM1
- [ ] STM32CubeMX: NVIC priorities configured (ADXL=5, Timebase=6)
- [ ] rtos_init.c compiles without errors
- [ ] Four task stubs created and compile
- [ ] main() calls rtos_init() successfully
- [ ] Serial output shows all tasks created
- [ ] Manual motion trigger via UART works
- [ ] No hard faults or system resets on boot
- [ ] Task switching visible in debug output

---

## Troubleshooting Common Issues

### Issue: "undefined reference to osEventFlagsNew"
**Solution:** Verify `freertos_os2.c` is in Makefile SOURCES

### Issue: "EXTI9_5_IRQHandler: multiple definition"
**Solution:** Make sure stm32f4xx_it.c has only one HAL_GPIO_EXTI_Callback definition

### Issue: "Hard Fault on osKernelStart()"
**Solution:** 
- Check stack sizes in tasks (may be too small)
- Verify NVIC priorities are correct
- Check FreeRTOSConfig.h for syntax errors

### Issue: Tasks don't run, just freeze
**Solution:**
- Ensure osKernelStart() is called (no code after it!)
- Check if SD card mount is blocking (use timeout)
- Verify Modem_PowerOn() doesn't block forever

---

## Next Steps (Week 3+)

Once Week 1-2 foundation is solid:
1. **Implement real sensor_task** with ADXL355 polling loop
2. **Implement real modem_task** with queue-based uploads
3. **Implement real file_task** with mutex-protected SD writes
4. **Migrate manual mode** to control_task
5. **Comprehensive testing** with concurrent operations

**Total Migration Time:** 3 weeks for full functional replacement
**Risk Level:** LOW (FreeRTOS is production-proven, changes are isolated to task boundaries)

---

**Document Version:** 1.0  
**Prepared:** April 2026  
**Ready:** Yes, start immediately
