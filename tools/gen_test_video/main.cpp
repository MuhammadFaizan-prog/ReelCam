#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <stdio.h>
#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "ole32.lib")

int wmain(int argc, wchar_t* argv[])
{
    const wchar_t* outputPath = (argc > 1) ? argv[1] : L"tests/media/sample_1080p30.mp4";
    printf("Generating sample MP4 video: %ls\n", outputPath);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return 1;

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) { CoUninitialize(); return 1; }

    const UINT32 width = 1920;
    const UINT32 height = 1080;
    const UINT32 fps = 30;
    const UINT32 numFrames = 150; // 5 seconds
    const UINT32 nv12Size = width * height * 3 / 2;

    IMFSinkWriter* pWriter = nullptr;
    hr = MFCreateSinkWriterFromURL(outputPath, nullptr, nullptr, &pWriter);
    if (FAILED(hr)) {
        printf("ERROR: MFCreateSinkWriterFromURL failed: 0x%08lX\n", hr);
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    // Configure H.264 video output
    IMFMediaType* pMediaTypeOut = nullptr;
    MFCreateMediaType(&pMediaTypeOut);
    pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, 4000000); // 4 Mbps
    pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, fps, 1);
    MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    DWORD streamIndex = 0;
    hr = pWriter->AddStream(pMediaTypeOut, &streamIndex);
    pMediaTypeOut->Release();
    if (FAILED(hr)) {
        printf("ERROR: AddStream failed: 0x%08lX\n", hr);
        pWriter->Release();
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    // Configure input format (uncompressed NV12)
    IMFMediaType* pMediaTypeIn = nullptr;
    MFCreateMediaType(&pMediaTypeIn);
    pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    pMediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, fps, 1);
    MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    hr = pWriter->SetInputMediaType(streamIndex, pMediaTypeIn, nullptr);
    pMediaTypeIn->Release();
    if (FAILED(hr)) {
        printf("ERROR: SetInputMediaType failed: 0x%08lX\n", hr);
        pWriter->Release();
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    hr = pWriter->BeginWriting();
    if (FAILED(hr)) {
        printf("ERROR: BeginWriting failed: 0x%08lX\n", hr);
        pWriter->Release();
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    std::vector<uint8_t> frameData(nv12Size, 0);

    for (UINT32 f = 0; f < numFrames; f++) {
        // Draw moving color gradient in NV12
        uint8_t* pY = frameData.data();
        uint8_t* pUV = frameData.data() + width * height;

        for (UINT32 y = 0; y < height; y++) {
            for (UINT32 x = 0; x < width; x++) {
                pY[y * width + x] = static_cast<uint8_t>((x + f * 5) % 256);
            }
        }
        for (UINT32 y = 0; y < height / 2; y++) {
            for (UINT32 x = 0; x < width; x += 2) {
                pUV[y * width + x] = static_cast<uint8_t>((y * 2) % 256);     // U
                pUV[y * width + x + 1] = static_cast<uint8_t>((f * 3) % 256); // V
            }
        }

        IMFMediaBuffer* pBuffer = nullptr;
        MFCreateMemoryBuffer(nv12Size, &pBuffer);

        BYTE* pData = nullptr;
        pBuffer->Lock(&pData, nullptr, nullptr);
        memcpy(pData, frameData.data(), nv12Size);
        pBuffer->Unlock();
        pBuffer->SetCurrentLength(nv12Size);

        IMFSample* pSample = nullptr;
        MFCreateSample(&pSample);
        pSample->AddBuffer(pBuffer);
        pBuffer->Release();

        LONGLONG duration = 10000000LL / fps;
        pSample->SetSampleTime(static_cast<LONGLONG>(f) * duration);
        pSample->SetSampleDuration(duration);

        hr = pWriter->WriteSample(streamIndex, pSample);
        pSample->Release();

        if (FAILED(hr)) {
            printf("ERROR: WriteSample %u failed: 0x%08lX\n", f, hr);
            break;
        }
    }

    pWriter->Finalize();
    pWriter->Release();

    printf("SUCCESS: Generated %u frames in %ls!\n", numFrames, outputPath);

    MFShutdown();
    CoUninitialize();
    return 0;
}
