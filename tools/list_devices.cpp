#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <stdio.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "ole32.lib")

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    IMFAttributes* pAttrs = nullptr;
    MFCreateAttributes(&pAttrs, 1);
    pAttrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    MFEnumDeviceSources(pAttrs, &ppDevices, &count);
    pAttrs->Release();
    printf("Total Devices: %u\n", count);
    for (UINT32 i = 0; i < count; i++) {
        WCHAR name[256] = {};
        UINT32 len = 0;
        ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, name, 256, &len);
        WCHAR sym[1024] = {};
        ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, sym, 1024, &len);
        wprintf(L"[%u] Name: '%s'\n    SymLink: '%s'\n", i, name, sym);
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
    MFShutdown();
    CoUninitialize();
    return 0;
}
