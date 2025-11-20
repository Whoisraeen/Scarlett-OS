/**
 * @file irq_handler.h
 * @brief User-space interrupt handler interface
 */

#ifndef KERNEL_HAL_IRQ_HANDLER_H
#define KERNEL_HAL_IRQ_HANDLER_H

#include "../../include/types.h"

// IRQ handler callback type
typedef void (*irq_handler_callback_t)(void* context);

/**
 * Register an IRQ handler from user-space
 */
int irq_register(uint8_t irq, irq_handler_callback_t handler, void* context);

/**
 * Unregister an IRQ handler
 */
int irq_unregister(uint8_t irq, irq_handler_callback_t handler);

/**
 * Call user-space IRQ handlers (called from interrupt context)
 */
void irq_call_handlers(uint8_t irq);

/**
 * Enable IRQ in PIC
 */
void irq_enable(uint8_t irq);

/**
 * Disable IRQ in PIC
 */
void irq_disable(uint8_t irq);

#endif // KERNEL_HAL_IRQ_HANDLER_H

