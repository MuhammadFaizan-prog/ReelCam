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

    hr = MFCreateSourceReaderFromMediaSource(m_pSource.Get(), nullptr, m_pReader.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("WebcamPassthrough", "MFCreateSourceReaderFromMediaSource failed: 0x%08lX", hr);
        StopCapture();
        return false;
    }

    // Set NV12 1080p output
    ReelCam::ComPtr<IMFMediaType> pType;
    MFCreateMediaType(pType.ReleaseAndGetAddressOf());
    pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    m_pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pType.Get());

    LOG_INFO("WebcamPassthrough", "Started capture from '%ls'", m_activeDeviceName.c_str());
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

    size_t copyLen = (curLen < maxLen) ? curLen : maxLen;
    memcpy(pOutNv12, pSrc, copyLen);

    pBuffer->Unlock();
    return true;
}

} // namespace ReelCam
