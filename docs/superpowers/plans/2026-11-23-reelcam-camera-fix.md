# ReelCam Virtual Camera Fix & Google Meet Verification Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the ReelCam Media Foundation virtual camera pipeline, resolve webcam passthrough YUY2->NV12 distortion, eliminate stale DirectShow registry entries causing "Camera unavailable", deploy updated binaries, and verify end-to-end streaming inside Google Meet via browser automation.

**Architecture:**
- Producer (`reelcam_core.exe`): Decodes video files (H.264/HEVC) and captures webcam feeds (YUY2/RGB32 converted to 1080p NV12), publishing frames into cross-session Shared Memory (`Global\ReelCam_FrameBuffer` with NULL DACL and `Local\` fallback).
- Consumer (`mfsource.dll`): Windows 11 Media Foundation Software Camera Source loaded by Windows Camera Frame Server (`svchost.exe -k Camera`), consuming NV12 frames from `Global\ReelCam_FrameBuffer` and delivering them via `IMFMediaStream`.
- Registration (`cam_register.exe`): Registers `mfsource.dll` as a COM in-proc server and creates persistent Windows 11 virtual camera `ReelCam (Windows Virtual Camera)`, cleaning up any obsolete DirectShow Filter entries.
- Verification (`browser` subagent): Navigates to Google Meet, verifies `ReelCam (Windows Virtual Camera)` is selectable, active, and streaming video/passthrough without errors.

**Tech Stack:** C++17, Windows Media Foundation (IMFVirtualCamera, IMFMediaSource, IMFSourceReader), DirectShow cleanup, Win32 Shared Memory / Security Descriptors, CMake, MSBuild, Google Meet.

## Global Constraints
- Target OS: Windows 11 Build 22000+.
- Video Format: 1920x1080 @ 30 FPS, progressive NV12 (BT.601 color space).
- Zero broken DirectShow entries in camera selection dropdown.
- Unit tests (`unit_tests.exe`) must pass 100%.

## Review Focus
1. Chrome selecting broken DirectShow CLSID instead of Media Foundation Virtual Camera -> Eliminate DirectShow key from registry.
2. Windows Camera Frame Server running in Session 0 failing to open Session 1 Shared Memory -> Enforce `Global\` namespace with permissive NULL DACL.
3. MediaStream not calling `m_shmConsumer.Open()` -> Ensure `Open()` is called on Initialize, Start, and sample request retries.
4. Webcam passthrough displaying green/purple interlaced lines -> Ensure YUY2->NV12 scaling correctly separates Y and UV planes.
5. Browser / Google Meet muted or permission-denied -> Automate browser navigation and verify video feed unmute.

---

### Task 1: Verify & Finalize Shared Memory & MFSource Fixes
**Files:**
- Modify: `src/common/SharedMemory.h`
- Modify: `src/mfsource/ReelCamMediaStream.cpp`
- Modify: `src/core/WebcamPassthrough.cpp`
- Modify: `tools/cam_register/main.cpp`
- Test: `tests/unit_tests/main.cpp`

**Steps:**
- [x] Implement dual Global/Local shared memory with NULL DACL in `SharedMemory.h`.
- [x] Wire `m_shmConsumer.Open()` in `ReelCamMediaStream.cpp` (`Initialize`, `Start`, `GenerateTestPatternSample`).
- [x] Fix YUY2->NV12 conversion in `WebcamPassthrough.cpp` to eliminate green/purple artifacts.
- [x] Remove unused `RegisterDirectShowCategory` and improve `UnregisterDirectShowCategory` in `cam_register/main.cpp`.
- [ ] Run full build with CMake and verify compilation succeeds with zero errors/warnings.
- [ ] Run `unit_tests.exe` and confirm all 32 unit tests pass.

---

### Task 2: Remove Stale DirectShow Registry Key & Clean Register Camera
**Files:**
- Script: `tools/clean_registry.bat`
- Binary: `bin/Release/cam_register.exe`

**Steps:**
- [ ] Create a cleanup helper or script to remove `HKLM\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\{E7B1A280-9A23-4560-BD92-358F58A009C1}`.
- [ ] Copy newly built binaries (`mfsource.dll`, `reelcam_core.exe`, `reelcam_ui.exe`, `cam_register.exe`) from `build/` to `bin/Release/`.
- [ ] Register `ReelCam (Windows Virtual Camera)` using `cam_register.exe --register` or `--register-user`.
- [ ] Verify device enumeration via `cam_test_enum.exe` confirms `ReelCam (Windows Virtual Camera)` is active and recognized by Media Foundation.

---

### Task 3: Launch ReelCam Core & UI in Session
**Files:**
- Binary: `bin/Release/reelcam_core.exe`
- Binary: `bin/Release/reelcam_ui.exe`

**Steps:**
- [ ] Check if existing ReelCam instances are running, terminate old processes if needed.
- [ ] Launch `reelcam_core.exe` and `reelcam_ui.exe`.
- [ ] Load sample video file into ReelCam and initiate playback.
- [ ] Verify shared memory heartbeat is ticking and frames are published.

---

### Task 4: Google Meet Verification via Browser Subagent
**Subagent:** `browser` (Model: `flash`)

**Steps:**
- [ ] Invoke `/browser` subagent to navigate to `https://meet.google.com/new` or an active meeting.
- [ ] Ensure camera permissions are granted in the browser.
- [ ] Open camera settings in Meet, verify device list shows `ReelCam (Windows Virtual Camera)`.
- [ ] Select `ReelCam (Windows Virtual Camera)` and unmute the camera (ensure camera button is not slashed).
- [ ] Take a snapshot/screenshot to verify that the video from ReelCam is actively playing in the Meet viewport.
- [ ] Test toggling "Webcam Passthrough" in ReelCam and verify the camera feed transitions cleanly without green/purple artifacts.

---

### Task 5: Git Commit & Remote Push
**Steps:**
- [ ] Review `git status` and `git diff`.
- [ ] Commit all changes with descriptive commit message.
- [ ] Push commits to `https://github.com/MuhammadFaizan-prog/ReelCam` (branch `master`).
