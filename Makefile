# Scarlett OS Makefile
# Main build system for the operating system
# Multi-architecture support: x86_64, arm64, riscv64

.PHONY: all clean bootloader kernel libs services drivers gui run debug setup help all-archs

# Architecture selection (default: x86_64)
ARCH ?= x86_64

# Architecture-specific configuration
ifeq ($(ARCH),x86_64)
    TARGET_TRIPLE = x86_64-elf
    QEMU_ARCH = qemu-system-x86_64
    RUST_TARGET = x86_64-unknown-none
    OVMF_CODE = /usr/share/OVMF/OVMF_CODE.fd
    BOOTLOADER_EFI = $(BOOT_DIR)/limine/BOOTX64.EFI
else ifeq ($(ARCH),arm64)
    TARGET_TRIPLE = aarch64-elf
    QEMU_ARCH = qemu-system-aarch64
    RUST_TARGET = aarch64-unknown-none
    OVMF_CODE = /usr/share/OVMF/OVMF_CODE_AA64.fd
    BOOTLOADER_EFI = $(BOOT_DIR)/limine/BOOTAA64.EFI
else ifeq ($(ARCH),riscv64)
    TARGET_TRIPLE = riscv64-elf
    QEMU_ARCH = qemu-system-riscv64
    RUST_TARGET = riscv64gc-unknown-none-elf
    OVMF_CODE = 
    BOOTLOADER_EFI = 
else
    $(error Unsupported architecture: $(ARCH). Supported: x86_64, arm64, riscv64)
endif

# Configuration
CC = $(TARGET_TRIPLE)-gcc
LD = $(TARGET_TRIPLE)-ld
AS = nasm
OBJCOPY = $(TARGET_TRIPLE)-objcopy

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
QEMU = $(QEMU_ARCH)
QEMU_FLAGS = -m 512M -serial stdio -no-reboot -no-shutdown
DISK_IMG = $(BUILD_DIR)/disk-$(ARCH).img

# Export architecture for sub-makes
export ARCH

# Default target
all: setup bootloader kernel libs services drivers gui disk

help:
	@echo "Scarlett OS Build System (Multi-Architecture)"
	@echo ""
	@echo "Architecture: $(ARCH) (set via ARCH= variable)"
	@echo "Supported architectures: x86_64, arm64, riscv64"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build everything for current architecture"
	@echo "  all-archs   - Build for all architectures"
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
	@echo "  verify      - Verify development environment"
	@echo "  help        - Show this help"
	@echo ""
	@echo "Examples:"
	@echo "  make ARCH=x86_64 all    - Build for x86_64"
	@echo "  make ARCH=arm64 all     - Build for ARM64"
	@echo "  make ARCH=riscv64 all   - Build for RISC-V 64"
	@echo "  make all-archs          - Build for all architectures"

setup:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/boot/EFI/BOOT
	@echo "[SETUP] Build directories created"

# Bootloader build (Limine)
bootloader: setup
	@echo "[BUILD] Preparing Limine bootloader for $(ARCH)..."
	@# We use the pre-built Limine binaries from bootloader/limine
	@# Just ensure they exist (x86_64 only for now)
	@if [ "$(ARCH)" = "x86_64" ]; then \
		if [ ! -f "$(BOOT_DIR)/limine/BOOTX64.EFI" ]; then \
			echo "[ERROR] Limine binaries not found in $(BOOT_DIR)/limine"; \
			exit 1; \
		fi; \
		echo "[INFO] Limine bootloader ready for $(ARCH)"; \
	else \
		echo "[WARN] Bootloader not yet implemented for $(ARCH)"; \
	fi

# Kernel build
kernel: setup
	@echo "[BUILD] Building kernel for $(ARCH)..."
	@$(MAKE) -C $(KERNEL_DIR) ARCH=$(ARCH)
	@cp $(KERNEL_DIR)/kernel-$(ARCH).elf $(BUILD_DIR)/kernel-$(ARCH).elf
	@ln -sf kernel-$(ARCH).elf $(BUILD_DIR)/kernel.elf
	@echo "[INFO] Kernel built and copied for $(ARCH)"

# Libraries build
libs: setup
	@echo "[BUILD] Building libraries..."
	@$(MAKE) -C libs/libc || echo "[WARN] libc build failed (may need cross-compiler)"
	@$(MAKE) -C libs/libcpp || echo "[WARN] libcpp build failed (may need cross-compiler)"
	@echo "[INFO] Libraries build complete"

# Services build (Rust)
services: setup
	@echo "[BUILD] Building user-space services (Rust) for $(ARCH)..."
	@if command -v cargo >/dev/null 2>&1; then \
		cd services && cargo build --target $(RUST_TARGET) || echo "[WARN] Services build failed"; \
	else \
		echo "[WARN] Cargo not found - skipping services build"; \
	fi
	@echo "[INFO] Services build complete for $(ARCH)"

# Drivers build (Rust)
drivers: setup
	@echo "[BUILD] Building drivers (Rust) for $(ARCH)..."
	@if command -v cargo >/dev/null 2>&1; then \
		cd drivers && cargo build --target $(RUST_TARGET) || echo "[WARN] Drivers build failed"; \
	else \
		echo "[WARN] Cargo not found - skipping drivers build"; \
	fi
	@echo "[INFO] Drivers build complete for $(ARCH)"

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
	@echo "[DISK] Creating GPT disk image with Limine for $(ARCH)..."
	@if [ "$(ARCH)" = "x86_64" ]; then \
		dd if=/dev/zero of=$(DISK_IMG) bs=1M count=128 2>/dev/null || true; \
		sgdisk -n 1:2048:0 -t 1:ef00 -c 1:'EFI' $(DISK_IMG) >/dev/null 2>&1 || true; \
		dd if=/dev/zero of=$(BUILD_DIR)/part.img bs=512 count=260063 2>/dev/null || true; \
		mkfs.fat -F 32 -n 'EFI' $(BUILD_DIR)/part.img >/dev/null 2>&1 || true; \
		MTOOLS_SKIP_CHECK=1 mmd -i $(BUILD_DIR)/part.img ::/EFI 2>/dev/null || true; \
		MTOOLS_SKIP_CHECK=1 mmd -i $(BUILD_DIR)/part.img ::/EFI/BOOT 2>/dev/null || true; \
		MTOOLS_SKIP_CHECK=1 mcopy -i $(BUILD_DIR)/part.img $(BOOT_DIR)/limine/BOOTX64.EFI ::/EFI/BOOT/ 2>/dev/null || true; \
		MTOOLS_SKIP_CHECK=1 mcopy -i $(BUILD_DIR)/part.img limine.cfg ::/ 2>/dev/null || true; \
		MTOOLS_SKIP_CHECK=1 mcopy -i $(BUILD_DIR)/part.img $(BUILD_DIR)/kernel-$(ARCH).elf ::/ 2>/dev/null || true; \
		dd if=$(BUILD_DIR)/part.img of=$(DISK_IMG) bs=512 seek=2048 conv=notrunc 2>/dev/null || true; \
		rm -f $(BUILD_DIR)/part.img; \
		echo "[DISK] GPT disk image created and populated for $(ARCH)"; \
	else \
		echo "[WARN] Disk image creation not yet implemented for $(ARCH)"; \
	fi

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
	@echo "[QEMU] Starting Scarlett OS for $(ARCH)..."
	@if [ "$(ARCH)" = "x86_64" ]; then \
		if [ -f "$(OVMF_CODE)" ]; then \
			$(QEMU) -bios $(OVMF_CODE) -drive file=$(DISK_IMG),format=raw $(QEMU_FLAGS); \
		else \
			echo "[ERROR] OVMF firmware not found at $(OVMF_CODE)"; \
			echo "[INFO] Please install OVMF or update OVMF_CODE path in Makefile"; \
		fi; \
	elif [ "$(ARCH)" = "arm64" ]; then \
		$(QEMU) -M virt -cpu cortex-a72 -smp 4 -m 512M \
			-kernel $(BUILD_DIR)/kernel-$(ARCH).elf \
			-serial stdio -no-reboot -no-shutdown; \
	elif [ "$(ARCH)" = "riscv64" ]; then \
		$(QEMU) -M virt -cpu rv64 -smp 4 -m 512M \
			-kernel $(BUILD_DIR)/kernel-$(ARCH).elf \
			-serial stdio -no-reboot -no-shutdown; \
	fi

# Debug with GDB
debug: all
	@echo "[DEBUG] Starting QEMU with GDB support for $(ARCH)..."
	@echo "[INFO] Connect with: gdb -ex 'target remote localhost:1234'"
	@if [ "$(ARCH)" = "x86_64" ]; then \
		if [ -f "$(OVMF_CODE)" ]; then \
			$(QEMU) -bios $(OVMF_CODE) -drive file=$(DISK_IMG),format=raw $(QEMU_FLAGS) -s -S; \
		else \
			echo "[ERROR] OVMF firmware not found at $(OVMF_CODE)"; \
		fi; \
	elif [ "$(ARCH)" = "arm64" ]; then \
		$(QEMU) -M virt -cpu cortex-a72 -smp 4 -m 512M \
			-kernel $(BUILD_DIR)/kernel-$(ARCH).elf \
			-serial stdio -no-reboot -no-shutdown -s -S; \
	elif [ "$(ARCH)" = "riscv64" ]; then \
		$(QEMU) -M virt -cpu rv64 -smp 4 -m 512M \
			-kernel $(BUILD_DIR)/kernel-$(ARCH).elf \
			-serial stdio -no-reboot -no-shutdown -s -S; \
	fi

# Clean build artifacts
clean:
	@echo "[CLEAN] Removing build artifacts for $(ARCH)..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -C $(BOOT_DIR)/uefi clean 2>/dev/null || true
	@$(MAKE) -C $(KERNEL_DIR) clean
	@$(MAKE) -C libs/libc clean 2>/dev/null || true
	@$(MAKE) -C libs/libcpp clean 2>/dev/null || true
	@if command -v cargo >/dev/null 2>&1; then \
		cd services && cargo clean 2>/dev/null || true; \
		cd ../drivers && cargo clean 2>/dev/null || true; \
	fi
	@rm -rf $(BUILD_DIR)/gui
	@echo "[CLEAN] Done"

# Clean all architectures
clean-all:
	@echo "[CLEAN] Removing all build artifacts..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -C $(BOOT_DIR)/uefi clean 2>/dev/null || true
	@$(MAKE) -C $(KERNEL_DIR) clean
	@for arch in x86_64 arm64 riscv64; do \
		$(MAKE) -C $(KERNEL_DIR) ARCH=$$arch clean; \
	done
	@$(MAKE) -C libs/libc clean 2>/dev/null || true
	@$(MAKE) -C libs/libcpp clean 2>/dev/null || true
	@if command -v cargo >/dev/null 2>&1; then \
		cd services && cargo clean 2>/dev/null || true; \
		cd ../drivers && cargo clean 2>/dev/null || true; \
	fi
	@rm -rf $(BUILD_DIR)/gui
	@echo "[CLEAN] Done"

# Build for all architectures
all-archs:
	@echo "[BUILD] Building for all architectures..."
	@for arch in x86_64 arm64 riscv64; do \
		echo "[BUILD] Building for $$arch..."; \
		$(MAKE) ARCH=$$arch kernel || echo "[WARN] Build failed for $$arch"; \
	done
	@echo "[INFO] Multi-arch build complete"

# Install verification
verify:
	@echo "Verifying development environment for $(ARCH)..."
	@echo "Architecture: $(ARCH)"
	@echo ""
	@command -v $(CC) >/dev/null 2>&1 && echo "✓ $(CC) found" || echo "✗ $(CC) NOT FOUND"
	@command -v $(LD) >/dev/null 2>&1 && echo "✓ $(LD) found" || echo "✗ $(LD) NOT FOUND"
	@command -v $(AS) >/dev/null 2>&1 && echo "✓ $(AS) found" || echo "✗ $(AS) NOT FOUND"
	@command -v $(QEMU) >/dev/null 2>&1 && echo "✓ $(QEMU) found" || echo "✗ $(QEMU) NOT FOUND"
	@if [ "$(ARCH)" = "x86_64" ]; then \
		test -f $(OVMF_CODE) && echo "✓ OVMF firmware found" || echo "✗ OVMF firmware NOT FOUND at $(OVMF_CODE)"; \
	fi
	@command -v cargo >/dev/null 2>&1 && echo "✓ cargo found" || echo "✗ cargo NOT FOUND (needed for Rust services/drivers)"
	@command -v cmake >/dev/null 2>&1 && echo "✓ cmake found" || echo "✗ cmake NOT FOUND (needed for C++ GUI)"
	@if command -v rustup >/dev/null 2>&1; then \
		rustup target list | grep -q "$(RUST_TARGET)" && echo "✓ Rust $(RUST_TARGET) target installed" || echo "✗ Rust target NOT INSTALLED (run: rustup target add $(RUST_TARGET))"; \
	else \
		echo "✗ rustup NOT FOUND"; \
	fi
	@echo ""
	@echo "To verify all architectures, run:"
	@echo "  make ARCH=x86_64 verify"
	@echo "  make ARCH=arm64 verify"
	@echo "  make ARCH=riscv64 verify"

