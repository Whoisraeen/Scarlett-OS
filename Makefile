# Scarlett OS Makefile
# Main build system for the operating system

.PHONY: all clean bootloader kernel libs services drivers gui run debug setup help

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
all: setup bootloader kernel libs services drivers gui disk

help:
	@echo "Scarlett OS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build everything"
	@echo "  bootloader  - Build UEFI bootloader"
	@echo "  kernel      - Build kernel"
	@echo "  libs        - Build libraries (libc, libcpp)"
	@echo "  services    - Build user-space services (Rust)"
	@echo "  drivers     - Build drivers (Rust)"
	@echo "  gui         - Build GUI subsystem (C++)"
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

# Bootloader build (Limine)
bootloader: setup
	@echo "[BUILD] Preparing Limine bootloader..."
	@# We use the pre-built Limine binaries from bootloader/limine
	@# Just ensure they exist
	@if [ ! -f "$(BOOT_DIR)/limine/BOOTX64.EFI" ]; then \
		echo "[ERROR] Limine binaries not found in $(BOOT_DIR)/limine"; \
		exit 1; \
	fi
	@echo "[INFO] Limine bootloader ready"

# Kernel build
kernel: setup
	@echo "[BUILD] Building kernel..."
	@$(MAKE) -C $(KERNEL_DIR)
	@cp $(KERNEL_DIR)/kernel.elf $(BUILD_DIR)/
	@echo "[INFO] Kernel built and copied"

# Libraries build
libs: setup
	@echo "[BUILD] Building libraries..."
	@$(MAKE) -C libs/libc || echo "[WARN] libc build failed (may need cross-compiler)"
	@$(MAKE) -C libs/libcpp || echo "[WARN] libcpp build failed (may need cross-compiler)"
	@echo "[INFO] Libraries build complete"

# Services build (Rust)
services: setup
	@echo "[BUILD] Building user-space services (Rust)..."
	@if command -v cargo >/dev/null 2>&1; then \
		cd services && cargo build --target x86_64-unknown-none || echo "[WARN] Services build failed"; \
	else \
		echo "[WARN] Cargo not found - skipping services build"; \
	fi
	@echo "[INFO] Services build complete"

# Drivers build (Rust)
drivers: setup
	@echo "[BUILD] Building drivers (Rust)..."
	@if command -v cargo >/dev/null 2>&1; then \
		cd drivers && cargo build --target x86_64-unknown-none || echo "[WARN] Drivers build failed"; \
	else \
		echo "[WARN] Cargo not found - skipping drivers build"; \
	fi
	@echo "[INFO] Drivers build complete"

# GUI build (C++)
gui: setup
	@echo "[BUILD] Building GUI subsystem (C++)..."
	@if command -v cmake >/dev/null 2>&1; then \
		mkdir -p $(BUILD_DIR)/gui && \
		cd $(BUILD_DIR)/gui && \
		cmake ../../gui -DCMAKE_CXX_COMPILER=$(TARGET_X86)-g++ || echo "[WARN] GUI cmake failed"; \
		$(MAKE) -C $(BUILD_DIR)/gui || echo "[WARN] GUI build failed"; \
	else \
		echo "[WARN] CMake not found - skipping GUI build"; \
	fi
	@echo "[INFO] GUI build complete"

# Create disk image (Limine)
disk: setup bootloader kernel
	@echo "[DISK] Creating GPT disk image with Limine..."
	@dd if=/dev/zero of=$(DISK_IMG) bs=1M count=128 2>/dev/null || true
	@sgdisk -n 1:2048:0 -t 1:ef00 -c 1:'EFI' $(DISK_IMG) >/dev/null 2>&1 || true
	@dd if=/dev/zero of=$(BUILD_DIR)/part.img bs=512 count=260063 2>/dev/null || true
	@mkfs.fat -F 32 -n 'EFI' $(BUILD_DIR)/part.img >/dev/null 2>&1 || true
	@MTOOLS_SKIP_CHECK=1 mmd -i $(BUILD_DIR)/part.img ::/EFI 2>/dev/null || true
	@MTOOLS_SKIP_CHECK=1 mmd -i $(BUILD_DIR)/part.img ::/EFI/BOOT 2>/dev/null || true
	@MTOOLS_SKIP_CHECK=1 mcopy -i $(BUILD_DIR)/part.img $(BOOT_DIR)/limine/BOOTX64.EFI ::/EFI/BOOT/ 2>/dev/null || true
	@MTOOLS_SKIP_CHECK=1 mcopy -i $(BUILD_DIR)/part.img limine.cfg ::/ 2>/dev/null || true
	@MTOOLS_SKIP_CHECK=1 mcopy -i $(BUILD_DIR)/part.img $(BUILD_DIR)/kernel.elf ::/ 2>/dev/null || true
	@dd if=$(BUILD_DIR)/part.img of=$(DISK_IMG) bs=512 seek=2048 conv=notrunc 2>/dev/null || true
	@rm -f $(BUILD_DIR)/part.img
	@echo "[DISK] GPT disk image created and populated"

# Create ISO image (Limine)
iso: setup bootloader kernel
	@echo "[ISO] Creating UEFI bootable ISO with Limine..."
	@mkdir -p $(BUILD_DIR)/iso/EFI/BOOT
	@cp $(BOOT_DIR)/limine/BOOTX64.EFI $(BUILD_DIR)/iso/EFI/BOOT/
	@cp limine.cfg $(BUILD_DIR)/iso/
	@cp $(BOOT_DIR)/limine/limine.sys $(BUILD_DIR)/iso/ 2>/dev/null || true
	@cp $(BOOT_DIR)/limine/limine-cd.bin $(BUILD_DIR)/iso/ 2>/dev/null || true
	@cp $(BOOT_DIR)/limine/limine-eltorito-efi.bin $(BUILD_DIR)/iso/ 2>/dev/null || true
	@cp $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/iso/
	@xorriso -as mkisofs -R -f -e EFI/BOOT/BOOTX64.EFI -no-emul-boot \
		-o $(BUILD_DIR)/boot.iso $(BUILD_DIR)/iso >/dev/null 2>&1 || \
		echo "[WARN] xorriso not found, skipping ISO creation"
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
	@$(MAKE) -C libs/libc clean 2>/dev/null || true
	@$(MAKE) -C libs/libcpp clean 2>/dev/null || true
	@if command -v cargo >/dev/null 2>&1; then \
		cd services && cargo clean 2>/dev/null || true; \
		cd ../drivers && cargo clean 2>/dev/null || true; \
	fi
	@rm -rf $(BUILD_DIR)/gui
	@echo "[CLEAN] Done"

# Install verification
verify:
	@echo "Verifying development environment..."
	@command -v $(CC) >/dev/null 2>&1 && echo "✓ $(CC) found" || echo "✗ $(CC) NOT FOUND"
	@command -v $(AS) >/dev/null 2>&1 && echo "✓ $(AS) found" || echo "✗ $(AS) NOT FOUND"
	@command -v $(QEMU) >/dev/null 2>&1 && echo "✓ $(QEMU) found" || echo "✗ $(QEMU) NOT FOUND"
	@test -f $(OVMF_CODE) && echo "✓ OVMF firmware found" || echo "✗ OVMF firmware NOT FOUND at $(OVMF_CODE)"
	@command -v cargo >/dev/null 2>&1 && echo "✓ cargo found" || echo "✗ cargo NOT FOUND (needed for Rust services/drivers)"
	@command -v cmake >/dev/null 2>&1 && echo "✓ cmake found" || echo "✗ cmake NOT FOUND (needed for C++ GUI)"
	@rustup target list | grep -q "x86_64-unknown-none" && echo "✓ Rust x86_64-unknown-none target installed" || echo "✗ Rust target NOT INSTALLED (run: rustup target add x86_64-unknown-none)"

