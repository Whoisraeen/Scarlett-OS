//! System call wrappers for network service

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

