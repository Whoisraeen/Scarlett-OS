# Scarlett OS

A production-grade, cross-platform microkernel operating system built from scratch.

## Project Overview

- **Architecture:** Microkernel
- **Target Platforms:** x86_64 (initial), ARM64, RISC-V (future)
- **Languages:** C, Assembly, Rust, C++
- **Security:** Hybrid capability-based and ACL model

## Current Status

**Phase 1: Bootloader & Minimal Kernel** (In Progress)

### Completed
- [x] Project structure setup
- [x] Build system configuration

### In Progress
- [ ] UEFI bootloader
- [ ] Minimal kernel with memory management

## Building

### Prerequisites

- Cross-compiler for x86_64-elf target
- NASM assembler
- QEMU for testing
- GNU Make

See `Docs/DEVELOPMENT_ENVIRONMENT_SETUP.md` for detailed setup instructions.

### Build Commands

```bash
# Build everything
make all

# Build bootloader only
make bootloader

# Build kernel only
make kernel

# Run in QEMU
make run

# Debug with GDB
make debug

# Clean build artifacts
make clean
```

## Testing

```bash
# Run in QEMU with serial output
make run

# Run with GDB attached
make debug
```

## Project Structure

```
scarlett-os/
├── bootloader/         # UEFI bootloader
│   ├── uefi/          # UEFI-specific code
│   └── common/        # Shared bootloader code
├── kernel/            # Kernel code
│   ├── core/          # Core kernel (C)
│   ├── hal/           # Hardware Abstraction Layer
│   │   └── x86_64/    # x86_64-specific code
│   ├── mm/            # Memory management
│   └── include/       # Kernel headers
├── drivers/           # Device drivers (Rust)
├── services/          # User-space services
├── build/             # Build output
├── tools/             # Build and debug tools
└── docs/              # Documentation
```

## Documentation

- [Development Plan](Docs/OS_DEVELOPMENT_PLAN.md)
- [Technical Architecture](Docs/TECHNICAL_ARCHITECTURE.md)
- [Phase 1 Tasks](Docs/PHASE_1_DETAILED_TASKS.md)
- [Environment Setup](Docs/DEVELOPMENT_ENVIRONMENT_SETUP.md)

## Contributing

See `Docs/TEAM_STRUCTURE_AND_WORKFLOW.md` for workflow and contribution guidelines.

## License

[To be determined]

## Contact

[Project information to be added]

---

*Scarlett OS - Built from scratch with precision and care*

