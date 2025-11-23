@echo off
echo Building tests...

gcc -I.. tests/test_framework.c tests/unit/test_scheduler.c -o tests/unit/test_scheduler.exe
if %errorlevel% neq 0 exit /b %errorlevel%

gcc -I.. tests/test_framework.c tests/unit/test_ipc.c -o tests/unit/test_ipc.exe
if %errorlevel% neq 0 exit /b %errorlevel%

gcc -I.. tests/test_framework.c tests/unit/test_syscall.c -o tests/unit/test_syscall.exe
if %errorlevel% neq 0 exit /b %errorlevel%

echo Running tests...
tests\unit\test_scheduler.exe
tests\unit\test_ipc.exe
tests\unit\test_syscall.exe

echo All tests completed.
