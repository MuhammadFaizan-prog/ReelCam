// cam_test_enum: Simple tool to enumerate video capture devices via MFEnumDeviceSources.
// Verifies that "ReelCam" appears in the device list after registration.

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <stdio.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

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

    bool found = EnumerateVideoDevices();

    printf("\n%s\n", found
        ? "SUCCESS: ReelCam is visible as a video capture device!"
        : "INFO: ReelCam not found in device list. This may be normal if no app is actively consuming it.");

    MFShutdown();
    CoUninitialize();
    return found ? 0 : 2;
}
