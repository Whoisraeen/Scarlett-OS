//! Memory-Mapped I/O utilities

use crate::syscalls;

/// MMIO region wrapper
pub struct MmioRegion {
    base: *mut u8,
    size: usize,
}

impl MmioRegion {
    /// Map a physical MMIO region
    pub fn map(physical_addr: u64, size: usize) -> Result<Self, ()> {
        let base = syscalls::mmio_map(physical_addr, size as u64).map_err(|_| ())?;
        Ok(Self { base, size })
    }
    
    /// Get base address
    pub fn base(&self) -> *mut u8 {
        self.base
    }
    
    /// Read 8-bit value
    pub unsafe fn read8(&self, offset: usize) -> u8 {
        if offset >= self.size {
            return 0;
        }
        core::ptr::read_volatile(self.base.add(offset))
    }
    
    /// Write 8-bit value
    pub unsafe fn write8(&self, offset: usize, value: u8) {
        if offset >= self.size {
            return;
        }
        core::ptr::write_volatile(self.base.add(offset), value);
    }
    
    /// Read 16-bit value
    pub unsafe fn read16(&self, offset: usize) -> u16 {
        if offset + 2 > self.size {
            return 0;
        }
        core::ptr::read_volatile(self.base.add(offset) as *const u16)
    }
    
    /// Write 16-bit value
    pub unsafe fn write16(&self, offset: usize, value: u16) {
        if offset + 2 > self.size {
            return;
        }
        core::ptr::write_volatile(self.base.add(offset) as *mut u16, value);
    }
    
    /// Read 32-bit value
    pub unsafe fn read32(&self, offset: usize) -> u32 {
        if offset + 4 > self.size {
            return 0;
        }
        core::ptr::read_volatile(self.base.add(offset) as *const u32)
    }
    
    /// Write 32-bit value
    pub unsafe fn write32(&self, offset: usize, value: u32) {
        if offset + 4 > self.size {
            return;
        }
        core::ptr::write_volatile(self.base.add(offset) as *mut u32, value);
    }
    
    /// Read 64-bit value
    pub unsafe fn read64(&self, offset: usize) -> u64 {
        if offset + 8 > self.size {
            return 0;
        }
        core::ptr::read_volatile(self.base.add(offset) as *const u64)
    }
    
    /// Write 64-bit value
    pub unsafe fn write64(&self, offset: usize, value: u64) {
        if offset + 8 > self.size {
            return;
        }
        core::ptr::write_volatile(self.base.add(offset) as *mut u64, value);
    }
}

impl Drop for MmioRegion {
    fn drop(&mut self) {
        let _ = syscalls::mmio_unmap(self.base, self.size as u64);
    }
}

