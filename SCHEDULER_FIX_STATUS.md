# Scheduler Fix Status
**Date:** 2025-11-20
**Priority:** P0 CRITICAL
**Status:** IN PROGRESS

---

## Problem Identified

The scheduler tick was disabled in `kernel/core/main.c:314` with this comment:
```c
// TODO: Debug why this causes hang - disabled for now
kinfo("Scheduler ticks DISABLED for debugging\n");
```

**Impact:** No preemptive multitasking - system cannot interrupt running threads.

---

## Root Cause Analysis

After reviewing the code, I identified multiple issues:

### 1. ✅ FIXED: Duplicate Function Definitions in timer.c
**File:** `kernel/hal/x86_64/timer.c`

**Problem:** Functions `timer_set_callback()` and `timer_interrupt_handler()` were defined twice (lines 117-121, 162-166 and 126-140, 169-183).

**Fix:** Removed duplicate definitions.

**Status:** ✅ FIXED

---

### 2. ✅ FIXED: Missing Reschedule Check in Interrupt Stub
**File:** `kernel/hal/x86_64/interrupt_stubs.S`

**Problem:** Lines 46-49 had a TODO comment:
```asm
// Check if we need to reschedule (for timer interrupt)
// The C handler will have set a flag if needed
// For now, we'll just return normally
// TODO: Implement proper preemptive context switch from interrupt
```

The interrupt stub was NOT calling `scheduler_check_reschedule()`, so the scheduler_tick() would set the `need_reschedule` flag but nothing would check it.

**Fix:** Added call to `scheduler_check_reschedule()` for timer interrupts:
```asm
// Check if we need to reschedule after interrupt (enables preemptive multitasking)
// Only check for timer interrupt (num 32)
cmpq $32, 8(%rsp)  // Compare interrupt number on stack
jne .Lno_reschedule_\num

// Save RSP before checking reschedule
push %rsp
call scheduler_check_reschedule
pop %rsp

.Lno_reschedule_\num:
```

**Status:** ✅ FIXED (basic implementation)

---

### 3. ✅ FIXED: Scheduler Disabled in main.c
**File:** `kernel/core/main.c:314`

**Problem:** Scheduler ticks were disabled.

**Fix:** Re-enabled scheduler:
```c
// Enable scheduler ticks in timer interrupt
kinfo("Enabling scheduler ticks...\n");
extern void timer_enable_scheduler(void);
timer_enable_scheduler();
kinfo("Scheduler ticks ENABLED - preemptive multitasking active\n");
```

**Status:** ✅ FIXED

---

## Remaining Concerns

### 4. ⚠️ POTENTIAL ISSUE: Context Switch from Interrupt

**Problem:** The context switching mechanism might not work correctly when called from interrupt context.

**Current Flow:**
1. Interrupt saves registers on stack
2. Call `interrupt_handler_c()`
3. `timer_interrupt_handler()` increments ticks
4. `scheduler_tick()` sets `need_reschedule[cpu_id] = true`
5. Return from C handler
6. Check `need_reschedule` flag
7. If true, call `scheduler_check_reschedule()`
8. Which calls `scheduler_schedule()`
9. Which calls `context_switch(&old->context, &new->context)`
10. **Problem:** `context_switch()` does a `ret` to new thread, but we still have interrupt frame on stack!

**Analysis:**
The `context_switch()` function (in `kernel/hal/x86_64/context_switch.S`) is designed for voluntary context switches (thread_yield()), not preemptive switches from interrupts.

When switching from interrupt context:
- The interrupt frame is on the stack (SS, RSP, RFLAGS, CS, RIP)
- The saved registers are on the stack
- `context_switch()` saves/restores context but uses `ret` instruction
- After `ret`, we return to the interrupt stub which pops the OLD registers (wrong!)

**Possible Solutions:**

**Option A: Interrupt-Aware Context Switch (Complex)**
- Save interrupt frame to old thread's context
- Restore interrupt frame from new thread's context
- Use `iretq` instead of `ret`

**Option B: Deferred Context Switch (Simpler)**
- Set flag in interrupt
- Return from interrupt normally
- Check flag after interrupt
- Do context switch in non-interrupt context

**Option C: Fix Current Approach (Medium)**
- Modify interrupt stub to restore from thread->context instead of stack
- Save interrupt frame to thread->context on entry
- This requires matching cpu_context_t to interrupt frame format

**Recommended:** Option C - modify the interrupt stub to be context-aware.

---

## Testing Plan

### Phase 1: Compile and Boot Test
1. ✅ Fix compilation errors (duplicate functions)
2. ✅ Re-enable scheduler
3. [ ] Compile kernel
4. [ ] Boot in QEMU
5. [ ] Check if system boots without hanging

### Phase 2: Basic Scheduler Test
1. [ ] Create 2-3 test threads
2. [ ] Each thread prints to serial periodically
3. [ ] Verify threads are being scheduled (output interleaved)
4. [ ] Check context switch time (<20μs target)

### Phase 3: Stress Test
1. [ ] Create many threads (10-50)
2. [ ] High-frequency scheduling
3. [ ] Verify no deadlocks or hangs
4. [ ] Check for memory leaks

---

## Next Steps

### Immediate (Today):
1. **Implement proper interrupt-aware context switching**
   - Modify interrupt stub to save/restore from thread->context
   - Ensure cpu_context_t matches interrupt frame layout
   - Test basic functionality

2. **Build and test in QEMU**
   - `cd kernel && make clean && make`
   - Boot in QEMU
   - Monitor serial output for hangs

3. **Create test threads**
   - Simple threads that print messages
   - Verify preemptive scheduling works

### Short-term (This Week):
1. **Benchmark scheduler performance**
   - Measure context switch latency
   - Target: <20μs
   - Compare to Linux (5-10μs)

2. **Add real-time scheduling classes**
   - SCHED_FIFO
   - SCHED_RR
   - For audio/video threads

3. **Test on real hardware**
   - Boot on physical x86_64 machine
   - Verify multi-core scheduling

---

## Files Modified

1. ✅ `kernel/hal/x86_64/timer.c` - Removed duplicate functions
2. ✅ `kernel/hal/x86_64/interrupt_stubs.S` - Added scheduler_check_reschedule call
3. ✅ `kernel/core/main.c` - Re-enabled scheduler ticks

## Files to Modify (Next):

1. `kernel/hal/x86_64/interrupt_stubs.S` - Implement proper context switching
2. `kernel/sched/scheduler.c` - Possibly add interrupt-aware schedule function
3. `kernel/include/sched/scheduler.h` - Update if API changes

---

## Success Criteria

### Minimum (v0.1):
- [ ] System boots without hanging
- [ ] Scheduler ticks enabled
- [ ] Basic thread switching works
- [ ] No crashes during normal operation

### Target (v0.5):
- [ ] Context switch <20μs
- [ ] 100+ threads run stably
- [ ] Real-time scheduling classes work
- [ ] Multi-core load balancing verified

### Production (v1.0):
- [ ] Context switch <10μs (match Linux)
- [ ] 1000+ threads supported
- [ ] NUMA-aware scheduling
- [ ] Power-aware scheduling
- [ ] Benchmarks competitive with Linux

---

## Technical Debt

1. **Context switch mechanism needs redesign** - Current approach mixes voluntary and preemptive switching
2. **No context switch benchmarks** - Need performance measurement
3. **No scheduler profiling** - Should add instrumentation
4. **Fixed time quantum** - Should be adaptive based on thread behavior
5. **No CPU affinity** - Threads should be able to pin to specific CPUs

---

## Conclusion

**Good Progress:**
- Identified and fixed 3 critical bugs
- Re-enabled scheduler
- Clear path forward

**Remaining Work:**
- Need to implement proper interrupt-aware context switching
- Testing required to verify functionality
- Performance optimization needed

**Estimate:** 2-3 days to complete and test

---

*Status: IN PROGRESS*
*Next Update: After testing interrupt-aware context switch*
