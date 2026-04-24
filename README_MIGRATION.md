# HERMES-A1 FreeRTOS Migration - Complete Planning Package

## 📖 Documentation Index

### 🚀 Start Here (Your Roadmap)

**1. [MIGRATION_SUMMARY.md](./MIGRATION_SUMMARY.md)** ⭐ **START HERE**
   - Executive overview of all changes
   - Before/After architecture comparison
   - Quick reference for task design
   - Timeline and success criteria
   - **Read time:** 15 minutes
   - **Next:** Go to #2

**2. [GETTING_STARTED.md](./GETTING_STARTED.md)** ⭐ **WEEK 1 GUIDE**
   - Step-by-step implementation (Day 1-7)
   - FreeRTOS kernel setup
   - Makefile updates
   - STM32CubeMX configuration
   - First compilation test
   - **Read time:** 30 minutes (skim for week 1 only)
   - **Coding time:** 3-4 hours for Week 1
   - **Next:** Follow Days 1-7 sequentially

**3. [FREERTOS_MIGRATION_PLAN.md](./FREERTOS_MIGRATION_PLAN.md)** 📋 **DETAILED REFERENCE**
   - Complete architectural blueprint (15+ pages)
   - All 4 tasks specified in detail
   - Synchronization primitives documented
   - Interrupt priority configuration
   - Code examples for each task
   - Performance analysis & timing
   - Integration checklist
   - **Read time:** 45 minutes (reference as needed)
   - **When to use:** Design decisions, task implementation phase
   - **Next:** Use when implementing sensor_task (Week 2)

**4. [WAKE_ON_MOTION_REFACTORING.md](./WAKE_ON_MOTION_REFACTORING.md)** 🔔 **ISR MIGRATION**
   - Current bare-metal interrupt implementation analyzed
   - FreeRTOS interrupt handler patterns
   - Complete sensor_task implementation with state machine
   - Testing strategy for motion detection
   - Detailed timing analysis (50ms → 51ms latency)
   - **Read time:** 30 minutes
   - **When to use:** When modifying stm32f4xx_it.c (Day 6 of Week 1)
   - **Next:** Reference during sensor_task implementation

---

## 🎯 Recommended Reading Order

### For Quick Overview (30 min)
1. This file (README)
2. MIGRATION_SUMMARY.md sections: "What We've Accomplished" + "New Architecture"

### For Full Understanding (90 min)
1. MIGRATION_SUMMARY.md (15 min)
2. FREERTOS_MIGRATION_PLAN.md - Executive Summary & Task Specification (45 min)
3. GETTING_STARTED.md - Week 1 overview (15 min)
4. WAKE_ON_MOTION_REFACTORING.md - Critical handler changes (15 min)

### For Implementation (Week 1-3)
- **Week 1:** Follow GETTING_STARTED.md Day 1-7 (reference FreeRTOS Kernel Integration section)
- **Week 2:** Implement sensor_task using FREERTOS_MIGRATION_PLAN.md Task 1 section
- **Week 3:** Implement modem_task using Task 2 section

---

## 🔑 Key Concepts at a Glance

### Current Architecture (Bare-Metal)
- ❌ Single volatile flag `g_event_pending` (can miss events)
- ❌ Main loop polls every 50ms (latency, power waste)
- ❌ Sequential operations (sensor blocks modem)
- ❌ No resource protection on SD card

### New Architecture (FreeRTOS)
- ✅ osEventFlags with 8 bits (queue events reliably)
- ✅ Task-based event-driven (ISR wakes task, <1ms latency)
- ✅ 4 concurrent tasks (sensor + modem in parallel)
- ✅ osMutex on SD card (safe concurrent access)

### Main Improvement: Wake-on-Motion
```
Old: EXTI → g_event_pending=1 → Main loop polls → IDLE_LOW_POWER check → 100ms delay
New: EXTI → osEventFlagsSet() → FreeRTOS scheduler → sensor_task wakes → 51ms delay
                                                                          ↑
                                                            2x faster! ✓
```

---

## 📊 Files Created for You

### Planning Documents (Read These)
| File | Purpose | Size |
|------|---------|------|
| MIGRATION_SUMMARY.md | High-level overview | 15KB |
| FREERTOS_MIGRATION_PLAN.md | Detailed architecture & specs | 25KB |
| GETTING_STARTED.md | Step-by-step implementation | 18KB |
| WAKE_ON_MOTION_REFACTORING.md | ISR & sensor_task deep-dive | 20KB |
| **README.md** (this file) | Navigation & quick reference | 6KB |

### To Be Created by You (Following Guides)
| File | Phase | Location |
|------|-------|----------|
| FreeRTOSConfig.h | Week 1, Day 2 | Core/Inc/ |
| rtos_init.c | Week 2, Day 1 | Core/Src/ |
| sensor_task.c | Week 2, Day 1 | Core/Src/tasks/ |
| modem_task.c | Week 2, Day 1 | Core/Src/tasks/ |
| file_task.c | Week 2, Day 1 | Core/Src/tasks/ |
| control_task.c | Week 2, Day 1 | Core/Src/tasks/ |
| tasks.h | Week 2, Day 1 | Core/Inc/ |

---

## ⏱️ Time Estimate

### Total Effort
- **Planning & Reading:** 2-3 hours (you're doing this now ✓)
- **Week 1 (Foundation):** 4-5 hours
  - Download & extract FreeRTOS (1h)
  - Create FreeRTOSConfig.h (1h)
  - Update Makefile (30 min)
  - STM32CubeMX config (1.5h)
  - First compilation test (30 min)
- **Week 2 (Core Tasks):** 8-10 hours
  - Create 5 new source files (3h)
  - First boot test (1h)
  - Debug serial output (2h)
  - Task communication testing (2-3h)
- **Week 3 (Integration):** 10-12 hours
  - Implement real sensor_task (3h)
  - Implement real modem_task (4h)
  - SD card mutex integration (2h)
  - Comprehensive testing (2-3h)

**Total:** 24-30 hours over 3 weeks = ~2-3 hours per day

---

## 🛠️ Prerequisites

Before starting, ensure you have:
- ✅ STM32CubeIDE or compatible IDE
- ✅ STM32F446 development board
- ✅ Serial terminal (PuTTY, TeraTerm)
- ✅ Understanding of FreeRTOS concepts (recommend 1-hour intro video)
- ✅ Ability to download ~2MB files (FreeRTOS kernel)
- ✅ Basic C coding experience
- ✅ Access to this repository (for reference documents)

---

## 🚨 Critical Reminders

### Must Remember
1. **Change SysTick to TIM1** in STM32CubeMX (FreeRTOS requirement)
2. **Set NVIC priorities:** ADXL_INT1 = 5, Timebase = 6
3. **Use osEventFlagsSet()** in ISR (not osThreadNotify)
4. **Protect SD card** with osMutex (no concurrent FAT FS)
5. **Follow task stacks** exactly (512B, 1K, 2K as specified)

### Common Mistakes to Avoid
- ❌ Forgetting to call osKernelStart() (main() must not return)
- ❌ Using HAL_Delay() in ISR (only ISR-safe APIs allowed)
- ❌ Setting ISR priorities above 4 (FreeRTOS conflict)
- ❌ Making stacks too small (check osThreadGetStackSpace)
- ❌ Not protecting FAT FS with mutex (data corruption risk)

---

## 📞 Support & Questions

### If You Get Stuck
1. **Compilation errors:** Check "Troubleshooting" in GETTING_STARTED.md
2. **Runtime crashes:** Look at FREERTOS_MIGRATION_PLAN.md "Validation Checklist"
3. **Task not running:** Verify osThreadNew() returned valid ID in rtos_init.c
4. **Interrupt not working:** Check NVIC priorities in STM32CubeMX

### FreeRTOS Resources
- Official docs: https://www.freertos.org/
- API reference: https://www.freertos.org/a00106.html
- CMSIS-RTOS v2: https://arm-software.github.io/CMSIS_5/RTOS2/

---

## ✅ Success Criteria

You'll know the migration is successful when:

### Week 1 Success
- [ ] FreeRTOS kernel boots without crash
- [ ] Serial debug shows "All tasks created successfully!"
- [ ] No hard faults or system resets
- [ ] Compilation completes in < 30 seconds

### Week 2 Success
- [ ] Manual motion trigger (UART 'T') wakes sensor_task
- [ ] sensor_task prints every 10ms
- [ ] Message queue contains sensor readings
- [ ] file_task dequeues and processes data

### Week 3 Success (Complete)
- [ ] Sensor acquisition + modem upload happen in parallel
- [ ] Upload can be interrupted by new motion trigger
- [ ] SD card writes are mutex-protected (no corruption)
- [ ] All CLI commands work (manual mode)
- [ ] Auto mode FSM transitions correctly
- [ ] Response latency < 60ms (improvement from 100ms)

---

## 🎓 Learning Path (Optional)

If FreeRTOS is new to you, consider:
1. **Watch:** "Introduction to FreeRTOS" (YouTube, ~60 min)
2. **Read:** FREERTOS_MIGRATION_PLAN.md "Task Specification" section
3. **Code:** Follow GETTING_STARTED.md examples line-by-line
4. **Debug:** Use serial output to understand task switching
5. **Experiment:** Modify task priorities and see impact on timing

---

## 📝 Decision Log

This migration package documents:
- **What changed:** 5 critical pain points → 5 improvements
- **Why it changed:** Latency, concurrency, resource safety
- **How it changed:** FSM → Event-driven tasks, flags → ISR signaling
- **When changed:** Scheduled over 3 weeks, part-time effort

---

## 🎯 Next Immediate Actions

### Right Now (Today)
1. Read MIGRATION_SUMMARY.md (15 min)
2. Open GETTING_STARTED.md (bookmark for Week 1)
3. Download FreeRTOS v10.4 LTS from freertos.org (30 min)

### This Week
1. Follow GETTING_STARTED.md Days 1-3 (FreeRTOSConfig.h)
2. Update your Makefile (Day 4-5)
3. Configure STM32CubeMX (Day 6-7)
4. First compilation test

### Next Week
1. Create rtos_init.c and task stubs
2. Boot test and verify task creation
3. Implement real sensor_task

**Total time to Week 1 completion:** 4-5 hours spread over 7 days (30 min - 1 hour daily)

---

## 📄 Document Versions

- **README.md:** v1.0 (Apr 24, 2026)
- **MIGRATION_SUMMARY.md:** v1.0 (Apr 24, 2026)
- **FREERTOS_MIGRATION_PLAN.md:** v1.0 (Apr 24, 2026)
- **GETTING_STARTED.md:** v1.0 (Apr 24, 2026)
- **WAKE_ON_MOTION_REFACTORING.md:** v1.0 (Apr 24, 2026)

---

## 🎉 Ready to Begin?

### Start Here → [GETTING_STARTED.md](./GETTING_STARTED.md) - Day 1

Good luck! The migration is well-planned, documented, and achievable. You've got this! 🚀

---

**Questions?** Refer to the relevant document section or check the "Troubleshooting" guides in GETTING_STARTED.md.

**Want to skip ahead?** Reference FREERTOS_MIGRATION_PLAN.md directly for specific task implementations.

**Need architectural clarity?** Review MIGRATION_SUMMARY.md task diagrams and timing analysis.

---

**Happy Coding!** 🔥
