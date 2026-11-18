// dllmain.cpp — COM DLL entry points for mfsource.dll
// This DLL is loaded by the Windows Camera Frame Server or consuming apps.
// It provides the ReelCam virtual camera IMFMediaSource.

#include <windows.h>
#include <new>

#include "ClassFactory.h"
#include "VirtualCameraCompat.h"
#include "Logging.h"

static HMODULE g_hModule = nullptr;
static long    g_cDllRef = 0;

void DllAddRef()  { InterlockedIncrement(&g_cDllRef); }
void DllRelease() { InterlockedDecrement(&g_cDllRef); }

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpReserved*/)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);
            LOG_INFO("DLL", "mfsource.dll loaded (PID=%lu)", GetCurrentProcessId());
            break;
        case DLL_PROCESS_DETACH:
            LOG_INFO("DLL", "mfsource.dll unloading (PID=%lu)", GetCurrentProcessId());
            break;
    }
    return TRUE;
}

// Called by COM runtime to create our media source
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (!IsEqualCLSID(rclsid, CLSID_ReelCamMediaSource)) {
        LOG_WARN("DLL", "DllGetClassObject: unknown CLSID requested");
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* pFactory = new (std::nothrow) ReelCamClassFactory();
    if (!pFactory) return E_OUTOFMEMORY;

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release(); // QI AddRef'd if successful
    return hr;
}

// Called by COM to check if DLL can be unloaded
STDAPI DllCanUnloadNow()
{
    return (g_cDllRef == 0) ? S_OK : S_FALSE;
}

// Self-registration: writes CLSID to HKLM (elevated) or HKCU (fallback).
// Frame Server (SYSTEM context) requires HKLM registration to find the COM class.
STDAPI DllRegisterServer()
{
    WCHAR dllPath[MAX_PATH];
    if (!GetModuleFileNameW(g_hModule, dllPath, MAX_PATH)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Try HKLM first (requires elevation). Fall back to HKCU.
    HKEY hRoot = HKEY_LOCAL_MACHINE;
    const char* rootLabel = "HKLM";
    WCHAR clsidPath[] = L"Software\\Classes\\CLSID\\" REELCAM_CLSID_STRING;

    HKEY hKeyCLSID = nullptr;
    LONG lRes = RegCreateKeyExW(hRoot, clsidPath, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyCLSID, nullptr);
    if (lRes == ERROR_ACCESS_DENIED) {
        hRoot = HKEY_CURRENT_USER;
        rootLabel = "HKCU";
        lRes = RegCreateKeyExW(hRoot, clsidPath, 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyCLSID, nullptr);
    }
    if (lRes != ERROR_SUCCESS) return HRESULT_FROM_WIN32(lRes);

    RegSetValueExW(hKeyCLSID, nullptr, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(L"ReelCam Virtual Camera Media Source"),
        static_cast<DWORD>((wcslen(L"ReelCam Virtual Camera Media Source") + 1) * sizeof(WCHAR)));

    HKEY hKeyInproc = nullptr;
    lRes = RegCreateKeyExW(hKeyCLSID, L"InprocServer32", 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyInproc, nullptr);
    if (lRes != ERROR_SUCCESS) { RegCloseKey(hKeyCLSID); return HRESULT_FROM_WIN32(lRes); }

    RegSetValueExW(hKeyInproc, nullptr, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(dllPath),
        static_cast<DWORD>((wcslen(dllPath) + 1) * sizeof(WCHAR)));

    WCHAR threadModel[] = L"Both";
    RegSetValueExW(hKeyInproc, L"ThreadingModel", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(threadModel),
        static_cast<DWORD>((wcslen(threadModel) + 1) * sizeof(WCHAR)));

    RegCloseKey(hKeyInproc);
    RegCloseKey(hKeyCLSID);

    LOG_INFO("DLL", "DllRegisterServer: registered CLSID under %s", rootLabel);
    return S_OK;
}

// Remove registration from both HKLM and HKCU
STDAPI DllUnregisterServer()
{
    WCHAR clsidPath[] = L"Software\\Classes\\CLSID\\" REELCAM_CLSID_STRING;
    WCHAR inprocPath[256];
    swprintf_s(inprocPath, L"%s\\InprocServer32", clsidPath);

    // Try HKLM first
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, inprocPath);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, clsidPath);

    // Also clean HKCU
    RegDeleteKeyW(HKEY_CURRENT_USER, inprocPath);
    RegDeleteKeyW(HKEY_CURRENT_USER, clsidPath);

    LOG_INFO("DLL", "DllUnregisterServer: removed CLSID from HKLM/HKCU");
    return S_OK;
}
