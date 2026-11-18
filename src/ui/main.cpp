#include <windows.h>
#include <objbase.h>
#include "MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow)
{
    // Single instance check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\ReelCam_UI_Mutex");
    if (hMutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"ReelCamMainWindowClass", nullptr);
        if (hExisting) {
            ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
            CloseHandle(hMutex);
            return 0;
        }
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    ReelCam::MainWindow window;
    if (!window.Create(hInstance, nCmdShow)) {
        CoUninitialize();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    int result = window.RunMessageLoop();

    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);
    return result;
}
