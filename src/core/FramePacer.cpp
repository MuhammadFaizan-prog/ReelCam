#include "FramePacer.h"
#include <algorithm>

namespace ReelCam {

FramePacer::FramePacer()
{
    QueryPerformanceFrequency(&m_qpcFreq);

    // Try high-resolution waitable timer (Windows 10 Build 1803+)
    m_hTimer = CreateWaitableTimerExW(
        nullptr,
        nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS
    );

    if (!m_hTimer) {
        // Fallback to standard waitable timer
        m_hTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
}

FramePacer::~FramePacer()
{
    if (m_hTimer) {
        CloseHandle(m_hTimer);
        m_hTimer = nullptr;
    }
}

void FramePacer::SetTargetFps(double fps)
{
    m_fps = (fps > 1.0 && fps <= 120.0) ? fps : 30.0;
}

void FramePacer::Start()
{
    m_frameIndex = 0;
    QueryPerformanceCounter(&m_qpcStart);
}

void FramePacer::Reset()
{
    m_frameIndex = 0;
    QueryPerformanceCounter(&m_qpcStart);
}

int64_t FramePacer::GetCurrentPts100ns() const
{
    // 1 second = 10,000,000 in 100ns units
    return static_cast<int64_t>(static_cast<double>(m_frameIndex) * 10000000.0 / m_fps);
}

void FramePacer::WaitNextFrame()
{
    m_frameIndex++;

    // Calculate target QPC timestamp for this frame
    int64_t targetTicks = m_qpcStart.QuadPart + static_cast<int64_t>(
        static_cast<double>(m_frameIndex) * static_cast<double>(m_qpcFreq.QuadPart) / m_fps
    );

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    int64_t diffTicks = targetTicks - now.QuadPart;
    if (diffTicks <= 0) {
        // Frame deadline already passed or exact, yield without sleep
        return;
    }

    // Convert diff in QPC ticks to 100-nanosecond units for waitable timer (negative = relative)
    int64_t diff100ns = (diffTicks * 10000000LL) / m_qpcFreq.QuadPart;
    if (diff100ns <= 0) return;

    if (m_hTimer && diff100ns > 10000) { // If more than 1ms
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -diff100ns;
        if (SetWaitableTimer(m_hTimer, &dueTime, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(m_hTimer, INFINITE);
        }
    } else {
        // Short spin-wait for sub-millisecond precision
        while (true) {
            QueryPerformanceCounter(&now);
            if (now.QuadPart >= targetTicks) break;
            YieldProcessor();
        }
    }
}

} // namespace ReelCam
