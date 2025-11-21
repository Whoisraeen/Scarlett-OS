/**
 * @file init.rs
 * @brief User-space init process - launches all system services and desktop
 */

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

// IPC syscall wrappers
extern "C" {
    fn sys_ipc_send(tid: u32, msg: *const IpcMessage) -> i32;
    fn sys_ipc_receive(port: u32, msg: *mut IpcMessage) -> i32;
    fn sys_exec(path: *const u8, args: *const *const u8) -> i32;
    fn sys_fork() -> i32;
    fn sys_wait(pid: i32) -> i32;
    fn sys_exit(code: i32) -> !;
}

#[repr(C)]
struct IpcMessage {
    sender_tid: u32,
    msg_type: u32,
    data: [u8; 256],
}

// Service launch order
const SERVICES: &[&str] = &[
    "/sbin/driver_manager",     // 1. Driver manager (manages all drivers)
    "/sbin/vfs",                // 2. Virtual File System
    "/sbin/security",           // 3. Security service (capabilities, ACL)
    "/sbin/network",            // 4. Network stack
    "/sbin/audio",              // 5. Audio server
    "/sbin/compositor",         // 6. Display compositor
    "/sbin/window_manager",     // 7. Window manager
];

// User applications (launched after services)
const APPS: &[&str] = &[
    "/bin/login",               // Login manager (first user app)
];

#[no_mangle]
pub extern "C" fn _start() -> ! {
    // Init process (PID 1) - runs in Ring 3
    
    // Phase 1: Launch system services
    launch_services();
    
    // Phase 2: Launch login manager
    launch_login();
    
    // Phase 3: Wait for login, then launch desktop
    wait_for_login_and_launch_desktop();
    
    // Phase 4: Reap zombie processes
    reaper_loop();
}

fn launch_services() {
    for service in SERVICES {
        let pid = unsafe { sys_fork() };
        
        if pid == 0 {
            // Child process - exec the service
            let args: [*const u8; 1] = [core::ptr::null()];
            unsafe {
                sys_exec(service.as_ptr(), args.as_ptr());
            }
            // If exec fails, exit
            unsafe { sys_exit(1); }
        }
        // Parent continues to launch next service
    }
}

fn launch_login() {
    let pid = unsafe { sys_fork() };
    
    if pid == 0 {
        // Child - exec login manager
        let args: [*const u8; 1] = [core::ptr::null()];
        unsafe {
            sys_exec("/bin/login".as_ptr(), args.as_ptr());
            sys_exit(1);
        }
    }
}

fn wait_for_login_and_launch_desktop() {
    // Wait for login manager to signal successful login
    // For now, just wait for login process to exit
    unsafe {
        sys_wait(-1);  // Wait for any child
    }
    
    // Launch desktop environment
    let pid = unsafe { sys_fork() };
    
    if pid == 0 {
        // Child - exec desktop
        let args: [*const u8; 1] = [core::ptr::null()];
        unsafe {
            sys_exec("/bin/desktop".as_ptr(), args.as_ptr());
            sys_exit(1);
        }
    }
}

fn reaper_loop() -> ! {
    // Init process must reap zombie processes
    loop {
        unsafe {
            sys_wait(-1);  // Wait for any child to exit
        }
        // Child exited, loop to wait for next one
    }
}
