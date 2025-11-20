//! Access Control List (ACL) System

/// ACL permissions
pub const ACL_READ: u32 = 0x01;
pub const ACL_WRITE: u32 = 0x02;
pub const ACL_EXECUTE: u32 = 0x04;
pub const ACL_DELETE: u32 = 0x08;
pub const ACL_APPEND: u32 = 0x10;
pub const ACL_CHOWN: u32 = 0x20;
pub const ACL_CHMOD: u32 = 0x40;

/// ACL entry type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum AclEntryType {
    User = 1,
    Group = 2,
    Other = 3,
    Mask = 4,
}

/// ACL entry
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AclEntry {
    pub entry_type: AclEntryType,
    pub id: u32,           // User ID or Group ID
    pub permissions: u32,
}

impl AclEntry {
    pub fn new(entry_type: AclEntryType, id: u32, permissions: u32) -> Self {
        Self {
            entry_type,
            id,
            permissions,
        }
    }

    pub fn has_permission(&self, perm: u32) -> bool {
        (self.permissions & perm) != 0
    }
}

/// Access Control List
pub struct Acl {
    entries: [Option<AclEntry>; 32],
    count: usize,
}

impl Acl {
    pub fn new() -> Self {
        Self {
            entries: [None; 32],
            count: 0,
        }
    }

    pub fn add_entry(&mut self, entry: AclEntry) -> Result<(), ()> {
        if self.count >= 32 {
            return Err(());
        }

        for i in 0..32 {
            if self.entries[i].is_none() {
                self.entries[i] = Some(entry);
                self.count += 1;
                return Ok(());
            }
        }

        Err(())
    }

    pub fn remove_entry(&mut self, entry_type: AclEntryType, id: u32) -> Result<(), ()> {
        for i in 0..32 {
            if let Some(entry) = self.entries[i] {
                if entry.entry_type == entry_type && entry.id == id {
                    self.entries[i] = None;
                    self.count -= 1;
                    return Ok(());
                }
            }
        }

        Err(())
    }

    pub fn check_access(&self, uid: u32, gid: u32, requested_perms: u32) -> bool {
        // Check user-specific ACL
        for entry in &self.entries {
            if let Some(e) = entry {
                if e.entry_type == AclEntryType::User && e.id == uid {
                    return e.has_permission(requested_perms);
                }
            }
        }

        // Check group ACL
        for entry in &self.entries {
            if let Some(e) = entry {
                if e.entry_type == AclEntryType::Group && e.id == gid {
                    return e.has_permission(requested_perms);
                }
            }
        }

        // Check other ACL
        for entry in &self.entries {
            if let Some(e) = entry {
                if e.entry_type == AclEntryType::Other {
                    return e.has_permission(requested_perms);
                }
            }
        }

        false
    }
}
