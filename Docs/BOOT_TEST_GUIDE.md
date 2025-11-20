# Boot Test Guide

**Quick Start:** Test your OS boot in QEMU

---

## Quick Boot Test

### Windows (PowerShell/CMD)
```cmd
scripts\boot_test.bat
```

### Linux/WSL
```bash
./scripts/boot_test.sh
```

### Minimal Test
```bash
./scripts/quick_boot.sh
```

---

## Manual Boot Test

### 1. Build the Kernel
```bash
cd kernel
make clean
make
cd ..
```

### 2. Boot with QEMU
```bash
qemu-system-x86_64 \
    -kernel kernel/kernel.elf \
    -m 512M \
    -serial stdio \
    -no-reboot \
    -no-shutdown
```

---

## Expected Output

When the kernel boots successfully, you should see:

```
[INFO] Kernel starting...
[INFO] Initializing serial port...
[INFO] Initializing GDT...
[INFO] Initializing IDT...
[INFO] Initializing memory manager...
[INFO] Initializing scheduler...
[INFO] Kernel initialized successfully
```

---

## Troubleshooting

### QEMU Not Found
**Windows:**
- Download QEMU from https://www.qemu.org/download/
- Add QEMU to PATH

**Linux/WSL:**
```bash
sudo apt install qemu-system-x86
```

### Kernel Build Fails
```bash
# Check compiler
gcc --version

# Install build tools (Linux/WSL)
sudo apt install build-essential nasm

# Clean and rebuild
cd kernel
make clean
make
```

### Kernel Not Found
- Ensure kernel builds successfully
- Check `kernel/kernel.elf` exists
- Verify file permissions

### QEMU Exits Immediately
- Check kernel output for errors
- Verify multiboot2 header is present
- Check linker script configuration

---

## Advanced Options

### With Debugging
```bash
qemu-system-x86_64 \
    -kernel kernel/kernel.elf \
    -m 512M \
    -serial stdio \
    -s -S \
    -no-reboot \
    -no-shutdown
```

Then in another terminal:
```bash
gdb kernel/kernel.elf
(gdb) target remote :1234
(gdb) continue
```

### With VGA Output
```bash
qemu-system-x86_64 \
    -kernel kernel/kernel.elf \
    -m 512M \
    -serial stdio \
    -vga std \
    -no-reboot \
    -no-shutdown
```

### With Logging
```bash
qemu-system-x86_64 \
    -kernel kernel/kernel.elf \
    -m 512M \
    -serial stdio \
    -d guest_errors,unimp \
    -D qemu.log \
    -no-reboot \
    -no-shutdown
```

---

## Success Criteria

✅ **Boot Successful If:**
- QEMU starts and doesn't exit immediately
- Kernel output appears on serial console
- No triple fault or panic messages
- Kernel reaches initialization messages

❌ **Boot Failed If:**
- QEMU exits immediately
- Triple fault error
- Kernel panic
- No output on serial console

---

*Last Updated: 2025-01-27*

