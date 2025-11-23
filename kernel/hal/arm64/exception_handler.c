/**
 * @file exception_handler.c
 * @brief ARM64 exception handler implementation
 */

#include "../../include/types.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/mm/vmm.h"

// Exception Syndrome Register (ESR_EL1) bit fields
#define ESR_EC_SHIFT    26
#define ESR_EC_MASK     0x3F
#define ESR_IL_SHIFT    25
#define ESR_IL_MASK    0x1
#define ESR_ISS_SHIFT   0
#define ESR_ISS_MASK    0x1FFFFFF

// Exception Class (EC) values
#define ESR_EC_UNKNOWN          0x00
#define ESR_EC_SVC64            0x15  // Supervisor Call (AArch64)
#define ESR_EC_INST_ABORT_LOW   0x20  // Instruction Abort (lower EL)
#define ESR_EC_INST_ABORT_CURR  0x21  // Instruction Abort (current EL)
#define ESR_EC_DATA_ABORT_LOW   0x24  // Data Abort (lower EL)
#define ESR_EC_DATA_ABORT_CURR  0x25  // Data Abort (current EL)
#define ESR_EC_FP_EXCEPTION     0x07  // Floating-point exception
#define ESR_EC_SERROR           0x2F  // SError interrupt

// Data Abort ISS bit fields
#define ISS_DFSC_SHIFT  0
#define ISS_DFSC_MASK   0x3F
#define ISS_WnR_SHIFT  6
#define ISS_WnR_MASK   0x1

// Data Fault Status Code (DFSC) values
#define DFSC_ADDRESS_SIZE_FAULT     0x00
#define DFSC_TRANSLATION_FAULT_L0   0x04
#define DFSC_TRANSLATION_FAULT_L1   0x05
#define DFSC_TRANSLATION_FAULT_L2   0x06
#define DFSC_TRANSLATION_FAULT_L3   0x07
#define DFSC_ACCESS_FLAG_FAULT_L0  0x09
#define DFSC_ACCESS_FLAG_FAULT_L1  0x0A
#define DFSC_ACCESS_FLAG_FAULT_L2  0x0B
#define DFSC_ACCESS_FLAG_FAULT_L3  0x0C
#define DFSC_PERMISSION_FAULT_L0   0x0D
#define DFSC_PERMISSION_FAULT_L1   0x0F
#define DFSC_PERMISSION_FAULT_L2   0x11
#define DFSC_PERMISSION_FAULT_L3   0x13

/**
 * Read Exception Syndrome Register
 */
static uint64_t read_esr_el1(void) {
    uint64_t esr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    return esr;
}

/**
 * Read Fault Address Register
 */
static uint64_t read_far_el1(void) {
    uint64_t far;
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    return far;
}

/**
 * Handle synchronous exception
 */
void arm64_handle_sync_exception(uint64_t esr, uint64_t far, uint64_t elr, uint64_t spsr) {
    uint64_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
    uint64_t iss = esr & ESR_ISS_MASK;
    
    kprintf("\n========== ARM64 EXCEPTION ==========\n");
    kprintf("ESR_EL1: 0x%016lx\n", esr);
    kprintf("FAR_EL1: 0x%016lx\n", far);
    kprintf("ELR_EL1: 0x%016lx\n", elr);
    kprintf("SPSR_EL1: 0x%016lx\n", spsr);
    kprintf("Exception Class: 0x%02lx\n", ec);
    
    switch (ec) {
        case ESR_EC_SVC64:
            // Supervisor call - should be handled by syscall handler
            kprintf("Exception: Supervisor Call (SVC)\n");
            kpanic("SVC exception reached sync handler (should be handled by syscall handler)");
            break;
            
        case ESR_EC_INST_ABORT_LOW:
        case ESR_EC_INST_ABORT_CURR:
            kprintf("Exception: Instruction Abort\n");
            kprintf("Fault Address: 0x%016lx\n", far);
            kpanic("Instruction abort - invalid instruction address");
            break;
            
        case ESR_EC_DATA_ABORT_LOW:
        case ESR_EC_DATA_ABORT_CURR: {
            kprintf("Exception: Data Abort (Page Fault)\n");
            uint64_t dfsc = (iss >> ISS_DFSC_SHIFT) & ISS_DFSC_MASK;
            uint64_t write = (iss >> ISS_WnR_SHIFT) & ISS_WnR_MASK;
            
            kprintf("Fault Address: 0x%016lx\n", far);
            kprintf("DFSC: 0x%02lx ", dfsc);
            
            switch (dfsc) {
                case DFSC_TRANSLATION_FAULT_L0:
                case DFSC_TRANSLATION_FAULT_L1:
                case DFSC_TRANSLATION_FAULT_L2:
                case DFSC_TRANSLATION_FAULT_L3:
                    kprintf("(Translation Fault)\n");
                    break;
                case DFSC_ACCESS_FLAG_FAULT_L0:
                case DFSC_ACCESS_FLAG_FAULT_L1:
                case DFSC_ACCESS_FLAG_FAULT_L2:
                case DFSC_ACCESS_FLAG_FAULT_L3:
                    kprintf("(Access Flag Fault)\n");
                    break;
                case DFSC_PERMISSION_FAULT_L0:
                case DFSC_PERMISSION_FAULT_L1:
                case DFSC_PERMISSION_FAULT_L2:
                case DFSC_PERMISSION_FAULT_L3:
                    kprintf("(Permission Fault)\n");
                    break;
                default:
                    kprintf("(Unknown)\n");
                    break;
            }
            
            kprintf("Access Type: %s\n", write ? "Write" : "Read");
            
            // Try to handle Copy-on-Write fault
            extern int vmm_handle_cow_fault(vaddr_t vaddr);
            if (vmm_handle_cow_fault((vaddr_t)far) == 0) {
                // CoW fault handled successfully
                kprintf("CoW fault handled successfully\n");
                return;
            }
            
            // Not a CoW fault - panic
            kpanic("Unhandled data abort");
            break;
        }
        
        case ESR_EC_FP_EXCEPTION:
            kprintf("Exception: Floating-Point Exception\n");
            kpanic("Floating-point exception");
            break;
            
        case ESR_EC_UNKNOWN:
        default:
            kprintf("Exception: Unknown (EC=0x%02lx)\n", ec);
            kpanic("Unknown synchronous exception");
            break;
    }
    
    kprintf("=====================================\n");
}

/**
 * Handle FIQ (Fast Interrupt Request)
 */
void arm64_handle_fiq(void) {
    // FIQ is typically used for high-priority interrupts
    // For now, treat it similar to IRQ
    kprintf("FIQ: Fast Interrupt Request\n");
    
    // Call IRQ handler (FIQ can use same infrastructure)
    extern void arm64_irq_handler(void);
    arm64_irq_handler();
}

/**
 * Handle SError (System Error)
 */
void arm64_handle_serror(void) {
    uint64_t esr = read_esr_el1();
    uint64_t far = read_far_el1();
    
    kprintf("\n========== ARM64 SERROR ==========\n");
    kprintf("ESR_EL1: 0x%016lx\n", esr);
    kprintf("FAR_EL1: 0x%016lx\n", far);
    kprintf("System Error detected\n");
    kprintf("==================================\n");
    
    // SError is typically unrecoverable
    kpanic("System Error (SError) - unrecoverable");
}

