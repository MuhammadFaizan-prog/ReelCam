#include <windows.h>
#include <objbase.h>
#include "MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;

    ReelCam::MainWindow window;
    if (!window.Create(hInstance, nCmdShow)) {
        CoUninitialize();
        return 1;
    }

    int result = window.RunMessageLoop();

    CoUninitialize();
    return result;
}
