# ScarlettOS User Guide

## Welcome to ScarlettOS

ScarlettOS is a modern, microkernel-based operating system designed for desktop and workstation use.

## Installation

### System Requirements

**Minimum:**
- CPU: x86_64 or ARM64 processor
- RAM: 2 GB
- Storage: 10 GB
- Graphics: VGA-compatible

**Recommended:**
- CPU: Multi-core x86_64 or ARM64
- RAM: 4 GB or more
- Storage: 20 GB or more
- Graphics: GPU with hardware acceleration

### Installation Steps

1. **Download ScarlettOS**
   - Get the latest ISO from https://scarlettos.org/download

2. **Create Bootable Media**
   ```bash
   dd if=scarlettos.iso of=/dev/sdX bs=4M status=progress
   ```

3. **Boot from Media**
   - Insert USB/DVD and reboot
   - Select boot device in BIOS/UEFI

4. **Run Installer**
   ```bash
   sudo ./install.sh
   ```

5. **Reboot**
   - Remove installation media
   - Boot into ScarlettOS

## Getting Started

### First Boot

On first boot, you'll see the desktop environment with:
- **Desktop** - Main workspace
- **Taskbar** - Application launcher and system tray
- **File Manager** - Browse files
- **Terminal** - Command line access

### Basic Usage

#### Opening Applications

Click the application launcher in the taskbar or press `Super` key.

#### File Management

- Open File Manager from the launcher
- Navigate using sidebar bookmarks
- Right-click for context menu
- Drag and drop to move files

#### Terminal Commands

Open Terminal and try:
```bash
# List files
ls -la

# Change directory
cd /home/user

# View system information
uname -a

# Check running processes
ps aux
```

### Desktop Features

#### Virtual Desktops

- Switch: `Ctrl+Alt+Left/Right`
- Move window: `Ctrl+Alt+Shift+Left/Right`

#### Window Snapping

- Drag window to screen edge
- Or use: `Super+Left/Right/Up`

#### Hot Corners

- Top-left: Show all windows
- Top-right: Show desktop
- Bottom-left: Application launcher
- Bottom-right: Lock screen

## Applications

### Included Applications

1. **File Manager** - Browse and manage files
2. **Terminal** - Command line interface
3. **Text Editor** - Edit text and code
4. **Settings** - Configure system
5. **Application Launcher** - Quick app access

### Installing Applications

Use the package manager:
```bash
scpkg search <app-name>
scpkg install <package>
scpkg list
scpkg uninstall <package>
```

## Configuration

### System Settings

Open Settings app to configure:
- Display resolution and scaling
- Network connections
- Sound devices and volume
- User accounts
- Power management
- Appearance and themes

### Configuration Files

System config: `/etc/scarlettos.conf`
User config: `~/.config/scarlettos/`

## Troubleshooting

### Boot Issues

If system won't boot:
1. Check BIOS/UEFI boot order
2. Verify installation completed
3. Try recovery mode

### Display Issues

If display is garbled:
1. Boot with `nomodeset` kernel parameter
2. Update graphics drivers
3. Check display settings

### Network Issues

If network doesn't work:
1. Check cable/WiFi connection
2. Verify network settings
3. Restart network service

## Support

- Documentation: https://docs.scarlettos.org
- Forums: https://forums.scarlettos.org
- Bug Reports: https://github.com/scarlettos/issues
- IRC: #scarlettos on Libera.Chat

## License

ScarlettOS is open source software licensed under the MIT License.
