//! Copy-on-Write Manager

use crate::file_ops::VfsResult;
use alloc::collections::BTreeMap;

/// CoW Manager
pub struct CowManager {
    /// Block reference counts
    refcounts: BTreeMap<u64, u32>,

    /// Modified blocks in current transaction
    modified_blocks: BTreeMap<u64, u64>,
}

impl CowManager {
    pub fn new() -> Self {
        Self {
            refcounts: BTreeMap::new(),
            modified_blocks: BTreeMap::new(),
        }
    }

    /// Check if block is shared (refcount > 1)
    pub fn is_shared(&self, block: u64) -> bool {
        self.refcounts.get(&block).map(|&count| count > 1).unwrap_or(false)
    }

    /// Increment block reference count
    pub fn inc_refcount(&mut self, block: u64) {
        let count = self.refcounts.entry(block).or_insert(0);
        *count += 1;
    }

    /// Decrement block reference count
    pub fn dec_refcount(&mut self, block: u64) -> u32 {
        if let Some(count) = self.refcounts.get_mut(&block) {
            if *count > 0 {
                *count -= 1;
            }
            *count
        } else {
            0
        }
    }

    /// Mark block as modified
    pub fn mark_modified(&mut self, block: u64) {
        self.modified_blocks.insert(block, block);
    }

    /// Clear modified blocks
    pub fn clear_modified(&mut self) {
        self.modified_blocks.clear();
    }
}
