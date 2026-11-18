#include <windows.h>
#include <mfapi.h>
#include <stdio.h>
#include <string>

#include "CoreServer.h"
#include "Logging.h"

static ReelCam::CoreServer* g_pServer = nullptr;

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_CLOSE_EVENT) {
        printf("\nShutting down ReelCam Core Engine...\n");
        if (g_pServer) {
            g_pServer->Stop();
        }
        return TRUE;
    }
    return FALSE;
}

int wmain(int argc, wchar_t* argv[])
{
    printf("=========================================\n");
    printf("   ReelCam Core Playback & IPC Engine    \n");
    printf("=========================================\n\n");

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        printf("ERROR: CoInitializeEx failed: 0x%08lX\n", hr);
        return 1;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        printf("ERROR: MFStartup failed: 0x%08lX\n", hr);
        CoUninitialize();
        return 1;
    }

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    ReelCam::CoreServer server;
    g_pServer = &server;

    if (!server.Start()) {
        printf("ERROR: Failed to start ReelCam Core Server\n");
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    printf("ReelCam Core Server is RUNNING.\n");
    printf("Shared memory: Local\\ReelCam_FrameBuffer active\n");
    printf("Named Pipe   : \\\\.\\pipe\\ReelCam_IPC listening\n\n");

    // Process CLI arguments if a video file was specified
    std::wstring videoToLoad;
    bool autoPlay = true;

    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"--file") == 0 && (i + 1) < argc) {
            videoToLoad = argv[++i];
        } else if (_wcsicmp(argv[i], L"--no-play") == 0) {
            autoPlay = false;
        } else if (_wcsicmp(argv[i], L"--mirror") == 0) {
            server.SetMirror(true);
        } else if (_wcsicmp(argv[i], L"--no-watermark") == 0) {
            server.SetWatermark(false);
        } else if (videoToLoad.empty() && argv[i][0] != L'-') {
            videoToLoad = argv[i];
        }
    }

    if (!videoToLoad.empty()) {
        printf("Loading video file: %ls\n", videoToLoad.c_str());
        if (server.LoadVideo(videoToLoad, autoPlay)) {
            printf("Video loaded successfully. Playing into virtual camera stream.\n");
        } else {
            printf("WARNING: Failed to load video '%ls'\n", videoToLoad.c_str());
        }
    } else {
        printf("No video specified. Virtual camera is in Standby mode.\n");
        printf("Open the ReelCam UI or send IPC command to load a video.\n");
    }

    printf("\nPress Ctrl+C or ENTER in this console to stop...\n");
    getchar();

    server.Stop();
    g_pServer = nullptr;

    MFShutdown();
    CoUninitialize();
    printf("ReelCam Core Engine stopped cleanly.\n");
    return 0;
}
