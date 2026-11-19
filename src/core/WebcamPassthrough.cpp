#include "WebcamPassthrough.h"
#include "Logging.h"
#include "HResultHelpers.h"

namespace ReelCam {

WebcamPassthrough::WebcamPassthrough()
{
}

WebcamPassthrough::~WebcamPassthrough()
{
    StopCapture();
}

std::vector<WebcamDeviceInfo> WebcamPassthrough::EnumeratePhysicalWebcams()
{
    std::vector<WebcamDeviceInfo> list;

    ReelCam::ComPtr<IMFAttributes> pAttributes;
    HRESULT hr = MFCreateAttributes(pAttributes.ReleaseAndGetAddressOf(), 1);
    if (FAILED(hr)) return list;

    pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(pAttributes.Get(), &ppDevices, &count);
    if (FAILED(hr) || !ppDevices) return list;

    for (UINT32 i = 0; i < count; i++) {
        WCHAR name[256] = {};
        UINT32 len = 0;
        ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, name, 256, &len);

        WCHAR symLink[512] = {};
        ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, symLink, 512, &len);

        // Skip virtual camera itself
        if (wcsstr(name, L"ReelCam") == nullptr) {
            WebcamDeviceInfo info;
            info.friendlyName = name;
            info.symbolicLink = symLink;
            list.push_back(info);
        }
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);

    return list;
}

bool WebcamPassthrough::StartCapture(int deviceIndex)
{
    StopCapture();

    ReelCam::ComPtr<IMFAttributes> pAttributes;
    HRESULT hr = MFCreateAttributes(pAttributes.ReleaseAndGetAddressOf(), 1);
    if (FAILED(hr)) return false;

    pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(pAttributes.Get(), &ppDevices, &count);
    if (FAILED(hr) || !ppDevices || count == 0) return false;

    int targetIndex = (deviceIndex >= 0 && deviceIndex < static_cast<int>(count)) ? deviceIndex : 0;
    WCHAR name[256] = {};
    UINT32 len = 0;
    ppDevices[targetIndex]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, name, 256, &len);
    m_activeDeviceName = name;

    hr = ppDevices[targetIndex]->ActivateObject(IID_IMFMediaSource, (void**)m_pSource.ReleaseAndGetAddressOf());
    for (UINT32 i = 0; i < count; i++) ppDevices[i]->Release();
    CoTaskMemFree(ppDevices);

    if (FAILED(hr) || !m_pSource) {
        LOG_ERROR("WebcamPassthrough", "ActivateObject failed: 0x%08lX", hr);
        return false;
    }

    // Enable MF video processor for automatic format conversion (MJPEG/YUY2 -> NV12, scaling -> 1080p)
    ReelCam::ComPtr<IMFAttributes> pReaderAttrs;
    hr = MFCreateAttributes(pReaderAttrs.ReleaseAndGetAddressOf(), 2);
    if (SUCCEEDED(hr)) {
        pReaderAttrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        pReaderAttrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }

    hr = MFCreateSourceReaderFromMediaSource(m_pSource.Get(), pReaderAttrs.Get(), m_pReader.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("WebcamPassthrough", "MFCreateSourceReaderFromMediaSource failed: 0x%08lX", hr);
        StopCapture();
        return false;
    }

    // Request NV12 output at 1920x1080 -- MF video processor will convert and scale automatically
    ReelCam::ComPtr<IMFMediaType> pType;
    MFCreateMediaType(pType.ReleaseAndGetAddressOf());
    pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(pType.Get(), MF_MT_FRAME_SIZE, 1920, 1080);
    MFSetAttributeRatio(pType.Get(), MF_MT_FRAME_RATE, 30, 1);

    hr = m_pReader->SetCurrentMediaType(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
        nullptr,
        pType.Get()
    );
    if (FAILED(hr)) {
        // Fallback: try without fixed size hint (let MF pick native resolution, we scale in ReadFrame)
        ReelCam::ComPtr<IMFMediaType> pFallback;
        MFCreateMediaType(pFallback.ReleaseAndGetAddressOf());
        pFallback->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pFallback->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        hr = m_pReader->SetCurrentMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            nullptr,
            pFallback.Get()
        );
        if (FAILED(hr)) {
            LOG_ERROR("WebcamPassthrough", "SetCurrentMediaType NV12 failed: 0x%08lX", hr);
        }
    }

    // Read back actual negotiated format
    ReelCam::ComPtr<IMFMediaType> pActual;
    hr = m_pReader->GetCurrentMediaType(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
        pActual.ReleaseAndGetAddressOf()
    );
    if (SUCCEEDED(hr)) {
        UINT32 w = 0, h = 0;
        MFGetAttributeSize(pActual.Get(), MF_MT_FRAME_SIZE, &w, &h);
        m_captureWidth  = (w > 0) ? w : 1920;
        m_captureHeight = (h > 0) ? h : 1080;
        LOG_INFO("WebcamPassthrough", "Negotiated format: %ux%u", m_captureWidth, m_captureHeight);
    } else {
        m_captureWidth  = 1920;
        m_captureHeight = 1080;
    }

    LOG_INFO("WebcamPassthrough", "Started capture from '%ls' (%ux%u)", m_activeDeviceName.c_str(), m_captureWidth, m_captureHeight);
    return true;
}

void WebcamPassthrough::StopCapture()
{
    if (m_pReader) {
        m_pReader.Release();
    }
    if (m_pSource) {
        m_pSource->Shutdown();
        m_pSource.Release();
    }
    m_activeDeviceName.clear();
    m_captureWidth  = 0;
    m_captureHeight = 0;
}

bool WebcamPassthrough::ReadFrame(uint8_t* pOutNv12, size_t maxLen, int64_t* pOutPts100ns)
{
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

    if (FAILED(hr) || !pSample) return false;

    if (pOutPts100ns) *pOutPts100ns = static_cast<int64_t>(llTimestamp);

    ReelCam::ComPtr<IMFMediaBuffer> pBuffer;
    hr = pSample->ConvertToContiguousBuffer(pBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    BYTE* pSrc = nullptr;
    DWORD curLen = 0;
    hr = pBuffer->Lock(&pSrc, nullptr, &curLen);
    if (FAILED(hr)) return false;

    const uint32_t capW = (m_captureWidth  > 0) ? m_captureWidth  : 1920;
    const uint32_t capH = (m_captureHeight > 0) ? m_captureHeight : 1080;
    const uint32_t dstW = 1920, dstH = 1080;
    const size_t expectedCapBytes = static_cast<size_t>(capW) * capH * 3 / 2;

    if (capW == dstW && capH == dstH) {
        // Direct match
        size_t copyLen = (curLen < maxLen) ? curLen : maxLen;
        memcpy(pOutNv12, pSrc, copyLen);
    } else if (curLen >= expectedCapBytes && maxLen >= static_cast<size_t>(dstW * dstH * 3 / 2)) {
        // Rescale NV12 to 1920x1080 (nearest-neighbour)
        uint8_t* dstY  = pOutNv12;
        uint8_t* dstUV = pOutNv12 + dstW * dstH;
        const uint8_t* srcY  = pSrc;
        const uint8_t* srcUV = pSrc + capW * capH;

        // Scale Y plane
        for (uint32_t dy = 0; dy < dstH; dy++) {
            uint32_t sy = dy * capH / dstH;
            const uint8_t* srcRow = srcY + sy * capW;
            uint8_t* dstRow = dstY + dy * dstW;
            for (uint32_t dx = 0; dx < dstW; dx++) {
                dstRow[dx] = srcRow[dx * capW / dstW];
            }
        }
        // Scale UV plane (half height)
        uint32_t uvSrcH = capH / 2, uvDstH = dstH / 2;
        for (uint32_t dy = 0; dy < uvDstH; dy++) {
            uint32_t sy = dy * uvSrcH / uvDstH;
            const uint8_t* srcRow = srcUV + sy * capW;
            uint8_t* dstRow = dstUV + dy * dstW;
            for (uint32_t dx = 0; dx < dstW; dx += 2) {
                uint32_t sx = (dx / 2) * capW / dstW * 2;
                if (sx + 1 < capW) {
                    dstRow[dx]     = srcRow[sx];
                    dstRow[dx + 1] = srcRow[sx + 1];
                }
            }
        }
    } else {
        size_t copyLen = (curLen < maxLen) ? curLen : maxLen;
        memcpy(pOutNv12, pSrc, copyLen);
    }

    pBuffer->Unlock();
    return true;
}

} // namespace ReelCam
