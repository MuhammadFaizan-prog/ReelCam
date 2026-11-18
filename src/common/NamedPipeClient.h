#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "IpcProtocol.h"
#include "Logging.h"

namespace ReelCam {

class NamedPipeClient {
public:
    using MessageCallback = std::function<void(const std::string& msg)>;

    NamedPipeClient() = default;
    ~NamedPipeClient() { Disconnect(); }

    bool Connect(MessageCallback onAsyncMessage = nullptr) {
        if (m_connected && m_hPipe != INVALID_HANDLE_VALUE) return true;
        if (onAsyncMessage) m_onAsyncMessage = std::move(onAsyncMessage);

        // Try connecting to pipe
        for (int i = 0; i < 20; i++) {
            m_hPipe = CreateFileW(
                IPC_PIPE_NAME,
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
            );

            if (m_hPipe != INVALID_HANDLE_VALUE) break;

            if (GetLastError() == ERROR_PIPE_BUSY) {
                if (WaitNamedPipeW(IPC_PIPE_NAME, 1000)) continue;
            }
            Sleep(100);
        }

        if (m_hPipe == INVALID_HANDLE_VALUE) return false;

        DWORD mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(m_hPipe, &mode, nullptr, nullptr);

        m_connected = true;
        if (m_readThread.joinable()) m_readThread.join();
        if (m_onAsyncMessage) {
            m_readThread = std::thread(&NamedPipeClient::ReadLoop, this);
        }
        return true;
    }

    bool SendCommand(const std::string& cmdJson) {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        if (m_hPipe == INVALID_HANDLE_VALUE || !m_connected) return false;

        DWORD bytesWritten = 0;
        BOOL ok = WriteFile(m_hPipe, cmdJson.data(), static_cast<DWORD>(cmdJson.length()), &bytesWritten, nullptr);
        return ok && (bytesWritten == cmdJson.length());
    }

    bool IsConnected() const { return m_connected; }

    void Disconnect() {
        m_connected = false;
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
        if (m_readThread.joinable()) {
            m_readThread.join();
        }
    }

private:
    void ReadLoop() {
        char buffer[4096];
        while (m_connected) {
            DWORD bytesRead = 0;
            BOOL success = ReadFile(m_hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
            if (!success || bytesRead == 0) {
                m_connected = false;
                break;
            }

            buffer[bytesRead] = '\0';
            std::string msg(buffer, bytesRead);
            if (m_onAsyncMessage) {
                m_onAsyncMessage(msg);
            }
        }
    }

    std::atomic<bool> m_connected{false};
    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
    std::mutex m_pipeMutex;
    std::thread m_readThread;
    MessageCallback m_onAsyncMessage;
};

} // namespace ReelCam
