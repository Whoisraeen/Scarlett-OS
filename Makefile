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

# Bootloader build (placeholder for now)
bootloader: setup
	@echo "[BUILD] Building UEFI bootloader..."
	@echo "[INFO] Bootloader build target prepared"

# Kernel build (placeholder for now)
kernel: setup
	@echo "[BUILD] Building kernel..."
	@echo "[INFO] Kernel build target prepared"

# Create disk image
disk: setup
	@echo "[DISK] Creating disk image..."
	@dd if=/dev/zero of=$(DISK_IMG) bs=1M count=128 2>/dev/null || true
	@echo "[DISK] Disk image created"

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
	@echo "[CLEAN] Done"

# Install verification
verify:
	@echo "Verifying development environment..."
	@command -v $(CC) >/dev/null 2>&1 && echo "✓ $(CC) found" || echo "✗ $(CC) NOT FOUND"
	@command -v $(AS) >/dev/null 2>&1 && echo "✓ $(AS) found" || echo "✗ $(AS) NOT FOUND"
	@command -v $(QEMU) >/dev/null 2>&1 && echo "✓ $(QEMU) found" || echo "✗ $(QEMU) NOT FOUND"
	@test -f $(OVMF_CODE) && echo "✓ OVMF firmware found" || echo "✗ OVMF firmware NOT FOUND at $(OVMF_CODE)"

