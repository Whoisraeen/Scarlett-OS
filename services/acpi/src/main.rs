#![no_std]
#![no_main]

extern crate alloc;

use core::panic::PanicInfo;

// Syscall numbers (from kernel/include/syscall/syscall.h)
const SYS_MMIO_MAP: u64 = 36;
const SYS_MMIO_UNMAP: u64 = 37;
const SYS_WRITE: u64 = 1;

// ACPI Signatures
const RSDP_SIGNATURE: &[u8; 8] = b"RSD PTR ";
const RSDT_SIGNATURE: &[u8; 4] = b"RSDT";
const XSDT_SIGNATURE: &[u8; 4] = b"XSDT";

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

#[repr(C, packed)]
struct AcpiTable {
    signature: [u8; 4],
    length: u32,
    revision: u8,
    checksum: u8,
    oem_id: [u8; 6],
    oem_table_id: [u8; 8],
    oem_revision: u32,
    creator_id: u32,
    creator_revision: u32,
}

// Checksum validation
fn validate_checksum(address: u64, length: usize) -> bool {
    let mut sum: u8 = 0;
    for i in 0..length {
        sum = sum.wrapping_add(unsafe { *(address as *const u8).add(i) });
    }
    sum == 0
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

    let rsdp_opt = find_rsdp(bios_area_virt, bios_area_size);

    if let Some(rsdp_addr) = rsdp_opt {
        let rsdp = unsafe { &*(rsdp_addr as *const RsdpDescriptor) };
        print("RSDP found!\n");
        
        if rsdp.revision == 0 { // ACPI 1.0
            if validate_checksum(rsdp_addr, core::mem::size_of::<RsdpDescriptor>()) {
                print("RSDP 1.0 checksum valid.\n");
                parse_rsdt(rsdp.rsdt_address);
            } else {
                print("RSDP 1.0 checksum invalid!\n");
            }
        } else { // ACPI 2.0+
            let rsdp20 = unsafe { &*(rsdp_addr as *const RsdpDescriptor20) };
            if validate_checksum(rsdp_addr, rsdp20.length as usize) {
                print("RSDP 2.0+ checksum valid.\n");
                parse_xsdt(rsdp20.xsdt_address);
            } else {
                print("RSDP 2.0+ checksum invalid!\n");
            }
        }
    } else {
        print("RSDP not found.\n");
    }

    // Keep service alive, waiting for IPC requests (not implemented yet)
    loop {}
}

fn find_rsdp(base_addr: u64, size: u64) -> Option<u64> {
    let start = base_addr as *const u8;
    
    for offset in (0..size).step_by(16) {
        unsafe {
            let ptr = start.add(offset as usize);
            let signature = core::slice::from_raw_parts(ptr, 8);
            
            if signature == RSDP_SIGNATURE {
                // Found potential RSDP, now validate checksum based on revision
                let rsdp_desc = &*(ptr as *const RsdpDescriptor);
                if rsdp_desc.revision == 0 { // ACPI 1.0
                    if validate_checksum(ptr as u64, core::mem::size_of::<RsdpDescriptor>()) {
                        return Some(ptr as u64);
                    }
                } else { // ACPI 2.0+
                    let rsdp20_desc = &*(ptr as *const RsdpDescriptor20);
                    // Check if entire RSDP 2.0+ structure is within bounds
                    if offset + rsdp20_desc.length as u64 <= size {
                        if validate_checksum(ptr as u64, rsdp20_desc.length as usize) {
                            return Some(ptr as u64);
                        }
                    }
                }
            }
        }
    }
    None
}

fn parse_rsdt(rsdt_phys_addr: u32) {
    print("Parsing RSDT (32-bit addresses)...");
    let rsdt_base_virt = unsafe { sys_mmio_map(rsdt_phys_addr as u64, 4096) }; // Map one page for now
    if rsdt_base_virt == 0 {
        print("Failed to map RSDT!\n");
        return;
    }

    let rsdt_header = unsafe { &*(rsdt_base_virt as *const AcpiTable) };
    if rsdt_header.signature != *RSDT_SIGNATURE {
        print("RSDT signature mismatch!\n");
        unsafe { sys_mmio_unmap(rsdt_base_virt, 4096) };
        return;
    }

    if !validate_checksum(rsdt_base_virt, rsdt_header.length as usize) {
        print("RSDT checksum invalid!\n");
        unsafe { sys_mmio_unmap(rsdt_base_virt, 4096) };
        return;
    }
    print("RSDT checksum valid.\n");

    let entry_count = (rsdt_header.length as usize - core::mem::size_of::<AcpiTable>()) / 4;
    let entries_ptr = unsafe { (rsdt_base_virt as *const u8).add(core::mem::size_of::<AcpiTable>()) as *const u32 };

    for i in 0..entry_count {
        let table_phys_addr = unsafe { *entries_ptr.add(i) };
        print("Found ACPI table entry (RSDT): ");
        print_hex(table_phys_addr as u64);
        print("\n");
        // Here we would map each table and parse it (FADT, MADT, etc.)
    }

    unsafe { sys_mmio_unmap(rsdt_base_virt, 4096) };
}

fn parse_xsdt(xsdt_phys_addr: u64) {
    print("Parsing XSDT (64-bit addresses)...");
    let xsdt_base_virt = unsafe { sys_mmio_map(xsdt_phys_addr, 4096) }; // Map one page for now
    if xsdt_base_virt == 0 {
        print("Failed to map XSDT!\n");
        return;
    }

    let xsdt_header = unsafe { &*(xsdt_base_virt as *const AcpiTable) };
    if xsdt_header.signature != *XSDT_SIGNATURE {
        print("XSDT signature mismatch!\n");
        unsafe { sys_mmio_unmap(xsdt_base_virt, 4096) };
        return;
    }

    if !validate_checksum(xsdt_base_virt, xsdt_header.length as usize) {
        print("XSDT checksum invalid!\n");
        unsafe { sys_mmio_unmap(xsdt_base_virt, 4096) };
        return;
    }
    print("XSDT checksum valid.\n");

    let entry_count = (xsdt_header.length as usize - core::mem::size_of::<AcpiTable>()) / 8;
    let entries_ptr = unsafe { (xsdt_base_virt as *const u8).add(core::mem::size_of::<AcpiTable>()) as *const u64 };

    for i in 0..entry_count {
        let table_phys_addr = unsafe { *entries_ptr.add(i) };
        print("Found ACPI table entry (XSDT): ");
        print_hex(table_phys_addr);
        print("\n");
        // Here we would map each table and parse it (FADT, MADT, etc.)
    }

    unsafe { sys_mmio_unmap(xsdt_base_virt, 4096) };
}

// Syscall wrappers
unsafe fn sys_mmio_map(paddr: u64, size: u64) -> u64 {
    let ret: u64;
    core::arch::asm!(
        "syscall",
        in("rax") SYS_MMIO_MAP, // syscall number
        in("rdi") paddr,
        in("rsi") size,
        out("rax") ret,
        lateout("rcx") _,
        lateout("r11") _,
    );
    ret
}

unsafe fn sys_mmio_unmap(vaddr: u64, size: u64) {
    core::arch::asm!(
        "syscall",
        in("rax") SYS_MMIO_UNMAP, // syscall number
        in("rdi") vaddr,
        in("rsi") size,
        lateout("rax") _,
        lateout("rcx") _,
        lateout("r11") _,
    );
}

fn print(s: &str) {
    unsafe {
        core::arch::asm!(
            "syscall",
            in("rax") SYS_WRITE, // syscall number
            in("rdi") 1, // stdout
            in("rsi") s.as_ptr(),
            in("rdx") s.len(),
            lateout("rax") _,
            lateout("rcx") _,
            lateout("r11") _,
        );
    }
}

// Helper to print hex numbers
fn print_hex(value: u64) {
    let mut buffer = [0u8; 18]; // 0x + 16 hex digits
    buffer[0] = b'0';
    buffer[1] = b'x';
    
    let mut val = value;
    for i in (0..16).rev() {
        let byte = (val & 0xF) as u8;
        buffer[i + 2] = if byte < 10 { b'0' + byte } else { b'a' + (byte - 10) };
        val >>= 4;
    }
    print(core::str::from_utf8(&buffer).unwrap());
}


#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    print("ACPI Service Panicked!\n");
    loop {}
}