#ifndef KERNEL_HAL_TIMER_H
#define KERNEL_HAL_TIMER_H

#include "../types.h"

void timer_init(void);
uint64_t timer_get_ticks(void);
uint64_t timer_get_ms(void);
void timer_interrupt_handler(void);
void timer_sleep_ms(uint64_t ms);

#endif // KERNEL_HAL_TIMER_H
