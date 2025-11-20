# Boot Test Instructions

## Quick Start

### Option 1: Windows Batch Script (Easiest)
```cmd
scripts\boot_test.bat
```

### Option 2: Linux/WSL
```bash
./scripts/boot_test.sh
```

### Option 3: Minimal Test
```bash
./scripts/quick_boot.sh
```

---

## Manual Boot Test

### Step 1: Build the Kernel
```bash
cd kernel
make clean
make
cd ..
```

### Step 2: Boot with QEMU

**Windows (PowerShell):**
```powershell
qemu-system-x86_64 -kernel kernel\kernel.elf -m 512M -serial stdio -no-reboot -no-shutdown
```

**Linux/WSL:**
```bash
qemu-system-x86_64 -kernel kernel/kernel.elf -m 512M -serial stdio -no-reboot -no-shutdown
```

---

## What to Expect

### Successful Boot Output:
```
[INFO] Kernel starting...
[INFO] Initializing serial port...
[INFO] Initializing GDT...
[INFO] Initializing IDT...
[INFO] Initializing memory manager...
[INFO] Kernel initialized successfully
```

### If Boot Fails:
- QEMU exits immediately → Check kernel build
- No output → Check serial console settings
- Triple fault → Check kernel code

---

## Troubleshooting

### QEMU Not Found
**Windows:** Download from https://www.qemu.org/download/
**Linux/WSL:** `sudo apt install qemu-system-x86`

### Kernel Build Fails
- Check compiler: `gcc --version`
- Install build tools: `sudo apt install build-essential nasm`
- Clean rebuild: `cd kernel && make clean && make`

---

*Ready to test!*

