#include <windows.h>
#include <mfapi.h>
#include <stdio.h>
#include <cassert>
#include <string>
#include <vector>

#include "SharedMemory.h"
#include "IpcProtocol.h"
#include "NamedPipeServer.h"
#include "NamedPipeClient.h"
#include "Decoder.h"
#include "VideoProcessor.h"
#include "FramePacer.h"

static int g_passCount = 0;
static int g_failCount = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            printf("  [PASS] %s\n", msg); \
            g_passCount++; \
        } else { \
            printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
            g_failCount++; \
        } \
    } while (0)

static void TestSharedMemory()
{
    printf("\n--- Test: Shared Memory Ring Buffer ---\n");

    ReelCam::SharedMemoryProducer producer;
    TEST_ASSERT(producer.Initialize(), "SharedMemoryProducer::Initialize");

    ReelCam::SharedMemoryConsumer consumer;
    TEST_ASSERT(consumer.Open(), "SharedMemoryConsumer::Open");

    std::vector<uint8_t> testData(1920 * 1080 * 3 / 2, 0xAB);
    producer.PublishFrame(testData.data(), 1920, 1080, 1920, 1920, 1234567LL, true, false);

    TEST_ASSERT(consumer.IsActive(), "SharedMemoryConsumer::IsActive reports true");

    std::vector<uint8_t> readData(1920 * 1080 * 3 / 2, 0);
    uint32_t w = 0, h = 0, strideY = 0, strideUV = 0;
    int64_t pts = 0;

    bool readOk = consumer.ReadLatestFrame(readData.data(), readData.size(), &w, &h, &strideY, &strideUV, &pts);
    TEST_ASSERT(readOk, "SharedMemoryConsumer::ReadLatestFrame returns true");
    TEST_ASSERT(w == 1920 && h == 1080, "Frame dimensions match 1920x1080");
    TEST_ASSERT(pts == 1234567LL, "PTS timestamp matches published value");
    TEST_ASSERT(readData[0] == 0xAB && readData[100] == 0xAB, "Frame payload verified");

    consumer.Close();
    producer.Close();
}

static void TestVideoProcessor()
{
    printf("\n--- Test: Video Processor (Mirror & Watermark) ---\n");

    const uint32_t w = 1920, h = 1080;
    std::vector<uint8_t> buffer(w * h * 3 / 2, 100);

    // Test Idle Slate
    ReelCam::VideoProcessor::GenerateIdleSlate(buffer.data(), w, h);
    TEST_ASSERT(buffer[0] >= 20 && buffer[0] <= 40, "Idle slate background luma range");

    // Test Mirror
    buffer[0] = 50;
    buffer[w - 1] = 200;
    ReelCam::VideoProcessor::ApplyHorizontalMirror(buffer.data(), w, h);
    TEST_ASSERT(buffer[0] == 200 && buffer[w - 1] == 50, "Horizontal flip row swap verified");

    // Test Watermark Badge
    ReelCam::VideoProcessor::ApplyWatermarkBadge(buffer.data(), w, h);
    // Top-right corner should have darkened box and border
    uint32_t badgeX = w - 260;
    uint32_t badgeY = 30;
    TEST_ASSERT(buffer[badgeY * w + badgeX] == 200, "Watermark border pixel drawn");
}

static void TestDecoder()
{
    printf("\n--- Test: Media Foundation Video Decoder ---\n");

    ReelCam::Decoder decoder;
    bool opened = decoder.OpenFile(L"tests/media/sample_1080p30.mp4");
    TEST_ASSERT(opened, "Decoder opened sample_1080p30.mp4");

    if (opened) {
        TEST_ASSERT(decoder.GetNativeWidth() == 1920, "Video native width is 1920");
        TEST_ASSERT(decoder.GetNativeHeight() == 1080, "Video native height is 1080");
        TEST_ASSERT(decoder.GetDurationMs() >= 4500, "Video duration >= 4.5 seconds");

        std::vector<uint8_t> frame(1920 * 1080 * 3 / 2, 0);
        int64_t pts = 0;
        bool eof = false;

        bool readOk = decoder.ReadFrame(frame.data(), frame.size(), &pts, &eof);
        TEST_ASSERT(readOk, "Decoded first video frame");
        TEST_ASSERT(pts >= 0, "Valid presentation timestamp");

        // Test Seek
        bool seekOk = decoder.Seek(20000000LL); // 2.0 seconds
        TEST_ASSERT(seekOk, "Seek to 2.0s succeeded");

        readOk = decoder.ReadFrame(frame.data(), frame.size(), &pts, &eof);
        TEST_ASSERT(readOk, "Decoded frame after seek");
        TEST_ASSERT(pts >= 18000000LL, "PTS after seek reflects target position");

        decoder.Close();
        TEST_ASSERT(!decoder.IsOpen(), "Decoder closed cleanly");
    }
}

static void TestIpcProtocol()
{
    printf("\n--- Test: Named Pipe IPC Protocol ---\n");

    ReelCam::NamedPipeServer server;
    bool serverStarted = server.Start([](const std::string& req) {
        std::string cmd = ReelCam::JsonGetString(req, "cmd");
        if (cmd == "ping") return "{\"type\":\"pong\"}\n";
        return "{\"type\":\"ack\"}\n";
    });
    TEST_ASSERT(serverStarted, "NamedPipeServer started");

    ReelCam::NamedPipeClient client;
    bool connected = client.Connect();
    TEST_ASSERT(connected, "NamedPipeClient connected to server");

    if (connected) {
        bool sent = client.SendCommand("{\"cmd\":\"ping\"}\n");
        TEST_ASSERT(sent, "Sent command over Named Pipe");
    }

    client.Disconnect();
    server.Stop();
    TEST_ASSERT(!client.IsConnected(), "NamedPipeClient disconnected");
}

int main()
{
    printf("=========================================\n");
    printf("     ReelCam Automated Test Suite        \n");
    printf("=========================================\n");

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return 1;

    MFStartup(MF_VERSION);

    TestSharedMemory();
    TestVideoProcessor();
    TestDecoder();
    TestIpcProtocol();

    MFShutdown();
    CoUninitialize();

    printf("\n=========================================\n");
    printf("Test Results: %d PASSED, %d FAILED\n", g_passCount, g_failCount);
    printf("=========================================\n");

    return (g_failCount == 0) ? 0 : 1;
}
