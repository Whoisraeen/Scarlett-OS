# How to Test Scarlett OS

## Quick Test

```bash
cd /mnt/c/Users/woisr/Downloads/OS
./tools/test_vga.sh
```

**What you should see:**
1. GRUB bootloader menu
2. "Scarlett OS - Phase 1" option
3. Screen clears
4. Look for debug markers: **SCP64**

Each letter shows progress:
- **S** = Started (in 32-bit mode)
- **C** = CPUID check passed
- **P** = Setting up page tables
- **64** = Successfully in 64-bit mode!

## Expected Behavior

### If you see "S":
✅ GRUB loaded the kernel  
✅ Boot code is executing  
✅ We're in 32-bit mode  

### If you see "SC":
✅ CPU supports long mode  
✅ Passed CPUID checks  

### If you see "SCP":
✅ Page table setup started  

### If you see "SCP64":
🎉 **HUGE SUCCESS!**  
✅ Successfully transitioned to 64-bit!  
✅ Ready for kernel_main  

## Troubleshooting

### See nothing / Black screen:
- GRUB might not be loading kernel
- Check ISO creation: `ls -lh scarlett.iso`
- Should be ~5MB

### See GRUB but crashes:
- Boot code has issues
- Check debug.log for errors

### See some letters but freezes:
- **PROGRESS!** Boot is partially working
- Check which letter you see:
  - Just "S" = issue in CPUID check
  - "SC" = issue in page table setup
  - "SCP" = issue in mode transition
  - "SCP6" = issue in 64-bit setup

## Full Debug Test

```bash
cd /mnt/c/Users/woisr/Downloads/OS

# Run with full debugging
qemu-system-x86_64 \
  -cdrom scarlett.iso \
  -m 512M \
  -serial file:serial.log \
  -d int,guest_errors \
  -D debug.log \
  -no-reboot &

# Let it run for 5 seconds
sleep 5
pkill -9 qemu

# Check logs
echo "=== Serial Output ===" 
cat serial.log

echo ""
echo "=== Debug Log (last 50 lines) ==="
tail -50 debug.log
```

## What Success Looks Like

### VGA Screen:
```
SCP64

========================================
  Welcome to Scarlett OS!
========================================
Kernel version: 0.1.0
...
```

### Serial Output:
```
[INFO] Scarlett OS - Phase 1
[INFO] Kernel version: 0.1.0
[INFO] Initializing GDT...
[INFO] Initializing IDT...
...
```

## Current Status

As of last build:
- ✅ Kernel compiles
- ✅ GRUB loads kernel  
- ✅ Boot code executes
- ⏳ Debugging memory issues
- ⏳ Not yet reaching kernel_main

**Expected to see:** "SCP" or "SCP64" on screen  
**Not expected yet:** Full boot messages

## Files Created

- `scarlett.iso` - Bootable ISO (5.0 MB)
- `kernel/kernel.elf` - Kernel binary (101 KB)
- `tools/test_vga.sh` - Quick VGA test
- `tools/create_iso.sh` - Rebuild ISO
- `tools/run_qemu.sh` - Run in QEMU

## Next Steps After Testing

1. Report which letters you see
2. Check serial.log for any output
3. We'll debug based on results
4. Fix remaining issues
5. Complete Phase 1!

---

**Good luck! You're testing REAL OS code!** 🚀

