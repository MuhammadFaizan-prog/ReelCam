# ReelCam Product Requirements Document (PRD)

## 1. Product Summary
**ReelCam** is a Windows desktop application that adds a transparent virtual camera device named `"ReelCam"` to Windows 11. Users pick a local video file (e.g., an MP4, MOV, MKV, or WebM recording) and ReelCam streams it as direct camera input into any application that enumerates video capture devices—including Google Chrome, Microsoft Edge, Zoom, Microsoft Teams, Google Meet, Discord, and OBS Studio.

Unlike complex multi-source broadcasting tools like OBS, ReelCam delivers a friction-free, single-window workflow: select a video file, click **Start Streaming**, and select `"ReelCam"` from any camera selector in under 10 seconds.

---

## 2. Target Users & Personas

### Persona A: The Webinar Presenter & Trainer (Marcus)
- **Role**: Technical evangelist, software trainer, corporate presenter.
- **Pain Point**: Live software demonstrations often experience network lag, service outages, or input mistakes. Presenting live via full screen-share degrades video quality and limits speaker-box layout.
- **Use Case**: Plays pre-recorded pristine 1080p demo clips directly through his meeting camera feed while narrating live. Seamlessly switches between his real webcam and the demo clip using global hotkeys.

### Persona B: The Remote Educator (Elena)
- **Role**: High school and university distance-learning teacher.
- **Pain Point**: Teaching identical lecture segments across multiple live sessions causes vocal fatigue and inconsistent lesson delivery. Setting up virtual OBS scenes is too difficult.
- **Use Case**: Queues 20-minute pre-recorded conceptual explanations in ReelCam, plays them during live Microsoft Teams classes, monitors progress, and switches back to live webcam for Q&A.

### Persona C: The Video App QA Engineer & Media Developer (Devon)
- **Role**: WebRTC software engineer, QA automation tester.
- **Pain Point**: Testing video calling software requires deterministic, repeatable, pixel-perfect camera feeds (known resolutions, frame rates, color patterns, and timestamps) across browsers.
- **Use Case**: Feeds standardized test clips (720p30, 1080p30, 1080p60) through ReelCam into automated browser suites to verify packet loss handling and frame rendering.

---

## 3. Goals & Non-Goals

### 3.1 Business & User Goals
- **Instant Availability**: From launching ReelCam and pressing Start, the `"ReelCam"` device enumerates in system camera selectors in < 10 seconds.
- **Fast Time to First Stream**: A first-time user achieves active video streaming into a target application in < 60 seconds after installer completion.
- **Rock-Solid Playback**: Smooth, glitch-free 30 to 60 minute continuous playback at 720p/1080p, 30fps (and 60fps where supported) with zero presentation timestamp drift.
- **Resource Efficiency**: CPU utilization strictly under 10% on a standard mid-range laptop (Intel Core i5 11th Gen / AMD Ryzen 5000 or newer) utilizing hardware D3D11 decoding.
- **Zero Configuration Setup**: Single-window UI with zero complex scene setup, canvas transformations, or manual audio bus configurations.

### 3.2 Non-Goals & Product Integrity Guardrails (Hard Inviolable Rules)
ReelCam is engineered exclusively as a transparent, honest productivity utility. The following guardrails are permanent:
1. **No Cloaking or Stealth**: ReelCam will never conceal its application window, process name (`reelcam_ui.exe`, `reelcam_core.exe`), taskbar button, or system tray presence.
2. **No Hardware Spoofing**: ReelCam will never disguise its virtual camera device as a physical USB webcam, spoof vendor/product IDs (VID/PID), or clone friendly hardware strings (e.g., "Logitech Brio").
3. **No Anti-Detection or Proctoring Evasion**: ReelCam will never bypass, hook, or tamper with detection logic used by browsers, enterprise meeting apps, proctoring software, or anti-cheat engines. If an app blocks virtual cameras, ReelCam will present a clear diagnostic message rather than trying to evade it.
4. **No Generative AI or Deepfakes**: ReelCam does not offer real-time facial swapping, biometric morphing, generative avatar animation, or automated lip-syncing.
5. **No Suppression of OS Indicators**: ReelCam will never suppress or tamper with the Windows 11 camera privacy LED or taskbar camera usage indicator.
6. **Pre-Recorded Badge**: A clear visual corner badge indicating `"Pre-recorded"` is enabled by default on all clean installations. Users may disable it for professional broadcasts, but the setting is never hidden or defaulted to off.

---

## 4. MVP Scope (v1)

1. **Virtual Camera Registration**: System-wide Media Foundation virtual camera device named `"ReelCam"` via `MFCreateVirtualCamera`.
2. **Media Source Compatibility**: Support for MP4, MOV, MKV, and WebM container formats using native OS decoders (H.264, VP9, AV1).
3. **Playback Engine**: Play, Pause, Seek (timeline scrub), Loop single clip, End-of-clip actions (stop on idle, loop, hold last frame).
4. **Idle Source Options**: Clean fallback frame (bundled default branded slate, custom still image PNG/JPEG, or live physical webcam passthrough).
5. **Output Formatting**: Selectable output resolution (720p, 1080p), frame rates (30 fps, 60 fps), scaling modes (Letterbox/Fit, Center Crop, Stretch), and horizontal Mirror toggle.
6. **Real-Time Preview**: Low-latency video preview window showing current playback timestamp, total duration, and frame drop metrics.
7. **System Tray & Taskbar Presence**: Dedicated system tray icon indicating live state (Streaming, Paused, Idle, Error) with context menu quick actions.
8. **Global Hotkeys**: System-wide key bindings for Play/Pause, Stop/Idle, and Quick-Switch to Physical Webcam.
9. **Persistent Settings**: Local storage for user configurations, window coordinates, hotkey bindings, and recent files list.
10. **Pre-recorded Badge**: Configurable corner watermark badge (default enabled).

---

## 5. User Stories & Acceptance Criteria

### Epics Overview
- **EP-1**: Virtual Device & Media Foundation Driver
- **EP-2**: Video Ingestion & Decoding Pipeline
- **EP-3**: Playback Engine & Pacing
- **EP-4**: UI & Preview Experience
- **EP-5**: System Integration & Hotkeys
- **EP-6**: Configuration & Packaging

---

### Epic 1: Virtual Device & Media Foundation Driver

#### US-01: Virtual Camera Registration
- **As a** user,
- **I want** ReelCam to register a virtual camera device named `"ReelCam"` in Windows 11,
- **So that** meeting and streaming apps can select it as a standard video capture device.
- **Acceptance Criteria**:
  - **AC-01.1**: When the ReelCam service initializes, `MFCreateVirtualCamera` registers a virtual device with the friendly name `"ReelCam"`.
  - **AC-01.2**: Within 10 seconds of clicking Start, `"ReelCam"` appears in Windows Settings > Bluetooth & Devices > Cameras.
  - **AC-01.3**: The device enumerates successfully in Google Chrome, Microsoft Edge, Zoom, Microsoft Teams, and Discord.
  - **AC-01.4**: If the camera is enabled but no video is playing, the camera feeds the designated Idle Source (never a blank/frozen frame or crash).

#### US-02: Format Negotiation
- **As a** consuming application (e.g., Zoom or Chrome),
- **I want** ReelCam to offer standard video media types,
- **So that** my app can negotiate a compatible video feed without conversion errors.
- **Acceptance Criteria**:
  - **AC-02.1**: The media source exposes `MFVideoFormat_NV12` as the primary format, with `MFVideoFormat_YUY2` and `MFVideoFormat_RGB32` supported.
  - **AC-02.2**: The media source offers 1280x720 and 1920x1080 resolutions at 30.0 fps and 60.0 fps.
  - **AC-02.3**: Format changes in ReelCam settings take effect immediately or on the next stream start without crashing the consumer.

---

### Epic 2: Video Ingestion & Decoding Pipeline

#### US-03: Local File Source Selection
- **As a** presenter,
- **I want** to select local video files via browse dialog, drag-and-drop, or recent files,
- **So that** I can quickly load my recorded presentation.
- **Acceptance Criteria**:
  - **AC-03.1**: Users can select video files with extensions `.mp4`, `.mov`, `.mkv`, and `.webm`.
  - **AC-03.2**: Dragging and dropping a supported video file onto the application window immediately loads it.
  - **AC-03.3**: The app maintains a recent files list of up to 10 entries with valid path, file name, duration, and thumbnail.
  - **AC-03.4**: If a selected file is moved, deleted, or inaccessible, the UI displays a clean error: `"Unable to access file: [Path]. Please check file permissions or location."`

#### US-04: Hardware-Accelerated Video Decoding
- **As a** user,
- **I want** video decoding to utilize Direct3D 11 hardware acceleration,
- **So that** ReelCam consumes minimal CPU and laptop battery life.
- **Acceptance Criteria**:
  - **AC-04.1**: Video decoding utilizes `MF_SOURCE_READER_D3D11_AWARE` with a shared D3D11 device.
  - **AC-04.2**: Decoding 1080p30 H.264 content maintains < 10% average CPU usage on standard test hardware.
  - **AC-04.3**: If hardware decoding is unavailable or fails, decoding falls back automatically to software decoding with an informational warning logged.

---

### Epic 3: Playback Engine & Frame Pacing

#### US-05: Accurate Frame Pacing
- **As a** viewer in a meeting,
- **I want** the video stream to maintain smooth, steady motion without stutter,
- **So that** the demo looks completely natural and professional.
- **Acceptance Criteria**:
  - **AC-05.1**: Frame delivery is paced using high-resolution QueryPerformanceCounter (QPC) clocks against presentation timestamps (PTS).
  - **AC-05.2**: No `Sleep()` calls are used for primary frame timing; pacing uses precise waitable timers or condition variables.
  - **AC-05.3**: Frame pacing jitter is maintained within ±2 milliseconds of target frame cadence (33.33ms for 30fps).
  - **AC-05.4**: Over a 60-minute continuous playback test, presentation timestamp drift remains under 40 milliseconds.

#### US-06: Playback Controls & Timeline Seeking
- **As a** presenter,
- **I want** standard playback controls (Play, Pause, Stop, Scrub),
- **So that** I can synchronize the video precisely with my spoken remarks.
- **Acceptance Criteria**:
  - **AC-06.1**: Clicking Pause immediately halts video progress while holding the current frame on the virtual camera feed.
  - **AC-06.2**: Scrubbing the timeline slider updates both the local preview and the virtual camera output within 100 milliseconds.
  - **AC-06.3**: When playback reaches the end of the file, the engine executes the configured end action: (a) Loop back to 00:00, (b) Freeze on final frame, or (c) Transition to Idle Source.

---

### Epic 4: UI & Preview Experience

#### US-07: Real-Time Preview Display
- **As a** presenter,
- **I want** to see an exact preview of what ReelCam is feeding to the virtual camera,
- **So that** I have complete confidence in what meeting participants are seeing.
- **Acceptance Criteria**:
  - **AC-07.1**: The main window hosts a high-performance preview viewport rendered via WinUI 3 `SwapChainPanel`.
  - **AC-07.2**: The preview displays the exact post-processed frame (including scaling, letterboxing, mirroring, and pre-recorded badge).
  - **AC-07.3**: Latency between the preview render and the virtual camera shared memory output is < 16 milliseconds.
  - **AC-07.4**: Overlaid time indicators show current elapsed time (`hh:mm:ss:ff`) and total clip duration.

#### US-08: Output Settings Customization
- **As a** user,
- **I want** to adjust output scaling, mirroring, and resolution,
- **So that** my video matches my meeting software's requirements.
- **Acceptance Criteria**:
  - **AC-08.1**: Scaling modes support `Fit (Letterbox/Pillarbox)`, `Fill (Center Crop)`, and `Stretch`.
  - **AC-08.2**: A "Mirror Video" toggle flips the horizontal orientation of the output frame in real time.
  - **AC-08.3**: Output resolution dropdown toggles between 1280x720 and 1920x1080.

#### US-09: Pre-Recorded Transparency Badge
- **As a** participant or meeting host,
- **I want** pre-recorded footage to be clearly identifiable,
- **So that** content transparency and authenticity standards are upheld.
- **Acceptance Criteria**:
  - **AC-09.1**: The pre-recorded badge displays an unobtrusive corner text overlay: `"● Pre-recorded"`.
  - **AC-09.2**: On clean installation, the badge is active (`Enabled = true`).
  - **AC-09.3**: The user can choose the corner position (Top-Left, Top-Right, Bottom-Left, Bottom-Right) and opacity (30% to 100%).
  - **AC-09.4**: The user can disable the badge in Settings; this preference persists across launches.

---

### Epic 5: System Integration & Hotkeys

#### US-10: Global Hotkeys
- **As a** presenter who is currently focused on a PowerPoint presentation or terminal,
- **I want** global keyboard shortcuts,
- **So that** I can trigger playback without bringing ReelCam to the foreground.
- **Acceptance Criteria**:
  - **AC-10.1**: ReelCam registers system-wide hotkeys via Win32 `RegisterHotKey`:
    - Toggle Play/Pause (Default: `Ctrl + Shift + F9`)
    - Stop / Switch to Idle (Default: `Ctrl + Shift + F10`)
    - Switch to Physical Webcam Passthrough (Default: `Ctrl + Shift + F11`)
  - **AC-10.2**: Hotkeys operate reliably even when third-party applications have input focus.
  - **AC-10.3**: Conflicting hotkeys show an in-app warning notification allowing reassignment.

#### US-11: System Tray Integration & Status
- **As a** user,
- **I want** ReelCam to have a system tray icon,
- **So that** I can monitor status and minimize window clutter while presenting.
- **Acceptance Criteria**:
  - **AC-11.1**: The system tray displays an icon whose color/badge reflects state:
    - Green dot: Live Streaming
    - Yellow dot: Paused
    - Gray dot: Idle (waiting or serving static frame)
    - Red dot: Pipeline Error
  - **AC-11.2**: Right-clicking the tray icon presents a context menu with options: Open ReelCam, Play/Pause, Switch to Webcam, Stop Streaming, Settings, and Exit.
  - **AC-11.3**: Minimizing the main window with "Minimize to Tray" enabled keeps the streaming engine active.

#### US-12: Physical Webcam Passthrough
- **As a** presenter,
- **I want** ReelCam to stream my physical webcam when paused or stopped,
- **So that** I don't have to switch camera devices in Zoom/Teams during a meeting.
- **Acceptance Criteria**:
  - **AC-12.1**: The user can select their real physical camera from an in-app dropdown.
  - **AC-12.2**: When switching to webcam mode, ReelCam captures the physical webcam via Media Foundation Capture Engine and relays frames into the `"ReelCam"` virtual device.
  - **AC-12.3**: Switching between video playback and webcam passthrough occurs in < 250 milliseconds without glitching or dropping connection in the consuming meeting app.

---

### Epic 6: Configuration, Stability & Packaging

#### US-13: Configuration Persistence
- **As a** user,
- **I want** my settings, window positions, and recent files remembered across sessions,
- **So that** I don't have to reconfigure the app each time I present.
- **Acceptance Criteria**:
  - **AC-13.1**: Settings are stored in `%LOCALAPPDATA%\ReelCam\settings.json`.
  - **AC-13.2**: Writing settings is performed atomically (temp file + replace) to eliminate file corruption risks during sudden power loss.
  - **AC-13.3**: Invalid or corrupt JSON files trigger an automatic fallback to defaults with a backup copy preserved.

#### US-14: Clean MSIX Installation & Uninstall
- **As a** Windows 11 user,
- **I want** a seamless, signed installer that registers the camera device and uninstalls cleanly,
- **So that** my system remains clean and stable.
- **Acceptance Criteria**:
  - **AC-14.1**: The app is packaged as a signed MSIX package.
  - **AC-14.2**: Installation requires no reboot and registers the virtual camera under the user's profile.
  - **AC-14.3**: Uninstallation cleanly unregisters the virtual camera device and COM classes, leaving no orphaned device entries in Windows Device Manager.

---

## 6. Success Metrics & Quality Gates

| Metric | Target | Measurement Method |
| :--- | :--- | :--- |
| **Time to Device Enumeration** | < 10 seconds | Clock time from Start click to appearance in Chrome camera picker |
| **Time to First Stream** | < 60 seconds | Fresh install on Windows 11 to active stream in browser |
| **CPU Utilization (1080p30)** | < 10% average | Windows Performance Monitor on reference Intel i5-1135G7 |
| **Timestamp Drift** | < 40 ms / 60 min | Continuous 60-min run comparing hardware QPC clock vs video PTS |
| **Dropped Frame Rate** | < 0.5% of total | Frame counter inside shared memory ring buffer |
| **Crash Recovery Time** | < 500 ms | Killing `reelcam_core.exe` mid-call transitions consumer to idle frame |
| **Verified Top-5 Apps** | 100% (5/5) | Verified working in Chrome, Edge, Zoom, Teams, and Discord |

---

## 7. Technical & System Constraints
1. **Operating System**: Windows 11 Build 22000 (21H2) or higher required (`MFCreateVirtualCamera` API requirement).
2. **Architecture**: 64-bit exclusively (x64 and ARM64 via emulation/native build).
3. **Privileges**: Runs entirely in user mode. No kernel drivers (`.sys`) are used.
4. **Licensing**: Uses native Windows Media Foundation decoders. No GPL libraries statically linked. H.265 excluded from v1 to prevent codec licensing liabilities.

---

## 8. Future Roadmap (v2+)
- **Scheduled Playback**: Schedule clips to automatically start at predetermined times.
- **Virtual Audio Cable / Mic Routing**: Synchronized virtual audio device streaming accompanying video audio.
- **Graphic Overlays & Lower Thirds**: Dynamic text overlays, presenter name cards, company logos, and SRT subtitle burn-in.
- **Multiple Virtual Devices**: Multiple independent virtual cameras (`ReelCam 1`, `ReelCam 2`) for multi-camera testing setups.
- **Automated Update Channel**: In-app notifications and background differential updates.
