# FreeRTOS Migration - Executive Summary

## What We've Accomplished

### 📋 Analysis Phase ✓
- **Reviewed** current wake-on-motion implementation (single volatile flag, polling-based)
- **Identified** 5 critical pain points:
  1. Single flag can miss rapid triggers
  2. Main loop blocks entire system with HAL_Delay()
  3. No concurrent operations (sensor vs. modem)
  4. Response latency: ~100ms (should be ~50ms)
  5. No resource protection for shared SD card

### 📚 Documentation Created

| Document | Purpose | Pages |
|----------|---------|-------|
| [FREERTOS_MIGRATION_PLAN.md](./FREERTOS_MIGRATION_PLAN.md) | Complete architectural blueprint | 15+ |
| [WAKE_ON_MOTION_REFACTORING.md](./WAKE_ON_MOTION_REFACTORING.md) | Detailed interrupt handler migration | 12+ |
| [GETTING_STARTED.md](./GETTING_STARTED.md) | Step-by-step implementation guide | 20+ |

---

## Quick Reference: New Architecture

### Before (Bare-Metal)
```
┌─ EXTI Interrupt
│     ↓
│  g_event_pending = 1   (simple flag, can miss events)
│     ↓
│  Main loop polls every 50ms
│     ↓
│  HAL_Delay(10) blocks CPU
│     ↓
│  Acquisition blocks modem operations
└─ Sequential, low concurrency
```

### After (FreeRTOS + CMSIS-RTOS v2)
```
┌─ EXTI Interrupt (ISR Priority = 5)
│     ↓
│  osEventFlagsSet(EVT_MOTION_DETECTED)  (safe, non-blocking)
│     ↓
│  FreeRTOS scheduler wakes sensor_task
│     ↓
│  ┌─────────────────┬──────────────┬─────────────┐
│  │ sensor_task     │ modem_task   │ file_task   │
│  │ (Prio=3)        │ (Prio=4)     │ (Prio=5)    │
│  │ HIGH            │ MEDIUM       │ LOW         │
│  └─────────────────┴──────────────┴─────────────┘
│     ↓
│  • Sensor acquires data every 10ms (blocked on settling)
│  • Modem uploads in parallel (blocked on upload done)
│  • SD writes protected by mutex
│  • All can run concurrently
└─ Event-driven, high concurrency
```

---

## Task Design at a Glance

### 🎯 sensor_task (Priority 3 = HIGH)
- **Cycle:** 10ms polling (100 Hz)
- **Trigger:** EXTI interrupt → osEventFlagsSet() → Task wakes
- **Output:** Pushes SensorReading_t to osMessageQueue
- **Duration:** Typically 5-60 seconds per event
- **Stack:** 512 bytes

**State Machine:**
```
WAITING → Motion detected → ACQUIRING → Silence detected → WAITING
                          (10ms loop)   (3 sec threshold)
```

---

### 📡 modem_task (Priority 4 = MEDIUM)
- **Trigger:** EVT_ACQSTN_DONE flag
- **Job:** Upload queued files to Google Drive
- **Duration:** 1-5+ minutes per file
- **Abort:** Interrupted by new motion (EVT_MOTION_DETECTED)
- **Stack:** 1KB

**Flow:**
```
Queue_Peek(oldest) → Modem_UploadFile() → ✓ Success → Queue_Pop()
                                        ✗ Motion → Abort & retry later
```

---

### 💾 file_task (Priority 5 = LOW)
- **Mechanism:** Mutex-protected FAT FS operations
- **Input:** Command queue from sensor & control tasks
- **Operations:** f_open, f_write, f_sync, f_close
- **Duration:** Microseconds per call (non-blocking to other tasks)
- **Stack:** 512 bytes

**Protection Pattern:**
```
Any task needing SD:
  osMutexAcquire(sd_mutex, osWaitForever)
  f_write(...) / f_open(...) / etc.
  osMutexRelease(sd_mutex)
```

---

### ⚙️ control_task (Priority 6 = LOWEST)
- **Role:** CLI menu, manual mode FSM, config management
- **Input:** UART2 (serial terminal)
- **Modes:** Manual (interactive) vs. Auto (event-driven)
- **Stack:** 2KB (largest, due to menu strings + FSM state)

**State Machine:**
```
MODE_SELECT → MANUAL_MODE ──┐
           ↗ AUTO_MODE     │
                            │
           ← ← ← ← ← ← ← ←─┘
```

---

## Key Synchronization Primitives

### 1️⃣ osEventFlags (sensor_event_flags)
8 bits for interrupt signaling:
```
Bit 0: EVT_MOTION_DETECTED        (EXTI → sensor_task)
Bit 1: EVT_ACQSTN_DONE            (sensor_task → modem_task)
Bit 2: EVT_UPLOAD_DONE            (modem_task → control_task)
Bit 3: EVT_CFG_CHECK              (Timer → modem_task)
Bit 4: EVT_SENSOR_FIFO_READY      (Reserved for DMA future use)
```

### 2️⃣ osMessageQueue (sensor_queue)
Ring buffer for sensor data:
```
sensor_task writes → SensorReading_t → file_task reads
                                    → modem_task reads
```
Size: 50 messages × 28 bytes = 1.4KB

### 3️⃣ osMutex (sd_mutex)
Protects all FAT FS operations:
```
Priority inheritance enabled (prevents priority inversion)
Acquired by: control_task, file_task
Maximum wait time: <100ms (file operations are fast)
```

---

## Performance Improvements

### Motion Response Latency
| Phase | Old | New | Gain |
|-------|-----|-----|------|
| Sensor sampling | 50ms | 50ms | - |
| Interrupt to task wake | 50ms | <1ms | **49ms faster** |
| Total latency | ~100ms | ~51ms | **2x faster** |

### Concurrency
| Operation | Old | New |
|-----------|-----|-----|
| Sensor acq + upload | Blocked sequential | Parallel |
| Modem interrupt handling | Polling (busy) | Event-driven (sleep) |
| SD card access | No protection | Mutex-safe |
| Menu response | Slow (FSM deep) | Fast (async) |

### Power Efficiency
| Scenario | Old | New |
|----------|-----|-----|
| Idle waiting (10 sec) | 100% CPU (busy loop) | <1% CPU (blocked in kernel) |
| During upload (5 min) | Sensor blocked | Sensor running @ full speed |

---

## File Structure Post-Migration

```
Core/
├── Inc/
│   ├── FreeRTOSConfig.h          ← NEW
│   └── tasks.h                   ← NEW
├── Src/
│   ├── main.c                    ← MODIFIED (add rtos_init call)
│   ├── rtos_init.c               ← NEW (task creation)
│   ├── stm32f4xx_it.c            ← MODIFIED (ISR → osEventFlagsSet)
│   └── tasks/                    ← NEW DIRECTORY
│       ├── sensor_task.c
│       ├── modem_task.c
│       ├── file_task.c
│       └── control_task.c
└── ...

Lib/
└── FreeRTOS/                     ← NEW DIRECTORY
    ├── kernel/
    │   ├── include/
    │   │   └── *.h (15+ headers)
    │   └── portable/GCC/ARM_CM4F/
    └── ...

Debug/
├── makefile                      ← MODIFIED (add FreeRTOS sources)
└── objects.mk                    ← AUTO-UPDATED
```

---

## Implementation Timeline

### Week 1: Foundation (7 days)
- Day 1-3: Download FreeRTOS, create FreeRTOSConfig.h
- Day 4-5: Update Makefile, verify compilation
- Day 6-7: STM32CubeMX timebase change, NVIC config

**Deliverable:** Kernel boots, 4 stub tasks created

### Week 2: Core Tasks (5 days)
- Day 1-2: Create task headers & stubs
- Day 3: First compilation & manual trigger test
- Day 4-5: Debug serial output, verify task switching

**Deliverable:** Tasks communicate via event flags

### Week 3: Integration (3-5 days)
- Implement real sensor_task with ADXL355 polling
- Implement real modem_task with upload logic
- Integrate file_task with SD mutex protection
- Comprehensive concurrent operation testing

**Deliverable:** Sensor + modem fully functional in parallel

---

## Critical Success Factors

✅ **Must Do:**
1. Change SysTick → TIM1 in STM32CubeMX (FreeRTOS requirement)
2. Set NVIC interrupt priorities < MAX_SYSCALL_INTERRUPT_PRIORITY
3. Use osEventFlagsSet() in ISR (NOT osThreadNotify)
4. Protect all FAT FS operations with sd_mutex

⚠️ **Watch Out:**
1. Stack overflow if stacks too small (monitor osThreadGetStackSpace())
2. Priority inversion if mutex doesn't use osMutexPrioInherit
3. ISR context: can only call ISR-safe APIs (osEventFlagsSet, no f_write)
4. Deadlock: don't acquire mutexes in nested priority tasks without care

---

## Quick Start Commands

```bash
# Download FreeRTOS
# https://www.freertos.org/ → Kernel only (v10.4.x LTS)

# Extract to project
unzip FreeRTOS_Kernel_V10.4.x.zip -d Lib/FreeRTOS/

# Download CMSIS-FreeRTOS
# https://github.com/ARM-software/CMSIS-FreeRTOS/releases

# Extract CMSIS adapter
unzip CMSIS-FreeRTOS-main.zip -d Lib/CMSIS-FreeRTOS/

# Follow GETTING_STARTED.md for detailed steps
```

---

## Testing Checklist

- [ ] Kernel boots without crash
- [ ] All 4 tasks created and running
- [ ] Manual motion trigger (UART 'T' command) wakes sensor_task
- [ ] Sensor data queued successfully
- [ ] File task dequeues and prints samples
- [ ] Modem task responds to upload triggers
- [ ] Concurrent ops: sensor running during upload
- [ ] SD card writes protected (no corruption)
- [ ] No stack overflows (monitor osThreadGetStackSpace)
- [ ] No deadlocks (task states visible in debug)

---

## Resources & References

- **FreeRTOS Official:** https://www.freertos.org/
- **CMSIS-RTOS v2 API:** https://arm-software.github.io/CMSIS_5/RTOS2/
- **STM32F4 HAL:** https://www.st.com/en/microcontrollers/stm32f4-series.html
- **ARM Cortex-M4 Interrupts:** https://www.arm.com/

---

## Contact & Decisions Log

**Decision 1:** Use osEventFlags instead of osThreadNotify
- **Rationale:** Supports multiple bits (up to 8), easier to add future event types
- **Alternative Considered:** Direct task wake (osSemaphore) - less flexible

**Decision 2:** Priority order: sensor(3) > modem(4) > file(5) > control(6)
- **Rationale:** Sensor is real-time critical (10ms), modem/file can wait, control is UI (lowest)
- **Impact:** Sensor gets CPU time even during upload

**Decision 3:** Mutex with priority inheritance for sd_mutex
- **Rationale:** Prevents priority inversion if low-priority file_task holds SD lock
- **Impact:** Small overhead but prevents rare deadlock scenarios

---

## Next Immediate Actions

1. **Read:** GETTING_STARTED.md (20 min read)
2. **Setup:** Download FreeRTOS and CMSIS-FreeRTOS (30 min)
3. **Code:** Follow Day 1-2 tasks (FreeRTOSConfig.h creation) (1-2 hours)
4. **Build:** First compilation test (30 min)
5. **Debug:** Serial output verification (1 hour)

**Total this week:** 5-6 hours → Core foundation complete

---

**Document Version:** 1.0  
**Prepared:** April 24, 2026  
**Status:** ✅ Ready for Implementation  
**Estimated Completion:** May 15, 2026 (3 weeks part-time)
