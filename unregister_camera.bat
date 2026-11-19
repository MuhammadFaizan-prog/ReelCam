@echo off
:: Batch script to unregister ReelCam Virtual Camera with Administrator privileges
echo ===================================================
echo     ReelCam Virtual Camera Unregistration Tool
echo ===================================================
echo.

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting Administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo Removing ReelCam Virtual Camera...
cd /d "%~dp0\bin\Release"
cam_register.exe --unregister

echo.
echo Unregistration complete.
pause
