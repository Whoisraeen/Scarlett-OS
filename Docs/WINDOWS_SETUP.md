# Scarlett OS - Windows Development Setup

## Overview

Since you're on Windows 10, you'll need to use WSL2 (Windows Subsystem for Linux) to build and run Scarlett OS. This guide will walk you through the setup.

---

## Step 1: Install WSL2

### Option A: Quick Install (Windows 11 / Windows 10 version 2004+)

Open PowerShell as Administrator and run:

```powershell
wsl --install
```

This installs Ubuntu by default. Restart your computer when prompted.

### Option B: Manual Install

If the quick install doesn't work:

1. Open PowerShell as Administrator:

```powershell
# Enable WSL
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart

# Enable Virtual Machine Platform
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart
```

2. Restart your computer

3. Download and install the WSL2 Linux kernel update:
   - Go to: https://aka.ms/wsl2kernel
   - Download and run the installer

4. Set WSL2 as default:

```powershell
wsl --set-default-version 2
```

5. Install Ubuntu from Microsoft Store:
   - Open Microsoft Store
   - Search for "Ubuntu 22.04 LTS"
   - Click Install

---

## Step 2: Set Up Ubuntu in WSL2

### First Launch

1. Launch Ubuntu from Start Menu
2. Create a username and password
3. Wait for installation to complete

### Update Ubuntu

```bash
sudo apt update
sudo apt upgrade -y
```

---

## Step 3: Install Development Tools

### Install Build Dependencies

```bash
# Essential build tools
sudo apt install -y build-essential git curl wget vim

# Cross-compiler dependencies
sudo apt install -y libgmp-dev libmpfr-dev libmpc-dev \
                    libisl-dev texinfo bison flex

# NASM assembler
sudo apt install -y nasm

# QEMU emulator
sudo apt install -y qemu-system-x86

# GDB debugger
sudo apt install -y gdb
```

### Build Cross-Compiler

```bash
# Set up environment
export PREFIX="$HOME/opt/cross"
export TARGET=x86_64-elf
export PATH="$PREFIX/bin:$PATH"

# Add to .bashrc for persistence
echo 'export PREFIX="$HOME/opt/cross"' >> ~/.bashrc
echo 'export TARGET=x86_64-elf' >> ~/.bashrc
echo 'export PATH="$PREFIX/bin:$PATH"' >> ~/.bashrc

# Create directory
mkdir -p ~/osdev/toolchain
cd ~/osdev/toolchain

# Download binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz
tar xf binutils-2.41.tar.xz

# Build binutils
mkdir build-binutils
cd build-binutils
../binutils-2.41/configure --target=$TARGET --prefix="$PREFIX" \
    --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install
cd ..

# Download GCC
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz
tar xf gcc-13.2.0.tar.xz

# Build GCC
mkdir build-gcc
cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix="$PREFIX" \
    --disable-nls --enable-languages=c,c++ --without-headers
make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc

# Verify installation
x86_64-elf-gcc --version
```

**Note:** Building the cross-compiler takes 30-60 minutes.

---

## Step 4: Access Your Project in WSL2

### Navigate to Windows Files

Your Windows C: drive is mounted at `/mnt/c/` in WSL2:

```bash
# Navigate to your project
cd /mnt/c/Users/woisr/Downloads/OS

# List files
ls -la
```

### Important: File Permissions

Files on `/mnt/c/` may have permission issues. Two options:

#### Option 1: Work in WSL Home (Recommended)

```bash
# Copy project to WSL home
cp -r /mnt/c/Users/woisr/Downloads/OS ~/scarlett-os
cd ~/scarlett-os

# Now you have proper permissions
ls -la
```

#### Option 2: Fix Permissions on Mounted Drive

Edit `/etc/wsl.conf`:

```bash
sudo nano /etc/wsl.conf
```

Add:

```ini
[automount]
options = "metadata"
```

Save (Ctrl+O, Enter, Ctrl+X), then restart WSL:

```powershell
# In PowerShell
wsl --shutdown
# Then relaunch Ubuntu
```

---

## Step 5: Build Scarlett OS

### From WSL2 Terminal

```bash
# Navigate to project
cd ~/scarlett-os  # or /mnt/c/Users/woisr/Downloads/OS

# Build kernel
cd kernel
make clean
make

# Should output:
# [AS] hal/x86_64/entry.S
# [CC] core/main.c
# ... (more files)
# [LD] Linking kernel.elf
# [INFO] Kernel built successfully
```

---

## Step 6: Run in QEMU

### Test the Kernel

```bash
# Make scripts executable (if in WSL home)
chmod +x tools/*.sh

# Run QEMU
./tools/qemu.sh

# Or manually:
qemu-system-x86_64 \
    -kernel kernel/kernel.elf \
    -m 512M \
    -serial stdio \
    -no-reboot \
    -no-shutdown
```

### Expected Output

You should see the Scarlett OS boot banner:

```
====================================================
                  Scarlett OS                       
        A Modern Microkernel Operating System      
====================================================
Version: 0.1.0 (Phase 1 - Development)
...
[INFO] Kernel initialization complete!
[INFO] System is now idle.
```

---

## Step 7: Debug with GDB

### Terminal 1: Start QEMU

```bash
./tools/debug.sh

# Or manually:
qemu-system-x86_64 \
    -kernel kernel/kernel.elf \
    -m 512M \
    -serial stdio \
    -s -S
```

### Terminal 2: Launch GDB

```bash
# In a new WSL terminal
cd ~/scarlett-os
gdb kernel/kernel.elf

# In GDB:
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

---

## Tips for WSL2 Development

### 1. VS Code Integration

Install VS Code on Windows with Remote-WSL extension:

1. Install VS Code: https://code.visualstudio.com/
2. Open VS Code
3. Install "Remote - WSL" extension
4. Click green icon in bottom-left
5. Select "New WSL Window"
6. Open your project folder

Now you can edit files in VS Code on Windows but compile in WSL!

### 2. File Editing

**Option A:** VS Code with Remote-WSL (recommended)
**Option B:** Vim/Nano in WSL terminal
**Option C:** Windows editor + save to WSL home

### 3. Performance

Working in WSL home (`~/scarlett-os`) is **much faster** than working on mounted drive (`/mnt/c/`).

### 4. Access WSL Files from Windows

Your WSL home directory is accessible from Windows:

```
\\wsl$\Ubuntu\home\yourusername\scarlett-os
```

You can browse this in File Explorer!

### 5. Terminal

Use Windows Terminal for better experience:
- Install from Microsoft Store: "Windows Terminal"
- Has tabs, better colors, copy/paste support

---

## Troubleshooting

### "command not found: x86_64-elf-gcc"

Solution:
```bash
# Add to PATH
export PATH="$HOME/opt/cross/bin:$PATH"

# Or reload .bashrc
source ~/.bashrc

# Verify
which x86_64-elf-gcc
```

### "Permission denied" when running scripts

Solution:
```bash
# Make scripts executable
chmod +x tools/*.sh

# Or run with bash
bash tools/qemu.sh
```

### QEMU window doesn't open

This is normal! QEMU output goes to terminal with `-serial stdio`.

### "Cannot access files"

If files are on `/mnt/c/`, copy to WSL home:
```bash
cp -r /mnt/c/Users/woisr/Downloads/OS ~/scarlett-os
cd ~/scarlett-os
```

### Build is very slow

Solution: Work in WSL home, not on `/mnt/c/`

---

## Quick Reference

### Common Commands

```bash
# Navigate to project
cd ~/scarlett-os

# Build
cd kernel && make

# Run
./tools/qemu.sh

# Debug
./tools/debug.sh  # Terminal 1
gdb kernel/kernel.elf -ex 'target remote :1234'  # Terminal 2

# Clean
make clean
```

### File Locations

```
Windows:     C:\Users\woisr\Downloads\OS
WSL Mount:   /mnt/c/Users/woisr/Downloads/OS
WSL Home:    ~/scarlett-os (recommended)
```

### Key Directories

```
~/scarlett-os/
├── kernel/              # Build here
│   └── kernel.elf      # Output binary
├── tools/              # Scripts
└── Docs/               # Documentation
```

---

## Next Steps

1. ✅ WSL2 installed
2. ✅ Tools installed
3. ✅ Cross-compiler built
4. ✅ Project accessible
5. ✅ Kernel builds
6. ✅ QEMU runs
7. ✅ Ready to develop!

Now follow the main development docs:
- `README.md` - Project overview
- `BUILD_INSTRUCTIONS.md` - Build guide
- `PHASE1_STATUS.md` - What's implemented
- `Docs/OS_DEVELOPMENT_PLAN.md` - Full roadmap

---

## Additional Resources

- WSL2 Documentation: https://docs.microsoft.com/en-us/windows/wsl/
- VS Code Remote: https://code.visualstudio.com/docs/remote/wsl
- Windows Terminal: https://aka.ms/terminal
- QEMU Documentation: https://www.qemu.org/docs/master/

---

**You're all set! Happy OS development on Windows + WSL2!** 🚀

