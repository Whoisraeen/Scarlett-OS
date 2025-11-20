//! System call wrappers for drivers

/// Raw syscall function (architecture-specific)
#[cfg(target_arch = "x86_64")]
pub unsafe fn syscall_raw(num: u64, arg1: u64, arg2: u64, arg3: u64, arg4: u64, arg5: u64) -> u64 {
    let ret: u64;
    core::arch::asm!(
        "syscall",
        in("rax") num,
        in("rdi") arg1,
        in("rsi") arg2,
        in("rdx") arg3,
        in("r10") arg4,
        in("r8") arg5,
        out("rax") ret,
        options(nostack, preserves_flags)
    );
    ret
}

#[cfg(not(target_arch = "x86_64"))]
pub unsafe fn syscall_raw(_num: u64, _arg1: u64, _arg2: u64, _arg3: u64, _arg4: u64, _arg5: u64) -> u64 {
    0
}

// System call numbers (from kernel/include/syscall/syscall.h)
const SYS_IPC_SEND: u64 = 9;
const SYS_IPC_RECEIVE: u64 = 10;
const SYS_IPC_CREATE_PORT: u64 = 26;
const SYS_MMIO_MAP: u64 = 36;
const SYS_MMIO_UNMAP: u64 = 37;
const SYS_DMA_ALLOC: u64 = 34;
const SYS_DMA_FREE: u64 = 35;
const SYS_DMA_GET_PHYSICAL: u64 = 36; // Note: This may need to be added to kernel
const SYS_IRQ_REGISTER: u64 = 30;
const SYS_IRQ_UNREGISTER: u64 = 31;
const SYS_IRQ_ENABLE: u64 = 32;
const SYS_IRQ_DISABLE: u64 = 33;
const SYS_PCI_READ_CONFIG: u64 = 28;
const SYS_PCI_WRITE_CONFIG: u64 = 29;

/// IPC send
pub fn ipc_send(port_id: u64, msg_ptr: u64) -> u64 {
    unsafe { syscall_raw(SYS_IPC_SEND, port_id, msg_ptr, 0, 0, 0) }
}

/// IPC receive
pub fn ipc_receive(port_id: u64, msg_ptr: u64) -> u64 {
    unsafe { syscall_raw(SYS_IPC_RECEIVE, port_id, msg_ptr, 0, 0, 0) }
}

/// Create IPC port
pub fn ipc_create_port() -> u64 {
    unsafe { syscall_raw(SYS_IPC_CREATE_PORT, 0, 0, 0, 0, 0) }
}

/// Map MMIO region
pub fn mmio_map(physical_addr: u64, size: u64) -> Result<*mut u8, u64> {
    let result = unsafe { syscall_raw(SYS_MMIO_MAP, physical_addr, size, 0, 0, 0) };
    if result == 0 {
        Err(1) // Invalid result
    } else {
        Ok(result as *mut u8)
    }
}

/// Unmap MMIO region
pub fn mmio_unmap(vaddr: *mut u8, size: u64) -> Result<(), u64> {
    let result = unsafe { syscall_raw(SYS_MMIO_UNMAP, vaddr as u64, size, 0, 0, 0) };
    if result == 0 {
        Ok(())
    } else {
        Err(result)
    }
}

/// Allocate DMA buffer
pub fn dma_alloc(size: u64, flags: u64) -> Result<*mut u8, u64> {
    let result = unsafe { syscall_raw(SYS_DMA_ALLOC, size, flags, 0, 0, 0) };
    if result == 0 {
        Err(1)
    } else {
        Ok(result as *mut u8)
    }
}

/// Free DMA buffer
pub fn dma_free(ptr: *mut u8) -> Result<(), u64> {
    let result = unsafe { syscall_raw(SYS_DMA_FREE, ptr as u64, 0, 0, 0, 0) };
    if result == 0 {
        Ok(())
    } else {
        Err(result)
    }
}

/// Get physical address of DMA buffer
pub fn dma_get_physical(vaddr: u64) -> Result<u64, u64> {
    let result = unsafe { syscall_raw(SYS_DMA_GET_PHYSICAL, vaddr, 0, 0, 0, 0) };
    if result == 0 {
        Err(1)
    } else {
        Ok(result)
    }
}

/// Register IRQ handler
pub fn irq_register(irq: u8, handler: extern "C" fn()) -> Result<(), u64> {
    let result = unsafe { syscall_raw(SYS_IRQ_REGISTER, irq as u64, handler as u64, 0, 0, 0) };
    if result == 0 {
        Ok(())
    } else {
        Err(result)
    }
}

/// Unregister IRQ handler
pub fn irq_unregister(irq: u8) -> Result<(), u64> {
    let result = unsafe { syscall_raw(SYS_IRQ_UNREGISTER, irq as u64, 0, 0, 0, 0) };
    if result == 0 {
        Ok(())
    } else {
        Err(result)
    }
}

/// Enable IRQ
pub fn irq_enable(irq: u8) -> Result<(), u64> {
    let result = unsafe { syscall_raw(SYS_IRQ_ENABLE, irq as u64, 0, 0, 0, 0) };
    if result == 0 {
        Ok(())
    } else {
        Err(result)
    }
}

/// Disable IRQ
pub fn irq_disable(irq: u8) -> Result<(), u64> {
    let result = unsafe { syscall_raw(SYS_IRQ_DISABLE, irq as u64, 0, 0, 0, 0) };
    if result == 0 {
        Ok(())
    } else {
        Err(result)
    }
}

/// Read PCI configuration
pub fn pci_read_config(bus: u8, device: u8, function: u8, offset: u8) -> u32 {
    let bdf = ((bus as u32) << 16) | ((device as u32) << 11) | ((function as u32) << 8) | (offset as u32);
    unsafe { syscall_raw(SYS_PCI_READ_CONFIG, bdf as u64, 0, 0, 0, 0) as u32 }
}

/// Write PCI configuration
pub fn pci_write_config(bus: u8, device: u8, function: u8, offset: u8, value: u32) -> Result<(), u64> {
    let bdf = ((bus as u32) << 16) | ((device as u32) << 11) | ((function as u32) << 8) | (offset as u32);
    let result = unsafe { syscall_raw(SYS_PCI_WRITE_CONFIG, bdf as u64, value as u64, 0, 0, 0) };
    if result == 0 {
        Ok(())
    } else {
        Err(result)
    }
}

