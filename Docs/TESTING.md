# Scarlett OS - Testing Guide

## Quick Test

Run the automated test:

```bash
chmod +x test_kernel.sh
./test_kernel.sh
```

This will:
1. Check toolchain
2. Build kernel
3. Boot in QEMU
4. Verify initialization
5. Report results

---

## Manual Testing

### 1. Compile Test

```bash
cd kernel
make clean
make
```

**Expected:** No errors, `kernel.elf` created

### 2. Boot Test

```bash
qemu-system-x86_64 -kernel kernel/kernel.elf -m 512M -serial stdio
```

**Expected output:**

```
====================================================
                  Scarlett OS                       
        A Modern Microkernel Operating System      
====================================================
Version: 0.1.0 (Phase 1 - Development)
Architecture: x86_64
Build: Nov 18 2025 XX:XX:XX
====================================================

[INFO] Verifying boot information...
[INFO] Boot info verified successfully
[INFO] Kernel loaded at: 0xFFFFFFFF80100000 - 0xFFFFFFFF8012XXXX
[INFO] Kernel size: XXX KB
...
[INFO] Initializing GDT...
[INFO] GDT initialized successfully
[INFO] Initializing IDT...
[INFO] IDT initialized successfully
[INFO] Initializing Physical Memory Manager...
[INFO] PMM initialized: XXX MB total, XXX MB free, XXX MB used

[INFO] Kernel initialization complete!
[INFO] System is now idle.
```

### 3. Exception Test

Uncomment in `kernel/core/main.c`:

```c
// Test exception handling (uncomment to test)
kinfo("Testing divide by zero exception...\n");
volatile int x = 1 / 0;
```

Rebuild and run. **Expected:** Exception handler catches divide-by-zero.

### 4. Memory Test

Add to `kernel/core/main.c` after PMM init:

```c
// Test memory allocation
kinfo("Testing PMM...\n");
paddr_t page1 = pmm_alloc_page();
kinfo("Allocated page at: 0x%lx\n", page1);

paddr_t page2 = pmm_alloc_page();
kinfo("Allocated page at: 0x%lx\n", page2);

pmm_free_page(page1);
kinfo("Freed first page\n");

paddr_t page3 = pmm_alloc_page();
kinfo("Allocated page at: 0x%lx (should reuse freed page)\n", page3);

kassert(page3 == page1, "Page allocation reuse failed");
kinfo("PMM test passed!\n");
```

**Expected:** Pages allocated, freed, and reused correctly.

---

## Debug Testing

### With GDB

```bash
# Terminal 1
./tools/debug.sh

# Terminal 2
gdb kernel/kernel.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
(gdb) step
```

**Test breakpoints at:**
- `kernel_main`
- `gdt_init`
- `idt_init`
- `pmm_init`
- `pmm_alloc_page`

### Serial Output Test

Check that all log levels work:

```c
kdebug("Debug message (only in debug builds)\n");
kinfo("Info message\n");
kwarn("Warning message\n");
kerror("Error message\n");
```

---

## Component Tests

### 1. Serial Driver

**File:** `kernel/hal/x86_64/serial.c`

```c
serial_init();
serial_puts("Hello World!\n");
```

**Expected:** Text appears in QEMU serial console

### 2. Printf

**File:** `kernel/core/kprintf.c`

```c
kprintf("String: %s\n", "test");
kprintf("Char: %c\n", 'A');
kprintf("Decimal: %d\n", -42);
kprintf("Unsigned: %u\n", 42);
kprintf("Hex: 0x%x\n", 0xDEADBEEF);
kprintf("Pointer: %p\n", (void*)0x12345678);
kprintf("Long: %lu\n", 1234567890UL);
```

**Expected:** All formats display correctly

### 3. GDT

**File:** `kernel/hal/x86_64/gdt.c`

- Should initialize without crash
- Check segments loaded: `CS=0x08`, `DS=0x10`

### 4. IDT

**File:** `kernel/hal/x86_64/idt.c`

- Should initialize without crash
- Trigger exception to verify handler works

### 5. PMM

**File:** `kernel/mm/pmm.c`

```c
// Allocate
paddr_t p1 = pmm_alloc_page();
kassert(p1 != 0, "Allocation failed");
kassert(IS_ALIGNED(p1, PAGE_SIZE), "Unaligned page");

// Free
pmm_free_page(p1);

// Allocate multiple
paddr_t p2 = pmm_alloc_pages(10);
kassert(p2 != 0, "Multi-page allocation failed");
pmm_free_pages(p2, 10);

// Stats
size_t free = pmm_get_free_pages();
kinfo("Free pages: %lu\n", free);
```

---

## Performance Tests

### Boot Time

Measure time from QEMU start to "System is now idle":

```bash
time qemu-system-x86_64 -kernel kernel/kernel.elf -m 512M -serial stdio -display none &
sleep 2
killall qemu-system-x86_64
```

**Target:** < 1 second

### Memory Usage

Check kernel size:

```bash
ls -lh kernel/kernel.elf
size kernel/kernel.elf
```

**Target:** < 500KB kernel binary

### PMM Performance

```c
// Allocate 1000 pages
uint64_t start = read_timestamp();
for (int i = 0; i < 1000; i++) {
    paddr_t p = pmm_alloc_page();
    pmm_free_page(p);
}
uint64_t end = read_timestamp();
kinfo("1000 alloc/free: %lu cycles\n", end - start);
```

---

## Regression Tests

After making changes, verify:

- [ ] Kernel still compiles
- [ ] No new warnings
- [ ] Boots successfully
- [ ] Serial output works
- [ ] All initialization messages appear
- [ ] No crashes or triple faults
- [ ] Memory statistics correct

---

## Known Issues to Test For

### Issue: Triple Fault on Boot

**Symptoms:** QEMU exits immediately or resets
**Check:**
- GDT properly loaded
- IDT properly loaded
- Stack pointer valid
- Page tables correct

### Issue: No Serial Output

**Symptoms:** Blank screen, no text
**Check:**
- `serial_init()` called first
- `-serial stdio` flag in QEMU
- Correct baud rate (38400)
- Port address (0x3F8)

### Issue: Exception Not Caught

**Symptoms:** Triple fault instead of exception handler
**Check:**
- IDT initialized before exception
- Exception handlers in IDT
- Proper stack in exception handler
- GDT loaded correctly

### Issue: Memory Allocation Fails

**Symptoms:** `pmm_alloc_page()` returns 0
**Check:**
- Memory map parsed correctly
- Free pages > 0
- Bitmap initialized
- No double allocation

---

## Automated Testing (Future)

### Unit Tests

Create `tests/` directory with:

```
tests/
├── test_pmm.c
├── test_kprintf.c
├── test_serial.c
└── run_tests.sh
```

### Integration Tests

```bash
# Boot test
./test_boot.sh

# Exception test
./test_exceptions.sh

# Memory test
./test_memory.sh
```

### CI/CD

GitHub Actions workflow:

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install toolchain
        run: ./scripts/install_toolchain.sh
      - name: Build kernel
        run: cd kernel && make
      - name: Run tests
        run: ./test_kernel.sh
```

---

## Success Criteria

### Phase 1 Complete When:

- ✅ Kernel compiles without warnings
- ✅ Boots in QEMU successfully
- ✅ Serial console works
- ✅ GDT initialized
- ✅ IDT initialized
- ✅ Exception handling works
- ✅ PMM allocates/frees correctly
- ✅ No memory leaks
- ✅ All tests pass
- ✅ Runs stable for 5 minutes

---

## Next Steps

Once all tests pass:

1. Mark Phase 1 complete
2. Create release tag `v0.1.0-phase1`
3. Document any issues found
4. Begin Phase 2 planning
5. Set up automated testing

---

## Quick Commands Reference

```bash
# Build
cd kernel && make

# Test
./test_kernel.sh

# Run
qemu-system-x86_64 -kernel kernel/kernel.elf -m 512M -serial stdio

# Debug
qemu-system-x86_64 -kernel kernel/kernel.elf -m 512M -serial stdio -s -S
gdb kernel/kernel.elf -ex 'target remote :1234'

# Clean
make clean
```

---

**Last Updated:** November 18, 2025

