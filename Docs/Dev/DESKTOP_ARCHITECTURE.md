# Desktop Architecture

## Correct Architecture (Microkernel)

According to the microkernel design principles:

### User-Space (Ring 3) ✅
- **Desktop Shell** (`apps/desktop/`) - Main desktop environment
- **Compositor** (`gui/compositor/`) - Window compositing
- **Window Manager** (`gui/window_manager/`) - Window management logic
- **Taskbar** - User-space application
- **Application Launcher** - User-space application

### Kernel-Space (Ring 0) ✅
- **Window Management Primitives** (`kernel/window/`) - Basic window structures and syscalls
- **Graphics Syscalls** - Framebuffer access, rendering primitives
- **IPC** - Communication between desktop components
- **Input Event Delivery** - Mouse/keyboard events to user-space

## What Was Wrong

The `kernel/desktop/` code violates microkernel principles:
- Desktop shell should run in user-space for isolation
- Desktop is a restartable service, not kernel code
- Desktop should use syscalls, not direct kernel access

## Correct Implementation

The desktop shell in `apps/desktop/` is the correct implementation:
- Runs as a user-space process
- Uses window management syscalls
- Communicates with compositor via IPC
- Can be restarted without affecting kernel

## Migration

1. ✅ User-space desktop exists: `apps/desktop/desktop.c`
2. ❌ Kernel desktop code (`kernel/desktop/`) should be removed
3. ✅ Window primitives in kernel (`kernel/window/`) are correct
4. ✅ Desktop launched via `launch_shell_userspace()` in kernel init

## References

- `Docs/Dev/OS_DEVELOPMENT_PLAN.md` - Section 1.1: Pragmatic Microkernel Design Philosophy
- `apps/desktop/` - Correct user-space desktop implementation

