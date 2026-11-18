// cam_register: Registers or unregisters the ReelCam virtual camera device.
// Usage:
//   cam_register --register     Register mfsource.dll COM server and create virtual camera (persistent)
//   cam_register --unregister   Remove virtual camera and COM registration
//   cam_register --status       Check if virtual camera is registered
//   cam_register --run          Run virtual camera in session mode (interactive)
//
// This tool dynamically loads MFCreateVirtualCamera from mfsensorgroup.dll
// (available on Windows 11 Build 22000+).

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <stdio.h>
#include <string.h>

#include "VirtualCameraCompat.h"
#include "Logging.h"

typedef HRESULT(STDAPICALLTYPE* PFN_DllRegisterServer)();
typedef HRESULT(STDAPICALLTYPE* PFN_DllUnregisterServer)();

static void PrintUsage()
{
    wprintf(L"ReelCam Virtual Camera Registration Tool\n");
    wprintf(L"Usage:\n");
    wprintf(L"  cam_register --register     Register virtual camera (persistent for user)\n");
    wprintf(L"  cam_register --unregister   Remove virtual camera and COM registration\n");
    wprintf(L"  cam_register --status       Check registration status\n");
    wprintf(L"  cam_register --run          Run virtual camera in session mode (interactive)\n");
}

static bool GetMFSourcePath(WCHAR* outPath, DWORD maxChars)
{
    WCHAR exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return false;

    WCHAR* pSlash = wcsrchr(exePath, L'\\');
    if (!pSlash) return false;
    *(pSlash + 1) = L'\0';

    swprintf_s(outPath, maxChars, L"%smfsource.dll", exePath);
    return true;
}

static bool IsElevated()
{
    BOOL elevated = FALSE;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elev = {};
        DWORD size = sizeof(elev);
        if (GetTokenInformation(hToken, TokenElevation, &elev, sizeof(elev), &size)) {
            elevated = elev.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return elevated != FALSE;
}

static HRESULT RegisterCOM()
{
    WCHAR dllPath[MAX_PATH];
    if (!GetMFSourcePath(dllPath, MAX_PATH)) {
        wprintf(L"ERROR: Cannot determine mfsource.dll path\n");
        return E_FAIL;
    }

    bool elevated = IsElevated();
    wprintf(L"Registering COM: %s (%s)\n", dllPath, elevated ? L"ELEVATED -> HKLM" : L"non-elevated -> HKCU");

    if (!elevated) {
        wprintf(L"WARNING: Running without elevation. Frame Server (SYSTEM) may not find HKCU COM registration.\n");
        wprintf(L"         Run as Administrator for HKLM registration (required for virtual camera to appear).\n");
    }

    HMODULE hMod = LoadLibraryW(dllPath);
    if (!hMod) {
        DWORD err = GetLastError();
        wprintf(L"ERROR: Cannot load mfsource.dll (error %lu)\n", err);
        return HRESULT_FROM_WIN32(err);
    }

    auto pfnRegister = reinterpret_cast<PFN_DllRegisterServer>(
        GetProcAddress(hMod, "DllRegisterServer"));
    if (!pfnRegister) {
        wprintf(L"ERROR: DllRegisterServer not found in mfsource.dll\n");
        FreeLibrary(hMod);
        return E_FAIL;
    }

    HRESULT hr = pfnRegister();
    FreeLibrary(hMod);

    if (SUCCEEDED(hr)) {
        wprintf(L"COM registration successful (%s)\n", elevated ? L"HKLM" : L"HKCU");
    } else {
        wprintf(L"ERROR: DllRegisterServer failed: 0x%08lX\n", hr);
    }
    return hr;
}

static HRESULT UnregisterCOM()
{
    WCHAR dllPath[MAX_PATH];
    if (!GetMFSourcePath(dllPath, MAX_PATH)) return E_FAIL;

    HMODULE hMod = LoadLibraryW(dllPath);
    if (!hMod) return S_OK;

    auto pfnUnregister = reinterpret_cast<PFN_DllUnregisterServer>(
        GetProcAddress(hMod, "DllUnregisterServer"));
    if (pfnUnregister) pfnUnregister();
    FreeLibrary(hMod);

    wprintf(L"COM unregistration complete\n");
    return S_OK;
}

static HRESULT CreateVirtualCamera(bool interactive)
{
    HMODULE hSensorGroup = LoadLibraryW(L"mfsensorgroup.dll");
    if (!hSensorGroup) {
        wprintf(L"ERROR: Cannot load mfsensorgroup.dll - is this Windows 11?\n");
        return HRESULT_FROM_WIN32(GetLastError());
    }

    auto pfnCreate = reinterpret_cast<PFN_MFCreateVirtualCamera>(
        GetProcAddress(hSensorGroup, "MFCreateVirtualCamera"));
    if (!pfnCreate) {
        wprintf(L"ERROR: MFCreateVirtualCamera not found - Windows 11 Build 22000+ required\n");
        FreeLibrary(hSensorGroup);
        return E_NOTIMPL;
    }

    auto pfnIsSupported = reinterpret_cast<PFN_MFIsVirtualCameraTypeSupported>(
        GetProcAddress(hSensorGroup, "MFIsVirtualCameraTypeSupported"));
    if (pfnIsSupported) {
        BOOL isSupported = FALSE;
        HRESULT hr = pfnIsSupported(MFVirtualCameraType_SoftwareCameraSource, &isSupported);
        if (SUCCEEDED(hr) && !isSupported) {
            wprintf(L"WARNING: Software virtual cameras may not be supported on this system\n");
        }
    }

    MFVirtualCameraLifetime lifetime = interactive 
        ? MFVirtualCameraLifetime_Session 
        : MFVirtualCameraLifetime_User;

    wprintf(L"Creating virtual camera device 'ReelCam' (%s)...\n",
        interactive ? L"Session mode" : L"Persistent User mode");

    printf("Calling MFCreateVirtualCamera...\n");
    fflush(stdout);

    GUID categories[] = { KSCATEGORY_VIDEO_CAMERA_GUID, KSCATEGORY_CAPTURE_GUID };
    IMFVirtualCamera* pVirtualCamera = nullptr;
    HRESULT hr = pfnCreate(
        MFVirtualCameraType_SoftwareCameraSource,
        lifetime,
        MFVirtualCameraAccess_CurrentUser,
        REELCAM_DEVICE_FRIENDLY_NAME,
        REELCAM_CLSID_STRING,
        categories,
        2,
        &pVirtualCamera
    );

    printf("MFCreateVirtualCamera result: hr = 0x%08lX\n", (unsigned long)hr);
    fflush(stdout);

    if (FAILED(hr)) {
        wprintf(L"ERROR: MFCreateVirtualCamera failed: 0x%08lX\n", hr);
        if (hr == E_ACCESSDENIED) {
            wprintf(L"       Check Windows Camera Privacy settings in Windows Settings > Privacy & security > Camera.\n");
        }
        FreeLibrary(hSensorGroup);
        return hr;
    }

    printf("Starting virtual camera...\n");
    fflush(stdout);
    hr = pVirtualCamera->Start(nullptr);
    printf("pVirtualCamera->Start result: hr = 0x%08lX\n", (unsigned long)hr);
    fflush(stdout);
    if (FAILED(hr)) {
        wprintf(L"ERROR: IMFVirtualCamera::Start failed: 0x%08lX\n", hr);
        pVirtualCamera->Release();
        FreeLibrary(hSensorGroup);
        return hr;
    }

    LPWSTR pszDeviceId = nullptr;
    HRESULT hrId = pVirtualCamera->GetVirtualCameraDeviceInstanceId(&pszDeviceId);
    if (SUCCEEDED(hrId) && pszDeviceId) {
        printf("Device Instance ID: %ls\n", pszDeviceId);
        CoTaskMemFree(pszDeviceId);
    } else {
        printf("GetVirtualCameraDeviceInstanceId returned hr = 0x%08lX\n", (unsigned long)hrId);
    }
    fflush(stdout);

    printf("\n=== ReelCam Virtual Camera is ACTIVE ===\n");
    printf("Device 'ReelCam' is now available in Chrome, Edge, Zoom, Teams, Discord, and Windows Camera app.\n");
    fflush(stdout);

    if (interactive) {
        printf("Press ENTER to stop and remove the session camera...\n");
        fflush(stdout);
        getchar();
        printf("Stopping virtual camera...\n");
        fflush(stdout);
        pVirtualCamera->Stop();
        pVirtualCamera->Remove();
        pVirtualCamera->Release();
        printf("Session virtual camera removed.\n");
        fflush(stdout);
    } else {
        printf("Device registered persistently. Run 'cam_register --unregister' to remove.\n");
        fflush(stdout);
    }

    FreeLibrary(hSensorGroup);
    return S_OK;
}

static HRESULT RemoveVirtualCamera()
{
    HMODULE hSensorGroup = LoadLibraryW(L"mfsensorgroup.dll");
    if (hSensorGroup) {
        auto pfnCreate = reinterpret_cast<PFN_MFCreateVirtualCamera>(
            GetProcAddress(hSensorGroup, "MFCreateVirtualCamera"));
        if (pfnCreate) {
            IMFVirtualCamera* pVirtualCamera = nullptr;
            HRESULT hr = pfnCreate(
                MFVirtualCameraType_SoftwareCameraSource,
                MFVirtualCameraLifetime_User,
                MFVirtualCameraAccess_CurrentUser,
                REELCAM_DEVICE_FRIENDLY_NAME,
                REELCAM_CLSID_STRING,
                nullptr,
                0,
                &pVirtualCamera
            );
            if (SUCCEEDED(hr) && pVirtualCamera) {
                pVirtualCamera->Stop();
                pVirtualCamera->Remove();
                pVirtualCamera->Release();
                wprintf(L"Virtual camera device removed from Windows.\n");
            }
        }
        FreeLibrary(hSensorGroup);
    }

    UnregisterCOM();
    wprintf(L"Virtual camera cleanup complete.\n");
    return S_OK;
}

static void CheckStatus()
{
    HKEY hKey = nullptr;
    WCHAR clsidPath[] = L"Software\\Classes\\CLSID\\" REELCAM_CLSID_STRING L"\\InprocServer32";
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, clsidPath, 0, KEY_READ, &hKey);
    if (lRes == ERROR_SUCCESS) {
        WCHAR dllPath[MAX_PATH] = {};
        DWORD cbPath = sizeof(dllPath);
        RegQueryValueExW(hKey, nullptr, nullptr, nullptr,
            reinterpret_cast<BYTE*>(dllPath), &cbPath);
        RegCloseKey(hKey);
        wprintf(L"COM Registration: REGISTERED\n");
        wprintf(L"  DLL Path: %s\n", dllPath);
    } else {
        wprintf(L"COM Registration: NOT REGISTERED\n");
    }

    HMODULE hMod = LoadLibraryW(L"mfsensorgroup.dll");
    if (hMod) {
        auto pfn = GetProcAddress(hMod, "MFCreateVirtualCamera");
        wprintf(L"MFCreateVirtualCamera API: %s\n", pfn ? L"AVAILABLE" : L"NOT FOUND");
        FreeLibrary(hMod);
    } else {
        wprintf(L"MFCreateVirtualCamera API: NOT AVAILABLE (mfsensorgroup.dll missing)\n");
    }
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        wprintf(L"ERROR: CoInitializeEx failed: 0x%08lX\n", hr);
        return 1;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        wprintf(L"ERROR: MFStartup failed: 0x%08lX\n", hr);
        CoUninitialize();
        return 1;
    }

    int exitCode = 0;
    if (_wcsicmp(argv[1], L"--register") == 0) {
        hr = RegisterCOM();
        if (SUCCEEDED(hr)) {
            hr = CreateVirtualCamera(false); // non-interactive, persistent
        }
        exitCode = SUCCEEDED(hr) ? 0 : 1;
    }
    else if (_wcsicmp(argv[1], L"--run") == 0) {
        hr = RegisterCOM();
        if (SUCCEEDED(hr)) {
            hr = CreateVirtualCamera(true); // interactive session
        }
        exitCode = SUCCEEDED(hr) ? 0 : 1;
    }
    else if (_wcsicmp(argv[1], L"--unregister") == 0) {
        hr = RemoveVirtualCamera();
        exitCode = SUCCEEDED(hr) ? 0 : 1;
    }
    else if (_wcsicmp(argv[1], L"--status") == 0) {
        CheckStatus();
        exitCode = 0;
    }
    else {
        PrintUsage();
        exitCode = 1;
    }

    MFShutdown();
    CoUninitialize();
    return exitCode;
}
