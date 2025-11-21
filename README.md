# ScarlettOS

**A Modern Microkernel Operating System**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()
[![Platform](https://img.shields.io/badge/platform-x86__64%20%7C%20ARM64-lightgrey)]()

## Overview

ScarlettOS is a production-grade, cross-platform microkernel operating system designed for desktop and workstation use. Built from scratch with modern security, performance, and usability in mind.

## Features

- **Microkernel Architecture** - Drivers and services in user-space for stability
- **Cross-Platform** - Runs on x86_64 and ARM64
- **Modern GUI** - Beautiful desktop environment with compositor
- **Secure** - Capability-based security + ACL model
- **Fast** - O(1) scheduler, optimized memory management
- **Developer-Friendly** - Complete SDK with samples and documentation

## Quick Start

### Download

Get the latest release from [Releases](https://github.com/scarlettos/releases)

### Install

```bash
# Write to USB drive
sudo dd if=scarlettos.iso of=/dev/sdX bs=4M status=progress

# Or run installer
sudo ./install.sh
```

### Build from Source

```bash
git clone https://github.com/scarlettos/scarlettos.git
cd scarlettos
make all
```

## System Requirements

**Minimum:**
- CPU: x86_64 or ARM64
- RAM: 2 GB
- Storage: 10 GB

**Recommended:**
- CPU: Multi-core processor
- RAM: 4 GB+
- Storage: 20 GB+

## Documentation

- [User Guide](docs/USER_GUIDE.md) - Installation and usage
- [Developer Guide](docs/DEVELOPER_GUIDE.md) - Building applications
- [Architecture](Docs/Dev/OS_DEVELOPMENT_PLAN.md) - System design

## Development

### Building

```bash
make kernel      # Build kernel
make drivers     # Build drivers
make apps        # Build applications
make all         # Build everything
```

### Testing

```bash
cd tests
make run         # Run test suite
```

### Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Project Status

**Current Version:** 0.1.0 (Foundation Release)

**Completed:**
- ✅ Bootloader & Kernel
- ✅ Memory Management
- ✅ Scheduler
- ✅ IPC System
- ✅ File System (VFS + SFS)
- ✅ Network Stack
- ✅ GUI & Desktop
- ✅ Audio Subsystem
- ✅ Developer SDK
- ✅ Test Suite

**In Progress:**
- 🔄 ARM64 Platform
- 🔄 Additional Drivers
- 🔄 Performance Optimization

## Architecture

```
┌─────────────────────────────────────┐
│     Applications (Ring 3)           │
├─────────────────────────────────────┤
│     System Services (Ring 3)        │
│  - File System  - Network           │
│  - GUI Server   - Audio Server      │
├─────────────────────────────────────┤
│     Drivers (Ring 3, Rust)          │
│  - USB  - NVMe  - Graphics          │
├─────────────────────────────────────┤
│     Microkernel (Ring 0, C)         │
│  - Scheduler  - Memory  - IPC       │
└─────────────────────────────────────┘
```

## License

ScarlettOS is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Community

- **Forums:** https://forums.scarlettos.org
- **Discord:** https://discord.gg/scarlettos
- **IRC:** #scarlettos on Libera.Chat
- **Twitter:** @ScarlettOS

## Acknowledgments

Built with passion by the ScarlettOS team and contributors.

Special thanks to:
- The open source community
- All our contributors
- Early testers and supporters

---

**Made with ❤️ for the open source community**
