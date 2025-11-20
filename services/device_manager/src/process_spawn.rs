//! Process spawning for driver loading

use crate::syscalls;

/// Spawn a driver process
/// Returns the IPC port of the spawned driver process
pub fn spawn_driver_process(driver_name: &str) -> Result<u64, ()> {
    // Driver entry points (would be loaded from filesystem in real implementation)
    let entry_point = match driver_name {
        "ahci" => 0x500000u64,      // Driver entry point
        "ethernet" => 0x510000u64,  // Driver entry point
        _ => return Err(()),
    };
    
    // Convert driver name to C string
    let name_bytes = driver_name.as_bytes();
    let mut name_buf = [0u8; 64];
    let len = name_bytes.len().min(63);
    name_buf[0..len].copy_from_slice(&name_bytes[0..len]);
    
    // Spawn the process
    let pid = match syscalls::spawn_process(name_buf.as_ptr() as *const u8, core::ptr::null(), entry_point) {
        Ok(p) => p,
        Err(_) => return Err(()),
    };
    
    // Get IPC port from the spawned process
    let port = match syscalls::get_process_ipc_port(pid) {
        Ok(p) => p,
        Err(_) => return Err(()),
    };
    
    Ok(port)
}

/// System call wrappers
#[allow(dead_code)]
mod syscalls {
    /// Spawn a new process
    /// Returns process ID on success, 0 on failure
    pub fn spawn_process(name: *const u8, path: *const u8, entry_point: u64) -> Result<u64, ()> {
        const SYS_SPAWN_PROCESS: u64 = 45;
        let pid = unsafe { syscall_raw(SYS_SPAWN_PROCESS, name as u64, path as u64, entry_point, 0, 0) };
        if pid == 0 {
            Err(())
        } else {
            Ok(pid)
        }
    }
    
    /// Get IPC port of a process
    /// Returns IPC port on success, 0 on failure
    pub fn get_process_ipc_port(pid: u64) -> Result<u64, ()> {
        const SYS_GET_PROCESS_IPC_PORT: u64 = 46;
        let port = unsafe { syscall_raw(SYS_GET_PROCESS_IPC_PORT, pid, 0, 0, 0, 0) };
        if port == 0 {
            Err(())
        } else {
            Ok(port)
        }
    }
    
    #[cfg(target_arch = "x86_64")]
    unsafe fn syscall_raw(num: u64, arg1: u64, arg2: u64, arg3: u64, arg4: u64, arg5: u64) -> u64 {
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
    unsafe fn syscall_raw(_num: u64, _arg1: u64, _arg2: u64, _arg3: u64, _arg4: u64, _arg5: u64) -> u64 {
        0
    }
}


