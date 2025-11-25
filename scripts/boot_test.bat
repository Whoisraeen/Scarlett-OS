@echo off
REM Simple Boot Test Script for Windows
REM Tests kernel boot in QEMU

setlocal

echo === OS Boot Test ===
echo.

REM Configuration
set QEMU=qemu-system-x86_64
set KERNEL_PATH=build\kernel.elf
set KERNEL_DIR=kernel
set MEMORY=512M

REM Check QEMU
where %QEMU% >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: QEMU not found. Please install QEMU.
    echo Install from: https://www.qemu.org/download/
    exit /b 1
)

REM Build kernel if needed
if not exist "%KERNEL_PATH%" (
    if not exist "%KERNEL_DIR%\kernel-x86_64.elf" (
        echo Kernel not found. Building...
        cd %KERNEL_DIR%
        make clean
        make
        cd ..
        if exist "%KERNEL_DIR%\kernel.elf" (
            copy "%KERNEL_DIR%\kernel.elf" "%KERNEL_PATH%" >nul 2>&1
        )
    )
)

REM Check if kernel exists
if not exist "%KERNEL_PATH%" (
    if exist "%KERNEL_DIR%\kernel-x86_64.elf" (
        set KERNEL_PATH=%KERNEL_DIR%\kernel-x86_64.elf
    ) else if exist "%KERNEL_DIR%\kernel.elf" (
        set KERNEL_PATH=%KERNEL_DIR%\kernel.elf
    ) else (
        echo Error: Kernel not found.
        echo Please build the kernel first: cd kernel ^&^& make
        exit /b 1
    )
)

echo Kernel found: %KERNEL_PATH%
echo Starting QEMU...
echo.
echo Press Ctrl+C to exit QEMU
echo.

REM Boot with QEMU using multiboot2
%QEMU% -kernel %KERNEL_PATH% -m %MEMORY% -serial stdio -display none -no-reboot -no-shutdown -d guest_errors -monitor stdio

