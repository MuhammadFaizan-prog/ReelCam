# ReelCam Data Schemas & Local Storage Architecture (Schema)

## 1. Storage Architecture Overview

> [!NOTE]
> **No Database Requirement**: ReelCam operates entirely without a relational or embedded database engine (no SQLite, no SQL Server, no NoSQL). All persistent state is strictly managed through local, version-controlled JSON configuration files, Windows Registry keys (for COM registration), and structured append-only text logs.

### Storage Directory Layout
```
%LOCALAPPDATA%\ReelCam\
  ├── settings.json               # Main application configuration
  ├── settings.json.bak           # Automatic recovery backup
  ├── playlists\                  # User-saved playlist files (.reelcam-playlist.json)
  │     └── Default.json
  └── logs\                       # Structured application logs (rotated daily)
        ├── reelcam_ui_2026-11-18.log
        └── reelcam_core_2026-11-18.log
```

---

## 2. Settings File Schema (`settings.json`)

- **Location**: `%LOCALAPPDATA%\ReelCam\settings.json`
- **Encoding**: UTF-8 without BOM
- **Persistence Strategy**: Atomic write via temporary file write + `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING)`.

### 2.1 Complete JSON Schema Definition

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "ReelCamSettings",
  "type": "object",
  "required": [
    "schemaVersion",
    "output",
    "badge",
    "idleSource",
    "hotkeys",
    "window",
    "recentFiles"
  ],
  "properties": {
    "schemaVersion": {
      "type": "integer",
      "minimum": 1,
      "description": "Monotonically increasing version integer used for schema migration."
    },
    "output": {
      "type": "object",
      "required": ["resolution", "frameRate", "scalingMode", "mirror"],
      "properties": {
        "resolution": {
          "type": "string",
          "enum": ["720p", "1080p"],
          "default": "1080p",
          "description": "Stream resolution: 720p (1280x720) or 1080p (1920x1080)."
        },
        "frameRate": {
          "type": "integer",
          "enum": [30, 60],
          "default": 30,
          "description": "Target presentation frame rate in fps."
        },
        "scalingMode": {
          "type": "string",
          "enum": ["fit", "crop", "stretch"],
          "default": "fit",
          "description": "Aspect ratio scaling mode: fit (letterbox/pillarbox), crop (center crop), or stretch."
        },
        "mirror": {
          "type": "boolean",
          "default": false,
          "description": "Horizontal flip transformation applied to output video."
        }
      }
    },
    "badge": {
      "type": "object",
      "required": ["enabled", "position", "opacity"],
      "properties": {
        "enabled": {
          "type": "boolean",
          "default": true,
          "description": "Displays the transparent Pre-recorded corner watermark. Enabled by default."
        },
        "position": {
          "type": "string",
          "enum": ["top-left", "top-right", "bottom-left", "bottom-right"],
          "default": "bottom-right",
          "description": "Corner quadrant for the watermark badge."
        },
        "opacity": {
          "type": "number",
          "minimum": 0.2,
          "maximum": 1.0,
          "default": 0.75,
          "description": "Watermark overlay opacity blending factor."
        }
      }
    },
    "idleSource": {
      "type": "object",
      "required": ["type"],
      "properties": {
        "type": {
          "type": "string",
          "enum": ["defaultSlate", "customImage", "black", "webcamPassthrough"],
          "default": "defaultSlate",
          "description": "Fallback stream source when no media file is actively playing."
        },
        "customImagePath": {
          "type": ["string", "null"],
          "default": null,
          "description": "Absolute filesystem path to custom PNG or JPEG idle still image."
        },
        "preferredWebcamId": {
          "type": ["string", "null"],
          "default": null,
          "description": "DirectShow / Media Foundation symbolic device link for physical camera passthrough."
        }
      }
    },
    "hotkeys": {
      "type": "object",
      "required": ["togglePlayPause", "stopStreaming", "switchWebcam"],
      "properties": {
        "togglePlayPause": {
          "type": "string",
          "default": "Ctrl+Shift+F9",
          "description": "System-wide global shortcut to toggle play/pause."
        },
        "stopStreaming": {
          "type": "string",
          "default": "Ctrl+Shift+F10",
          "description": "System-wide global shortcut to stop playback and switch to idle."
        },
        "switchWebcam": {
          "type": "string",
          "default": "Ctrl+Shift+F11",
          "description": "System-wide global shortcut to switch between video and real webcam."
        }
      }
    },
    "window": {
      "type": "object",
      "required": ["x", "y", "width", "height", "isMaximized", "minimizeToTray"],
      "properties": {
        "x": { "type": "integer", "default": 100 },
        "y": { "type": "integer", "default": 100 },
        "width": { "type": "integer", "default": 960, "minimum": 640 },
        "height": { "type": "integer", "default": 640, "minimum": 480 },
        "isMaximized": { "type": "boolean", "default": false },
        "minimizeToTray": { "type": "boolean", "default": true }
      }
    },
    "playback": {
      "type": "object",
      "required": ["defaultLoopMode", "endOfClipAction"],
      "properties": {
        "defaultLoopMode": {
          "type": "boolean",
          "default": false,
          "description": "Automatically loop video playback upon reaching end of file."
        },
        "endOfClipAction": {
          "type": "string",
          "enum": ["stopOnIdle", "freezeLastFrame", "loop"],
          "default": "stopOnIdle",
          "description": "Behavior when non-looping video reaches the end."
        }
      }
    },
    "recentFiles": {
      "type": "array",
      "maxItems": 10,
      "items": {
        "type": "object",
        "required": ["filePath", "fileName", "durationMs", "lastUsed"],
        "properties": {
          "filePath": { "type": "string" },
          "fileName": { "type": "string" },
          "durationMs": { "type": "integer", "minimum": 0 },
          "lastUsed": { "type": "string", "format": "date-time" }
        }
      }
    }
  }
}
```

### 2.2 Default Factory `settings.json` Example

```json
{
  "schemaVersion": 1,
  "output": {
    "resolution": "1080p",
    "frameRate": 30,
    "scalingMode": "fit",
    "mirror": false
  },
  "badge": {
    "enabled": true,
    "position": "bottom-right",
    "opacity": 0.75
  },
  "idleSource": {
    "type": "defaultSlate",
    "customImagePath": null,
    "preferredWebcamId": null
  },
  "hotkeys": {
    "togglePlayPause": "Ctrl+Shift+F9",
    "stopStreaming": "Ctrl+Shift+F10",
    "switchWebcam": "Ctrl+Shift+F11"
  },
  "window": {
    "x": 150,
    "y": 150,
    "width": 960,
    "height": 640,
    "isMaximized": false,
    "minimizeToTray": true
  },
  "playback": {
    "defaultLoopMode": false,
    "endOfClipAction": "stopOnIdle"
  },
  "recentFiles": []
}
```

---

## 3. IPC Message Schemas (Named Pipe Protocol)

All inter-process communication between `reelcam_ui.exe` and `reelcam_core.exe` uses a structured JSON envelope:

### 3.1 Base IPC Envelope Schema

```json
{
  "type": "object",
  "required": ["version", "messageId", "type", "name", "payload"],
  "properties": {
    "version": { "type": "integer", "const": 1 },
    "messageId": { "type": "string", "format": "uuid" },
    "type": { "type": "string", "enum": ["Command", "Response", "Notification"] },
    "name": { "type": "string" },
    "payload": { "type": "object" }
  }
}
```

### 3.2 Command & Notification Payloads

#### `LoadFile` (Command)
```json
{
  "version": 1,
  "messageId": "c88f98ec-6725-4c07-ba7d-3d4df43685f0",
  "type": "Command",
  "name": "LoadFile",
  "payload": {
    "filePath": "C:\\Presentations\\Demo.mp4",
    "autoPlay": false
  }
}
```

#### `LoadFileResponse` (Response)
```json
{
  "version": 1,
  "messageId": "c88f98ec-6725-4c07-ba7d-3d4df43685f0",
  "type": "Response",
  "name": "LoadFileResponse",
  "payload": {
    "success": true,
    "durationMs": 862400,
    "videoWidth": 1920,
    "videoHeight": 1080,
    "frameRate": 30.0,
    "codec": "H.264",
    "errorCode": null,
    "errorMessage": null
  }
}
```

#### `StateChangedNotification` (Periodic Notification, 10 Hz)
```json
{
  "version": 1,
  "messageId": "2c94314e-4f32-4e4b-9b7e-976d8b948df9",
  "type": "Notification",
  "name": "StateChangedNotification",
  "payload": {
    "state": "Playing",
    "currentPtsMs": 142300,
    "durationMs": 862400,
    "droppedFrames": 0,
    "currentFps": 30.0,
    "isHardwareDecoded": true
  }
}
```

---

## 4. COM Registration Registry Schema

ReelCam registers its user-mode virtual camera Media Source in the Windows Registry under the calling user's hive (`HKEY_CURRENT_USER`):

```ini
[HKEY_CURRENT_USER\Software\Classes\CLSID\{E7B1A280-9A23-4560-BD92-358F58A009C1}]
@="ReelCam Virtual Camera Media Source"

[HKEY_CURRENT_USER\Software\Classes\CLSID\{E7B1A280-9A23-4560-BD92-358F58A009C1}\InprocServer32]
@="C:\\Program Files\\ReelCam\\bin\\mfsource.dll"
"ThreadingModel"="Both"
```

---

## 5. Log File Format (`logs\`)

- **Rotation Policy**: New file created daily or whenever the active file exceeds 10 MB. Maximum 7 daily retention files kept.
- **Log Format**: JSON Lines (JSONL) with ISO-8601 timestamps and scrubbed PII.

```json
{"timestamp":"2026-11-18T19:42:00.124Z","level":"INFO","process":"reelcam_core","threadId":4912,"component":"Decoder","message":"D3D11 Video Decoder initialized successfully for H.264 stream (1920x1080@30.0fps)"}
{"timestamp":"2026-11-18T19:42:00.138Z","level":"INFO","process":"reelcam_core","threadId":4912,"component":"Pacer","message":"High-resolution waitable timer initialized. QPC Frequency: 10000000 Hz"}
```

---

## 6. Migration & Ownership Strategy

### 6.1 Schema Version Migration
- The application compares the integer `schemaVersion` on load:
  - If `savedVersion == currentVersion`: Parse directly.
  - If `savedVersion < currentVersion`: Sequentially apply upgrade migration functions (`Migrate_v1_to_v2`, etc.).
  - If `savedVersion > currentVersion` (user downgraded app): Preserve unknown fields; clamp recognized fields to defaults.

### 6.2 Process Ownership & Concurrency Rules
- **`settings.json`**: Exclusively owned and written by `reelcam_ui.exe`. `reelcam_core.exe` reads settings only when passed directly via IPC commands.
- **`logs\`**: Each process writes strictly to its own dedicated file (`reelcam_ui_{date}.log` and `reelcam_core_{date}.log`), avoiding inter-process file locking conflicts.
