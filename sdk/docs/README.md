# ScarlettOS SDK Documentation

## Overview

The ScarlettOS SDK provides everything you need to develop applications for ScarlettOS.

## Getting Started

### Installation

1. Download the SDK from the ScarlettOS website
2. Extract to `/usr/local/scarlettos-sdk`
3. Add SDK to your PATH:
   ```bash
   export PATH=$PATH:/usr/local/scarlettos-sdk/bin
   ```

### Your First Application

1. Create a new directory for your project
2. Copy the application template:
   ```bash
   cp /usr/local/scarlettos-sdk/templates/app.mk Makefile
   ```
3. Create `main.c`:
   ```c
   #include <scarlettos.h>
   #include <stdio.h>
   
   int main(int argc, char** argv) {
       printf("Hello, ScarlettOS!\n");
       return 0;
   }
   ```
4. Build your application:
   ```bash
   make
   ```
5. Run your application:
   ```bash
   ./build/myapp
   ```

## SDK Structure

```
scarlettos-sdk/
├── include/          # Header files
│   └── scarlettos/   # ScarlettOS API headers
├── lib/              # Libraries
├── templates/        # Project templates
├── samples/          # Sample applications
├── tools/            # Development tools
└── docs/             # Documentation
```

## Core APIs

### System Calls

```c
#include <scarlettos/syscall.h>

pid_t pid = sys_getpid();
tid_t tid = sys_gettid();
sys_exit(0);
```

### IPC (Inter-Process Communication)

```c
#include <scarlettos/ipc.h>

port_t port;
ipc_create_port(&port);

// Send message
ipc_send(port, data, size);

// Receive message
ipc_msg_t msg;
ipc_recv(port, &msg, 1000);

ipc_destroy_port(port);
```

### File I/O

```c
#include <scarlettos/file.h>

fd_t fd = file_open("/path/to/file", O_RDWR);
file_read(fd, buffer, size);
file_write(fd, data, size);
file_close(fd);
```

### GUI Development

```c
#include <scarlettos/gui.h>

window_t* window = window_create("My App", 800, 600);
window_show(window);
window_run(window);
window_destroy(window);
```

## Building Applications

### Using the Makefile Template

Edit the template variables:
- `APP_NAME`: Your application name
- `SOURCES`: List of source files
- `LIBS`: Additional libraries to link

### Manual Compilation

```bash
gcc -I/usr/local/scarlettos-sdk/include \
    -L/usr/local/scarlettos-sdk/lib \
    -lscarlettos \
    -o myapp main.c
```

## Package Management

### Creating a Package

1. Build your application
2. Create package manifest
3. Package with `scpkg`:
   ```bash
   scpkg create myapp
   ```

### Installing Packages

```bash
scpkg install myapp.scpkg
```

### Listing Packages

```bash
scpkg list
```

## Sample Applications

See `samples/` directory for example applications:
- `hello_world/` - Basic console application
- `gui_hello/` - Simple GUI application
- `ipc_demo/` - IPC communication example

## Support

- Documentation: https://docs.scarlettos.org
- Forums: https://forums.scarlettos.org
- GitHub: https://github.com/scarlettos

## License

ScarlettOS SDK is licensed under the MIT License.
