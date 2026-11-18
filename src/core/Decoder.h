#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <string>
#include <cstdint>
#include <vector>

#include "ComPtr.h"

namespace ReelCam {

class Decoder {
public:
    Decoder();
    ~Decoder();

    bool OpenFile(const std::wstring& filePath);
    void Close();

    // Read next video frame into NV12 buffer (1920x1080)
    bool ReadFrame(uint8_t* pOutNv12, size_t maxLen, int64_t* pOutPts100ns, bool* pOutEof);

    // Seek to position in 100ns units
    bool Seek(int64_t timestamp100ns);

    bool IsOpen() const { return m_pReader != nullptr; }
    int64_t GetDuration100ns() const { return m_duration100ns; }
    int64_t GetDurationMs() const { return m_duration100ns / 10000; }
    uint32_t GetNativeWidth() const { return m_nativeWidth; }
    uint32_t GetNativeHeight() const { return m_nativeHeight; }
    double GetNativeFps() const { return m_nativeFps; }
    const std::wstring& GetFilePath() const { return m_filePath; }

private:
    HRESULT ConfigureOutputFormat();

    std::wstring                  m_filePath;
    ReelCam::ComPtr<IMFSourceReader> m_pReader;
    int64_t                       m_duration100ns = 0;
    uint32_t                      m_nativeWidth = 1920;
    uint32_t                      m_nativeHeight = 1080;
    double                        m_nativeFps = 30.0;
    std::vector<uint8_t>          m_sampleBuffer;
};

} // namespace ReelCam
