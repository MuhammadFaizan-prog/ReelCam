#pragma once

#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "IpcProtocol.h"

namespace ReelCam {

struct AppSettings {
    bool isLooping = true;
    bool isMirror = false;
    bool isWatermark = true;
    int  aspectRatioMode = 0; // 0=Fit, 1=Fill, 2=Stretch
    std::string lastFile;
    std::vector<std::string> playlist;

    static std::wstring GetSettingsDir() {
        WCHAR appData[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData))) {
            std::wstring dir = std::wstring(appData) + L"\\ReelCam";
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir;
        }
        return L".";
    }

    static std::wstring GetSettingsFilePath() {
        return GetSettingsDir() + L"\\settings.json";
    }

    bool Load() {
        std::wstring path = GetSettingsFilePath();
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        isLooping = JsonGetBool(json, "loop", true);
        isMirror = JsonGetBool(json, "mirror", false);
        isWatermark = JsonGetBool(json, "watermark", true);
        aspectRatioMode = static_cast<int>(JsonGetInt(json, "aspectRatio", 0));
        lastFile = JsonGetString(json, "lastFile");

        return true;
    }

    bool Save() const {
        std::wstring path = GetSettingsFilePath();
        std::ofstream file(path);
        if (!file.is_open()) return false;

        file << "{\n"
             << "  \"loop\": " << (isLooping ? "true" : "false") << ",\n"
             << "  \"mirror\": " << (isMirror ? "true" : "false") << ",\n"
             << "  \"watermark\": " << (isWatermark ? "true" : "false") << ",\n"
             << "  \"aspectRatio\": " << aspectRatioMode << ",\n"
             << "  \"lastFile\": \"" << EscapeJson(lastFile) << "\"\n"
             << "}\n";
        file.close();
        return true;
    }
};

} // namespace ReelCam
