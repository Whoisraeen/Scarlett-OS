# Phase 1: Bootloader & Minimal Kernel - Detailed Task Breakdown

## Phase Overview
**Duration:** Months 2-4 (8-12 weeks)
**Team:** Kernel Core (3-4 devs) + HAL (2-3 devs)
**Objective:** Boot on x86_64 hardware with basic memory management

---

## Week 1-2: Development Environment Setup

### Task 1.1: Toolchain Setup
**Assigned to:** Infrastructure Team
**Estimated effort:** 2-3 days
**Priority:** Critical

**Subtasks:**
- [ ] Install cross-compiler for x86_64-elf target
  ```bash
  # Build GCC cross-compiler
  - Download binutils 2.40
  - Download GCC 13.2
  - Build binutils with target x86_64-elf
  - Build GCC with target x86_64-elf
  - Verify compiler works with simple test
  ```
- [ ] Install NASM 2.16 or later
- [ ] Install QEMU for x86_64 system emulation
- [ ] Create Makefile template with cross-compilation support
- [ ] Document toolchain installation in README

**Acceptance criteria:**
- Cross-compiler can compile C code to x86_64-elf
- NASM can assemble x86_64 assembly
- QEMU can boot test image
- All team members have working toolchain

### Task 1.2: Project Structure Setup
**Assigned to:** Team Lead
**Estimated effort:** 1 day
**Priority:** Critical

**Subtasks:**
- [ ] Create directory structure:
  ```
  os-project/
  ├── bootloader/
  │   ├── uefi/
  │   └── common/
  ├── kernel/
  │   ├── core/
  │   ├── hal/x86_64/
  │   ├── mm/
  │   └── include/
  ├── build/
  ├── docs/
  └── tools/
  ```
- [ ] Create initial Makefile with targets: `all`, `clean`, `run`, `debug`
- [ ] Set up Git repository with `.gitignore`
- [ ] Create initial `README.md`
- [ ] Set up linker scripts directory

**Acceptance criteria:**
- Directory structure exists
- `make clean` works
- Git repository initialized

### Task 1.3: QEMU Testing Environment
**Assigned to:** Infrastructure
**Estimated effort:** 1 day
**Priority:** High

**Subtasks:**
- [ ] Create QEMU launch script `tools/qemu.sh`
- [ ] Configure QEMU with:
  - UEFI firmware (OVMF)
  - Serial console redirection
  - Memory size (512MB for testing)
  - Debugger support (GDB stub on port 1234)
- [ ] Create GDB script for kernel debugging
- [ ] Test booting UEFI shell in QEMU

**Acceptance criteria:**
- QEMU boots UEFI firmware
- Serial output visible in terminal
- GDB can attach to QEMU

---

## Week 2-4: UEFI Bootloader

### Task 2.1: UEFI Boot Stub
**Assigned to:** HAL Team Member 1
**Estimated effort:** 3-4 days
**Priority:** Critical

**Subtasks:**
- [ ] Create `bootloader/uefi/main.c` with `efi_main` entry point
- [ ] Implement basic UEFI protocol usage:
  - Simple Text Output Protocol (console print)
  - Load File Protocol (for loading kernel)
  - Graphics Output Protocol (for framebuffer)
- [ ] Print "Bootloader starting..." to UEFI console
- [ ] Retrieve memory map from UEFI
- [ ] Exit boot services
- [ ] Create PE32+ executable for UEFI
  - Write PE header
  - Set up relocations if needed
- [ ] Test booting in QEMU

**Files to create:**
- `bootloader/uefi/main.c`
- `bootloader/uefi/uefi.h` (UEFI types and structures)
- `bootloader/uefi/boot.ld` (linker script)

**Code structure:**
```c
// bootloader/uefi/main.c
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab) {
    // Initialize UEFI services
    // Print banner
    // Get memory map
    // Load kernel
    // Set up page tables
    // Exit boot services
    // Jump to kernel
}
```

**Acceptance criteria:**
- Bootloader prints to UEFI console
- Bootloader boots successfully in QEMU
- Can retrieve UEFI memory map

### Task 2.2: Kernel Loading
**Assigned to:** HAL Team Member 1
**Estimated effort:** 2-3 days
**Priority:** Critical

**Subtasks:**
- [ ] Define kernel file format (ELF64)
- [ ] Implement ELF64 parser in bootloader
  - Parse ELF header
  - Validate ELF is valid kernel image
  - Parse program headers
  - Load segments to correct physical addresses
- [ ] Load kernel from ESP (EFI System Partition)
- [ ] Find kernel entry point from ELF
- [ ] Pass boot information to kernel:
  - Memory map
  - Framebuffer info
  - ACPI tables pointer
  - Kernel command line

**Files to create:**
- `bootloader/uefi/elf.c`
- `bootloader/uefi/elf.h`
- `bootloader/common/boot_info.h` (shared with kernel)

**Acceptance criteria:**
- Bootloader can parse ELF64 file
- Kernel segments loaded to correct addresses
- Entry point identified correctly

### Task 2.3: Early Paging Setup
**Assigned to:** HAL Team Member 2
**Estimated effort:** 3-4 days
**Priority:** Critical

**Subtasks:**
- [ ] Allocate memory for page tables from UEFI
- [ ] Create identity mapping for first 1GB
- [ ] Create higher-half mapping (kernel at 0xFFFF800000000000)
- [ ] Map framebuffer
- [ ] Set CR3 to new page table
- [ ] Enable PAE and long mode (should already be enabled by UEFI)
- [ ] Document page table layout

**Files to create:**
- `bootloader/uefi/paging.c`
- `kernel/hal/x86_64/paging.h`

**Page table layout:**
```
Virtual Address                Physical Address
0x0000000000000000 - 0x40000000  → 0x0000000000000000 - 0x40000000 (identity)
0xFFFF800000000000 - ...         → Kernel physical address
```

**Acceptance criteria:**
- Page tables created successfully
- Identity mapping works
- Higher-half mapping works
- No page faults during setup

### Task 2.4: Kernel Entry
**Assigned to:** HAL Team Member 1
**Estimated effort:** 1 day
**Priority:** Critical

**Subtasks:**
- [ ] Jump to kernel entry point
- [ ] Pass boot info structure pointer in RDI (x86_64 calling convention)
- [ ] Ensure stack is set up correctly
- [ ] Ensure interrupts are disabled

**Files to modify:**
- `bootloader/uefi/main.c`

**Code:**
```c
// Jump to kernel
void (*kernel_entry)(boot_info_t *) = (void *)entry_point;
kernel_entry(&boot_info);
```

**Acceptance criteria:**
- Successfully jumps to kernel
- Boot info passed correctly

---

## Week 3-5: Minimal Kernel

### Task 3.1: Kernel Entry Point
**Assigned to:** Kernel Core Team Member 1
**Estimated effort:** 2 days
**Priority:** Critical

**Subtasks:**
- [ ] Create `kernel/core/main.c` with `kernel_main(boot_info_t *)`
- [ ] Create early assembly stub `kernel/hal/x86_64/entry.S`
  - Set up kernel stack
  - Clear BSS section
  - Call `kernel_main`
- [ ] Create linker script `kernel/kernel.ld`
  - Place kernel at higher-half (0xFFFFFFFF80000000)
  - Define sections: .text, .rodata, .data, .bss
  - Export symbols: `_kernel_start`, `_kernel_end`, `_bss_start`, `_bss_end`
- [ ] Receive boot info from bootloader

**Files to create:**
- `kernel/core/main.c`
- `kernel/hal/x86_64/entry.S`
- `kernel/kernel.ld`
- `kernel/include/boot_info.h`

**Code structure:**
```c
// kernel/core/main.c
void kernel_main(boot_info_t *boot_info) {
    // Early initialization
    // Print banner to serial
    // Initialize memory management
    // Infinite loop (for now)
    while (1) halt();
}
```

**Acceptance criteria:**
- Kernel entry point executes
- Boot info received correctly
- Kernel doesn't crash immediately

### Task 3.2: Serial Console Output
**Assigned to:** HAL Team Member 2
**Estimated effort:** 1-2 days
**Priority:** High

**Subtasks:**
- [ ] Implement serial driver for COM1 (0x3F8)
  - Initialize UART
  - Implement `serial_putc(char c)`
  - Implement `serial_puts(const char *s)`
- [ ] Implement simple `kprintf` (like printf)
  - Support %s, %d, %x, %p format specifiers
  - Use serial output as backend
- [ ] Print kernel banner on boot

**Files to create:**
- `kernel/hal/x86_64/serial.c`
- `kernel/core/kprintf.c`
- `kernel/include/kprintf.h`

**Code structure:**
```c
void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB
    outb(COM1 + 0, 0x03);    // Divisor low (38400 baud)
    outb(COM1 + 1, 0x00);    // Divisor high
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO
}

void kprintf(const char *fmt, ...);
```

**Acceptance criteria:**
- Serial output working in QEMU
- `kprintf` supports basic format specifiers
- Banner printed successfully

### Task 3.3: Early Exception Handling
**Assigned to:** Kernel Core Team Member 2
**Estimated effort:** 3-4 days
**Priority:** High

**Subtasks:**
- [ ] Set up GDT (Global Descriptor Table)
  - Null segment
  - Kernel code segment (64-bit)
  - Kernel data segment
- [ ] Set up IDT (Interrupt Descriptor Table)
  - 256 entries for x86_64
  - Create exception handlers for:
    - Divide by zero (#DE)
    - Debug (#DB)
    - Page fault (#PF)
    - General protection fault (#GP)
    - etc. (all 32 exceptions)
- [ ] Write assembly stubs for exception handlers
  - Save CPU state
  - Call C handler
  - Restore CPU state
  - Return with `iretq`
- [ ] Implement C exception handlers
  - Print exception info
  - Dump registers
  - Halt

**Files to create:**
- `kernel/hal/x86_64/gdt.c`
- `kernel/hal/x86_64/idt.c`
- `kernel/hal/x86_64/exceptions.S`
- `kernel/core/exceptions.c`
- `kernel/include/exceptions.h`

**Code structure:**
```asm
; exceptions.S
exception_handler_0:
    push 0                ; Dummy error code
    push 0                ; Exception number
    jmp exception_common

exception_handler_13:     ; GP fault
    push 13
    jmp exception_common

exception_common:
    ; Save all registers
    push rax
    push rbx
    ; ... etc
    mov rdi, rsp          ; Pass stack frame
    call exception_handler_c
    ; Restore registers
    pop rbx
    pop rax
    ; ... etc
    add rsp, 16           ; Remove error code and exception number
    iretq
```

```c
// exceptions.c
void exception_handler_c(exception_frame_t *frame) {
    kprintf("Exception %d at RIP: 0x%lx\n", frame->exception, frame->rip);
    kprintf("Error code: 0x%lx\n", frame->error_code);
    // Dump all registers
    halt();
}
```

**Acceptance criteria:**
- GDT loaded successfully
- IDT populated with handlers
- Exception triggers handler
- Exception info printed to serial

---

## Week 4-6: Physical Memory Management

### Task 4.1: Memory Map Parsing
**Assigned to:** Kernel Core Team Member 1
**Estimated effort:** 2 days
**Priority:** Critical

**Subtasks:**
- [ ] Parse UEFI memory map from boot info
- [ ] Identify usable memory regions
- [ ] Mark kernel memory as reserved
- [ ] Mark bootloader memory as reclaimable
- [ ] Print memory map for debugging

**Files to create:**
- `kernel/mm/mmap.c`
- `kernel/include/mm/mmap.h`

**Data structure:**
```c
typedef struct memory_region {
    uint64_t base;
    uint64_t length;
    uint32_t type;        // Usable, Reserved, ACPI, etc.
} memory_region_t;
```

**Acceptance criteria:**
- Memory map parsed correctly
- Usable regions identified
- Memory map printed to serial

### Task 4.2: Physical Page Allocator (Bitmap)
**Assigned to:** Kernel Core Team Member 2
**Estimated effort:** 4-5 days
**Priority:** Critical

**Subtasks:**
- [ ] Design bitmap-based page allocator
  - 1 bit per page (4KB)
  - 0 = free, 1 = allocated
- [ ] Allocate bitmap in kernel BSS
- [ ] Initialize bitmap based on memory map
- [ ] Implement `pmm_alloc_page()` - allocate single page
- [ ] Implement `pmm_free_page(paddr)` - free single page
- [ ] Implement `pmm_alloc_pages(count)` - allocate contiguous pages
- [ ] Add statistics tracking (free pages, used pages)

**Files to create:**
- `kernel/mm/pmm.c`
- `kernel/include/mm/pmm.h`

**Code structure:**
```c
#define PAGE_SIZE 4096

void pmm_init(memory_region_t *regions, size_t count);
paddr_t pmm_alloc_page(void);
void pmm_free_page(paddr_t page);
paddr_t pmm_alloc_pages(size_t count);
void pmm_free_pages(paddr_t base, size_t count);
size_t pmm_get_free_pages(void);
```

**Note:** Bitmap allocator is simple but not final. Will be replaced with buddy allocator in Phase 3.

**Acceptance criteria:**
- Bitmap initialized correctly
- Can allocate single page
- Can free page
- Can allocate multiple contiguous pages
- Statistics accurate

### Task 4.3: Testing Physical Allocator
**Assigned to:** Kernel Core Team Member 2
**Estimated effort:** 1 day
**Priority:** High

**Subtasks:**
- [ ] Write test code in `kernel_main`
  - Allocate 10 pages
  - Free 5 pages
  - Allocate 3 contiguous pages
  - Check statistics
- [ ] Verify no memory corruption
- [ ] Test edge cases:
  - Out of memory
  - Double free (should be detected)
  - Allocate more pages than available

**Acceptance criteria:**
- All tests pass
- No crashes during allocation/free
- Statistics correct

---

## Week 5-7: Virtual Memory Management

### Task 5.1: Page Table Management
**Assigned to:** Kernel Core Team Member 3
**Estimated effort:** 5-6 days
**Priority:** Critical

**Subtasks:**
- [ ] Define page table structures for x86_64
  ```c
  typedef struct {
      uint64_t entries[512];
  } page_table_t;
  ```
- [ ] Implement page table allocation
  - Use physical allocator for page tables
  - Clear newly allocated tables
- [ ] Implement `vmm_map_page(vaddr, paddr, flags)`
  - Walk 4-level page table
  - Create intermediate tables if needed
  - Set PTE with correct flags (Present, R/W, U/S, etc.)
- [ ] Implement `vmm_unmap_page(vaddr)`
  - Clear PTE
  - Flush TLB
  - Optionally free page table if empty
- [ ] Implement `vmm_get_mapping(vaddr)` - return physical address
- [ ] Implement TLB flushing
  - `flush_tlb_single(vaddr)` - invlpg
  - `flush_tlb_all()` - reload CR3

**Files to create:**
- `kernel/mm/vmm.c`
- `kernel/include/mm/vmm.h`
- `kernel/hal/x86_64/paging.c`

**Code structure:**
```c
#define PTE_PRESENT (1 << 0)
#define PTE_WRITE   (1 << 1)
#define PTE_USER    (1 << 2)
#define PTE_NX      (1ULL << 63)

int vmm_map_page(vaddr_t vaddr, paddr_t paddr, uint64_t flags);
int vmm_unmap_page(vaddr_t vaddr);
paddr_t vmm_get_mapping(vaddr_t vaddr);
void flush_tlb_single(vaddr_t vaddr);
void flush_tlb_all(void);
```

**Acceptance criteria:**
- Can map virtual to physical addresses
- Can unmap addresses
- TLB flushed correctly
- Page faults work as expected

### Task 5.2: Kernel Address Space
**Assigned to:** Kernel Core Team Member 3
**Estimated effort:** 3 days
**Priority:** Critical

**Subtasks:**
- [ ] Define kernel virtual memory layout:
  ```
  0xFFFF800000000000 - Physical memory direct map
  0xFFFFFFFF80000000 - Kernel code/data
  0xFFFFFFFFC0000000 - Kernel heap
  ```
- [ ] Create kernel page table
- [ ] Map kernel code/data sections
- [ ] Create direct mapping of physical memory (for easy access)
- [ ] Switch to kernel page table from bootloader's
- [ ] Document memory layout

**Files to modify:**
- `kernel/mm/vmm.c`
- `kernel/include/mm/memory_layout.h`

**Acceptance criteria:**
- Kernel page table created
- All kernel code/data accessible
- Physical memory accessible via direct map
- No page faults during normal operation

### Task 5.3: Kernel Heap Allocator (Simple)
**Assigned to:** Kernel Core Team Member 4
**Estimated effort:** 4 days
**Priority:** High

**Subtasks:**
- [ ] Implement simple bump allocator (temporary)
  - Start at kernel heap base
  - Allocate linearly
  - No free (simple, fast for bootstrap)
- [ ] Implement `kmalloc(size)` and `kfree(ptr)` stubs
- [ ] Reserve virtual address space for heap
- [ ] Map pages on demand (grow heap as needed)

**Files to create:**
- `kernel/mm/heap.c`
- `kernel/include/mm/heap.h`

**Code structure:**
```c
void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);  // No-op for now
```

**Note:** This is a temporary simple allocator. Will be replaced with slab allocator in Phase 2.

**Acceptance criteria:**
- Can allocate memory from heap
- Heap grows as needed
- No crashes during allocation

---

## Week 6-8: Integration & Testing

### Task 6.1: Framebuffer Driver (Basic)
**Assigned to:** HAL Team Member 2
**Estimated effort:** 2-3 days
**Priority:** Medium

**Subtasks:**
- [ ] Get framebuffer info from boot info
- [ ] Map framebuffer to virtual memory
- [ ] Implement `fb_putpixel(x, y, color)`
- [ ] Implement `fb_fill_rect(x, y, w, h, color)`
- [ ] Implement basic bitmap font rendering
- [ ] Implement `fb_putc(x, y, char, color)`
- [ ] Print "Hello from kernel" on screen

**Files to create:**
- `kernel/drivers/fb.c`
- `kernel/include/drivers/fb.h`

**Acceptance criteria:**
- Framebuffer accessible
- Can draw pixels
- Can draw text on screen

### Task 6.2: Debugging Infrastructure
**Assigned to:** Infrastructure
**Estimated effort:** 2 days
**Priority:** Medium

**Subtasks:**
- [ ] Implement `kassert(condition, message)`
- [ ] Implement `kpanic(message)` - halt system
- [ ] Implement stack trace printing (basic)
- [ ] Add debug symbols to kernel image
- [ ] Test GDB debugging with QEMU

**Files to create:**
- `kernel/core/debug.c`
- `kernel/include/debug.h`

**Code structure:**
```c
#define kassert(cond, msg) \
    if (!(cond)) kpanic("Assertion failed: " msg)

void kpanic(const char *msg);
void print_stack_trace(void);
```

**Acceptance criteria:**
- Assert works correctly
- Panic halts system and prints message
- Can debug kernel with GDB

### Task 6.3: Comprehensive Testing
**Assigned to:** All team members
**Estimated effort:** 3-4 days
**Priority:** Critical

**Subtasks:**
- [ ] Test on QEMU x86_64
- [ ] Test on real hardware (if available)
- [ ] Stress test memory allocator
  - Allocate/free many pages
  - Check for leaks
- [ ] Test page mapping/unmapping
- [ ] Test exception handling
  - Trigger divide by zero
  - Trigger page fault
  - Trigger GP fault
- [ ] Verify all serial output
- [ ] Create test report

**Acceptance criteria:**
- All tests pass
- No memory leaks
- System stable
- Exception handling works

### Task 6.4: Documentation
**Assigned to:** Team Lead + all members
**Estimated effort:** 2-3 days
**Priority:** High

**Subtasks:**
- [ ] Document boot process in `docs/boot.md`
- [ ] Document memory layout in `docs/memory.md`
- [ ] Document building and running in `README.md`
- [ ] Add code comments to complex functions
- [ ] Create architecture diagram
- [ ] Write Phase 1 completion report

**Acceptance criteria:**
- All major components documented
- README has build instructions
- Architecture diagram created

---

## Phase 1 Deliverables Checklist

### Bootloader
- [x] UEFI bootloader boots successfully
- [x] Kernel loaded from EFI System Partition
- [x] ELF64 parsing implemented
- [x] Page tables set up (identity + higher-half)
- [x] Boot info passed to kernel

### Kernel
- [x] Kernel entry point functional
- [x] Serial console output working
- [x] GDT and IDT set up
- [x] Exception handling working
- [x] Memory map parsed

### Memory Management
- [x] Physical page allocator (bitmap-based)
- [x] Virtual memory manager (page table manipulation)
- [x] Kernel heap allocator (simple bump allocator)
- [x] Direct physical memory mapping

### Debug & Testing
- [x] Serial debugging working
- [x] GDB debugging functional
- [x] Basic framebuffer output (optional)
- [x] All tests passing

### Documentation
- [x] Boot process documented
- [x] Memory layout documented
- [x] Build instructions in README
- [x] Code comments added

---

## Success Criteria

**Must Have:**
- ✓ System boots successfully on QEMU x86_64
- ✓ Kernel can print to serial console
- ✓ Physical memory allocator works
- ✓ Virtual memory management works
- ✓ Exception handling functional
- ✓ No memory leaks or crashes

**Should Have:**
- ✓ Boots on real x86_64 hardware
- ✓ Framebuffer output working
- ✓ GDB debugging setup
- ✓ Comprehensive documentation

**Nice to Have:**
- ○ Legacy BIOS bootloader (in addition to UEFI)
- ○ Advanced debugging features
- ○ Performance benchmarks

---

## Risk Mitigation

**Risk: UEFI boot issues**
- Mitigation: Test early and frequently with OVMF in QEMU
- Fallback: Implement legacy BIOS boot

**Risk: Page table bugs**
- Mitigation: Extensive testing, verify with GDB
- Fallback: Use bootloader's page tables temporarily

**Risk: Memory allocator bugs**
- Mitigation: Comprehensive test suite, assertions
- Fallback: Use simpler bump allocator

**Risk: Team member unavailability**
- Mitigation: Ensure knowledge sharing, documentation
- Fallback: Reassign tasks, extend timeline slightly

---

## Tools & Resources

**Development:**
- GCC cross-compiler (x86_64-elf)
- NASM assembler
- GNU Make
- QEMU
- GDB

**References:**
- Intel Software Developer Manual (Vol. 3)
- UEFI Specification 2.10
- OSDev Wiki
- ELF64 Specification

**Testing:**
- QEMU x86_64 system emulation
- Real hardware (optional)
- Serial console (COM1)

---

*Phase 1 Document Version: 1.0*
*Last Updated: 2025-11-17*
