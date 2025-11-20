/**
 * @file irq_handler.c
 * @brief User-space interrupt handler registration
 */

#include "../../include/types.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/debug.h"
#include "../../include/sync/spinlock.h"
#include "../../include/mm/heap.h"
#include "../../include/sched/scheduler.h"

// IRQ handler callback type
typedef void (*irq_handler_callback_t)(void* context);

// IRQ handler entry
typedef struct irq_handler_entry {
    uint8_t irq;
    irq_handler_callback_t handler;
    void* context;
    uint64_t tid;  // Thread ID that registered this handler
    struct irq_handler_entry* next;
} irq_handler_entry_t;

#define MAX_IRQ_HANDLERS 64
static irq_handler_entry_t* irq_handlers[MAX_IRQ_HANDLERS] = {NULL};
static spinlock_t irq_handler_lock = SPINLOCK_INIT;

/**
 * Register an IRQ handler from user-space
 */
int irq_register(uint8_t irq, irq_handler_callback_t handler, void* context) {
    if (irq >= MAX_IRQ_HANDLERS) {
        return -1;
    }
    
    if (!handler) {
        return -1;
    }
    
    spinlock_lock(&irq_handler_lock);
    
    // Check if handler already exists for this IRQ
    irq_handler_entry_t* entry = irq_handlers[irq];
    while (entry) {
        if (entry->handler == handler && entry->context == context) {
            spinlock_unlock(&irq_handler_lock);
            return -1;  // Already registered
        }
        entry = entry->next;
    }
    
    // Allocate new entry
    irq_handler_entry_t* new_entry = (irq_handler_entry_t*)kmalloc(sizeof(irq_handler_entry_t));
    if (!new_entry) {
        spinlock_unlock(&irq_handler_lock);
        return -1;
    }
    
    new_entry->irq = irq;
    new_entry->handler = handler;
    new_entry->context = context;
    extern thread_t* thread_current(void);
    thread_t* current = thread_current();
    new_entry->tid = current ? current->tid : 0;
    new_entry->next = irq_handlers[irq];
    irq_handlers[irq] = new_entry;
    
    kinfo("Registered IRQ handler for IRQ %u\n", irq);
    
    spinlock_unlock(&irq_handler_lock);
    
    return 0;
}

/**
 * Unregister an IRQ handler
 */
int irq_unregister(uint8_t irq, irq_handler_callback_t handler) {
    if (irq >= MAX_IRQ_HANDLERS) {
        return -1;
    }
    
    spinlock_lock(&irq_handler_lock);
    
    irq_handler_entry_t** entry = &irq_handlers[irq];
    while (*entry) {
        if ((*entry)->handler == handler) {
            irq_handler_entry_t* to_free = *entry;
            *entry = (*entry)->next;
            kfree(to_free);
            spinlock_unlock(&irq_handler_lock);
            kinfo("Unregistered IRQ handler for IRQ %u\n", irq);
            return 0;
        }
        entry = &(*entry)->next;
    }
    
    spinlock_unlock(&irq_handler_lock);
    return -1;  // Handler not found
}

/**
 * Call user-space IRQ handlers
 */
void irq_call_handlers(uint8_t irq) {
    if (irq >= MAX_IRQ_HANDLERS) {
        return;
    }
    
    // Don't lock here - handlers are called from interrupt context
    // We'll use a lock-free approach or ensure handlers are safe
    
    irq_handler_entry_t* entry = irq_handlers[irq];
    while (entry) {
        if (entry->handler) {
            entry->handler(entry->context);
        }
        entry = entry->next;
    }
}

/**
 * Enable IRQ in PIC
 */
void irq_enable(uint8_t irq) {
    if (irq >= 16) {
        return;  // Invalid IRQ
    }
    
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = 0x21;  // Master PIC
    } else {
        port = 0xA1;  // Slave PIC
        irq -= 8;
    }
    
    // Read current mask
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    
    // Clear bit to enable IRQ
    value &= ~(1 << irq);
    
    // Write back
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * Disable IRQ in PIC
 */
void irq_disable(uint8_t irq) {
    if (irq >= 16) {
        return;  // Invalid IRQ
    }
    
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = 0x21;  // Master PIC
    } else {
        port = 0xA1;  // Slave PIC
        irq -= 8;
    }
    
    // Read current mask
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    
    // Set bit to disable IRQ
    value |= (1 << irq);
    
    // Write back
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

