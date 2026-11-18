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

    NamedPipeClient() {
        m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }

    ~NamedPipeClient() {
        Disconnect();
        if (m_hStopEvent) {
            CloseHandle(m_hStopEvent);
            m_hStopEvent = nullptr;
        }
    }

    // Connects to named pipe.
    // timeoutMs = 0: non-blocking immediate attempt (for UI message loop / WM_TIMER).
    // timeoutMs > 0: waits/retries up to timeoutMs (for unit tests and startup).
    bool Connect(MessageCallback onAsyncMessage = nullptr, int timeoutMs = 1000) {
        if (m_connected && m_hPipe != INVALID_HANDLE_VALUE) return true;
        if (onAsyncMessage) m_onAsyncMessage = std::move(onAsyncMessage);

        DWORD startTime = GetTickCount();
        do {
            m_hPipe = CreateFileW(
                IPC_PIPE_NAME,
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                nullptr
            );

            if (m_hPipe != INVALID_HANDLE_VALUE) break;

            DWORD err = GetLastError();
            if (err == ERROR_PIPE_BUSY) {
                DWORD waitTime = (timeoutMs > 0) ? ((timeoutMs > 100) ? 100 : timeoutMs) : 1;
                WaitNamedPipeW(IPC_PIPE_NAME, waitTime);
            }

            if (timeoutMs <= 0) break;
            Sleep(20);
        } while (static_cast<int>(GetTickCount() - startTime) < timeoutMs);

        if (m_hPipe == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(m_hPipe, &mode, nullptr, nullptr);

        m_connected = true;
        if (m_hStopEvent) ResetEvent(m_hStopEvent);

        if (m_readThread.joinable()) {
            m_readThread.join();
        }

        if (m_onAsyncMessage) {
            m_readThread = std::thread(&NamedPipeClient::ReadLoop, this);
        }
        return true;
    }

    bool SendCommand(const std::string& cmdJson) {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        if (m_hPipe == INVALID_HANDLE_VALUE || !m_connected) return false;

        HANDLE hWriteEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!hWriteEvent) return false;

        OVERLAPPED ov{};
        ov.hEvent = hWriteEvent;

        DWORD bytesWritten = 0;
        BOOL ok = WriteFile(m_hPipe, cmdJson.data(), static_cast<DWORD>(cmdJson.length()), &bytesWritten, &ov);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                if (WaitForSingleObject(hWriteEvent, 1000) == WAIT_OBJECT_0) {
                    ok = GetOverlappedResult(m_hPipe, &ov, &bytesWritten, FALSE);
                } else {
                    CancelIo(m_hPipe);
                    ok = FALSE;
                }
            }
        }
        CloseHandle(hWriteEvent);
        return ok && (bytesWritten == cmdJson.length());
    }

    bool IsConnected() const { return m_connected; }

    void Disconnect() {
        m_connected = false;
        if (m_hStopEvent) {
            SetEvent(m_hStopEvent);
        }
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            CancelIo(m_hPipe);
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
        HANDLE hReadEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!hReadEvent) return;

        OVERLAPPED ov{};
        ov.hEvent = hReadEvent;

        while (m_connected) {
            DWORD bytesRead = 0;
            ResetEvent(hReadEvent);
            BOOL ok = ReadFile(m_hPipe, buffer, sizeof(buffer) - 1, &bytesRead, &ov);
            if (!ok) {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING) {
                    HANDLE waitHandles[2] = { hReadEvent, m_hStopEvent };
                    DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                    if (waitRes == WAIT_OBJECT_0) {
                        if (!GetOverlappedResult(m_hPipe, &ov, &bytesRead, FALSE)) {
                            m_connected = false;
                            break;
                        }
                    } else {
                        // Stop signaled
                        CancelIo(m_hPipe);
                        break;
                    }
                } else {
                    m_connected = false;
                    break;
                }
            }

            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::string msg(buffer, bytesRead);
                if (m_onAsyncMessage) {
                    m_onAsyncMessage(msg);
                }
            }
        }

        CloseHandle(hReadEvent);
    }

    std::atomic<bool> m_connected{false};
    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
    HANDLE m_hStopEvent = nullptr;
    std::mutex m_pipeMutex;
    std::thread m_readThread;
    MessageCallback m_onAsyncMessage;
};

} // namespace ReelCam
