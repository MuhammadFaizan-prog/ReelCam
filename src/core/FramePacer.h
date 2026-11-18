#pragma once

#include <windows.h>
#include <cstdint>

namespace ReelCam {

class FramePacer {
public:
    FramePacer();
    ~FramePacer();

    void SetTargetFps(double fps);
    double GetTargetFps() const { return m_fps; }

    void Start();
    void Reset();

    // Waits with microsecond accuracy until the next frame presentation time
    void WaitNextFrame();

    uint64_t GetCurrentFrameIndex() const { return m_frameIndex; }
    int64_t GetCurrentPts100ns() const;

private:
    double        m_fps = 30.0;
    HANDLE        m_hTimer = nullptr;
    LARGE_INTEGER m_qpcFreq{};
    LARGE_INTEGER m_qpcStart{};
    uint64_t      m_frameIndex = 0;
};

} // namespace ReelCam
