/**
 * @file cpu_init.c
 * @brief ARM64 CPU initialization
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"

// Exception levels
#define EL0 0
#define EL1 1
#define EL2 2
#define EL3 3

// System registers
static inline uint64_t read_currentel(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, currentel" : "=r"(val));
    return (val >> 2) & 3;
}

static inline uint64_t read_midr(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(val));
    return val;
}

static inline uint64_t read_mpidr(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(val));
    return val;
}

static inline void write_vbar_el1(uint64_t val) {
    __asm__ volatile("msr vbar_el1, %0" :: "r"(val));
}

static inline void write_sctlr_el1(uint64_t val) {
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(val) : "memory");
}

static inline uint64_t read_sctlr_el1(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(val));
    return val;
}

// Exception vector table (defined in vectors.S)
extern void arm64_exception_vectors(void);

// Per-CPU data structure
typedef struct {
    uint32_t cpu_id;
    uint64_t mpidr;
    uint64_t midr;
    uint8_t exception_level;
    void* stack_ptr;
} arm64_cpu_data_t;

#define MAX_CPUS 64
static arm64_cpu_data_t cpu_data[MAX_CPUS];
static uint32_t num_cpus = 0;

error_code_t arm64_cpu_init(void) {
    kinfo("ARM64 CPU initialization...\n");

    // Detect current exception level
    uint64_t current_el = read_currentel();
    kinfo("Current Exception Level: EL%lu\n", current_el);

    if (current_el != EL1) {
        kwarn("Not running at EL1! Attempting to drop to EL1...\n");
        // TODO: Implement EL2->EL1 or EL3->EL1 transition
        return ERR_NOT_SUPPORTED;
    }

    // Read CPU identification
    uint64_t midr = read_midr();
    uint64_t mpidr = read_mpidr();
    
    kinfo("MIDR_EL1: 0x%016lx\n", midr);
    kinfo("MPIDR_EL1: 0x%016lx\n", mpidr);

    // Extract CPU info
    uint32_t implementer = (midr >> 24) & 0xFF;
    uint32_t variant = (midr >> 20) & 0xF;
    uint32_t architecture = (midr >> 16) & 0xF;
    uint32_t partnum = (midr >> 4) & 0xFFF;
    uint32_t revision = midr & 0xF;

    kinfo("CPU: Implementer 0x%02x, Part 0x%03x, Variant 0x%x, Revision 0x%x\n",
          implementer, partnum, variant, revision);

    // Initialize BSP CPU data
    cpu_data[0].cpu_id = 0;
    cpu_data[0].mpidr = mpidr;
    cpu_data[0].midr = midr;
    cpu_data[0].exception_level = (uint8_t)current_el;
    num_cpus = 1;

    // Set up exception vector table
    kinfo("Setting up exception vectors...\n");
    write_vbar_el1((uint64_t)arm64_exception_vectors);

    // Configure SCTLR_EL1
    uint64_t sctlr = read_sctlr_el1();
    
    // Enable:
    // - I-cache (bit 12)
    // - D-cache (bit 2)
    // - Alignment checking (bit 1)
    // - MMU (bit 0) - will be enabled later by MMU init
    sctlr |= (1 << 12) | (1 << 2) | (1 << 1);
    
    // Disable:
    // - WXN (Write permission implies XN) for now
    // - EE (Exception Endianness) - little endian
    sctlr &= ~((1 << 19) | (1 << 25));
    
    write_sctlr_el1(sctlr);
    __asm__ volatile("isb");

    kinfo("ARM64 CPU initialization complete\n");
    return ERR_OK;
}

uint32_t arm64_cpu_get_count(void) {
    return num_cpus;
}

void* arm64_get_per_cpu_data(uint32_t cpu_id) {
    if (cpu_id >= MAX_CPUS) {
        return NULL;
    }
    return &cpu_data[cpu_id];
}

uint32_t arm64_cpu_get_id(void) {
    uint64_t mpidr = read_mpidr();
    return (uint32_t)(mpidr & 0xFF);
}
