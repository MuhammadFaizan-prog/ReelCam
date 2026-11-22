@echo off
:: Removes stale DirectShow CLSID for ReelCam
reg delete "HKLM\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1
reg delete "HKCU\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}" /f >nul 2>&1
echo DirectShow stale registration cleaned.
