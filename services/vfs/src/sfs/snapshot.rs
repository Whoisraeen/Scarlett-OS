//! Snapshot Manager

use crate::file_ops::{VfsResult, VfsError};
use alloc::collections::BTreeMap;
use alloc::string::String;

/// Snapshot metadata
#[derive(Clone)]
pub struct Snapshot {
    pub id: u64,
    pub name: String,
    pub generation: u64,
    pub root_inode: u64,
    pub timestamp: u64,
    pub parent_snapshot: Option<u64>,
}

/// Snapshot Manager
pub struct SnapshotManager {
    snapshots: BTreeMap<u64, Snapshot>,
    next_id: u64,
}

impl SnapshotManager {
    pub fn new() -> Self {
        Self {
            snapshots: BTreeMap::new(),
            next_id: 1,
        }
    }

    /// Create a new snapshot
    pub fn create_snapshot(
        &mut self,
        generation: u64,
        name: &str,
        root_inode: u64,
    ) -> VfsResult<u64> {
        let id = self.next_id;
        self.next_id += 1;

        let snapshot = Snapshot {
            id,
            name: String::from(name),
            generation,
            root_inode,
            timestamp: 0, // TODO: Get current time
            parent_snapshot: None,
        };

        self.snapshots.insert(id, snapshot);

        Ok(id)
    }

    /// Get snapshot by ID
    pub fn get_snapshot(&self, id: u64) -> VfsResult<&Snapshot> {
        self.snapshots.get(&id).ok_or(VfsError::NotFound)
    }

    /// Delete snapshot
    pub fn delete_snapshot(&mut self, id: u64) -> VfsResult<()> {
        self.snapshots.remove(&id).ok_or(VfsError::NotFound)?;
        Ok(())
    }

    /// List all snapshots
    pub fn list_snapshots(&self) -> Vec<u64> {
        self.snapshots.keys().copied().collect()
    }
}
