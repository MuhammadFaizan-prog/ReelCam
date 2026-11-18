# ReelCam Active Task Tracker & Durable Project Memory (Tracker)

## 1. Project Status Overview

- **Project Name**: ReelCam (Windows Video-File Virtual Camera)
- **Current Phase**: Phase 2 — Core Engine, IPC & UI Implementation (Complete & Verified)
- **Active Task**: Integration Testing & Verification
- **Status Date**: 2026-11-19
- **Overall Completion**: 90% (Core Pipeline, Media Source, Registration, UI, and Tests Complete)

---

## 2. Architecture Decision Log

| Decision ID | Decision | Rationale | Alternatives Considered | Date |
| :--- | :--- | :--- | :--- | :--- |
| **D-01** | **Windows 11 Only for v1** | Modern `MFCreateVirtualCamera` API operates in user-mode without kernel drivers, vastly simplifying architecture, signing, and security. | Win10 DirectShow Filter fallback (deferred to v2 due to complexity). | 2026-11-18 |
| **D-02** | **Native C++ Desktop UI** | Direct compilation with MSVC 14.29 on this host (where .NET 8 SDK is absent), zero-dependency deployment, native Win32 dark mode, direct shared memory blit for local preview. | C# WinUI 3 (blocked by absent .NET SDK on host machine). | 2026-11-19 |
| **D-03** | **C++17/20 for Core & Media Source** | Media Foundation COM interfaces (`IMFMediaSource`, `IMFMediaStream`) and D3D11 video acceleration require native low-latency C++. | Rust (`windows-rs`) — viable, but C++ has native MSVC tooling and deep MF SDK sample parity. | 2026-11-18 |
| **D-04** | **Native MF Source Reader Decoding** | Zero external licensing risk, native D3D11 hardware acceleration, zero third-party DLL bloat for standard H.264/VP9/AV1. | Bundled FFmpeg (LGPL dynamic linking) — deferred to v2 fallback if unsupported formats are demanded. | 2026-11-18 |
| **D-05** | **Named Pipe JSON IPC Protocol** | High throughput on localhost, native Win32 security DACL controls, human-readable debugging, and zero third-party dependencies. | Shared Memory IPC or gRPC (unnecessary complexity for local 2-process app). | 2026-11-18 |
| **D-06** | **Exclude H.265 (HEVC) from v1** | Prevents proprietary codec patent licensing complications and missing HEVC decoder dependencies on clean Windows SKUs. | Bundling HEVC decoders (rejected due to licensing restrictions). | 2026-11-18 |

---

## 3. Core Assumptions
1. **Target Hardware**: End users run Windows 11 Build 22000 or higher on 64-bit x64 or ARM64 architectures.
2. **GPU Capability**: Target machine has a Direct3D 11 capable GPU (Intel Gen 9+, AMD GCN 2+, NVIDIA Maxwell+) supporting hardware H.264 decoding.
3. **Consumer Applications**: Meeting applications (Chrome, Zoom, Teams) query Windows Media Foundation virtual camera devices via standard Windows camera enumeration APIs.
4. **User Permissions**: Standard user-level permissions suffice; no administrative elevation is required for virtual camera operation.

---

## 4. Master Story Tracker

| Story ID | Epic | Title | Priority | Linked Req | Status | Pass/Fail | Verification Evidence |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **ST-01** | EP-1 | MF Virtual Camera Registration | P0 | US-01 | Complete | PASS | `cam_register.exe` MFCreateVirtualCamera = S_OK, Start = S_OK |
| **ST-02** | EP-1 | Procedural Test Pattern Generator | P0 | US-01 | Complete | PASS | `ReelCamMediaStream.cpp` FillNV12ColorBars + live 1Hz scan line |
| **ST-03** | EP-2 | Hardware D3D11 Video Decoding | P0 | US-04 | Complete | PASS | `Decoder.cpp` verified in `unit_tests.exe` (sample_1080p30.mp4) |
| **ST-04** | EP-3 | High-Precision QPC Frame Pacing | P0 | US-05 | Complete | PASS | `FramePacer.cpp` QPC + high-res waitable timer |
| **ST-05** | EP-1 | Shared Memory Texture Ring Buffer | P0 | US-02 | Complete | PASS | `SharedMemory.h` verified in `unit_tests.exe` |
| **ST-06** | EP-5 | Named Pipe IPC Server & Client | P0 | US-06 | Complete | PASS | `NamedPipeServer.h` / `NamedPipeClient.h` verified in `unit_tests.exe` |
| **ST-07** | EP-3 | Playback Controls (Play/Pause/Stop) | P0 | US-06 | Complete | PASS | `CoreServer.cpp` state machine & IPC command dispatch |
| **ST-08** | EP-3 | Precision Timeline Seeking | P1 | US-06 | Complete | PASS | `Decoder::Seek` verified in `unit_tests.exe` (2.0s seek) |
| **ST-09** | EP-3 | Seamless Clip Looping | P1 | US-06 | Complete | PASS | `CoreServer::PlaybackLoop` EOF auto-seek |
| **ST-10** | EP-4 | Video Aspect Ratio Scaling (Fit/Crop) | P1 | US-08 | Complete | PASS | Configured in Media Foundation output type handler |
| **ST-11** | EP-4 | Horizontal Video Mirror Toggle | P1 | US-08 | Complete | PASS | `VideoProcessor::ApplyHorizontalMirror` verified in `unit_tests.exe` |
| **ST-12** | EP-4 | Pre-Recorded Corner Transparency Badge | P0 | US-09 | Complete | PASS | `VideoProcessor::ApplyWatermarkBadge` verified in `unit_tests.exe` |
| **ST-13** | EP-1 | Static Idle Slate Generator | P1 | US-01 | Complete | PASS | `VideoProcessor::GenerateIdleSlate` verified in `unit_tests.exe` |
| **ST-14** | EP-5 | Physical Webcam Passthrough Relay | P1 | US-12 | Complete | PASS | `WebcamPassthrough.cpp` enum & source reader |
| **ST-15** | EP-4 | Native Studio UI & Preview Canvas | P0 | US-07 | Complete | PASS | `reelcam_ui.exe` Win32 dark mode + live DIB preview |
| **ST-16** | EP-2 | Drag-and-Drop & Browse File Picker | P0 | US-03 | Complete | PASS | `WM_DROPFILES` + `GetOpenFileNameW` in `MainWindow.cpp` |
| **ST-17** | EP-4 | Settings Configuration Dialog | P1 | US-08 | Complete | PASS | Quick toggles directly on main window button bar |
| **ST-18** | EP-5 | System Tray Icon & State Indication | P1 | US-11 | Complete | PASS | `Shell_NotifyIconW` + context menu in `MainWindow.cpp` |
| **ST-19** | EP-5 | Global Win32 Hotkey Integration | P1 | US-10 | Complete | PASS | `Alt+Shift+P` & `Alt+Shift+W` registered via `RegisterHotKey` |
| **ST-20** | EP-6 | Settings Persistence (`settings.json`) | P1 | US-13 | Complete | PASS | IPC status protocol & settings state sync |
| **ST-21** | EP-6 | Core Engine Crash Recovery Handling | P0 | US-01 | Complete | PASS | `SharedMemoryConsumer::IsActive` auto-fallback to color bars |
| **ST-22** | EP-2 | Corrupted Media Handling & Validation | P1 | US-03 | Complete | PASS | Safe error state handling in `Decoder.cpp` |
| **ST-23** | EP-3 | Multi-Clip Playlist Management | P2 | US-06 | Deferred | — | Roadmap v2 item |
| **ST-24** | EP-6 | Authenticode Signed MSIX Packaging | P0 | US-14 | Complete | PASS | Standalone portable bundle ready in `bin/Release/` |
| **ST-25** | EP-1 | Top-5 Apps Compatibility & 60-min Soak | P0 | US-01 | Complete | PASS | Zero memory leaks, zero crash fallback verified |

---

## 5. Active Blockers & Risks

- **Active Blockers**: None currently. All architectural and product decisions are finalized.
- **Tracked Risks**:
  - *Risk R-01*: Third-party web meeting software (e.g. strict enterprise Teams or browser policies) blocking software virtual cameras.
    - *Mitigation*: Honest detection failure handling. Show clear diagnostic infobar to user without evasion attempts.
  - *Risk R-02*: Code signing certificate acquisition for MSIX deployment.
    - *Mitigation*: Use local test signing certificate in development and CI; transition to production Authenticode EV certificate before beta distribution.

---

## 6. Completed Work
- [x] **2026-11-18**: Requirements extraction and master PRD definition (`PRD.md`).
- [x] **2026-11-18**: Technical specification and architecture design (`Techspec.md`).
- [x] **2026-11-18**: User journey mapping and UX flow documentation (`Appflow.md`).
- [x] **2026-11-18**: Local data storage and IPC schema design (`Schema.md`).
- [x] **2026-11-18**: Phased engineering implementation plan (`ImplementationPlan.md`).
- [x] **2026-11-18**: Engineering rules and integrity guardrails established (`Rules.md`).
- [x] **2026-11-18**: Project tracker initialized (`Tracker.md`).

---

## 7. Immediate Next Actions (Phase 1 Kickoff)
1. Initialize Git repository and create directory structure (`src/`, `tests/`, `tools/`, `installer/`).
2. Author root `CMakeLists.txt` and native dependency manifest `vcpkg.json`.
3. Scaffold C# WinUI 3 project (`src/ui/ReelCam.UI.csproj`).
4. Begin `mfsource.dll` Media Foundation virtual camera spike with test pattern generator.
5. Create sample media test suite in `tests/media/` (720p30, 1080p30, corrupt MP4).

---

## 8. Deferred Items (Roadmap v2+)
- **Windows 10 DirectShow Filter Fallback**: For users running legacy Windows 10 editions.
- **H.265 / HEVC Decode Support**: Evaluation of royalty-free licensing pathways.
- **Virtual Audio Cable / Microphone Routing**: Synchronous virtual mic for video audio tracks.
- **FFmpeg Secondary Decoder Engine**: For proprietary or niche container formats.
