//! System call wrappers for VFS service

/// Yield to scheduler
pub fn sys_yield() {
    const SYS_YIELD: u64 = 6;
    unsafe {
        #[cfg(target_arch = "x86_64")]
        core::arch::asm!(
            "syscall",
            in("rax") SYS_YIELD,
            options(nostack, preserves_flags)
        );
    }
}

/// Get system uptime in milliseconds
pub fn sys_get_uptime_ms() -> u64 {
    const SYS_GET_UPTIME_MS: u64 = 47;
    unsafe {
        #[cfg(target_arch = "x86_64")]
        {
            let ret: u64;
            core::arch::asm!(
                "syscall",
                in("rax") SYS_GET_UPTIME_MS,
                out("rax") ret,
                options(nostack, preserves_flags)
            );
            ret
        }
        #[cfg(not(target_arch = "x86_64"))]
        0
    }
}

