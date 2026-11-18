# ReelCam Project Structure & Module Boundaries (Structure)

## 1. Top-Level Directory Tree

```
c:\Users\DELL\Downloads\cam\
├── PRD.md                         # Product Requirements Document
├── Techspec.md                    # Technical Architecture & Systems Specification
├── Appflow.md                     # User Journeys, UI Wireframes & State Diagrams
├── Schema.md                      # Local Storage, Settings & IPC JSON Schemas
├── ImplementationPlan.md          # 6-Week Phased Engineering Roadmap
├── Tracker.md                     # Story Tracker, Decision Log & Project Memory
├── Rules.md                       # Engineering Standards & Non-Negotiable Guardrails
├── Security.md                    # Threat Model, IPC Security & Anti-Abuse Standards
├── Structure.md                   # Repository Directory Structure & Module Boundaries
├── CMakeLists.txt                 # Root CMake Build Configuration (C++20)
├── vcpkg.json                     # Native C++ Dependency Manifest (WIL, GTest)
├── Directory.Build.props          # Global MSBuild Properties for C# Projects
├── .editorconfig                  # Code formatting conventions (C++, C#, XAML)
├── .gitignore                     # Git ignore rules
├── README.md                      # Developer Quickstart & Build Instructions
├── LICENSE                        # Apache 2.0 / Commercial License
├── SECURITY.md                    # Vulnerability Disclosure & Policy
│
├── src/                           # Application Source Code
│   ├── core/                      # Native Playback Engine Process (C++20)
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp               # reelcam_core.exe entry point
│   │   ├── Decoder.h / .cpp       # MF Source Reader D3D11 HW decode
│   │   ├── FramePacer.h / .cpp    # High-precision QPC waitable timer pacing
│   │   ├── VideoProcessor.h / .cpp# D3D11 scaling, mirror, watermark blending
│   │   ├── SharedMemoryProducer.h # Shared DXGI texture ring buffer writer
│   │   ├── PlaybackEngine.h / .cpp# High-level playback coordinator
│   │   ├── StateMachine.h / .cpp  # Core state transition controller
│   │   └── CommandHandler.h / .cpp# IPC request handler
│   │
│   ├── mfsource/                  # Media Foundation Virtual Camera COM DLL (C++20)
│   │   ├── CMakeLists.txt
│   │   ├── dllmain.cpp            # DllGetClassObject, DllCanUnloadNow, DllRegister
│   │   ├── ReelCamMediaSource.h   # IMFMediaSource implementation
│   │   ├── ReelCamMediaStream.h   # IMFMediaStream frame delivery
│   │   ├── ReelCamActivate.h     # IMFActivate object provider
│   │   ├── MediaTypeHandler.h     # NV12/YUY2/RGB32 format negotiation
│   │   ├── SharedMemoryConsumer.h # Shared DXGI texture ring buffer reader
│   │   └── ReelCamMF.idl          # COM Interface Definition file
│   │
│   ├── ipc/                       # Inter-Process Communication Protocol (Shared)
│   │   ├── CMakeLists.txt
│   │   ├── PipeServer.h / .cpp    # Win32 Named Pipe Server (Async Overlapped)
│   │   ├── PipeClient.h / .cpp    # Win32 Named Pipe Client (C++)
│   │   ├── Protocol.h             # Message framing & length header helpers
│   │   ├── Messages.h             # C++ Command/Response DTO structs
│   │   └── Schemas/               # Canonical JSON Schema files
│   │       ├── envelope-v1.json
│   │       └── commands-v1.json
│   │
│   ├── ui/                        # Desktop Interface Application (C# / WinUI 3)
│   │   ├── ReelCam.UI.csproj      # .NET 8, Windows App SDK 1.5+
│   │   ├── App.xaml / .cs         # Application Lifecycle & Service Registration
│   │   ├── MainWindow.xaml / .cs  # Primary Viewport, Controls & Layout
│   │   │
│   │   ├── Views/                 # Secondary XAML Pages & Dialogs
│   │   │   ├── SettingsDialog.xaml / .cs
│   │   │   └── OnboardingBanner.xaml / .cs
│   │   │
│   │   ├── ViewModels/            # MVVM ViewModels (CommunityToolkit.Mvvm)
│   │   │   ├── MainViewModel.cs   # Playback state, scrubbing, telemetry
│   │   │   └── SettingsViewModel.cs
│   │   │
│   │   ├── Services/              # UI Integration Services
│   │   │   ├── IpcClientService.cs# Named pipe client (.NET System.IO.Pipes)
│   │   │   ├── SettingsService.cs # settings.json serialization & atomic write
│   │   │   ├── HotkeyService.cs   # Win32 RegisterHotKey P/Invoke interop
│   │   │   └── TrayIconService.cs # System tray lifecycle & context menus
│   │   │
│   │   ├── Models/                # C# UI Data Models
│   │   │   ├── ClipItem.cs        # Playlist clip metadata
│   │   │   ├── OutputFormat.cs    # Resolution / FPS enum definitions
│   │   │   └── StreamTelemetry.cs # Live fps, dropped frames, PTS
│   │   │
│   │   ├── Controls/              # Custom UI Controls
│   │   │   ├── PreviewViewport.xaml / .cs # SwapChainPanel video renderer
│   │   │   └── TimecodeDisplay.xaml / .cs
│   │   │
│   │   └── Assets/                # Visual Media Assets
│   │       ├── AppIcon.ico        # High-DPI application icon
│   │       ├── TrayIcons/         # State icons (Green, Amber, Slate, Red)
│   │       ├── DefaultSlate.png   # Bundled 1080p fallback idle graphic
│   │       └── BadgeOverlay.png   # Pre-recorded watermark asset
│   │
│   └── common/                    # Shared C++ Utility Library
│       ├── CMakeLists.txt
│       ├── Logging.h / .cpp       # Daily rotated JSONL file logger
│       ├── HResultHelpers.h       # CHECK_HR / RETURN_IF_FAILED macros
│       ├── StringUtils.h / .cpp   # UTF-8 <-> UTF-16 conversions
│       └── SystemInfo.h / .cpp    # Windows version checks & GPU capabilities
│
├── installer/                     # Packaging & Deployment
│   ├── ReelCam.Package/           # MSIX Windows Application Packaging Project
│   │   ├── Package.appxmanifest   # Virtual camera extensions declaration
│   │   └── Images/                # Store logos & splash screen assets
│   └── scripts/                   # Local Developer Convenience Scripts
│       ├── RegisterDevCamera.ps1  # Manual dev-mode camera registration
│       └── UnregisterDevCamera.ps1
│
├── tests/                         # Automated Test Suites
│   ├── unit/                      # Google Test C++ & MSTest C# Suites
│   │   ├── PacingMathTests.cpp    # QPC timestamp math verification
│   │   ├── IpcProtocolTests.cpp   # Framing & JSON parsing verification
│   │   ├── ScalingMathTests.cpp   # Aspect ratio letterboxing calculations
│   │   └── SettingsTests.cs       # C# settings schema validation tests
│   │
│   ├── integration/               # End-to-End System Tests
│   │   ├── CameraRegistrationTest.cpp # Verify MFCreateVirtualCamera registers
│   │   ├── FrameDeliveryTest.cpp      # Verify frames traverse to shared memory
│   │   └── HeartbeatRecoveryTest.cpp  # Verify idle frame on core termination
│   │
│   └── media/                     # Standardized Sample Video Assets
│       ├── 720p30.mp4             # 10-second H.264 MP4 (1280x720 @ 30fps)
│       ├── 1080p30.mp4            # 30-second H.264 MP4 (1920x1080 @ 30fps)
│       ├── 1080p60.mp4            # 15-second H.264 MP4 (1920x1080 @ 60fps)
│       ├── corrupt.mp4            # Truncated file for error testing
│       ├── no_video.mp4           # Audio-only file to verify rejection
│       └── README.md              # Instructions for downloading 60-min soak test file
│
└── tools/                         # Diagnostic & Development Utilities
    ├── cam_timing_probe/          # Measures frame arrival jitter (QPC probe)
    │   └── main.cpp
    ├── test_harness_consumer/     # Minimal headless MF camera reader
    │   └── main.cpp
    └── soak_test_runner/          # Automated 60-minute stress testing runner
        └── main.cpp
```

---

## 2. Module Boundaries & Responsibilities

### 2.1 `src/core/` — Native Playback Engine Process (`reelcam_core.exe`)
- **Technology**: C++20, Direct3D 11, Media Foundation Source Reader.
- **Responsibilities**:
  1. Opens local video file using `IMFSourceReader` configured for hardware acceleration.
  2. Runs high-resolution QPC frame pacing loop.
  3. Pre-processes frames (aspect scaling, horizontal mirroring, pre-recorded badge blending).
  4. Commits processed textures into shared memory DXGI handles.
  5. Listens on named pipe server; executes commands (`Play`, `Pause`, `Seek`, `Stop`).
- **Thread Model**:
  - `Main Thread`: Win32 message pump and IPC connection loop.
  - `Decode Thread`: Pulls samples asynchronously from `IMFSourceReader`.
  - `Pacing & Render Thread`: Waits on high-resolution timer, processes D3D11 surface, commits to shared memory.
- **Forbidden Actions**: Must never display windows, show UI message boxes, or access network sockets.

---

### 2.2 `src/mfsource/` — Media Foundation Virtual Camera COM DLL (`mfsource.dll`)
- **Technology**: C++20, COM In-Proc Server (`ThreadingModel = Both`).
- **Responsibilities**:
  1. Implements `IMFMediaSource`, `IMFMediaStream`, and `IMFMediaTypeHandler`.
  2. Registered under CLSID `{E7B1A280-9A23-4560-BD92-358F58A009C1}`.
  3. Loaded directly by Windows Camera Frame Server or consuming meeting apps.
  4. Reads frames from shared memory ring buffer.
  5. If `reelcam_core.exe` is absent or crashes, emits the static Idle Slate.
- **Hot-Path Rules**:
  - Zero memory allocations in `IMFMediaStream::RequestSample()`.
  - Zero blocking disk I/O.
  - Strict lockless reader design to prevent hanging consuming applications.

---

### 2.3 `src/ipc/` — Inter-Process Protocol Layer
- **Technology**: C++ / C# shared protocol specification.
- **Responsibilities**:
  1. Handles length-prefixed JSON serialization and framing over Windows Named Pipes.
  2. Enforces strict DACL access control (restricted to current user SID).
  3. Provides connection retry logic with exponential backoff.

---

### 2.4 `src/ui/` — Desktop Interface Application (`reelcam_ui.exe`)
- **Technology**: C# 12, .NET 8, Windows App SDK 1.5+ (WinUI 3).
- **Responsibilities**:
  1. Provides user-facing desktop window, video canvas, controls, and settings.
  2. Hosts zero-latency `SwapChainPanel` preview.
  3. Manages `%LOCALAPPDATA%\ReelCam\settings.json` lifecycle.
  4. Registers system-wide hotkeys via Win32 `RegisterHotKey`.
  5. Controls system tray icon and state menu.

---

### 2.5 `src/common/` — Shared Native Utilities
- **Technology**: C++20 Header/Source library.
- **Responsibilities**:
  1. `Logging`: Lock-free daily rotated JSONL file logger.
  2. `HResultHelpers`: Exception-free COM HRESULT error handling macros.
  3. `StringUtils`: UTF-8/UTF-16 conversions.

---

## 3. Generated Files & Build Artifacts (Do NOT Edit by Hand)

The following directories and files are machine-generated during build and must never be edited manually:
- `build/`, `out/`, `bin/`, `obj/`
- MIDL compiler generated stubs: `*_i.c`, `*_i.h`, `*_p.c`, `dlldata.c`
- NuGet packages and restore caches: `packages/`, `project.assets.json`
- Visual Studio user preferences: `.vs/`, `*.suo`, `*.user`
- vcpkg installed tree: `vcpkg_installed/`

---

## 4. Configuration Locations Summary

| Configuration Category | Location | Managed By | Format |
| :--- | :--- | :--- | :--- |
| **User Settings** | `%LOCALAPPDATA%\ReelCam\settings.json` | `reelcam_ui.exe` | UTF-8 JSON |
| **Application Logs** | `%LOCALAPPDATA%\ReelCam\logs\*.log` | Both Core & UI | UTF-8 JSONL |
| **Virtual Camera Registration** | `HKCU\Software\Classes\CLSID\{...}` | Installer / Setup tool | Windows Registry |
| **C++ Build Configuration** | `CMakeLists.txt` & `vcpkg.json` | Developer / CI | CMake & JSON |
| **UI Build Configuration** | `src/ui/ReelCam.UI.csproj` | Developer / MSBuild | MSBuild XML |
| **CI Automation** | `.github/workflows/` | GitHub Actions | YAML |
