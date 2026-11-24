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
echo Unregistering stale DirectShow 'ReelCam' entry...
echo (Preserving 'ReelCam (Windows Virtual Camera)')
echo ========================================================

reg delete "HKLM\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f
reg delete "HKCU\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Classes\WOW6432Node\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1

echo.
echo DirectShow 'ReelCam' unregistered successfully!
echo Restart or refresh your browser / Google Meet to update the camera list.
timeout /t 5
