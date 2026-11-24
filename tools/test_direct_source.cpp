#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdio.h>

#include "src/mfsource/ReelCamMediaSource.h"
#include "src/mfsource/ReelCamMediaStream.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

void DllAddRef() {}
void DllRelease() {}

#define LOG(fmt, ...) do { printf(fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);

    LOG("--- Testing ReelCamMediaSource via SourceReader directly ---");
    ReelCam::ComPtr<ReelCamMediaSource> pSource = new (std::nothrow) ReelCamMediaSource();
    if (!pSource) { LOG("Failed to allocate ReelCamMediaSource"); return 1; }

    HRESULT hr = pSource->Initialize();
    LOG("Initialize: 0x%08lX", hr);

    ReelCam::ComPtr<IMFSourceReader> pReader;
    hr = MFCreateSourceReaderFromMediaSource(pSource.Get(), nullptr, pReader.ReleaseAndGetAddressOf());
    LOG("MFCreateSourceReaderFromMediaSource: 0x%08lX", hr);

    if (SUCCEEDED(hr) && pReader) {
        for (int s = 0; s < 5; s++) {
            DWORD streamIndex = 0;
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            IMFSample* pSample = nullptr;
            LOG("Calling ReadSample %d...", s);
            hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &pSample);
            LOG("ReadSample[%d]: 0x%08lX, flags: 0x%lx, pSample: %p, ts: %lld", s, hr, flags, pSample, timestamp);
            if (pSample) pSample->Release();
        }
        pReader.Reset();
    }

    pSource->Shutdown();

    MFShutdown();
    CoUninitialize();
    LOG("Direct SourceReader test finished successfully!");
    return 0;
}
