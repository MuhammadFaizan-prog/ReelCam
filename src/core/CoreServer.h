#pragma once

#include <windows.h>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>

#include "SharedMemory.h"
#include "NamedPipeServer.h"
#include "IpcProtocol.h"
#include "FramePacer.h"
#include "Decoder.h"
#include "VideoProcessor.h"
#include "WebcamPassthrough.h"
#include "Settings.h"

namespace ReelCam {

class CoreServer {
public:
    CoreServer();
    ~CoreServer();

    bool Start();
    void Stop();

    // IPC Command Handlers
    std::string HandleIpcMessage(const std::string& requestJson);

    // Direct API (for testing or embedding)
    bool LoadVideo(const std::wstring& path, bool autoPlay = true);
    void Play();
    void Pause();
    void StopPlayback();
    void Seek(int64_t ms);
    void SetLooping(bool loop);
    void SetMirror(bool mirror);
    void SetWatermark(bool watermark);
    void SetPassthrough(bool passthrough);

    // Playlist API
    void AddToPlaylist(const std::wstring& path);
    void ClearPlaylist();
    void NextClip();
    void PrevClip();
    size_t GetPlaylistCount() const;

    CoreStatus GetStatus() const;

private:
    void PlaybackLoop();
    void BroadcastState();

    std::atomic<bool>     m_running{false};
    std::thread           m_workerThread;
    mutable std::mutex    m_stateMutex;

    CoreStatus            m_status;
    SharedMemoryProducer  m_shmProducer;
    NamedPipeServer       m_pipeServer;
    FramePacer            m_pacer;
    Decoder               m_decoder;
    WebcamPassthrough     m_passthrough;
    AppSettings           m_settings;

    std::vector<std::wstring> m_playlist;
    size_t                    m_playlistIndex = 0;

    std::vector<uint8_t>  m_frameBuffer; // 1920x1080 NV12 buffer
};

} // namespace ReelCam
