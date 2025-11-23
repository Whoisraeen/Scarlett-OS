#![no_std]
#![no_main]

extern crate alloc;

use core::panic::PanicInfo;

// Syscall numbers
const SYS_MMIO_MAP: u64 = 508;
const SYS_MMIO_UNMAP: u64 = 509;
const SYS_WRITE: u64 = 1;

// ACPI Signatures
const RSDP_SIGNATURE: &[u8; 8] = b"RSD PTR ";

#[repr(C, packed)]
struct RsdpDescriptor {
    signature: [u8; 8],
    checksum: u8,
    oem_id: [u8; 6],
    revision: u8,
    rsdt_address: u32,
}

#[repr(C, packed)]
struct RsdpDescriptor20 {
    first_part: RsdpDescriptor,
    length: u32,
    xsdt_address: u64,
    extended_checksum: u8,
    reserved: [u8; 3],
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    print("ACPI Service Starting...\n");

    // Map BIOS area to search for RSDP (0xE0000 - 0xFFFFF)
    let bios_area_phys = 0xE0000;
    let bios_area_size = 0x20000; // 128KB (covers up to 0xFFFFF)
    
    let bios_area_virt = unsafe { sys_mmio_map(bios_area_phys, bios_area_size) };
    
    if bios_area_virt == 0 {
        print("Failed to map BIOS area!\n");
        loop {}
    }

    print("BIOS area mapped. Searching for RSDP...\n");

    let rsdp = find_rsdp(bios_area_virt, bios_area_size);

    if let Some(rsdp_addr) = rsdp {
        print("RSDP found!\n");
        // TODO: Parse RSDT/XSDT
    } else {
        print("RSDP not found.\n");
    }

    loop {}
}

fn find_rsdp(base_addr: u64, size: u64) -> Option<u64> {
    let start = base_addr as *const u8;
    
    // Search in 16-byte boundaries
    for offset in (0..size).step_by(16) {
        unsafe {
            let ptr = start.add(offset as usize);
            let signature = core::slice::from_raw_parts(ptr, 8);
            
            if signature == RSDP_SIGNATURE {
                // TODO: Validate checksum
                return Some(base_addr + offset);
            }
        }
    }
    None
}

// Syscall wrappers
unsafe fn sys_mmio_map(paddr: u64, size: u64) -> u64 {
    let ret: u64;
    core::arch::asm!(
        "syscall",
        in("rdi") SYS_MMIO_MAP,
        in("rsi") paddr,
        in("rdx") size,
        out("rax") ret,
        lateout("rcx") _,
        lateout("r11") _,
    );
    ret
}

fn print(s: &str) {
    unsafe {
        core::arch::asm!(
            "syscall",
            in("rdi") SYS_WRITE,
            in("rsi") 1, // stdout
            in("rdx") s.as_ptr(),
            in("r10") s.len(),
            lateout("rax") _,
            lateout("rcx") _,
            lateout("r11") _,
        );
    }
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    print("ACPI Service Panicked!\n");
    loop {}
}
