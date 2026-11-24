#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdio.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

#define LOG(fmt, ...) do { printf(fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);

    IMFAttributes* pAttrs = nullptr;
    MFCreateAttributes(&pAttrs, 1);
    pAttrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    HRESULT hr = MFEnumDeviceSources(pAttrs, &ppDevices, &count);
    pAttrs->Release();

    LOG("Found %u devices", count);
    for (UINT32 i = 0; i < count; i++) {
        WCHAR name[256] = {};
        UINT32 len = 0;
        ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, name, 256, &len);
        wprintf(L"Device %u: %s\n", i, name);
        fflush(stdout);

        if (wcsstr(name, L"ReelCam") != nullptr) {
            LOG("Testing ReelCam with MFCreateSourceReaderFromMediaSource...");
            IMFMediaSource* pSource = nullptr;
            hr = ppDevices[i]->ActivateObject(IID_IMFMediaSource, (void**)&pSource);
            LOG("ActivateObject: 0x%08lX", hr);
            if (SUCCEEDED(hr) && pSource) {
                IMFSourceReader* pReader = nullptr;
                hr = MFCreateSourceReaderFromMediaSource(pSource, nullptr, &pReader);
                LOG("MFCreateSourceReaderFromMediaSource: 0x%08lX", hr);
                if (SUCCEEDED(hr) && pReader) {
                    for (int s = 0; s < 2; s++) {
                        DWORD streamIndex = 0;
                        DWORD flags = 0;
                        LONGLONG timestamp = 0;
                        IMFSample* pSample = nullptr;
                        LOG("Calling ReadSample %d...", s);
                        hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &pSample);
                        LOG("ReadSample[%d]: 0x%08lX, flags: 0x%lx, pSample: %p, ts: %lld", s, hr, flags, pSample, timestamp);
                        if (pSample) pSample->Release();
                    }
                    LOG("Releasing pReader...");
                    pReader->Release();
                }
                LOG("Shutting down pSource...");
                pSource->Shutdown();
                pSource->Release();
            }
        }
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);

    MFShutdown();
    CoUninitialize();
    LOG("Test finished!");
    return 0;
}
