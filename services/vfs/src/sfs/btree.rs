//! B-Tree for directory entries and extent trees

use crate::file_ops::{VfsResult, VfsError};

/// B-Tree node
pub struct BTreeNode {
    pub keys: Vec<u64>,
    pub values: Vec<u64>,
    pub children: Vec<u64>,
    pub is_leaf: bool,
}

impl BTreeNode {
    pub fn new(is_leaf: bool) -> Self {
        Self {
            keys: Vec::new(),
            values: Vec::new(),
            children: Vec::new(),
            is_leaf,
        }
    }
}

/// B-Tree
pub struct BTree {
    root: u64,
    order: usize,
}

impl BTree {
    pub fn new(order: usize) -> Self {
        Self { root: 0, order }
    }

    pub fn search(&self, key: u64) -> VfsResult<u64> {
        // TODO: Implement B-tree search
        Err(VfsError::NotFound)
    }

    pub fn insert(&mut self, key: u64, value: u64) -> VfsResult<()> {
        // TODO: Implement B-tree insert
        Ok(())
    }

    pub fn delete(&mut self, key: u64) -> VfsResult<()> {
        // TODO: Implement B-tree delete
        Ok(())
    }
}
