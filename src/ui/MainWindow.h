#pragma once

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

#include "NamedPipeClient.h"
#include "SharedMemory.h"
#include "IpcProtocol.h"

namespace ReelCam {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInstance, int nCmdShow);
    int RunMessageLoop();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void InitControls();
    void SetupSystemTray();
    void RemoveSystemTray();
    void RegisterHotkeys();
    void UnregisterHotkeys();

    void EnsureCoreRunning();
    void ConnectToCore();
    void OnIpcStatusReceived(const std::string& json);

    void OpenFileDialog();
    void HandleDropFile(HDROP hDrop);

    void SendPlay();
    void SendPause();
    void SendStop();
    void SendSeek(int64_t ms);
    void SendToggleLoop();
    void SendToggleMirror();
    void SendToggleWatermark();
    void SendTogglePassthrough();

    void PaintPreview(HDC hdc, const RECT& rc);
    void UpdateControlsFromStatus();

    HINSTANCE            m_hInstance = nullptr;
    HWND                 m_hWnd = nullptr;
    HWND                 m_hBtnOpen = nullptr;
    HWND                 m_hBtnPlay = nullptr;
    HWND                 m_hBtnPause = nullptr;
    HWND                 m_hBtnStop = nullptr;
    HWND                 m_hBtnLoop = nullptr;
    HWND                 m_hBtnMirror = nullptr;
    HWND                 m_hBtnWatermark = nullptr;
    HWND                 m_hBtnWebcam = nullptr;
    HWND                 m_hSlider = nullptr;
    HWND                 m_hLblTime = nullptr;
    HWND                 m_hLblStatus = nullptr;
    HWND                 m_hLblFile = nullptr;

    NOTIFYICONDATAW      m_nid{};
    bool                 m_trayAdded = false;

    NamedPipeClient      m_ipcClient;
    SharedMemoryConsumer m_shmConsumer;
    CoreStatus           m_lastStatus;
    std::mutex           m_statusMutex;

    // Local RGB preview buffer converted from NV12
    std::vector<uint8_t> m_previewRgb;
    std::vector<uint8_t> m_previewNv12;
    BITMAPINFO           m_bmi{};

    UINT_PTR             m_timerId = 1;
    bool                 m_userDraggingSlider = false;
};

} // namespace ReelCam
