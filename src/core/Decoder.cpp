#include "Decoder.h"
#include "Logging.h"
#include "HResultHelpers.h"

namespace ReelCam {

Decoder::Decoder()
{
}

Decoder::~Decoder()
{
    Close();
}

void Decoder::Close()
{
    m_pReader.Reset();
    m_filePath.clear();
    m_duration100ns = 0;
    m_nativeWidth = 1920;
    m_nativeHeight = 1080;
    m_nativeFps = 30.0;
}

bool Decoder::OpenFile(const std::wstring& filePath)
{
    Close();

    // Create source reader attributes
    ReelCam::ComPtr<IMFAttributes> pAttributes;
    HRESULT hr = MFCreateAttributes(pAttributes.ReleaseAndGetAddressOf(), 4);
    if (FAILED(hr)) {
        LOG_ERROR("Decoder", "MFCreateAttributes failed: 0x%08lX", hr);
        return false;
    }

    pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
    pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

    hr = MFCreateSourceReaderFromURL(filePath.c_str(), pAttributes.Get(), m_pReader.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("Decoder", "MFCreateSourceReaderFromURL failed for %ls: 0x%08lX", filePath.c_str(), hr);
        return false;
    }

    // Read duration
    PROPVARIANT var;
    PropVariantInit(&var);
    hr = m_pReader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &var);
    if (SUCCEEDED(hr) && var.vt == VT_UI8) {
        m_duration100ns = static_cast<int64_t>(var.uhVal.QuadPart);
    }
    PropVariantClear(&var);

    // Query native media type
    ReelCam::ComPtr<IMFMediaType> pNativeType;
    hr = m_pReader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, pNativeType.ReleaseAndGetAddressOf());
    if (SUCCEEDED(hr)) {
        UINT32 w = 0, h = 0;
        if (SUCCEEDED(MFGetAttributeSize(pNativeType.Get(), MF_MT_FRAME_SIZE, &w, &h))) {
            m_nativeWidth = w;
            m_nativeHeight = h;
        }
        UINT32 num = 0, den = 0;
        if (SUCCEEDED(MFGetAttributeRatio(pNativeType.Get(), MF_MT_FRAME_RATE, &num, &den)) && den > 0) {
            m_nativeFps = static_cast<double>(num) / static_cast<double>(den);
        }
    }

    // Configure NV12 output format (1920x1080)
    if (FAILED(ConfigureOutputFormat())) {
        Close();
        return false;
    }

    m_filePath = filePath;
    LOG_INFO("Decoder", "Loaded '%ls' [%ux%u @ %.2f fps, duration=%lld ms]",
        filePath.c_str(), m_nativeWidth, m_nativeHeight, m_nativeFps, m_duration100ns / 10000);

    return true;
}

HRESULT Decoder::ConfigureOutputFormat()
{
    if (!m_pReader) return E_POINTER;

    // Deselect audio stream, keep only video
    m_pReader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    m_pReader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);

    // Create target media type: NV12 1920x1080
    ReelCam::ComPtr<IMFMediaType> pOutputType;
    RETURN_IF_FAILED(MFCreateMediaType(pOutputType.ReleaseAndGetAddressOf()));
    RETURN_IF_FAILED(pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    RETURN_IF_FAILED(pOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
    RETURN_IF_FAILED(pOutputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
    RETURN_IF_FAILED(MFSetAttributeSize(pOutputType.Get(), MF_MT_FRAME_SIZE, 1920, 1080));
    RETURN_IF_FAILED(MFSetAttributeRatio(pOutputType.Get(), MF_MT_FRAME_RATE, 30, 1));
    RETURN_IF_FAILED(MFSetAttributeRatio(pOutputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1));

    HRESULT hr = m_pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pOutputType.Get());
    if (FAILED(hr)) {
        LOG_WARN("Decoder", "Failed to set NV12 1080p directly (0x%08lX), trying native format", hr);
        // Fallback: let Source Reader pick compatible NV12
        pOutputType.Release();
        RETURN_IF_FAILED(MFCreateMediaType(pOutputType.ReleaseAndGetAddressOf()));
        RETURN_IF_FAILED(pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
        RETURN_IF_FAILED(pOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
        RETURN_IF_FAILED(m_pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pOutputType.Get()));
    }

    return S_OK;
}

bool Decoder::Seek(int64_t timestamp100ns)
{
    if (!m_pReader) return false;

    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = timestamp100ns;

    HRESULT hr = m_pReader->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);

    return SUCCEEDED(hr);
}

bool Decoder::ReadFrame(uint8_t* pOutNv12, size_t maxLen, int64_t* pOutPts100ns, bool* pOutEof)
{
    if (pOutEof) *pOutEof = false;
    if (!m_pReader || !pOutNv12) return false;

    DWORD streamIndex = 0;
    DWORD flags = 0;
    LONGLONG llTimestamp = 0;
    ReelCam::ComPtr<IMFSample> pSample;

    HRESULT hr = m_pReader->ReadSample(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
        0,
        &streamIndex,
        &flags,
        &llTimestamp,
        pSample.ReleaseAndGetAddressOf()
    );

    if (FAILED(hr)) {
        LOG_WARN("Decoder", "ReadSample failed: 0x%08lX", hr);
        return false;
    }

    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
        if (pOutEof) *pOutEof = true;
        return false;
    }

    if (!pSample) {
        // Gap in stream or format change, return false to retry next cycle
        return false;
    }

    if (pOutPts100ns) {
        *pOutPts100ns = static_cast<int64_t>(llTimestamp);
    }

    // Extract contiguous buffer
    ReelCam::ComPtr<IMFMediaBuffer> pBuffer;
    hr = pSample->ConvertToContiguousBuffer(pBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    BYTE* pSrc = nullptr;
    DWORD curLen = 0;
    hr = pBuffer->Lock(&pSrc, nullptr, &curLen);
    if (FAILED(hr)) return false;

    size_t copyLen = (curLen < maxLen) ? curLen : maxLen;
    memcpy(pOutNv12, pSrc, copyLen);

    pBuffer->Unlock();
    return true;
}

} // namespace ReelCam
