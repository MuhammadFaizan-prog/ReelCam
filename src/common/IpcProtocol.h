#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace ReelCam {

static constexpr const wchar_t* IPC_PIPE_NAME = L"\\\\.\\pipe\\ReelCam_IPC";

enum class PlaybackState : uint32_t {
    Idle = 0,
    Playing = 1,
    Paused = 2,
    Passthrough = 3,
    Error = 4
};

inline const char* PlaybackStateToString(PlaybackState state) {
    switch (state) {
        case PlaybackState::Idle: return "Idle";
        case PlaybackState::Playing: return "Playing";
        case PlaybackState::Paused: return "Paused";
        case PlaybackState::Passthrough: return "Passthrough";
        case PlaybackState::Error: return "Error";
        default: return "Unknown";
    }
}

inline PlaybackState StringToPlaybackState(const std::string& str) {
    if (str == "Playing") return PlaybackState::Playing;
    if (str == "Paused") return PlaybackState::Paused;
    if (str == "Passthrough") return PlaybackState::Passthrough;
    if (str == "Error") return PlaybackState::Error;
    return PlaybackState::Idle;
}

struct CoreStatus {
    PlaybackState state = PlaybackState::Idle;
    std::string currentFilePath;
    int64_t positionMs = 0;
    int64_t durationMs = 0;
    uint32_t width = 1920;
    uint32_t height = 1080;
    double fps = 30.0;
    bool isLooping = true;
    bool isMirror = false;
    bool isWatermark = true;
    bool isPassthrough = false;
    std::string selectedWebcam;
    std::string errorMessage;

    std::string ToJson() const {
        std::ostringstream ss;
        ss << "{"
           << "\"type\":\"status\","
           << "\"state\":\"" << PlaybackStateToString(state) << "\","
           << "\"file\":\"" << EscapeJson(currentFilePath) << "\","
           << "\"positionMs\":" << positionMs << ","
           << "\"durationMs\":" << durationMs << ","
           << "\"width\":" << width << ","
           << "\"height\":" << height << ","
           << "\"fps\":" << std::fixed << std::setprecision(2) << fps << ","
           << "\"isLooping\":" << (isLooping ? "true" : "false") << ","
           << "\"isMirror\":" << (isMirror ? "true" : "false") << ","
           << "\"isWatermark\":" << (isWatermark ? "true" : "false") << ","
           << "\"isPassthrough\":" << (isPassthrough ? "true" : "false") << ","
           << "\"webcam\":\"" << EscapeJson(selectedWebcam) << "\","
           << "\"error\":\"" << EscapeJson(errorMessage) << "\""
           << "}\n";
        return ss.str();
    }

private:
    static std::string EscapeJson(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out += c;
        }
        return out;
    }
};

// Helper to extract JSON string value: "key":"value"
inline std::string JsonGetString(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\":\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    pos += pattern.length();
    size_t endPos = json.find("\"", pos);
    if (endPos == std::string::npos) return "";
    return json.substr(pos, endPos - pos);
}

// Helper to extract JSON int value: "key":1234
inline int64_t JsonGetInt(const std::string& json, const std::string& key, int64_t defVal = 0) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return defVal;
    pos += pattern.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    try {
        return std::stoll(json.substr(pos));
    } catch (...) {
        return defVal;
    }
}

// Helper to extract JSON bool value: "key":true
inline bool JsonGetBool(const std::string& json, const std::string& key, bool defVal = false) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return defVal;
    pos += pattern.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (json.compare(pos, 4, "true") == 0) return true;
    if (json.compare(pos, 5, "false") == 0) return false;
    return defVal;
}

} // namespace ReelCam
