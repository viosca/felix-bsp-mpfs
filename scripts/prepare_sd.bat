@echo off
setlocal
cd /d "%~dp0"

echo ==============================================================
echo  Felix OS: MicroSD Card Preparation for PolarFire SoC HSS Boot
echo ==============================================================
echo.

:: Test for administrative privileges by accessing a system directory
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Requesting Administrator privileges (UAC prompt)...
    powershell -Command "Start-Process cmd.exe -ArgumentList '/c \"\"%~f0\"\"' -Verb RunAs"
    exit /b
)

echo [INFO] Running with Administrator privileges.
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare_sd.ps1"

echo.
echo ==============================================================
echo Press any key to exit...
pause >nul
