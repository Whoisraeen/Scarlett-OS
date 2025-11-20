/**
 * @file multiboot2.S
 * @brief Multiboot2 header for QEMU testing
 *
 * This allows the kernel to boot with QEMU's -kernel flag
 * without needing a full UEFI bootloader.
 */

.section .multiboot, "a"
.align 8

multiboot2_header_start:
    .long 0xE85250D6                    # Multiboot2 magic
    .long 0                             # Architecture (0 = i386)
    .long multiboot2_header_end - multiboot2_header_start  # Header length
    .long -(0xE85250D6 + 0 + (multiboot2_header_end - multiboot2_header_start))  # Checksum

    # Framebuffer tag
    .align 8
framebuffer_tag_start:
    .short 5                            # Type: framebuffer
    .short 0                            # Flags
    .long framebuffer_tag_end - framebuffer_tag_start  # Size
    .long 1024                          # Width
    .long 768                           # Height
    .long 32                            # Depth
framebuffer_tag_end:

    # End tag
    .align 8
    .short 0                            # Type: end
    .short 0                            # Flags
    .long 8                             # Size

multiboot2_header_end:

.section .text
.global multiboot2_entry
.extern _start

multiboot2_entry:
    # Save multiboot2 info
    # EAX contains magic (0x36D76289)
    # EBX contains physical address of multiboot2 info structure
    
    # For now, just jump to our normal entry point
    # In a complete implementation, we would parse the multiboot2 info
    # and populate our boot_info structure
    
    # Clear boot_info register (RDI)
    xor %rdi, %rdi
    
    # Jump to kernel entry
    jmp _start

