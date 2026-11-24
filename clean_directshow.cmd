@echo off
setlocal
:: Request Administrator elevation if not already running as admin
net session >nul 2>&1
if %errorLevel% NEQ 0 (
    echo Requesting Administrator privileges...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo ========================================================
echo Updating Virtual Camera to 'HP HD CAMERA 2'
echo ========================================================

cd /d "%~dp0"

echo 1. Removing old 'ReelCam' registrations...
bin\Release\cam_register.exe --unregister

echo 2. Registering 'HP HD CAMERA 2'...
bin\Release\cam_register.exe --register

echo 3. Cleaning stale DirectShow entries...
reg delete "HKLM\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1
reg delete "HKCU\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Classes\WOW6432Node\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1

echo.
echo ========================================================
echo Done! Camera is now registered as 'HP HD CAMERA 2'.
echo Please refresh Google Meet / restart your browser.
echo ========================================================
timeout /t 5
