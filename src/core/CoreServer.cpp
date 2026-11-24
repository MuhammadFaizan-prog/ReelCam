#include "CoreServer.h"
#include "Logging.h"
#include <codecvt>
#include <locale>

namespace ReelCam {

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0);
    std::wstring out(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), out.data(), size);
    return out;
}

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.length()), nullptr, 0, nullptr, nullptr);
    std::string out(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.length()), out.data(), size, nullptr, nullptr);
    return out;
}

CoreServer::CoreServer()
{
    m_rawFrameBuffer.resize(NV12_MAX_BUFFER_SIZE, 0);
    m_outFrameBuffer.resize(NV12_MAX_BUFFER_SIZE, 0);
    VideoProcessor::GenerateIdleSlate(m_rawFrameBuffer.data(), 1920, 1080);
}

CoreServer::~CoreServer()
{
    Stop();
}

bool CoreServer::Start()
{
    if (m_running) return true;

    if (!m_shmProducer.Initialize()) {
        LOG_ERROR("CoreServer", "Failed to initialize shared memory producer");
        return false;
    }

    // Load persisted settings
    if (m_settings.Load()) {
        m_status.isLooping = m_settings.isLooping;
        m_status.isMirror = m_settings.isMirror;
        m_status.isWatermark = m_settings.isWatermark;
    }

    m_running = true;

    // Start IPC server
    m_pipeServer.Start([this](const std::string& msg) {
        return this->HandleIpcMessage(msg);
    });

    m_workerThread = std::thread(&CoreServer::PlaybackLoop, this);
    LOG_INFO("CoreServer", "CoreServer started");
    return true;
}

void CoreServer::Stop()
{
    if (!m_running) return;

    m_running = false;
    m_pipeServer.Stop();
    m_shmProducer.SetActive(false);

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_decoder.Close();
    m_passthrough.StopCapture();
    m_shmProducer.Close();

    LOG_INFO("CoreServer", "CoreServer stopped");
}

bool CoreServer::LoadVideo(const std::wstring& path, bool autoPlay)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (!m_decoder.OpenFile(path)) {
        m_status.errorMessage = "Failed to open video file";
        m_status.state = PlaybackState::Error;
        BroadcastState();
        return false;
    }

    m_status.currentFilePath = WideToUtf8(path);
    m_settings.lastFile = m_status.currentFilePath;
    m_settings.Save();
    m_status.durationMs = m_decoder.GetDurationMs();
    m_status.width = m_decoder.GetNativeWidth();
    m_status.height = m_decoder.GetNativeHeight();
    m_status.fps = m_decoder.GetNativeFps();
    m_status.positionMs = 0;
    m_status.errorMessage.clear();

    m_pacer.SetTargetFps(m_status.fps > 0 ? m_status.fps : 30.0);

    if (autoPlay) {
        m_status.state = PlaybackState::Playing;
        m_pacer.Start();
    } else {
        m_status.state = PlaybackState::Paused;
    }

    BroadcastState();
    return true;
}

void CoreServer::Play()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_status.state == PlaybackState::Paused || m_status.state == PlaybackState::Idle) {
        if (m_decoder.IsOpen()) {
            m_status.state = PlaybackState::Playing;
            m_pacer.Start();
            BroadcastState();
        }
    }
}

void CoreServer::Pause()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_status.state == PlaybackState::Playing) {
        m_status.state = PlaybackState::Paused;
        BroadcastState();
    }
}

void CoreServer::StopPlayback()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_status.state = PlaybackState::Idle;
    m_status.positionMs = 0;
    if (m_decoder.IsOpen()) {
        m_decoder.Seek(0);
    }
    VideoProcessor::GenerateIdleSlate(m_rawFrameBuffer.data(), 1920, 1080);
    BroadcastState();
}

void CoreServer::Seek(int64_t ms)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_decoder.IsOpen()) {
        m_decoder.Seek(ms * 10000LL);
        m_status.positionMs = ms;
        m_pacer.Reset();
        if (m_status.state == PlaybackState::Paused) {
            int64_t pts = 0;
            bool eof = false;
            m_decoder.ReadFrame(m_rawFrameBuffer.data(), m_rawFrameBuffer.size(), &pts, &eof);
        }
        BroadcastState();
    }
}

void CoreServer::SetLooping(bool loop)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_status.isLooping = loop;
    m_settings.isLooping = loop;
    m_settings.Save();
    BroadcastState();
}

void CoreServer::SetMirror(bool mirror)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_status.isMirror = mirror;
    m_settings.isMirror = mirror;
    m_settings.Save();
    BroadcastState();
}

void CoreServer::SetWatermark(bool watermark)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_status.isWatermark = watermark;
    m_settings.isWatermark = watermark;
    m_settings.Save();
    BroadcastState();
}

void CoreServer::SetPassthrough(bool passthrough)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_status.isPassthrough = passthrough;
    if (passthrough) {
        m_status.state = PlaybackState::Passthrough;
        m_passthrough.StartCapture();
        m_status.selectedWebcam = WideToUtf8(m_passthrough.GetActiveDeviceName());
    } else {
        m_passthrough.StopCapture();
        m_status.state = m_decoder.IsOpen() ? PlaybackState::Playing : PlaybackState::Idle;
    }
    BroadcastState();
}

void CoreServer::AddToPlaylist(const std::wstring& path)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    bool wasEmpty = m_playlist.empty();
    m_playlist.push_back(path);
    if (wasEmpty) {
        m_playlistIndex = 0;
        LoadVideo(path, true);
    }
}

void CoreServer::ClearPlaylist()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_playlist.clear();
    m_playlistIndex = 0;
}

void CoreServer::NextClip()
{
    std::wstring nextFile;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_playlist.empty()) return;
        m_playlistIndex = (m_playlistIndex + 1) % m_playlist.size();
        nextFile = m_playlist[m_playlistIndex];
    }
    LoadVideo(nextFile, true);
}

void CoreServer::PrevClip()
{
    std::wstring prevFile;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_playlist.empty()) return;
        m_playlistIndex = (m_playlistIndex == 0) ? (m_playlist.size() - 1) : (m_playlistIndex - 1);
        prevFile = m_playlist[m_playlistIndex];
    }
    LoadVideo(prevFile, true);
}

size_t CoreServer::GetPlaylistCount() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_playlist.size();
}

CoreStatus CoreServer::GetStatus() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_status;
}

void CoreServer::BroadcastState()
{
    std::string json = m_status.ToJson();
    m_pipeServer.BroadcastMessage(json);
}

std::string CoreServer::HandleIpcMessage(const std::string& requestJson)
{
    std::string cmd = JsonGetString(requestJson, "cmd");

    if (cmd == "play") {
        Play();
    } else if (cmd == "pause") {
        Pause();
    } else if (cmd == "stop") {
        StopPlayback();
    } else if (cmd == "seek") {
        int64_t ms = JsonGetInt(requestJson, "ms", 0);
        Seek(ms);
    } else if (cmd == "open") {
        std::string fileUtf8 = JsonGetString(requestJson, "path");
        bool autoPlay = JsonGetBool(requestJson, "autoPlay", true);
        LoadVideo(Utf8ToWide(fileUtf8), autoPlay);
    } else if (cmd == "set_loop") {
        bool loop = JsonGetBool(requestJson, "loop", true);
        SetLooping(loop);
    } else if (cmd == "set_mirror") {
        bool mirror = JsonGetBool(requestJson, "mirror", false);
        SetMirror(mirror);
    } else if (cmd == "set_watermark") {
        bool wm = JsonGetBool(requestJson, "watermark", true);
        SetWatermark(wm);
    } else if (cmd == "set_passthrough") {
        bool pass = JsonGetBool(requestJson, "passthrough", false);
        SetPassthrough(pass);
    } else if (cmd == "get_webcams") {
        auto cams = WebcamPassthrough::EnumeratePhysicalWebcams();
        std::ostringstream ss;
        ss << "{\"type\":\"webcams\",\"list\":[";
        for (size_t i = 0; i < cams.size(); i++) {
            if (i > 0) ss << ",";
            ss << "\"" << WideToUtf8(cams[i].friendlyName) << "\"";
        }
        ss << "]}\n";
        return ss.str();
    } else if (cmd == "add_playlist") {
        std::string fileUtf8 = JsonGetString(requestJson, "path");
        AddToPlaylist(Utf8ToWide(fileUtf8));
    } else if (cmd == "clear_playlist") {
        ClearPlaylist();
    } else if (cmd == "next_clip") {
        NextClip();
    } else if (cmd == "prev_clip") {
        PrevClip();
    } else if (cmd == "shutdown") {
        HANDLE hStop = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\ReelCam_Core_StopEvent");
        if (hStop) {
            SetEvent(hStop);
            CloseHandle(hStop);
        }
        return "{\"type\":\"shutdown_ack\"}\n";
    }

    return GetStatus().ToJson();
}

void CoreServer::PlaybackLoop()
{
    LOG_INFO("CoreServer", "Playback loop started");
    m_pacer.Start();

    int64_t lastBroadcastPts = 0;

    while (m_running) {
        PlaybackState currentState;
        bool mirror = false;
        bool watermark = true;
        bool looping = true;
        size_t playlistCount = 0;

        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            currentState = m_status.state;
            mirror = m_status.isMirror;
            watermark = m_status.isWatermark;
            looping = m_status.isLooping;
            playlistCount = m_playlist.size();
        }

        int64_t pts100ns = m_pacer.GetCurrentPts100ns();

        if (currentState == PlaybackState::Playing) {
            bool eof = false;
            bool readOk = m_decoder.ReadFrame(m_rawFrameBuffer.data(), m_rawFrameBuffer.size(), &pts100ns, &eof);

            if (eof) {
                if (playlistCount > 1) {
                    LOG_INFO("CoreServer", "Playlist auto-advancing to next clip");
                    NextClip();
                } else if (looping) {
                    LOG_INFO("CoreServer", "Looping video to start");
                    m_decoder.Seek(0);
                    m_pacer.Reset();
                    {
                        std::lock_guard<std::mutex> lock(m_stateMutex);
                        m_status.positionMs = 0;
                    }
                    BroadcastState();
                } else {
                    LOG_INFO("CoreServer", "End of file reached, stopping");
                    {
                        std::lock_guard<std::mutex> lock(m_stateMutex);
                        m_status.state = PlaybackState::Paused;
                    }
                    BroadcastState();
                }
            } else if (readOk) {
                {
                    std::lock_guard<std::mutex> lock(m_stateMutex);
                    m_status.positionMs = pts100ns / 10000;
                }
                // Broadcast progress to UI every ~250ms during playback
                if (pts100ns - lastBroadcastPts >= 2500000LL || pts100ns < lastBroadcastPts) {
                    lastBroadcastPts = pts100ns;
                    BroadcastState();
                }
            }
        }
        else if (currentState == PlaybackState::Passthrough) {
            m_passthrough.ReadFrame(m_rawFrameBuffer.data(), m_rawFrameBuffer.size(), &pts100ns);
        }
        else if (currentState == PlaybackState::Idle) {
            VideoProcessor::GenerateIdleSlate(m_rawFrameBuffer.data(), 1920, 1080);
        }
        // If Paused, keep existing m_rawFrameBuffer pristine without modifying it in-place

        // Freshly copy raw frame to out buffer so effects do not corrupt/flip the raw frame repeatedly
        memcpy(m_outFrameBuffer.data(), m_rawFrameBuffer.data(), m_rawFrameBuffer.size());

        // Apply real-time effects in NV12 (only when active video/webcam is playing or paused)
        if (currentState != PlaybackState::Idle) {
            if (mirror) {
                VideoProcessor::ApplyHorizontalMirror(m_outFrameBuffer.data(), 1920, 1080);
            }
            if (watermark) {
                VideoProcessor::ApplyWatermarkBadge(m_outFrameBuffer.data(), 1920, 1080);
            }
        }

        // Publish to Shared Memory ring buffer for mfsource.dll
        m_shmProducer.PublishFrame(
            m_outFrameBuffer.data(),
            1920, 1080,
            1920, 1920,
            pts100ns,
            watermark,
            mirror
        );

        // High-precision frame pacing (30 or 60 fps)
        m_pacer.WaitNextFrame();
    }

    LOG_INFO("CoreServer", "Playback loop terminated");
}

} // namespace ReelCam
