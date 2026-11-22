#pragma once
// ReelCam simple logging utility.
// Writes structured messages to stderr and OutputDebugString.
// File-based logging deferred to Phase 2 UI integration.

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

namespace ReelCam {

enum class LogLevel { Debug, Info, Warn, Error };

inline const char* LogLevelStr(LogLevel level)
{
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        default:              return "???";
    }
}

inline void Log(LogLevel level, const char* component, const char* fmt, ...)
{
    char userMsg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(userMsg, sizeof(userMsg), fmt, args);
    va_end(args);

    char fullMsg[1280];
    snprintf(fullMsg, sizeof(fullMsg),
        "[ReelCam][%s][%s] %s\n",
        LogLevelStr(level), component, userMsg);

    OutputDebugStringA(fullMsg);
    fprintf(stderr, "%s", fullMsg);
    FILE* f = nullptr;
    if (fopen_s(&f, "C:\\Users\\DELL\\Downloads\\cam\\reelcam.log", "a") == 0 && f) {
        fprintf(f, "%s", fullMsg);
        fclose(f);
    }
}

#define LOG_DEBUG(comp, ...) ReelCam::Log(ReelCam::LogLevel::Debug, comp, __VA_ARGS__)
#define LOG_INFO(comp, ...)  ReelCam::Log(ReelCam::LogLevel::Info,  comp, __VA_ARGS__)
#define LOG_WARN(comp, ...)  ReelCam::Log(ReelCam::LogLevel::Warn,  comp, __VA_ARGS__)
#define LOG_ERROR(comp, ...) ReelCam::Log(ReelCam::LogLevel::Error, comp, __VA_ARGS__)

} // namespace ReelCam
