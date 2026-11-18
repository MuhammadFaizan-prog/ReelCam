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

    NamedPipeServer() {
        m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }

    ~NamedPipeServer() {
        Stop();
        if (m_hStopEvent) {
            CloseHandle(m_hStopEvent);
            m_hStopEvent = nullptr;
        }
    }

    bool Start(MessageCallback onMessage) {
        m_onMessage = std::move(onMessage);
        m_running = true;
        if (m_hStopEvent) ResetEvent(m_hStopEvent);
        m_serverThread = std::thread(&NamedPipeServer::ServerLoop, this);
        return true;
    }

    void Stop() {
        m_running = false;
        if (m_hStopEvent) {
            SetEvent(m_hStopEvent);
        }
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            CancelIo(m_hPipe);
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
            HANDLE hWriteEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!hWriteEvent) return;

            OVERLAPPED ov{};
            ov.hEvent = hWriteEvent;

            DWORD bytesWritten = 0;
            BOOL ok = WriteFile(m_hPipe, msg.data(), static_cast<DWORD>(msg.length()), &bytesWritten, &ov);
            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                if (WaitForSingleObject(hWriteEvent, 1000) == WAIT_OBJECT_0) {
                    GetOverlappedResult(m_hPipe, &ov, &bytesWritten, FALSE);
                } else {
                    CancelIo(m_hPipe);
                }
            }
            CloseHandle(hWriteEvent);
        }
    }

private:
    void ServerLoop() {
        while (m_running) {
            m_hPipe = CreateNamedPipeW(
                IPC_PIPE_NAME,
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                4096,
                4096,
                5000,
                nullptr
            );

            if (m_hPipe == INVALID_HANDLE_VALUE) {
                if (!m_running) break;
                Sleep(200);
                continue;
            }

            HANDLE hConnectEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            OVERLAPPED ovConnect{};
            ovConnect.hEvent = hConnectEvent;

            BOOL connected = ConnectNamedPipe(m_hPipe, &ovConnect);
            if (!connected) {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING) {
                    HANDLE waitEvents[2] = { hConnectEvent, m_hStopEvent };
                    DWORD waitRes = WaitForMultipleObjects(2, waitEvents, FALSE, INFINITE);
                    if (waitRes == WAIT_OBJECT_0) {
                        connected = TRUE;
                    } else {
                        // Stop signaled
                        CancelIo(m_hPipe);
                        CloseHandle(hConnectEvent);
                        CloseHandle(m_hPipe);
                        m_hPipe = INVALID_HANDLE_VALUE;
                        break;
                    }
                } else if (err == ERROR_PIPE_CONNECTED) {
                    connected = TRUE;
                }
            }
            CloseHandle(hConnectEvent);

            if (!connected) {
                CloseHandle(m_hPipe);
                m_hPipe = INVALID_HANDLE_VALUE;
                continue;
            }

            m_clientConnected = true;
            LOG_INFO("IPC", "Client connected to Named Pipe");

            char buffer[4096];
            HANDLE hReadEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            OVERLAPPED ovRead{};
            ovRead.hEvent = hReadEvent;

            while (m_running && m_clientConnected) {
                DWORD bytesRead = 0;
                ResetEvent(hReadEvent);
                BOOL success = ReadFile(m_hPipe, buffer, sizeof(buffer) - 1, &bytesRead, &ovRead);
                if (!success) {
                    DWORD err = GetLastError();
                    if (err == ERROR_IO_PENDING) {
                        HANDLE waitEvents[2] = { hReadEvent, m_hStopEvent };
                        DWORD waitRes = WaitForMultipleObjects(2, waitEvents, FALSE, INFINITE);
                        if (waitRes == WAIT_OBJECT_0) {
                            if (!GetOverlappedResult(m_hPipe, &ovRead, &bytesRead, FALSE)) {
                                break;
                            }
                        } else {
                            CancelIo(m_hPipe);
                            break;
                        }
                    } else {
                        if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                            LOG_INFO("IPC", "Client disconnected from Named Pipe");
                        }
                        break;
                    }
                }

                if (bytesRead == 0) break;

                buffer[bytesRead] = '\0';
                std::string request(buffer, bytesRead);

                if (m_onMessage) {
                    std::string response = m_onMessage(request);
                    if (!response.empty()) {
                        std::lock_guard<std::mutex> lock(m_pipeMutex);
                        HANDLE hWriteEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                        if (hWriteEvent) {
                            OVERLAPPED ovWrite{};
                            ovWrite.hEvent = hWriteEvent;
                            DWORD bytesWritten = 0;
                            BOOL ok = WriteFile(m_hPipe, response.data(), static_cast<DWORD>(response.length()), &bytesWritten, &ovWrite);
                            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                                if (WaitForSingleObject(hWriteEvent, 1000) == WAIT_OBJECT_0) {
                                    GetOverlappedResult(m_hPipe, &ovWrite, &bytesWritten, FALSE);
                                } else {
                                    CancelIo(m_hPipe);
                                }
                            }
                            CloseHandle(hWriteEvent);
                        }
                    }
                }
            }

            CloseHandle(hReadEvent);
            m_clientConnected = false;
            DisconnectNamedPipe(m_hPipe);
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
    }

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_clientConnected{false};
    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
    HANDLE m_hStopEvent = nullptr;
    std::mutex m_pipeMutex;
    std::thread m_serverThread;
    MessageCallback m_onMessage;
};

} // namespace ReelCam
