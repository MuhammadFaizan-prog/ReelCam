// cam_test_enum: Simple tool to enumerate video capture devices via MFEnumDeviceSources.
// Verifies that "ReelCam" appears in the device list after registration.

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <stdio.h>
#include "VirtualCameraCompat.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

static void TestCreate(MFVirtualCameraLifetime lifetime, MFVirtualCameraAccess access, const char* desc)
{
    printf("\n--- Testing MFCreateVirtualCamera: %s ---\n", desc);
    HMODULE hSensorGroup = LoadLibraryW(L"mfsensorgroup.dll");
    if (!hSensorGroup) {
        printf("mfsensorgroup.dll not found\n");
        return;
    }
    auto pfnCreate = reinterpret_cast<PFN_MFCreateVirtualCamera>(
        GetProcAddress(hSensorGroup, "MFCreateVirtualCamera"));
    if (!pfnCreate) {
        printf("MFCreateVirtualCamera not found\n");
        FreeLibrary(hSensorGroup);
        return;
    }

    IMFVirtualCamera* pVCam = nullptr;
    HRESULT hr = pfnCreate(
        MFVirtualCameraType_SoftwareCameraSource,
        lifetime,
        access,
        L"ReelCam",
        REELCAM_CLSID_STRING,
        nullptr,
        0,
        &pVCam
    );
    printf("MFCreateVirtualCamera result: hr = 0x%08lX\n", (unsigned long)hr);

    if (SUCCEEDED(hr) && pVCam) {
        HRESULT hrStart = pVCam->Start(nullptr);
        printf("pVCam->Start result: hr = 0x%08lX\n", (unsigned long)hrStart);
        fflush(stdout);

        // Now enumerate while it is active!
        printf("Enumerating devices while active...\n");
        fflush(stdout);
        IMFAttributes* pAttributes = nullptr;
        MFCreateAttributes(&pAttributes, 1);
        pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        IMFActivate** ppDevices = nullptr;
        UINT32 count = 0;
        MFEnumDeviceSources(pAttributes, &ppDevices, &count);
        printf("Device count: %u\n", count);
        fflush(stdout);
        for (UINT32 i = 0; i < count; i++) {
            WCHAR name[256] = {};
            UINT32 nameLen = 0;
            ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, name, 256, &nameLen);
            printf("  [%u] %ls\n", i, name);
            fflush(stdout);
            ppDevices[i]->Release();
        }
        CoTaskMemFree(ppDevices);
        pAttributes->Release();

        pVCam->Stop();
        pVCam->Remove();
        pVCam->Release();
    }
    FreeLibrary(hSensorGroup);
}

static bool EnumerateVideoDevices()
{
    IMFAttributes* pAttributes = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) {
        printf("MFCreateAttributes failed: 0x%08lX\n", (unsigned long)hr);
        return false;
    }

    pAttributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    pAttributes->Release();

    if (FAILED(hr)) {
        printf("MFEnumDeviceSources failed: 0x%08lX\n", (unsigned long)hr);
        return false;
    }

    printf("Video capture devices found: %u\n", count);
    bool foundReelCam = false;

    for (UINT32 i = 0; i < count; i++) {
        WCHAR name[256] = {};
        UINT32 nameLen = 0;
        ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, name, 256, &nameLen);

        WCHAR symLink[512] = {};
        UINT32 symLen = 0;
        ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            symLink, 512, &symLen);

        printf("  [%u] %-30ls  %ls\n", i, name, symLink);

        if (wcsstr(name, L"ReelCam") != nullptr) {
            foundReelCam = true;
            printf("       >>> FOUND ReelCam! Attempting to activate...\n");

            IMFMediaSource* pSource = nullptr;
            hr = ppDevices[i]->ActivateObject(IID_IMFMediaSource, (void**)&pSource);
            if (SUCCEEDED(hr) && pSource) {
                printf("       >>> IMFMediaSource activated successfully!\n");

                // Check presentation descriptor
                IMFPresentationDescriptor* pPD = nullptr;
                hr = pSource->CreatePresentationDescriptor(&pPD);
                if (SUCCEEDED(hr) && pPD) {
                    DWORD streamCount = 0;
                    pPD->GetStreamDescriptorCount(&streamCount);
                    printf("       >>> Streams: %lu\n", streamCount);

                    for (DWORD s = 0; s < streamCount; s++) {
                        BOOL selected = FALSE;
                        IMFStreamDescriptor* pSD = nullptr;
                        pPD->GetStreamDescriptorByIndex(s, &selected, &pSD);
                        if (pSD) {
                            IMFMediaTypeHandler* pHandler = nullptr;
                            pSD->GetMediaTypeHandler(&pHandler);
                            if (pHandler) {
                                DWORD typeCount = 0;
                                pHandler->GetMediaTypeCount(&typeCount);
                                printf("       >>> Stream[%lu] selected=%s, media types=%lu\n",
                                    s, selected ? "YES" : "NO", typeCount);

                                for (DWORD t = 0; t < typeCount; t++) {
                                    IMFMediaType* pType = nullptr;
                                    pHandler->GetMediaTypeByIndex(t, &pType);
                                    if (pType) {
                                        GUID subtype = {};
                                        pType->GetGUID(MF_MT_SUBTYPE, &subtype);
                                        UINT32 w = 0, h = 0;
                                        MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &w, &h);
                                        UINT32 num = 0, den = 0;
                                        MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &num, &den);
                                        printf("       >>>   Type[%lu]: %lux%lu @ %lu/%lu fps, subtype=%08lx-%04x-%04x\n",
                                            t, w, h, num, den,
                                            subtype.Data1, subtype.Data2, subtype.Data3);
                                        pType->Release();
                                    }
                                }
                                pHandler->Release();
                            }
                            pSD->Release();
                        }
                    }
                    pPD->Release();
                }

                pSource->Shutdown();
                pSource->Release();
            } else {
                printf("       >>> ActivateObject failed: 0x%08lX\n", (unsigned long)hr);
            }
        }

        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);

    return foundReelCam;
}

int main()
{
    printf("=== ReelCam Device Enumeration Test ===\n\n");

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        printf("CoInitializeEx failed: 0x%08lX\n", (unsigned long)hr);
        return 1;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        printf("MFStartup failed: 0x%08lX\n", (unsigned long)hr);
        CoUninitialize();
        return 1;
    }

    TestCreate(MFVirtualCameraLifetime_Session, MFVirtualCameraAccess_CurrentUser, "Session + CurrentUser");
    TestCreate(MFVirtualCameraLifetime_User, MFVirtualCameraAccess_CurrentUser, "System (Persistent) + CurrentUser");
    TestCreate(MFVirtualCameraLifetime_Session, MFVirtualCameraAccess_AllUsers, "Session + AllUsers");

    bool found = EnumerateVideoDevices();

    printf("\n%s\n", found
        ? "SUCCESS: ReelCam is visible as a video capture device!"
        : "INFO: ReelCam not found in device list. This may be normal if no app is actively consuming it.");

    MFShutdown();
    CoUninitialize();
    return found ? 0 : 2;
}
