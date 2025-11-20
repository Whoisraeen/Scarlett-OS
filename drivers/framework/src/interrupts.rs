//! Interrupt handling for drivers

use crate::syscalls;

/// IRQ handler function type
pub type IrqHandler = extern "C" fn();

/// Register IRQ handler
pub fn register_irq(irq: u8, handler: IrqHandler) -> Result<(), ()> {
    syscalls::irq_register(irq, handler).map_err(|_| ())
}

/// Unregister IRQ handler
pub fn unregister_irq(irq: u8) -> Result<(), ()> {
    syscalls::irq_unregister(irq).map_err(|_| ())
}

/// Enable IRQ
pub fn enable_irq(irq: u8) -> Result<(), ()> {
    syscalls::irq_enable(irq).map_err(|_| ())
}

/// Disable IRQ
pub fn disable_irq(irq: u8) -> Result<(), ()> {
    syscalls::irq_disable(irq).map_err(|_| ())
}

