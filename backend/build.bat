@echo off
REM Smart Deadlock Detection and Prevention System - Windows Build Script
REM Requires gcc (MinGW) installed and in PATH

echo Compiling Smart Deadlock System...
gcc -Wall -Wextra -O2 -o deadlock_system.exe main.c process.c resource.c deadlock.c

if %errorlevel% equ 0 (
    echo Build successful! Output: deadlock_system.exe
) else (
    echo Build failed! Please check error messages above.
    exit /b 1
)
