# Scarlett OS Makefile
# Main build system for the operating system

.PHONY: all clean bootloader kernel run debug setup help

# Configuration
TARGET_X86 = x86_64-elf
CC = $(TARGET_X86)-gcc
LD = $(TARGET_X86)-ld
AS = nasm
OBJCOPY = $(TARGET_X86)-objcopy

# Directories
BUILD_DIR = build
BOOT_DIR = bootloader
KERNEL_DIR = kernel
TOOLS_DIR = tools

# Flags
CFLAGS = -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
         -mno-red-zone -Wall -Wextra -O2 -g
LDFLAGS = -nostdlib
ASFLAGS = -f elf64

# QEMU settings
QEMU = qemu-system-x86_64
QEMU_FLAGS = -m 512M -serial stdio -no-reboot -no-shutdown
OVMF_CODE = /usr/share/OVMF/OVMF_CODE.fd
DISK_IMG = $(BUILD_DIR)/disk.img

# Default target
all: setup bootloader kernel disk

help:
	@echo "Scarlett OS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build everything"
	@echo "  bootloader  - Build UEFI bootloader"
	@echo "  kernel      - Build kernel"
	@echo "  disk        - Create disk image"
	@echo "  run         - Run in QEMU"
	@echo "  debug       - Run with GDB debugging"
	@echo "  clean       - Clean build artifacts"
	@echo "  setup       - Create build directories"
	@echo "  help        - Show this help"

setup:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/boot/EFI/BOOT
	@echo "[SETUP] Build directories created"

# Bootloader build
bootloader: setup
	@echo "[BUILD] Building UEFI bootloader..."
	@$(MAKE) -C $(BOOT_DIR)/uefi
	@cp $(BOOT_DIR)/uefi/BOOTX64.EFI $(BUILD_DIR)/boot/EFI/BOOT/
	@echo "[INFO] Bootloader built and copied"

# Kernel build
kernel: setup
	@echo "[BUILD] Building kernel..."
	@$(MAKE) -C $(KERNEL_DIR)
	@cp $(KERNEL_DIR)/kernel.elf $(BUILD_DIR)/
	@echo "[INFO] Kernel built and copied"

# Create disk image
disk: setup bootloader kernel
	@echo "[DISK] Creating GPT disk image..."
	@dd if=/dev/zero of=$(DISK_IMG) bs=1M count=128 2>/dev/null || true
	@sgdisk -n 1:2048:0 -t 1:ef00 -c 1:'EFI' $(DISK_IMG) >/dev/null 2>&1 || true
	@dd if=/dev/zero of=$(BUILD_DIR)/part.img bs=512 count=260063 2>/dev/null || true
	@mkfs.fat -F 32 -n 'EFI' $(BUILD_DIR)/part.img >/dev/null 2>&1 || true
	@MTOOLS_SKIP_CHECK=1 mmd -i $(BUILD_DIR)/part.img ::/EFI 2>/dev/null || true
	@MTOOLS_SKIP_CHECK=1 mmd -i $(BUILD_DIR)/part.img ::/EFI/BOOT 2>/dev/null || true
	@MTOOLS_SKIP_CHECK=1 mcopy -i $(BUILD_DIR)/part.img $(BUILD_DIR)/boot/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT/ 2>/dev/null || true
	@MTOOLS_SKIP_CHECK=1 mcopy -i $(BUILD_DIR)/part.img $(BUILD_DIR)/kernel.elf ::/ 2>/dev/null || true
	@dd if=$(BUILD_DIR)/part.img of=$(DISK_IMG) bs=512 seek=2048 conv=notrunc 2>/dev/null || true
	@rm -f $(BUILD_DIR)/part.img
	@echo "[DISK] GPT disk image created and populated"

# Create ISO image (alternative boot method)
iso: setup bootloader kernel
	@echo "[ISO] Creating UEFI bootable ISO..."
	@mkdir -p $(BUILD_DIR)/iso/EFI/BOOT
	@cp $(BUILD_DIR)/boot/EFI/BOOT/BOOTX64.EFI $(BUILD_DIR)/iso/EFI/BOOT/
	@cp $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/iso/
	@xorriso -as mkisofs -R -f -e EFI/BOOT/BOOTX64.EFI -no-emul-boot \
		-o $(BUILD_DIR)/boot.iso $(BUILD_DIR)/iso >/dev/null 2>&1 || \
		genisoimage -R -f -e EFI/BOOT/BOOTX64.EFI -no-emul-boot \
		-o $(BUILD_DIR)/boot.iso $(BUILD_DIR)/iso >/dev/null 2>&1
	@echo "[ISO] ISO image created: $(BUILD_DIR)/boot.iso"

# Run from ISO
run-iso: iso
	@echo "[QEMU] Starting Scarlett OS from ISO..."
	@if [ -f "$(OVMF_CODE)" ]; then \
		$(QEMU) -bios $(OVMF_CODE) -cdrom $(BUILD_DIR)/boot.iso $(QEMU_FLAGS); \
	else \
		echo "[ERROR] OVMF firmware not found at $(OVMF_CODE)"; \
	fi

# Run in QEMU
run: all
	@echo "[QEMU] Starting Scarlett OS..."
	@if [ -f "$(OVMF_CODE)" ]; then \
		$(QEMU) -bios $(OVMF_CODE) -drive file=$(DISK_IMG),format=raw $(QEMU_FLAGS); \
	else \
		echo "[ERROR] OVMF firmware not found at $(OVMF_CODE)"; \
		echo "[INFO] Please install OVMF or update OVMF_CODE path in Makefile"; \
	fi

# Debug with GDB
debug: all
	@echo "[DEBUG] Starting QEMU with GDB support..."
	@echo "[INFO] Connect with: gdb -ex 'target remote localhost:1234'"
	@if [ -f "$(OVMF_CODE)" ]; then \
		$(QEMU) -bios $(OVMF_CODE) -drive file=$(DISK_IMG),format=raw $(QEMU_FLAGS) -s -S; \
	else \
		echo "[ERROR] OVMF firmware not found at $(OVMF_CODE)"; \
	fi

# Clean build artifacts
clean:
	@echo "[CLEAN] Removing build artifacts..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -C $(BOOT_DIR)/uefi clean
	@$(MAKE) -C $(KERNEL_DIR) clean
	@echo "[CLEAN] Done"

# Install verification
verify:
	@echo "Verifying development environment..."
	@command -v $(CC) >/dev/null 2>&1 && echo "✓ $(CC) found" || echo "✗ $(CC) NOT FOUND"
	@command -v $(AS) >/dev/null 2>&1 && echo "✓ $(AS) found" || echo "✗ $(AS) NOT FOUND"
	@command -v $(QEMU) >/dev/null 2>&1 && echo "✓ $(QEMU) found" || echo "✗ $(QEMU) NOT FOUND"
	@test -f $(OVMF_CODE) && echo "✓ OVMF firmware found" || echo "✗ OVMF firmware NOT FOUND at $(OVMF_CODE)"

