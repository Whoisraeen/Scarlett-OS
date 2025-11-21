# ScarlettOS Developer Guide

## Building ScarlettOS from Source

### Prerequisites

**Required Tools:**
- GCC 11+ or Clang 13+
- GNU Make 4.0+
- Rust 1.70+ (for drivers)
- NASM 2.15+ (for assembly)
- Git
- Python 3.8+ (for build scripts)

**Optional Tools:**
- QEMU 6.0+ (for testing)
- GDB (for debugging)
- Valgrind (for memory analysis)

### Getting the Source

```bash
git clone https://github.com/scarlettos/scarlettos.git
cd scarlettos
```

### Building

#### Full Build

```bash
make all
```

This builds:
- Kernel (`kernel.elf`)
- Drivers (in `drivers/`)
- System services (in `services/`)
- GUI subsystem (in `gui/`)
- Applications (in `apps/`)

#### Component Builds

```bash
make kernel      # Build kernel only
make drivers     # Build drivers only
make services    # Build services only
make gui         # Build GUI subsystem
make apps        # Build applications
```

#### Cross-Platform Builds

**ARM64:**
```bash
make ARCH=arm64
```

**RISC-V:**
```bash
make ARCH=riscv64
```

### Testing

#### Run in QEMU

```bash
make qemu        # x86_64
make qemu-arm64  # ARM64
```

#### Run Tests

```bash
cd tests
make run
```

### Installation

#### Create Bootable USB

```bash
sudo dd if=scarlettos.iso of=/dev/sdX bs=4M status=progress
```

#### Install to Disk

```bash
sudo ./install.sh
```

---

## Creating Applications

### Hello World Application

1. **Create project directory:**
```bash
mkdir my_app
cd my_app
```

2. **Copy application template:**
```bash
cp $SDK_PATH/templates/app.mk Makefile
```

3. **Create source file (`main.c`):**
```c
#include <scarlettos.h>
#include <stdio.h>

int main(int argc, char** argv) {
    printf("Hello, ScarlettOS!\n");
    return 0;
}
```

4. **Build:**
```bash
make
```

5. **Run:**
```bash
./build/myapp
```

### GUI Application

```c
#include <scarlettos.h>
#include <scarlettos/gui.h>

int main(int argc, char** argv) {
    // Create window
    window_t* window = window_create("My App", 800, 600);
    
    // Add widgets
    button_t* button = button_create(window, "Click Me", 10, 10, 100, 30);
    
    // Show window
    window_show(window);
    
    // Event loop
    window_run(window);
    
    // Cleanup
    window_destroy(window);
    
    return 0;
}
```

### Using IPC

```c
#include <scarlettos/ipc.h>

// Create IPC port
port_t port;
ipc_create_port(&port);

// Send message
const char* msg = "Hello!";
ipc_send(port, msg, strlen(msg) + 1);

// Receive message
ipc_msg_t received;
ipc_recv(port, &received, 1000); // 1 second timeout

// Cleanup
ipc_destroy_port(port);
```

---

## Creating Drivers

### Driver Structure

```c
#include <scarlettos.h>

// Driver initialization
int driver_init(void) {
    // Initialize hardware
    // Register with device manager
    return 0;
}

// Driver main loop
int main(int argc, char** argv) {
    if (driver_init() != 0) {
        return 1;
    }
    
    // Handle requests via IPC
    while (1) {
        // Wait for requests
        // Process requests
        // Send responses
    }
    
    return 0;
}
```

### PCI Driver Example

```c
#include <scarlettos/pci.h>

#define VENDOR_ID 0x1234
#define DEVICE_ID 0x5678

int driver_init(void) {
    // Find PCI device
    pci_device_t* dev = pci_find_device(VENDOR_ID, DEVICE_ID);
    if (!dev) {
        return -1;
    }
    
    // Map MMIO regions
    void* mmio = pci_map_bar(dev, 0);
    
    // Register interrupt handler
    pci_register_irq(dev, irq_handler);
    
    return 0;
}
```

---

## Contributing

### Code Style

**C Code:**
- Follow Linux kernel style
- 4-space indentation (tabs)
- 80-character line limit
- Meaningful variable names

**Rust Code:**
- Use `rustfmt`
- Use `clippy` for linting
- Follow Rust conventions

### Commit Guidelines

```
component: Brief description (50 chars max)

Detailed explanation if needed. Wrap at 72 characters.

- Bullet points for multiple changes
- Reference issues: Fixes #123

Signed-off-by: Your Name <email@example.com>
```

### Pull Request Process

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `make test`
5. Submit pull request
6. Address review comments

### Testing Requirements

- All new code must have tests
- Tests must pass before merging
- Maintain >70% code coverage

---

## Debugging

### Using GDB

```bash
# Start QEMU with GDB server
make qemu-gdb

# In another terminal
gdb kernel.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

### Serial Console

```bash
# View kernel logs
screen /dev/ttyS0 115200
```

### Kernel Debugging

```c
// Add debug prints
kprintf("Debug: value=%d\n", value);

// Assert conditions
kassert(ptr != NULL);

// Dump stack trace
dump_stack();
```

---

## API Reference

### System Calls

- `sys_exit(int code)` - Exit process
- `sys_fork()` - Fork process
- `sys_read(fd, buf, count)` - Read from file
- `sys_write(fd, buf, count)` - Write to file
- `sys_getpid()` - Get process ID
- `sys_gettid()` - Get thread ID

### IPC

- `ipc_create_port(port_t* port)` - Create IPC port
- `ipc_destroy_port(port_t port)` - Destroy port
- `ipc_send(port, data, size)` - Send message
- `ipc_recv(port, msg, timeout)` - Receive message
- `ipc_reply(msg_id, data, size)` - Reply to message

### Memory

- `mmap(addr, size, prot, flags)` - Map memory
- `munmap(addr, size)` - Unmap memory
- `mprotect(addr, size, prot)` - Change protection

### File System

- `open(path, flags)` - Open file
- `close(fd)` - Close file
- `read(fd, buf, count)` - Read file
- `write(fd, buf, count)` - Write file
- `stat(path, statbuf)` - Get file info

---

## Resources

- **Documentation:** https://docs.scarlettos.org
- **API Reference:** https://api.scarlettos.org
- **Forums:** https://forums.scarlettos.org
- **GitHub:** https://github.com/scarlettos
- **Discord:** https://discord.gg/scarlettos

## License

ScarlettOS is licensed under the MIT License. See LICENSE for details.
