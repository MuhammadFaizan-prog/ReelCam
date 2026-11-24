// cam_register: Registers or unregisters the ReelCam virtual camera device.
// Usage:
//   cam_register --register     Register mfsource.dll COM server and create virtual camera (persistent)
//   cam_register --unregister   Remove virtual camera and COM registration
//   cam_register --status       Check if virtual camera is registered
//   cam_register --run          Run virtual camera in session mode (interactive)

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <shellapi.h>
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
    wprintf(L"  cam_register --register     Register virtual camera (persistent machine-wide)\n");
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

static bool SelfElevate(LPCWSTR parameter)
{
    WCHAR exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return false;

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.lpParameters = parameter;
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    wprintf(L"Requesting administrator privileges to register machine-wide virtual camera...\n");
    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            CloseHandle(sei.hProcess);
        }
        return true;
    }
    return false;
}

static void UnregisterDirectShowCategory()
{
    WCHAR catPath[] = L"Software\\Classes\\CLSID\\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\\Instance\\" REELCAM_CLSID_STRING;
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, catPath);
    RegDeleteTreeW(HKEY_CURRENT_USER, catPath);
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
        UnregisterDirectShowCategory();
    } else {
        wprintf(L"ERROR: DllRegisterServer failed: 0x%08lX\n", hr);
    }
    return hr;
}

static HRESULT UnregisterCOM()
{
    WCHAR dllPath[MAX_PATH];
    if (!GetMFSourcePath(dllPath, MAX_PATH)) return E_FAIL;

    UnregisterDirectShowCategory();

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

    bool elevated = IsElevated();
    MFVirtualCameraLifetime lifetime = interactive 
        ? MFVirtualCameraLifetime_Session 
        : MFVirtualCameraLifetime_User;

    MFVirtualCameraAccess access = elevated
        ? MFVirtualCameraAccess_AllUsers
        : MFVirtualCameraAccess_CurrentUser;

    wprintf(L"Creating virtual camera device '%ls' (%s, %s)...\n",
        REELCAM_DEVICE_FRIENDLY_NAME,
        interactive ? L"Session mode" : L"Persistent mode",
        elevated ? L"AllUsers (HKLM)" : L"CurrentUser (HKCU)");

    IMFVirtualCamera* pVirtualCamera = nullptr;
    HRESULT hr = pfnCreate(
        MFVirtualCameraType_SoftwareCameraSource,
        lifetime,
        access,
        REELCAM_DEVICE_FRIENDLY_NAME,
        REELCAM_CLSID_STRING,
        nullptr, // Default categories automatically assigned by Windows 11
        0,
        &pVirtualCamera
    );

    if (FAILED(hr) && access == MFVirtualCameraAccess_AllUsers) {
        access = MFVirtualCameraAccess_CurrentUser;
        hr = pfnCreate(
            MFVirtualCameraType_SoftwareCameraSource,
            lifetime,
            access,
            REELCAM_DEVICE_FRIENDLY_NAME,
            REELCAM_CLSID_STRING,
            nullptr,
            0,
            &pVirtualCamera
        );
    }

    if (FAILED(hr)) {
        wprintf(L"ERROR: MFCreateVirtualCamera failed: 0x%08lX\n", hr);
        if (hr == E_ACCESSDENIED) {
            wprintf(L"       Check Windows Camera Privacy settings in Settings > Privacy & security > Camera.\n");
        }
        FreeLibrary(hSensorGroup);
        return hr;
    }

    hr = pVirtualCamera->Start(nullptr);
    if (FAILED(hr) && access == MFVirtualCameraAccess_AllUsers) {
        wprintf(L"AllUsers Start returned 0x%08lX, falling back to CurrentUser...\n", hr);
        pVirtualCamera->Release();
        pVirtualCamera = nullptr;
        access = MFVirtualCameraAccess_CurrentUser;
        hr = pfnCreate(
            MFVirtualCameraType_SoftwareCameraSource,
            lifetime,
            access,
            REELCAM_DEVICE_FRIENDLY_NAME,
            REELCAM_CLSID_STRING,
            nullptr,
            0,
            &pVirtualCamera
        );
        if (SUCCEEDED(hr)) {
            hr = pVirtualCamera->Start(nullptr);
        }
    }

    if (FAILED(hr)) {
        wprintf(L"ERROR: IMFVirtualCamera::Start failed: 0x%08lX\n", hr);
        if (pVirtualCamera) pVirtualCamera->Release();
        FreeLibrary(hSensorGroup);
        return hr;
    }

    wprintf(L"\n=== ReelCam Virtual Camera is ACTIVE ===\n");
    wprintf(L"Device 'ReelCam' is now registered and recognized by Windows.\n");

    if (interactive) {
        wprintf(L"Press ENTER to stop and remove the session camera...\n");
        getchar();
        pVirtualCamera->Stop();
        pVirtualCamera->Remove();
        pVirtualCamera->Release();
        wprintf(L"Session virtual camera removed.\n");
    } else {
        wprintf(L"Device registered persistently. Run 'cam_register --unregister' to remove.\n");
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
            LPCWSTR namesToRemove[] = { L"ReelCam", REELCAM_DEVICE_FRIENDLY_NAME };
            MFVirtualCameraAccess accesses[] = { MFVirtualCameraAccess_CurrentUser, MFVirtualCameraAccess_AllUsers };

            for (LPCWSTR camName : namesToRemove) {
                for (auto access : accesses) {
                    IMFVirtualCamera* pVirtualCamera = nullptr;
                    HRESULT hr = pfnCreate(
                        MFVirtualCameraType_SoftwareCameraSource,
                        MFVirtualCameraLifetime_User,
                        access,
                        camName,
                        REELCAM_CLSID_STRING,
                        nullptr,
                        0,
                        &pVirtualCamera
                    );
                    if (SUCCEEDED(hr) && pVirtualCamera) {
                        pVirtualCamera->Stop();
                        pVirtualCamera->Remove();
                        pVirtualCamera->Release();
                        wprintf(L"Virtual camera device '%ls' removed from Windows.\n", camName);
                    }
                }
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
    LONG lRes = RegOpenKeyExW(HKEY_LOCAL_MACHINE, clsidPath, 0, KEY_READ, &hKey);
    bool inHklm = (lRes == ERROR_SUCCESS);
    if (!inHklm) {
        lRes = RegOpenKeyExW(HKEY_CURRENT_USER, clsidPath, 0, KEY_READ, &hKey);
    }

    if (lRes == ERROR_SUCCESS) {
        WCHAR dllPath[MAX_PATH] = {};
        DWORD cbPath = sizeof(dllPath);
        RegQueryValueExW(hKey, nullptr, nullptr, nullptr,
            reinterpret_cast<BYTE*>(dllPath), &cbPath);
        RegCloseKey(hKey);
        wprintf(L"COM Registration: REGISTERED (%s)\n", inHklm ? L"HKLM - Machine Wide" : L"HKCU - User Only");
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

    bool forceElevate = (argc > 2 && _wcsicmp(argv[2], L"--elevate") == 0);
    if (forceElevate && !IsElevated()) {
        if (SelfElevate(argv[1])) {
            return 0;
        }
    }
    wprintf(L"Proceeding with %s registration...\n", IsElevated() ? L"machine-wide" : L"CurrentUser");

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
    if (_wcsicmp(argv[1], L"--register") == 0 || _wcsicmp(argv[1], L"--register-user") == 0) {
        RegisterCOM();
        hr = CreateVirtualCamera(false); // persistent
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
