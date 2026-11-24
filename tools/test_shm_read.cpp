#include <windows.h>
#include <stdio.h>
#include <vector>
#include <stdint.h>
#include "SharedMemory.h"

int main() {
    printf("Opening SharedMemoryConsumer...\n");
    ReelCam::SharedMemoryConsumer consumer;
    if (!consumer.Open()) {
        printf("Failed to open consumer!\n");
        return 1;
    }
    printf("Consumer opened. IsActive=%d\n", consumer.IsActive());

    std::vector<uint8_t> buffer(1920 * 1080 * 3 / 2, 0);
    uint32_t w = 0, h = 0, strideY = 0, strideUV = 0;
    int64_t pts = 0;

    for (int i = 0; i < 5; i++) {
        bool ok = consumer.ReadLatestFrame(buffer.data(), buffer.size(), &w, &h, &strideY, &strideUV, &pts);
        printf("Read frame[%d]: ok=%d, w=%u, h=%u, pts=%lld, firstByte=%u\n", i, ok, w, h, pts, buffer[0]);
        Sleep(100);
    }
    consumer.Close();
    return 0;
}
