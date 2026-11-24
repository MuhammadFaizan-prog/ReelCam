// End-to-end test of the registered ReelCam device through Windows Frame Server.
// A stream tick is NOT a video sample. Return failure on missing/empty frames.
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdio.h>
#include <new>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

class ReaderCallback final : public IMFSourceReaderCallback {
    long refs_ = 1;
    HANDLE event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
public:
    HRESULT status = E_PENDING;
    DWORD flags = 0;
    DWORD bytes = 0;
    LONGLONG timestamp = 0;
    bool hasSample = false;
    BYTE firstByte = 0;
    ~ReaderCallback() { if (event_) CloseHandle(event_); }
    bool Ready() const { return event_ != nullptr; }
    DWORD Wait(DWORD milliseconds) const { return WaitForSingleObject(event_, milliseconds); }
    STDMETHODIMP QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_IMFSourceReaderCallback) {
            *out = static_cast<IMFSourceReaderCallback*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override {
        long n = InterlockedDecrement(&refs_);
        if (!n) delete this;
        return n;
    }
    STDMETHODIMP OnReadSample(HRESULT hr, DWORD, DWORD streamFlags, LONGLONG ts, IMFSample* sample) override {
        status = hr;
        flags = streamFlags;
        timestamp = ts;
        hasSample = sample != nullptr;
        bytes = 0;
        firstByte = 0;
        if (sample) {
            sample->GetTotalLength(&bytes);
            IMFMediaBuffer* buffer = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer))) {
                BYTE* data = nullptr;
                DWORD length = 0;
                if (SUCCEEDED(buffer->Lock(&data, nullptr, &length))) {
                    if (data && length) firstByte = data[0];
                    buffer->Unlock();
                }
                buffer->Release();
            }
        }
        printf("Callback: hr=0x%08lX flags=0x%lx timestamp=%lld sample=%s bytes=%lu firstByte=%u\n",
               hr, streamFlags, ts, sample ? "yes" : "no", bytes, static_cast<unsigned>(firstByte));
        fflush(stdout);
        SetEvent(event_);
        return S_OK;
    }
    STDMETHODIMP OnFlush(DWORD) override { return S_OK; }
    STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*) override { return S_OK; }
};

int main() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { printf("FAIL CoInitializeEx: 0x%08lX\n", hr); return 1; }
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) { printf("FAIL MFStartup: 0x%08lX\n", hr); CoUninitialize(); return 1; }

    IMFAttributes* attrs = nullptr;
    IMFActivate** devices = nullptr;
    IMFMediaSource* source = nullptr;
    IMFSourceReader* reader = nullptr;
    ReaderCallback* callback = nullptr;
    UINT32 count = 0;
    bool passed = false;
    hr = MFCreateAttributes(&attrs, 1);
    if (SUCCEEDED(hr)) hr = attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (SUCCEEDED(hr)) hr = MFEnumDeviceSources(attrs, &devices, &count);
    if (attrs) { attrs->Release(); attrs = nullptr; }
    if (FAILED(hr)) { printf("FAIL enumeration: 0x%08lX\n", hr); goto cleanup; }
    for (UINT32 i = 0; i < count; ++i) {
        WCHAR name[256] = {};
        if (SUCCEEDED(devices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, name, 256, nullptr)) &&
            wcsstr(name, L"ReelCam") != nullptr && wcsstr(name, L"Virtual Camera") != nullptr) {
            printf("Testing registered ReelCam virtual camera\n");
            hr = devices[i]->ActivateObject(IID_IMFMediaSource, reinterpret_cast<void**>(&source));
            break;
        }
    }
    if (!source || FAILED(hr)) { printf("FAIL device missing or activation failed: 0x%08lX\n", hr); goto cleanup; }
    callback = new (std::nothrow) ReaderCallback();
    if (!callback || !callback->Ready()) { printf("FAIL callback allocation\n"); goto cleanup; }
    hr = MFCreateAttributes(&attrs, 1);
    if (SUCCEEDED(hr)) hr = attrs->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, callback);
    if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromMediaSource(source, attrs, &reader);
    if (attrs) { attrs->Release(); attrs = nullptr; }
    if (FAILED(hr) || !reader) { printf("FAIL source reader: 0x%08lX\n", hr); goto cleanup; }
    {
        int frames = 0;
        int ticks = 0;
        LONGLONG lastTimestamp = -1;
        constexpr int requiredFrames = 30;
        const ULONGLONG deadline = GetTickCount64() + 15000;
        while (frames < requiredFrames && GetTickCount64() < deadline) {
            hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, nullptr, nullptr, nullptr);
            if (FAILED(hr)) { printf("FAIL ReadSample: 0x%08lX\n", hr); break; }
            if (callback->Wait(2500) != WAIT_OBJECT_0) { printf("FAIL timed out waiting for a sample callback\n"); break; }
            if (FAILED(callback->status) || (callback->flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM))) {
                printf("FAIL stream error/end\n"); break;
            }
            if (!callback->hasSample) { ++ticks; continue; }
            if (!callback->bytes || (lastTimestamp >= 0 && callback->timestamp <= lastTimestamp)) {
                printf("FAIL empty sample or non-increasing timestamps\n"); break;
            }
            lastTimestamp = callback->timestamp;
            ++frames;
        }
        passed = frames == requiredFrames;
        printf("%s: %d actual non-empty video samples; %d non-sample callbacks\n", passed ? "PASS" : "FAIL", frames, ticks);
    }
cleanup:
    if (reader) reader->Release();
    if (source) { source->Shutdown(); source->Release(); }
    if (callback) callback->Release();
    if (attrs) attrs->Release();
    if (devices) { for (UINT32 i = 0; i < count; ++i) devices[i]->Release(); CoTaskMemFree(devices); }
    MFShutdown();
    CoUninitialize();
    return passed ? 0 : 1;
}
