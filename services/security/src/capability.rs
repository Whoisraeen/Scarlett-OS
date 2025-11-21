//! Capability System Implementation
//!
//! Provides unforgeable capability tokens for resource access control

use core::sync::atomic::{AtomicU64, Ordering};
use crate::syscalls::sys_get_uptime_ms;

/// Capability types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum CapabilityType {
    // File system capabilities
    FileRead = 1,
    FileWrite = 2,
    FileExecute = 3,
    FileDelete = 4,
    DirectoryCreate = 5,
    DirectoryList = 6,

    // Network capabilities
    NetworkSend = 10,
    NetworkReceive = 11,
    NetworkBind = 12,
    NetworkListen = 13,

    // Device capabilities
    DeviceRead = 20,
    DeviceWrite = 21,
    DeviceControl = 22,

    // Process capabilities
    ProcessCreate = 30,
    ProcessKill = 31,
    ProcessDebug = 32,

    // Memory capabilities
    MemoryAllocate = 40,
    MemoryMap = 41,
    MemoryDMA = 42,

    // IPC capabilities
    IpcSend = 50,
    IpcReceive = 51,
    IpcCreatePort = 52,

    // System capabilities
    SystemShutdown = 60,
    SystemReboot = 61,
    SystemTime = 62,

    // Hardware capabilities
    HardwareMMIO = 70,
    HardwareIRQ = 71,
    HardwareDMA = 72,
}

/// Capability structure
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Capability {
    /// Unique capability ID
    pub id: u64,

    /// Capability type
    pub cap_type: CapabilityType,

    /// Resource ID (file descriptor, device ID, etc.)
    pub resource_id: u64,

    /// Permissions mask
    pub permissions: u64,

    /// Owner process ID
    pub owner_pid: u32,

    /// Creation timestamp
    pub timestamp: u64,

    /// Expiration time (0 = never)
    pub expiration: u64,

    /// Delegation depth (how many times can be delegated)
    pub delegation_depth: u8,

    /// Flags
    pub flags: u8,

    /// Reserved
    _reserved: [u8; 6],
}

impl Capability {
    pub fn new(cap_type: CapabilityType, resource_id: u64, owner_pid: u32) -> Self {
        static NEXT_CAP_ID: AtomicU64 = AtomicU64::new(1);

        Self {
            id: NEXT_CAP_ID.fetch_add(1, Ordering::SeqCst),
            cap_type,
            resource_id,
            permissions: 0xFFFFFFFFFFFFFFFF, // All permissions by default
            owner_pid,
            timestamp: sys_get_uptime_ms(),
            expiration: 0, // Never expires
            delegation_depth: 3, // Can be delegated 3 times
            flags: 0,
            _reserved: [0; 6],
        }
    }

    /// Check if capability has specific permission
    pub fn has_permission(&self, permission: u64) -> bool {
        (self.permissions & permission) != 0
    }

    /// Attenuate capability (reduce permissions)
    pub fn attenuate(&self, new_permissions: u64) -> Capability {
        let mut new_cap = *self;
        new_cap.permissions &= new_permissions; // Intersection of permissions
        new_cap
    }

    /// Delegate capability to another process
    pub fn delegate(&self, target_pid: u32) -> Result<Capability, ()> {
        if self.delegation_depth == 0 {
            return Err(()); // Cannot delegate further
        }

        let mut new_cap = *self;
        new_cap.owner_pid = target_pid;
        new_cap.delegation_depth -= 1;
        Ok(new_cap)
    }

    /// Check if capability has expired
    pub fn is_expired(&self, current_time: u64) -> bool {
        if self.expiration == 0 {
            false // Never expires
        } else {
            current_time >= self.expiration
        }
    }
}

/// Capability table entry
#[derive(Clone, Copy)]
pub struct CapabilityEntry {
    pub capability: Capability,
    pub valid: bool,
}

const MAX_CAPABILITIES: usize = 4096;

/// Per-process capability table
pub struct CapabilityTable {
    entries: [CapabilityEntry; MAX_CAPABILITIES],
    count: usize,
}

impl CapabilityTable {
    pub fn new() -> Self {
        Self {
            entries: [CapabilityEntry {
                capability: Capability {
                    id: 0,
                    cap_type: CapabilityType::FileRead,
                    resource_id: 0,
                    permissions: 0,
                    owner_pid: 0,
                    timestamp: 0,
                    expiration: 0,
                    delegation_depth: 0,
                    flags: 0,
                    _reserved: [0; 6],
                },
                valid: false,
            }; MAX_CAPABILITIES],
            count: 0,
        }
    }

    /// Add capability to table
    pub fn add(&mut self, cap: Capability) -> Result<usize, ()> {
        // Find free slot
        for i in 0..MAX_CAPABILITIES {
            if !self.entries[i].valid {
                self.entries[i].capability = cap;
                self.entries[i].valid = true;
                self.count += 1;
                return Ok(i);
            }
        }

        Err(()) // Table full
    }

    /// Remove capability from table
    pub fn remove(&mut self, cap_idx: usize) -> Result<(), ()> {
        if cap_idx >= MAX_CAPABILITIES {
            return Err(());
        }

        if self.entries[cap_idx].valid {
            self.entries[cap_idx].valid = false;
            self.count -= 1;
            Ok(())
        } else {
            Err(())
        }
    }

    /// Get capability by index
    pub fn get(&self, cap_idx: usize) -> Option<&Capability> {
        if cap_idx >= MAX_CAPABILITIES {
            return None;
        }

        if self.entries[cap_idx].valid {
            Some(&self.entries[cap_idx].capability)
        } else {
            None
        }
    }

    /// Verify capability
    pub fn verify(&self, cap_idx: usize, cap_type: CapabilityType, resource_id: u64) -> bool {
        if let Some(cap) = self.get(cap_idx) {
            cap.cap_type == cap_type && cap.resource_id == resource_id
        } else {
            false
        }
    }

    /// Find capability for resource
    pub fn find(&self, cap_type: CapabilityType, resource_id: u64) -> Option<usize> {
        for i in 0..MAX_CAPABILITIES {
            if self.entries[i].valid {
                let cap = &self.entries[i].capability;
                if cap.cap_type == cap_type && cap.resource_id == resource_id {
                    return Some(i);
                }
            }
        }

        None
    }
}

/// Global capability manager
pub struct CapabilityManager {
    // Per-process capability tables
    process_tables: [Option<CapabilityTable>; 256],
}

impl CapabilityManager {
    pub fn new() -> Self {
        Self {
            process_tables: [None; 256],
        }
    }

    /// Initialize capability table for process
    pub fn init_process(&mut self, pid: u32) -> Result<(), ()> {
        if pid as usize >= 256 {
            return Err(());
        }

        self.process_tables[pid as usize] = Some(CapabilityTable::new());
        Ok(())
    }

    /// Grant capability to process
    pub fn grant(&mut self, pid: u32, cap: Capability) -> Result<usize, ()> {
        if pid as usize >= 256 {
            return Err(());
        }

        if let Some(ref mut table) = self.process_tables[pid as usize] {
            table.add(cap)
        } else {
            Err(())
        }
    }

    /// Revoke capability from process
    pub fn revoke(&mut self, pid: u32, cap_idx: usize) -> Result<(), ()> {
        if pid as usize >= 256 {
            return Err(());
        }

        if let Some(ref mut table) = self.process_tables[pid as usize] {
            table.remove(cap_idx)
        } else {
            Err(())
        }
    }

    /// Check if process has capability
    pub fn check(&self, pid: u32, cap_type: CapabilityType, resource_id: u64) -> bool {
        if pid as usize >= 256 {
            return false;
        }

        if let Some(ref table) = self.process_tables[pid as usize] {
            table.find(cap_type, resource_id).is_some()
        } else {
            false
        }
    }
}
