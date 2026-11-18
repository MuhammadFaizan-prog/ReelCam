#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>

namespace ReelCam {

class VideoProcessor {
public:
    VideoProcessor() = default;

    // Generates a clean dark idle slate ("ReelCam Standby") into target NV12 buffer
    static void GenerateIdleSlate(uint8_t* pNv12, uint32_t width, uint32_t height);

    // Applies horizontal flip (mirroring) in-place on NV12 buffer
    static void ApplyHorizontalMirror(uint8_t* pNv12, uint32_t width, uint32_t height);

    // Overlays the "PRE-RECORDED" transparency badge onto NV12 buffer
    static void ApplyWatermarkBadge(uint8_t* pNv12, uint32_t width, uint32_t height);

private:
    // Simple 5x7 bitmap font renderer for NV12 overlay
    static void DrawChar(uint8_t* pY, uint32_t stride, uint32_t x, uint32_t y, char c, uint8_t luma);
    static void DrawString(uint8_t* pY, uint32_t stride, uint32_t x, uint32_t y, const char* str, uint8_t luma);
};

} // namespace ReelCam
