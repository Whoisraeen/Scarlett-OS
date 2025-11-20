# Scarlett OS - Full Microkernel Architecture

## Directory Structure

```
├── bootloader/          # UEFI/BIOS bootloader (C + Assembly)
├── kernel/              # Minimal microkernel (C + Assembly)
│   ├── core/           # Kernel initialization
│   ├── mm/             # Memory management
│   ├── sched/          # Scheduler
│   ├── ipc/            # IPC primitives
│   ├── syscall/        # Syscall handlers
│   ├── hal/            # Hardware abstraction
│   └── security/       # Capabilities
├── services/            # User-space services (Rust)
│   ├── device_manager/ # Device enumeration and management
│   ├── init/           # System initialization service
│   └── vfs/            # Virtual file system service
├── drivers/             # User-space drivers (Rust)
│   ├── input/          # Keyboard, mouse drivers
│   ├── storage/        # AHCI, ATA, NVMe drivers
│   ├── network/        # Ethernet drivers
│   └── graphics/       # GPU drivers
├── gui/                 # GUI subsystem (C++)
│   ├── compositor/     # Window compositor
│   ├── window_manager/ # Window management
│   └── toolkit/        # UI toolkit
├── apps/                # Applications (C++)
│   ├── desktop/        # Desktop shell
│   ├── taskbar/        # Taskbar application
│   └── terminal/       # Terminal emulator
├── libs/                # Shared libraries
│   ├── libc/           # Custom C library
│   ├── librust_std/    # Rust standard library
│   └── libgui/         # C++ GUI library
├── tools/               # Development tools
├── Docs/                # Documentation
└── tests/               # Test suites
```

## Architecture

**Microkernel**: Minimal kernel (< 100KB) with only essential services  
**Services**: Rust user-space processes communicating via IPC  
**Drivers**: Rust user-space processes with hardware access  
**GUI**: C++ user-space processes for desktop environment  
**Security**: Capability-based access control  

## Build System

- **Kernel**: Make (C + Assembly)
- **Services**: Cargo (Rust)
- **Drivers**: Cargo (Rust)
- **GUI**: CMake (C++)
- **Apps**: CMake (C++)

## Status

**Phase**: 1 - Infrastructure Setup  
**Compliance**: Targeting 90%+ with OS_DEVELOPMENT_PLAN.md  
**Timeline**: 6-12 months for full migration
