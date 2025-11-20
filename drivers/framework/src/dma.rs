//! DMA buffer management

use crate::syscalls;

/// DMA buffer wrapper
pub struct DmaBuffer {
    ptr: *mut u8,
    size: usize,
}

impl DmaBuffer {
    /// Allocate DMA buffer
    pub fn alloc(size: usize, flags: u64) -> Result<Self, ()> {
        let ptr = syscalls::dma_alloc(size as u64, flags).map_err(|_| ())?;
        Ok(Self { ptr, size })
    }
    
    /// Get pointer
    pub fn as_ptr(&self) -> *mut u8 {
        self.ptr
    }
    
    /// Get size
    pub fn size(&self) -> usize {
        self.size
    }
    
    /// Get as mutable slice (unsafe)
    pub unsafe fn as_mut_slice(&mut self) -> &mut [u8] {
        core::slice::from_raw_parts_mut(self.ptr, self.size)
    }
    
    /// Get physical address
    pub fn get_physical(&self) -> Result<u64, ()> {
        syscalls::dma_get_physical(self.ptr as u64).map_err(|_| ())
    }
}

impl Drop for DmaBuffer {
    fn drop(&mut self) {
        let _ = syscalls::dma_free(self.ptr);
    }
}

