#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>
#include <string>
#include <cstdint>

#include "ComPtr.h"

namespace ReelCam {

struct WebcamDeviceInfo {
    std::wstring friendlyName;
    std::wstring symbolicLink;
};

class WebcamPassthrough {
public:
    WebcamPassthrough();
    ~WebcamPassthrough();

    static std::vector<WebcamDeviceInfo> EnumeratePhysicalWebcams();

    bool StartCapture(int deviceIndex = 0);
    void StopCapture();

    bool ReadFrame(uint8_t* pOutNv12, size_t maxLen, int64_t* pOutPts100ns);

    bool IsCapturing() const { return m_pReader != nullptr; }
    const std::wstring& GetActiveDeviceName() const { return m_activeDeviceName; }

private:
    std::wstring                     m_activeDeviceName;
    ReelCam::ComPtr<IMFSourceReader> m_pReader;
    ReelCam::ComPtr<IMFMediaSource>  m_pSource;
};

} // namespace ReelCam
