@echo off
setlocal
:: Check for admin elevation; if not elevated, request it via UAC prompt
net session >nul 2>&1
if %errorLevel% NEQ 0 (
    echo Requesting Administrator privileges...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo ========================================================
echo Unregistering both 'ReelCam' cameras...
echo (Keeping 'HP HD CAMERA 2', 'HP HD Camera', 'OBS Virtual Camera')
echo ========================================================

cd /d "%~dp0"

echo 1. Removing old ReelCam virtual camera devices via pnputil...
pnputil /remove-device "SWD\VCAMDEVAPI\7DADE2775059AABB99387D6187C9D68AE45858C588AFA5099CB5E9D0628B63A1"

echo 2. Removing any other stale ReelCam VCAM instances...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-PnpDevice | Where-Object { $_.InstanceId -like '*VCAMDEVAPI*' -and $_.InstanceId -notlike '*AD78AA0E5B5E3AD6D6D8B53EF34082AB3B3A170115958A6021C526BC9C7A805D*' } | ForEach-Object { pnputil /remove-device $_.InstanceId }"

echo 3. Removing ReelCam DirectShow entries...
reg delete "HKLM\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1
reg delete "HKCU\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Classes\WOW6432Node\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1

echo.
echo ========================================================
echo Done! Both ReelCam entries have been unregistered.
echo 'HP HD CAMERA 2' is active and ready.
echo Refresh Google Meet to see the cleaned camera list.
echo ========================================================
pause
