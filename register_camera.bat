@echo off
:: Batch script to register ReelCam Virtual Camera with Administrator privileges
echo ===================================================
echo     ReelCam Virtual Camera Registration Tool
echo ===================================================
echo.

:: Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting Administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo Registering ReelCam Virtual Camera into Windows...
cd /d "%~dp0\bin\Release"
cam_register.exe --register

echo.
echo ===================================================
echo Registration complete! You can now use ReelCam in:
echo  - Google Chrome / Google Meet
echo  - Zoom, Microsoft Teams, Discord, OBS
echo ===================================================
pause
