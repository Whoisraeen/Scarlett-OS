//! Application Sandboxing System

use crate::capability::{Capability, CapabilityType, CapabilityTable};

/// Sandbox configuration
#[repr(C)]
pub struct SandboxConfig {
    /// Allowed file system paths
    pub allowed_paths: [[u8; 256]; 16],
    pub path_count: usize,

    /// Allowed network addresses
    pub allowed_networks: [u32; 16],
    pub network_count: usize,

    /// Allowed device access
    pub allowed_devices: [u64; 16],
    pub device_count: usize,

    /// Memory limit (bytes)
    pub memory_limit: u64,

    /// CPU time limit (microseconds)
    pub cpu_limit: u64,

    /// File descriptor limit
    pub fd_limit: u32,

    /// Network bandwidth limit (bytes/sec)
    pub bandwidth_limit: u64,

    /// Can fork processes
    pub can_fork: bool,

    /// Can execute programs
    pub can_exec: bool,

    /// Can access network
    pub can_network: bool,

    /// Can access hardware
    pub can_hardware: bool,
}

impl SandboxConfig {
    pub fn new_default() -> Self {
        Self {
            allowed_paths: [[0; 256]; 16],
            path_count: 0,
            allowed_networks: [0; 16],
            network_count: 0,
            allowed_devices: [0; 16],
            device_count: 0,
            memory_limit: 512 * 1024 * 1024, // 512 MB
            cpu_limit: 60 * 1000000, // 60 seconds
            fd_limit: 256,
            bandwidth_limit: 10 * 1024 * 1024, // 10 MB/s
            can_fork: false,
            can_exec: false,
            can_network: false,
            can_hardware: false,
        }
    }

    pub fn new_permissive() -> Self {
        Self {
            allowed_paths: [[0; 256]; 16],
            path_count: 0,
            allowed_networks: [0; 16],
            network_count: 0,
            allowed_devices: [0; 16],
            device_count: 0,
            memory_limit: u64::MAX,
            cpu_limit: u64::MAX,
            fd_limit: 4096,
            bandwidth_limit: u64::MAX,
            can_fork: true,
            can_exec: true,
            can_network: true,
            can_hardware: true,
        }
    }

    pub fn add_allowed_path(&mut self, path: &str) -> Result<(), ()> {
        if self.path_count >= 16 {
            return Err(());
        }

        let len = path.len().min(255);
        self.allowed_paths[self.path_count][0..len].copy_from_slice(&path.as_bytes()[0..len]);
        self.path_count += 1;

        Ok(())
    }

    pub fn check_path_allowed(&self, path: &str) -> bool {
        for i in 0..self.path_count {
            let allowed_path = core::str::from_utf8(&self.allowed_paths[i])
                .unwrap_or("");

            if path.starts_with(allowed_path) {
                return true;
            }
        }

        false
    }
}

/// Sandbox instance
pub struct Sandbox {
    pub pid: u32,
    pub config: SandboxConfig,
    pub capabilities: CapabilityTable,
    pub memory_used: u64,
    pub cpu_used: u64,
    pub fd_count: u32,
    pub bandwidth_used: u64,
}

impl Sandbox {
    pub fn new(pid: u32, config: SandboxConfig) -> Self {
        Self {
            pid,
            config,
            capabilities: CapabilityTable::new(),
            memory_used: 0,
            cpu_used: 0,
            fd_count: 0,
            bandwidth_used: 0,
        }
    }

    /// Check if resource access is allowed
    pub fn check_resource_access(&self, resource_type: &str, resource_id: &str) -> bool {
        match resource_type {
            "file" => self.config.check_path_allowed(resource_id),
            "network" => self.config.can_network,
            "device" => self.config.can_hardware,
            "fork" => self.config.can_fork,
            "exec" => self.config.can_exec,
            _ => false,
        }
    }

    /// Check if resource limit is exceeded
    pub fn check_limit(&self, limit_type: &str) -> bool {
        match limit_type {
            "memory" => self.memory_used < self.config.memory_limit,
            "cpu" => self.cpu_used < self.config.cpu_limit,
            "fd" => self.fd_count < self.config.fd_limit,
            "bandwidth" => self.bandwidth_used < self.config.bandwidth_limit,
            _ => true,
        }
    }

    /// Grant capability to sandbox
    pub fn grant_capability(&mut self, cap: Capability) -> Result<usize, ()> {
        self.capabilities.add(cap)
    }

    /// Revoke capability from sandbox
    pub fn revoke_capability(&mut self, cap_idx: usize) -> Result<(), ()> {
        self.capabilities.remove(cap_idx)
    }
}

/// Sandbox manager
pub struct SandboxManager {
    sandboxes: [Option<Sandbox>; 256],
}

impl SandboxManager {
    pub fn new() -> Self {
        Self {
            sandboxes: [None; 256],
        }
    }

    /// Create sandbox for process
    pub fn create_sandbox(&mut self, pid: u32, config: SandboxConfig) -> Result<(), ()> {
        if pid as usize >= 256 {
            return Err(());
        }

        self.sandboxes[pid as usize] = Some(Sandbox::new(pid, config));
        Ok(())
    }

    /// Get sandbox for process
    pub fn get_sandbox(&self, pid: u32) -> Option<&Sandbox> {
        if pid as usize >= 256 {
            return None;
        }

        self.sandboxes[pid as usize].as_ref()
    }

    /// Get mutable sandbox for process
    pub fn get_sandbox_mut(&mut self, pid: u32) -> Option<&mut Sandbox> {
        if pid as usize >= 256 {
            return None;
        }

        self.sandboxes[pid as usize].as_mut()
    }

    /// Destroy sandbox for process
    pub fn destroy_sandbox(&mut self, pid: u32) -> Result<(), ()> {
        if pid as usize >= 256 {
            return Err(());
        }

        self.sandboxes[pid as usize] = None;
        Ok(())
    }

    /// Check if process has access to resource
    pub fn check_access(&self, pid: u32, resource_type: &str, resource_id: &str) -> bool {
        if let Some(sandbox) = self.get_sandbox(pid) {
            sandbox.check_resource_access(resource_type, resource_id)
        } else {
            false
        }
    }
}
