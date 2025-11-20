/**
 * @file exceptions.c
 * @brief CPU exception handlers
 */

#include "../include/types.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Exception stack frame
typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t exception_num;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) exception_frame_t;

// Exception names
static const char* exception_names[] = {
    "Divide Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"
};

/**
 * Common exception handler
 */
void exception_handler_c(exception_frame_t* frame) {
    kprintf("\n");
    kprintf("========== EXCEPTION ==========\n");
    
    // Print exception information
    if (frame->exception_num < 32) {
        kprintf("Exception: %s (#%lu)\n", 
                exception_names[frame->exception_num],
                frame->exception_num);
    } else {
        kprintf("Exception: Unknown (#%lu)\n", frame->exception_num);
    }
    
    kprintf("Error Code: 0x%016lx\n", frame->error_code);
    kprintf("\n");
    
    // Print register dump
    kprintf("Register Dump:\n");
    kprintf("  RAX: 0x%016lx  RBX: 0x%016lx\n", frame->rax, frame->rbx);
    kprintf("  RCX: 0x%016lx  RDX: 0x%016lx\n", frame->rcx, frame->rdx);
    kprintf("  RSI: 0x%016lx  RDI: 0x%016lx\n", frame->rsi, frame->rdi);
    kprintf("  RBP: 0x%016lx  RSP: 0x%016lx\n", frame->rbp, frame->rsp);
    kprintf("  R8:  0x%016lx  R9:  0x%016lx\n", frame->r8, frame->r9);
    kprintf("  R10: 0x%016lx  R11: 0x%016lx\n", frame->r10, frame->r11);
    kprintf("  R12: 0x%016lx  R13: 0x%016lx\n", frame->r12, frame->r13);
    kprintf("  R14: 0x%016lx  R15: 0x%016lx\n", frame->r14, frame->r15);
    kprintf("\n");
    kprintf("  RIP: 0x%016lx  CS:  0x%016lx\n", frame->rip, frame->cs);
    kprintf("  RFLAGS: 0x%016lx\n", frame->rflags);
    kprintf("  SS:  0x%016lx\n", frame->ss);
    kprintf("\n");
    
    // Special handling for page fault
    if (frame->exception_num == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        
        // Try to handle Copy-on-Write fault
        extern int vmm_handle_cow_fault(vaddr_t vaddr);
        if (vmm_handle_cow_fault((vaddr_t)cr2) == 0) {
            // CoW fault handled successfully, return from exception
            return;
        }
        
        // Not a CoW fault, print debug info
        kprintf("Page Fault Address: 0x%016lx\n", cr2);
        
        kprintf("Fault Type: ");
        if (frame->error_code & 1) kprintf("Protection violation ");
        else kprintf("Non-present page ");
        
        if (frame->error_code & 2) kprintf("(Write) ");
        else kprintf("(Read) ");
        
        if (frame->error_code & 4) kprintf("(User mode)");
        else kprintf("(Kernel mode)");
        kprintf("\n");
    }
    
    kprintf("===============================\n");
    
    // Halt the system
    kpanic("Unhandled exception");
}

