# Kernel Makefile
# Multi-architecture build system

# Include architecture-specific configuration
include Makefile.arch

# Export architecture for sub-makes
export ARCH

# Build rules
all: $(TARGET)

# Rust libraries (architecture-specific)
RUST_LIBS = ../drivers/target/$(RUST_TARGET)/debug/libserial.a

$(TARGET): $(OBJS)
	@echo "[LD] Linking $@ (ARCH=$(ARCH))"
	@$(LD) $(LDFLAGS) $(OBJS) $(RUST_LIBS) -o $@ 2>/dev/null || $(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "[INFO] Kernel built successfully for $(ARCH)"

# Create symlink for default kernel.elf (backward compatibility)
kernel.elf: $(TARGET)
	@ln -sf $(TARGET) kernel.elf

%.o: %.c
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Special rule for math.c, stdio.c, graphics.c, and cursor.c - allow SSE for floating point (x86_64 only)
ifeq ($(ARCH),x86_64)
core/math.o: core/math.c
	@echo "[CC] $< (with SSE for FP)"
	@$(CC) $(filter-out -mno-sse -mno-sse2,$(CFLAGS)) -c $< -o $@

core/stdio.o: core/stdio.c
	@echo "[CC] $< (with SSE for FP)"
	@$(CC) $(filter-out -mno-sse -mno-sse2,$(CFLAGS)) -c $< -o $@

graphics/graphics.o: graphics/graphics.c
	@echo "[CC] $< (with SSE for FP)"
	@$(CC) $(filter-out -mno-sse -mno-sse2,$(CFLAGS)) -c $< -o $@

graphics/cursor.o: graphics/cursor.c
	@echo "[CC] $< (with SSE for FP)"
	@$(CC) $(filter-out -mno-sse -mno-sse2,$(CFLAGS)) -c $< -o $@
endif

%.o: %.S
	@echo "[AS] $< (ARCH=$(ARCH))"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	@echo "[NASM] $<"
	@nasm -f elf64 $< -o $@

clean:
	@echo "[CLEAN] Removing object files and kernel binaries"
	@rm -f $(OBJS) kernel-*.elf kernel.elf

# Clean for specific architecture
clean-$(ARCH):
	@echo "[CLEAN] Removing $(ARCH) build artifacts"
	@rm -f $(OBJS) kernel-$(ARCH).elf

# Build for all architectures
all-archs:
	@echo "[BUILD] Building for all architectures..."
	@$(MAKE) clean
	@$(MAKE) ARCH=x86_64 all
	@$(MAKE) ARCH=arm64 all
	@$(MAKE) ARCH=riscv64 all
	@echo "[INFO] Multi-arch build complete"

.PHONY: all clean clean-$(ARCH) all-archs kernel.elf

