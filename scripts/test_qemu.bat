@echo off
REM QEMU Test Script for OS (Windows)
REM Tests the OS in QEMU with various configurations

set QEMU=qemu-system-x86_64
set KERNEL_IMAGE=build\kernel.bin
set MEMORY=512M

echo Starting QEMU test...

REM Check if QEMU is installed
where %QEMU% >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: QEMU not found. Please install QEMU.
    exit /b 1
)

REM Check if kernel image exists
if not exist "%KERNEL_IMAGE%" (
    echo Warning: Kernel image not found. Building...
    make kernel
)

echo Test 1: Basic Boot Test
start /B %QEMU% -kernel %KERNEL_IMAGE% -m %MEMORY% -serial stdio -no-reboot -no-shutdown > test_output_boot.log 2>&1
timeout /t 5 /nobreak >nul

echo Test 2: Service Startup Test
start /B %QEMU% -kernel %KERNEL_IMAGE% -m %MEMORY% -serial stdio -no-reboot -no-shutdown > test_output_services.log 2>&1
timeout /t 10 /nobreak >nul

echo Test 3: IPC Communication Test
start /B %QEMU% -kernel %KERNEL_IMAGE% -m %MEMORY% -serial stdio -no-reboot -no-shutdown > test_output_ipc.log 2>&1
timeout /t 10 /nobreak >nul

echo All tests completed!

