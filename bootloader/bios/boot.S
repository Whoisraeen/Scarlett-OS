/**
 * @file boot.S
 * @brief BIOS bootloader first stage (512 bytes)
 * 
 * This is the MBR boot sector that loads the second stage bootloader
 */

.code16
.section .text
.global _start

_start:
    // Clear segment registers
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    
    // Set up stack
    mov $0x7C00, %sp
    
    // Save boot drive
    mov %dl, boot_drive
    
    // Print boot message
    mov $boot_msg, %si
    call print_string
    
    // Load second stage (sectors 2-17, 16 sectors = 8KB)
    mov $0x02, %ah          // Read sectors
    mov $16, %al            // Number of sectors
    mov $0x0002, %cx        // Cylinder 0, sector 2
    mov $0x0000, %dx        // Head 0, drive in DL
    mov $0x7E00, %bx        // Load to 0x7E00
    int $0x13
    
    jc disk_error
    
    // Jump to second stage
    jmp $0x0000, $0x7E00
    
disk_error:
    mov $error_msg, %si
    call print_string
    jmp halt

print_string:
    lodsb
    or %al, %al
    jz print_done
    mov $0x0E, %ah
    int $0x10
    jmp print_string
print_done:
    ret

halt:
    cli
    hlt
    jmp halt

boot_msg: .asciz "Scarlett OS BIOS Bootloader\r\n"
error_msg: .asciz "Disk error!\r\n"
boot_drive: .byte 0

// Pad to 510 bytes
.org 510
// Boot signature
.word 0xAA55

