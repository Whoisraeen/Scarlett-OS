//! Block cache for SFS

use alloc::collections::BTreeMap;
use alloc::vec::Vec;

const CACHE_SIZE: usize = 1024; // Cache 1024 blocks (4MB)

/// Cached block
struct CachedBlock {
    block_num: u64,
    data: Vec<u8>,
    dirty: bool,
    access_time: u64,
}

/// Block cache
pub struct BlockCache {
    cache: BTreeMap<u64, CachedBlock>,
    access_counter: u64,
}

impl BlockCache {
    pub fn new() -> Self {
        Self {
            cache: BTreeMap::new(),
            access_counter: 0,
        }
    }

    /// Get block from cache
    pub fn get(&mut self, block_num: u64) -> Option<&[u8]> {
        self.access_counter += 1;

        if let Some(block) = self.cache.get_mut(&block_num) {
            block.access_time = self.access_counter;
            Some(&block.data)
        } else {
            None
        }
    }

    /// Put block in cache
    pub fn put(&mut self, block_num: u64, data: Vec<u8>, dirty: bool) {
        self.access_counter += 1;

        // Evict if cache is full
        if self.cache.len() >= CACHE_SIZE {
            self.evict_lru();
        }

        let block = CachedBlock {
            block_num,
            data,
            dirty,
            access_time: self.access_counter,
        };

        self.cache.insert(block_num, block);
    }

    /// Mark block as dirty
    pub fn mark_dirty(&mut self, block_num: u64) {
        if let Some(block) = self.cache.get_mut(&block_num) {
            block.dirty = true;
        }
    }

    /// Evict least recently used block
    fn evict_lru(&mut self) {
        if let Some((&block_num, block)) = self
            .cache
            .iter()
            .min_by_key(|(_, b)| b.access_time)
        {
            // Write back if dirty
            if block.dirty {
                // Write to disk via block device
                use crate::block_device::write_blocks;
                let _ = write_blocks(0, block_num, 1, &block.data);
            }
            self.cache.remove(&block_num);
        }
    }

    /// Flush all dirty blocks
    pub fn flush_all(&mut self) {
        // Write back all dirty blocks
        use crate::block_device::write_blocks;
        let mut blocks_to_flush = Vec::new();
        
        // Collect dirty blocks
        for (block_num, block) in self.cache.iter() {
            if block.dirty {
                blocks_to_flush.push((*block_num, block.data.clone()));
            }
        }
        
        // Write dirty blocks to disk
        for (block_num, data) in blocks_to_flush {
            let _ = write_blocks(0, block_num, 1, &data);
            if let Some(block) = self.cache.get_mut(&block_num) {
                block.dirty = false;
            }
        }
    }
}
