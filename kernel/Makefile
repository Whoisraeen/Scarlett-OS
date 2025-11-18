# Kernel Makefile

TARGET = kernel.elf

# Use native tools for now (we're on x86_64)
# TODO: Set up proper cross-compiler later
CC = gcc
AS = as
LD = ld

# Compiler flags
CFLAGS = -std=c11 \
         -ffreestanding \
         -nostdlib \
         -fno-builtin \
         -fno-stack-protector \
         -mno-red-zone \
         -mno-mmx \
         -mno-sse \
         -mno-sse2 \
         -mcmodel=large \
         -Wall \
         -Wextra \
         -O2 \
         -g \
         -I./include

# Assembler flags (GAS)
ASFLAGS = --64

# Linker flags
LDFLAGS = -n \
          -nostdlib \
          -T kernel.ld

# Phase 1 sources
ASM_SRCS = hal/x86_64/boot32.S \
           hal/x86_64/gdt_load.S \
           hal/x86_64/idt_load.S \
           hal/x86_64/exceptions.S \
           hal/x86_64/user_mode.S \
           hal/x86_64/interrupts.S

C_SRCS = core/main.c \
         core/kprintf.c \
         core/exceptions.c \
         core/errors.c \
         core/error_recovery.c \
         core/string.c \
         core/math.c \
         core/stdio.c \
         hal/x86_64/serial.c \
         hal/x86_64/vga.c \
         hal/x86_64/gdt.c \
         hal/x86_64/idt.c \
         hal/x86_64/timer.c \
         hal/x86_64/interrupts.c \
         mm/pmm.c \
         mm/bootstrap.c \
         mm/vmm.c \
         mm/heap.c

# Phase 2 sources
ASM_SRCS += hal/x86_64/context_switch.S \
            hal/x86_64/syscall_entry.S

C_SRCS += sched/scheduler.c \
          sched/load_balance.c \
          sched/cpu_affinity.c \
          sched/work_stealing.c \
          ipc/ipc.c \
          syscall/syscall.c \
          syscall/registry.c \
          process/process.c \
          process/user_mode.c \
          process/fork_exec.c \
          loader/elf.c \
          shell/shell.c \
          fs/vfs.c \
          fs/block.c \
          fs/fat32.c \
          fs/fat32_file.c \
          fs/fat32_vfs.c \
          fs/fat32_create.c \
          fs/permissions.c \
          drivers/ata/ata.c \
          mm/mmap.c \
          core/test.c \
          core/userspace_launch.c \
          hal/x86_64/cpu.c \
          hal/x86_64/apic.c \
          sync/spinlock.c \
          sync/mutex.c \
          sync/semaphore.c \
          sync/rwlock.c

# Object files
ASM_OBJS = $(ASM_SRCS:.S=.o)
C_OBJS = $(C_SRCS:.c=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "[LD] Linking $@"
	@$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "[INFO] Kernel built successfully"

%.o: %.c
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@

%.o: %.asm
	@echo "[NASM] $<"
	@nasm -f elf64 $< -o $@

clean:
	@echo "[CLEAN] Removing object files and kernel binary"
	@rm -f $(OBJS) $(TARGET)

.PHONY: all clean

