#include "MainWindow.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <commdlg.h>
#include <stdio.h>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define ID_BTN_OPEN       101
#define ID_BTN_PLAY       102
#define ID_BTN_PAUSE      103
#define ID_BTN_STOP       104
#define ID_BTN_LOOP       105
#define ID_BTN_MIRROR     106
#define ID_BTN_WATERMARK  107
#define ID_BTN_WEBCAM     108
#define ID_SLIDER_SEEK    109
#define ID_BTN_REGISTER   110
#define ID_TRAY_EXIT      201
#define ID_TRAY_OPEN      202
#define ID_TRAY_PLAYPAUSE 203
#define ID_TRAY_WEBCAM    204
#define WM_TRAYICON       (WM_USER + 10)
#define HOTKEY_PLAYPAUSE  1
#define HOTKEY_PASSTHROUGH 2

namespace ReelCam {

static inline uint8_t Clamp8(int val) {
    return static_cast<uint8_t>((val < 0) ? 0 : ((val > 255) ? 255 : val));
}

MainWindow::MainWindow()
{
    m_previewRgb.resize(PREVIEW_WIDTH * PREVIEW_HEIGHT * 4, 0);
    m_previewNv12.resize(NV12_MAX_BUFFER_SIZE, 0);

    memset(&m_bmi, 0, sizeof(m_bmi));
    m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_bmi.bmiHeader.biWidth = PREVIEW_WIDTH;
    m_bmi.bmiHeader.biHeight = -static_cast<LONG>(PREVIEW_HEIGHT); // Top-down DIB: row 0 is top
    m_bmi.bmiHeader.biPlanes = 1;
    m_bmi.bmiHeader.biBitCount = 32;
    m_bmi.bmiHeader.biCompression = BI_RGB;
}

MainWindow::~MainWindow()
{
    UnregisterHotkeys();
    RemoveSystemTray();
    m_ipcClient.Disconnect();
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow)
{
    m_hInstance = hInstance;

    // Enable visual styles
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(24, 24, 27)); // Dark slate background
    wc.lpszClassName = L"ReelCamMainWindowClass";
    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        wc.lpszClassName,
        L"ReelCam — Virtual Camera Studio",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 740,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) return false;

    // Enable Windows 11 immersive dark mode for title bar
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(m_hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));

    InitControls();
    SetupSystemTray();
    RegisterHotkeys();

    EnsureCoreRunning();
    ConnectToCore();

    // 30 fps preview refresh timer
    SetTimer(m_hWnd, m_timerId, 33, nullptr);

    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    return true;
}

void MainWindow::InitControls()
{
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT hFontBold = CreateFontW(17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    // File label
    m_hLblFile = CreateWindowW(L"STATIC", L"No file loaded (Displaying Standby Slate)",
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
        20, 480, 600, 22, m_hWnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_hLblFile, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    // Status label (e.g. "Status: Idle • Output: 1080p30 NV12 • Device: ReelCam")
    m_hLblStatus = CreateWindowW(L"STATIC", L"Status: Standby • Output: 1080p30 NV12 • Device: ReelCam",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        500, 480, 440, 22, m_hWnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_hLblStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Timeline Slider
    m_hSlider = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS | WS_TABSTOP,
        20, 508, 800, 30, m_hWnd, (HMENU)ID_SLIDER_SEEK, m_hInstance, nullptr);
    SendMessageW(m_hSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 1000));

    // Time Indicator (00:00 / 00:00)
    m_hLblTime = CreateWindowW(L"STATIC", L"00:00 / 00:00",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        830, 514, 110, 22, m_hWnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_hLblTime, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Control Buttons Row
    int btnY = 550;
    int btnH = 34;

    m_hBtnOpen = CreateWindowW(L"BUTTON", L"Open Video...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, btnY, 110, btnH, m_hWnd, (HMENU)ID_BTN_OPEN, m_hInstance, nullptr);

    m_hBtnPlay = CreateWindowW(L"BUTTON", L"Play",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        140, btnY, 80, btnH, m_hWnd, (HMENU)ID_BTN_PLAY, m_hInstance, nullptr);

    m_hBtnPause = CreateWindowW(L"BUTTON", L"Pause",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        228, btnY, 80, btnH, m_hWnd, (HMENU)ID_BTN_PAUSE, m_hInstance, nullptr);

    m_hBtnStop = CreateWindowW(L"BUTTON", L"Stop",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        316, btnY, 80, btnH, m_hWnd, (HMENU)ID_BTN_STOP, m_hInstance, nullptr);

    m_hBtnRegister = CreateWindowW(L"BUTTON", IsCameraRegisteredInHklm() ? L"Camera: Registered" : L"Install Camera (Admin)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        404, btnY, 170, btnH, m_hWnd, (HMENU)ID_BTN_REGISTER, m_hInstance, nullptr);

    // Feature Toggles Row
    int toggleY = 600;
    m_hBtnLoop = CreateWindowW(L"BUTTON", L"Loop: ON",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, toggleY, 130, btnH, m_hWnd, (HMENU)ID_BTN_LOOP, m_hInstance, nullptr);

    m_hBtnMirror = CreateWindowW(L"BUTTON", L"Mirror: OFF",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        160, toggleY, 130, btnH, m_hWnd, (HMENU)ID_BTN_MIRROR, m_hInstance, nullptr);

    m_hBtnWatermark = CreateWindowW(L"BUTTON", L"Watermark: ON",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        300, toggleY, 150, btnH, m_hWnd, (HMENU)ID_BTN_WATERMARK, m_hInstance, nullptr);

    m_hBtnWebcam = CreateWindowW(L"BUTTON", L"Webcam Passthrough",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        460, toggleY, 180, btnH, m_hWnd, (HMENU)ID_BTN_WEBCAM, m_hInstance, nullptr);

    HWND buttons[] = { m_hBtnOpen, m_hBtnPlay, m_hBtnPause, m_hBtnStop, m_hBtnRegister,
                       m_hBtnLoop, m_hBtnMirror, m_hBtnWatermark, m_hBtnWebcam };
    for (HWND hBtn : buttons) {
        SendMessageW(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
}

void MainWindow::SetupSystemTray()
{
    memset(&m_nid, 0, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hWnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(m_nid.szTip, L"ReelCam — Virtual Camera Studio", _TRUNCATE);

    Shell_NotifyIconW(NIM_ADD, &m_nid);
    m_trayAdded = true;
}

void MainWindow::RemoveSystemTray()
{
    if (m_trayAdded) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_trayAdded = false;
    }
}

void MainWindow::RegisterHotkeys()
{
    // Alt + Shift + P: Toggle Play/Pause
    RegisterHotKey(m_hWnd, HOTKEY_PLAYPAUSE, MOD_ALT | MOD_SHIFT, 'P');
    // Alt + Shift + W: Toggle Webcam Passthrough
    RegisterHotKey(m_hWnd, HOTKEY_PASSTHROUGH, MOD_ALT | MOD_SHIFT, 'W');
}

void MainWindow::UnregisterHotkeys()
{
    UnregisterHotKey(m_hWnd, HOTKEY_PLAYPAUSE);
    UnregisterHotKey(m_hWnd, HOTKEY_PASSTHROUGH);
}

void MainWindow::EnsureCoreRunning()
{
    if (!WaitNamedPipeW(IPC_PIPE_NAME, 50)) {
        HANDLE hCoreCheck = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\ReelCam_Core_Mutex");
        if (hCoreCheck) {
            CloseHandle(hCoreCheck);
            return;
        }

        WCHAR exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        WCHAR* pSlash = wcsrchr(exePath, L'\\');
        if (pSlash) *(pSlash + 1) = L'\0';

        WCHAR corePath[MAX_PATH];
        swprintf_s(corePath, L"%sreelcam_core.exe", exePath);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (CreateProcessW(corePath, nullptr, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
}

void MainWindow::ConnectToCore()
{
    ConnectToCoreWithTimeout(0);
}

void MainWindow::ConnectToCoreBlocking()
{
    ConnectToCoreWithTimeout(3000);
}

void MainWindow::ConnectToCoreWithTimeout(int timeoutMs)
{
    if (m_ipcClient.IsConnected()) return;

    if (m_ipcClient.Connect([this](const std::string& msg) {
        this->OnIpcStatusReceived(msg);
    }, timeoutMs)) {
        m_shmConsumer.Open();
        m_ipcClient.SendCommand("{\"cmd\":\"get_state\"}\n");
    }
}

void MainWindow::OnIpcStatusReceived(const std::string& json)
{
    std::lock_guard<std::mutex> lock(m_statusMutex);
    std::string type = JsonGetString(json, "type");
    if (type == "status") {
        m_lastStatus.state = StringToPlaybackState(JsonGetString(json, "state"));
        m_lastStatus.currentFilePath = JsonGetString(json, "file");
        m_lastStatus.positionMs = JsonGetInt(json, "positionMs");
        m_lastStatus.durationMs = JsonGetInt(json, "durationMs");
        m_lastStatus.width = static_cast<uint32_t>(JsonGetInt(json, "width", 1920));
        m_lastStatus.height = static_cast<uint32_t>(JsonGetInt(json, "height", 1080));
        m_lastStatus.isLooping = JsonGetBool(json, "isLooping", true);
        m_lastStatus.isMirror = JsonGetBool(json, "isMirror", false);
        m_lastStatus.isWatermark = JsonGetBool(json, "isWatermark", true);
        m_lastStatus.isPassthrough = JsonGetBool(json, "isPassthrough", false);
        m_lastStatus.selectedWebcam = JsonGetString(json, "webcam");
        m_lastStatus.errorMessage = JsonGetString(json, "error");
    }
}

void MainWindow::OpenFileDialog()
{
    WCHAR filename[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"Video Files (*.mp4;*.mov;*.mkv;*.webm)\0*.mp4;*.mov;*.mkv;*.webm\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, filename, -1, nullptr, 0, nullptr, nullptr);
        std::string utf8Path;
        if (utf8Size > 1) {
            utf8Path.resize(utf8Size, 0);
            WideCharToMultiByte(CP_UTF8, 0, filename, -1, utf8Path.data(), utf8Size, nullptr, nullptr);
            utf8Path.resize(utf8Size - 1);
        }
        // Use forward slashes to avoid JSON escaping issues
        for (char& c : utf8Path) {
            if (c == '\\') c = '/';
        }

        if (!m_ipcClient.IsConnected()) {
            EnsureCoreRunning();
            ConnectToCoreBlocking();
        }

        if (m_ipcClient.IsConnected()) {
            std::string cmd = "{\"cmd\":\"open\",\"path\":\"" + EscapeJson(utf8Path) + "\",\"autoPlay\":true}\n";
            if (m_ipcClient.SendCommand(cmd)) {
                SetWindowTextW(m_hLblStatus, L"Status: Loading video...");
            } else {
                SetWindowTextW(m_hLblStatus, L"Status: Failed to send command to core engine");
            }
        } else {
            SetWindowTextW(m_hLblStatus, L"Status: Core engine not connected");
        }
    }
}

void MainWindow::HandleDropFile(HDROP hDrop)
{
    WCHAR filename[MAX_PATH] = {};
    if (DragQueryFileW(hDrop, 0, filename, MAX_PATH)) {
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, filename, -1, nullptr, 0, nullptr, nullptr);
        std::string utf8Path;
        if (utf8Size > 1) {
            utf8Path.resize(utf8Size, 0);
            WideCharToMultiByte(CP_UTF8, 0, filename, -1, utf8Path.data(), utf8Size, nullptr, nullptr);
            utf8Path.resize(utf8Size - 1);
        }
        for (char& c : utf8Path) {
            if (c == '\\') c = '/';
        }

        if (!m_ipcClient.IsConnected()) {
            EnsureCoreRunning();
            ConnectToCoreBlocking();
        }

        if (m_ipcClient.IsConnected()) {
            std::string cmd = "{\"cmd\":\"open\",\"path\":\"" + EscapeJson(utf8Path) + "\",\"autoPlay\":true}\n";
            if (m_ipcClient.SendCommand(cmd)) {
                SetWindowTextW(m_hLblStatus, L"Status: Loading video...");
            }
        }
    }
    DragFinish(hDrop);
}

void MainWindow::SendPlay() { m_ipcClient.SendCommand("{\"cmd\":\"play\"}\n"); }
void MainWindow::SendPause() { m_ipcClient.SendCommand("{\"cmd\":\"pause\"}\n"); }
void MainWindow::SendStop() { m_ipcClient.SendCommand("{\"cmd\":\"stop\"}\n"); }
void MainWindow::SendSeek(int64_t ms) {
    std::string cmd = "{\"cmd\":\"seek\",\"ms\":" + std::to_string(ms) + "}\n";
    m_ipcClient.SendCommand(cmd);
}

void MainWindow::SendToggleLoop() {
    bool cur = m_lastStatus.isLooping;
    m_ipcClient.SendCommand("{\"cmd\":\"set_loop\",\"loop\":" + std::string(cur ? "false" : "true") + "}\n");
}

void MainWindow::SendToggleMirror() {
    bool cur = m_lastStatus.isMirror;
    m_ipcClient.SendCommand("{\"cmd\":\"set_mirror\",\"mirror\":" + std::string(cur ? "false" : "true") + "}\n");
}

void MainWindow::SendToggleWatermark() {
    bool cur = m_lastStatus.isWatermark;
    m_ipcClient.SendCommand("{\"cmd\":\"set_watermark\",\"watermark\":" + std::string(cur ? "false" : "true") + "}\n");
}

void MainWindow::SendTogglePassthrough() {
    bool cur = m_lastStatus.isPassthrough;
    m_ipcClient.SendCommand("{\"cmd\":\"set_passthrough\",\"passthrough\":" + std::string(cur ? "false" : "true") + "}\n");
}

bool MainWindow::IsCameraRegisteredInHklm()
{
    HKEY hKey = nullptr;
    LONG lRes = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"Software\\Classes\\CLSID\\{E7B1A280-9A23-4560-BD92-358F58A009C1}\\InprocServer32",
        0, KEY_READ, &hKey);
    if (lRes == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

void MainWindow::RegisterVirtualCamera()
{
    WCHAR exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    WCHAR* pSlash = wcsrchr(exePath, L'\\');
    if (pSlash) *(pSlash + 1) = L'\0';
    WCHAR regPath[MAX_PATH];
    swprintf_s(regPath, L"%scam_register.exe", exePath);

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = regPath;
    sei.lpParameters = L"--register";
    sei.hwnd = m_hWnd;
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            CloseHandle(sei.hProcess);
        }
        bool registered = IsCameraRegisteredInHklm();
        if (registered) {
            SetWindowTextW(m_hBtnRegister, L"Camera: Registered");
            MessageBoxW(m_hWnd,
                L"ReelCam Virtual Camera is now registered and active!\n\n"
                L"You can now select 'ReelCam' in Google Meet, Chrome, Zoom, Teams, and Discord.\n"
                L"If Google Meet or Chrome is already open, please refresh the webpage or restart Chrome.",
                L"ReelCam Registration Complete", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(m_hWnd,
                L"Registration was not completed.\n"
                L"Please run register_camera.bat as Administrator.",
                L"ReelCam Registration", MB_OK | MB_ICONWARNING);
        }
    }
}

void MainWindow::PaintPreview(HDC hdc, const RECT& rc)
{
    uint32_t w = 1920, h = 1080, strideY = 1920, strideUV = 1920;
    int64_t pts = 0;
    uint64_t seq = 0;

    bool hasFrame = m_shmConsumer.ReadLatestFrame(
        m_previewNv12.data(), m_previewNv12.size(), &w, &h, &strideY, &strideUV, &pts, &seq);

    if (hasFrame && w > 0 && h > 0) {
        // Fast NV12 -> RGB32 downsampled conversion (only if frame sequence changed)
        if (seq != m_lastRenderedSeq || !m_hasValidRgb) {
            m_lastRenderedSeq = seq;
            m_hasValidRgb = true;

            const uint8_t* pY = m_previewNv12.data();
            const uint8_t* pUV = m_previewNv12.data() + (w * h);
            uint8_t* pRgb = m_previewRgb.data();

            // Downsample directly to PREVIEW_WIDTH x PREVIEW_HEIGHT (960x540)
            // Top-down DIB: row 0 in pRgb corresponds to row 0 in NV12
            for (uint32_t y = 0; y < PREVIEW_HEIGHT; y++) {
                uint32_t srcY = (y * h) / PREVIEW_HEIGHT;
                uint32_t uvY = srcY / 2;
                const uint8_t* rowY = pY + (srcY * strideY);
                const uint8_t* rowUV = pUV + (uvY * strideUV);
                uint8_t* rowRgb = pRgb + (y * PREVIEW_WIDTH * 4);

                for (uint32_t x = 0; x < PREVIEW_WIDTH; x++) {
                    uint32_t srcX = (x * w) / PREVIEW_WIDTH;
                    uint8_t Y = rowY[srcX];
                    uint32_t uvX = (srcX / 2) * 2;
                    uint8_t U = rowUV[uvX];
                    uint8_t V = rowUV[uvX + 1];

                    int C = static_cast<int>(Y) - 16;
                    int D = static_cast<int>(U) - 128;
                    int E = static_cast<int>(V) - 128;

                    uint8_t R = Clamp8((298 * C + 409 * E + 128) >> 8);
                    uint8_t G = Clamp8((298 * C - 100 * D - 208 * E + 128) >> 8);
                    uint8_t B = Clamp8((298 * C + 516 * D + 128) >> 8);

                    uint32_t rgbIdx = x * 4;
                    rowRgb[rgbIdx + 0] = B;
                    rowRgb[rgbIdx + 1] = G;
                    rowRgb[rgbIdx + 2] = R;
                    rowRgb[rgbIdx + 3] = 255;
                }
            }
        }

        // Draw preview with letterboxing
        int previewW = rc.right - rc.left;
        int previewH = rc.bottom - rc.top;

        SetStretchBltMode(hdc, COLORONCOLOR);
        StretchDIBits(
            hdc,
            rc.left, rc.top, previewW, previewH,
            0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT,
            m_previewRgb.data(),
            &m_bmi,
            DIB_RGB_COLORS,
            SRCCOPY
        );
    } else {
        m_hasValidRgb = false;
        HBRUSH hBrush = CreateSolidBrush(RGB(15, 15, 18));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(180, 180, 190));
        DrawTextW(hdc, L"ReelCam Standby — No video playing", -1, const_cast<LPRECT>(&rc),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void MainWindow::UpdateControlsFromStatus()
{
    std::lock_guard<std::mutex> lock(m_statusMutex);

    // Update Slider & Time Label if not dragging
    if (!m_userDraggingSlider && m_lastStatus.durationMs > 0) {
        int pos = static_cast<int>(m_lastStatus.positionMs * 1000 / m_lastStatus.durationMs);
        SendMessageW(m_hSlider, TBM_SETPOS, TRUE, pos);

        int curSec = static_cast<int>(m_lastStatus.positionMs / 1000);
        int durSec = static_cast<int>(m_lastStatus.durationMs / 1000);
        WCHAR timeStr[64];
        swprintf_s(timeStr, L"%02d:%02d / %02d:%02d",
            curSec / 60, curSec % 60, durSec / 60, durSec % 60);
        SetWindowTextW(m_hLblTime, timeStr);
    }

    // Toggle button labels
    SetWindowTextW(m_hBtnLoop, m_lastStatus.isLooping ? L"Loop: ON" : L"Loop: OFF");
    SetWindowTextW(m_hBtnMirror, m_lastStatus.isMirror ? L"Mirror: ON" : L"Mirror: OFF");
    SetWindowTextW(m_hBtnWatermark, m_lastStatus.isWatermark ? L"Watermark: ON" : L"Watermark: OFF");
    SetWindowTextW(m_hBtnWebcam, m_lastStatus.isPassthrough ? L"Passthrough: ACTIVE" : L"Webcam Passthrough");

    if (!m_lastStatus.currentFilePath.empty()) {
        std::string filename = m_lastStatus.currentFilePath;
        size_t slash = filename.find_last_of("\\/");
        if (slash != std::string::npos) filename = filename.substr(slash + 1);

        int wSize = MultiByteToWideChar(CP_UTF8, 0, filename.data(), -1, nullptr, 0);
        std::wstring wFile(wSize, 0);
        MultiByteToWideChar(CP_UTF8, 0, filename.data(), -1, wFile.data(), wSize);
        SetWindowTextW(m_hLblFile, wFile.c_str());
    }

    if (!m_lastStatus.errorMessage.empty()) {
        int wSize = MultiByteToWideChar(CP_UTF8, 0, m_lastStatus.errorMessage.data(), -1, nullptr, 0);
        std::wstring wErr(wSize, 0);
        MultiByteToWideChar(CP_UTF8, 0, m_lastStatus.errorMessage.data(), -1, wErr.data(), wSize);
        std::wstring errText = L"Error: " + wErr;
        SetWindowTextW(m_hLblStatus, errText.c_str());
    } else {
        WCHAR statusText[128];
        const char* stateStr = PlaybackStateToString(m_lastStatus.state);
        swprintf_s(statusText, L"Status: %hs • Output: 1080p30 NV12 • Device: ReelCam", stateStr);
        SetWindowTextW(m_hLblStatus, statusText);
    }
}

int MainWindow::RunMessageLoop()
{
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_TIMER: {
            if (wParam == m_timerId) {
                if (!m_ipcClient.IsConnected()) {
                    ConnectToCore();
                }
                // Request redraw of preview area (upper 460px)
                RECT rcPreview = { 20, 20, 940, 465 };
                InvalidateRect(hwnd, &rcPreview, FALSE);
                UpdateControlsFromStatus();
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rcPreview = { 20, 20, 940, 465 };
            PaintPreview(hdc, rcPreview);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            switch (id) {
                case ID_BTN_OPEN:      OpenFileDialog(); break;
                case ID_BTN_PLAY:      SendPlay(); break;
                case ID_BTN_PAUSE:     SendPause(); break;
                case ID_BTN_STOP:      SendStop(); break;
                case ID_BTN_LOOP:      SendToggleLoop(); break;
                case ID_BTN_MIRROR:    SendToggleMirror(); break;
                case ID_BTN_WATERMARK: SendToggleWatermark(); break;
                case ID_BTN_WEBCAM:    SendTogglePassthrough(); break;
                case ID_BTN_REGISTER:  RegisterVirtualCamera(); break;
            }
            return 0;
        }

        case WM_HSCROLL: {
            if ((HWND)lParam == m_hSlider) {
                WORD code = LOWORD(wParam);
                if (code == TB_THUMBTRACK) {
                    m_userDraggingSlider = true;
                    int pos = static_cast<int>(SendMessageW(m_hSlider, TBM_GETPOS, 0, 0));
                    if (m_lastStatus.durationMs > 0) {
                        int64_t previewMs = (static_cast<int64_t>(pos) * m_lastStatus.durationMs) / 1000;
                        int curSec = static_cast<int>(previewMs / 1000);
                        int durSec = static_cast<int>(m_lastStatus.durationMs / 1000);
                        WCHAR timeStr[64];
                        swprintf_s(timeStr, L"%02d:%02d / %02d:%02d",
                            curSec / 60, curSec % 60, durSec / 60, durSec % 60);
                        SetWindowTextW(m_hLblTime, timeStr);
                    }
                } else if (code == TB_THUMBPOSITION || code == TB_ENDTRACK) {
                    int pos = static_cast<int>(SendMessageW(m_hSlider, TBM_GETPOS, 0, 0));
                    if (m_lastStatus.durationMs > 0) {
                        int64_t targetMs = (static_cast<int64_t>(pos) * m_lastStatus.durationMs) / 1000;
                        SendSeek(targetMs);
                    }
                    m_userDraggingSlider = false;
                }
            }
            return 0;
        }

        case WM_DROPFILES: {
            HandleDropFile((HDROP)wParam);
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == HOTKEY_PLAYPAUSE) {
                if (m_lastStatus.state == PlaybackState::Playing) SendPause();
                else SendPlay();
            } else if (wParam == HOTKEY_PASSTHROUGH) {
                SendTogglePassthrough();
            }
            return 0;
        }

        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN, L"Open ReelCam Studio");
                InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
                InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_TRAY_PLAYPAUSE, L"Play / Pause (Alt+Shift+P)");
                InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_STRING, ID_TRAY_WEBCAM, L"Toggle Webcam (Alt+Shift+W)");
                InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
                InsertMenuW(hMenu, 5, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit ReelCam");

                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(hMenu);

                if (cmd == ID_TRAY_OPEN) {
                    ShowWindow(hwnd, SW_RESTORE);
                    SetForegroundWindow(hwnd);
                } else if (cmd == ID_TRAY_PLAYPAUSE) {
                    if (m_lastStatus.state == PlaybackState::Playing) SendPause();
                    else SendPlay();
                } else if (cmd == ID_TRAY_WEBCAM) {
                    SendTogglePassthrough();
                } else if (cmd == ID_TRAY_EXIT) {
                    DestroyWindow(hwnd);
                }
            } else if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            }
            return 0;
        }

        case WM_CLOSE: {
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, m_timerId);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

} // namespace ReelCam
