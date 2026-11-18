#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include "IpcProtocol.h"
#include "Logging.h"

namespace ReelCam {

class NamedPipeServer {
public:
    using MessageCallback = std::function<std::string(const std::string& msg)>;

    NamedPipeServer() = default;
    ~NamedPipeServer() { Stop(); }

    bool Start(MessageCallback onMessage) {
        m_onMessage = std::move(onMessage);
        m_running = true;
        m_serverThread = std::thread(&NamedPipeServer::ServerLoop, this);
        return true;
    }

    void Stop() {
        m_running = false;
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
    }

    void BroadcastMessage(const std::string& msg) {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        if (m_hPipe != INVALID_HANDLE_VALUE && m_clientConnected) {
            DWORD bytesWritten = 0;
            WriteFile(m_hPipe, msg.data(), static_cast<DWORD>(msg.length()), &bytesWritten, nullptr);
            FlushFileBuffers(m_hPipe);
        }
    }

private:
    void ServerLoop() {
        while (m_running) {
            // Create Named Pipe with restricted DACL (current user session)
            m_hPipe = CreateNamedPipeW(
                IPC_PIPE_NAME,
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                1,              // Max 1 client instance for v1
                4096,           // Out buffer
                4096,           // In buffer
                5000,           // Default timeout ms
                nullptr         // Default security attribute
            );

            if (m_hPipe == INVALID_HANDLE_VALUE) {
                if (!m_running) break;
                Sleep(200);
                continue;
            }

            BOOL connected = ConnectNamedPipe(m_hPipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (!connected) {
                CloseHandle(m_hPipe);
                m_hPipe = INVALID_HANDLE_VALUE;
                continue;
            }

            m_clientConnected = true;
            LOG_INFO("IPC", "Client connected to Named Pipe");

            char buffer[4096];
            while (m_running && m_clientConnected) {
                DWORD bytesRead = 0;
                BOOL success = ReadFile(m_hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
                if (!success || bytesRead == 0) {
                    DWORD err = GetLastError();
                    if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                        LOG_INFO("IPC", "Client disconnected from Named Pipe");
                    }
                    break;
                }

                buffer[bytesRead] = '\0';
                std::string request(buffer, bytesRead);

                if (m_onMessage) {
                    std::string response = m_onMessage(request);
                    if (!response.empty()) {
                        std::lock_guard<std::mutex> lock(m_pipeMutex);
                        DWORD bytesWritten = 0;
                        WriteFile(m_hPipe, response.data(), static_cast<DWORD>(response.length()), &bytesWritten, nullptr);
                        FlushFileBuffers(m_hPipe);
                    }
                }
            }

            m_clientConnected = false;
            DisconnectNamedPipe(m_hPipe);
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
    }

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_clientConnected{false};
    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
    std::mutex m_pipeMutex;
    std::thread m_serverThread;
    MessageCallback m_onMessage;
};

} // namespace ReelCam
