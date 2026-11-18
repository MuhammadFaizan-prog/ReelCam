# ReelCam Project Rules & Engineering Standards

## 1. Document Overview and Authority
This document defines the binding engineering rules, architectural constraints, coding standards, and governance policies for the **ReelCam** project. 

Every contributor—human engineer or automated AI agent—must strictly comply with these rules. Pull requests violating these rules will be rejected automatically by CI or during code review. No feature, deadline, or performance shortcut justifies bypassing these standards.

---

## 2. Product Integrity & Non-Negotiable Guardrails

ReelCam is engineered exclusively as a legitimate, transparent productivity tool for presenters, educators, streamers, and developers. It is not, and will never be, a stealth utility, evasion tool, or identity-falsification system. The following guardrails are permanent and inviolable:

### 2.1 Device Identity & Naming
- **Fixed Device Name**: The virtual camera must always register under the display name `"ReelCam"`. Dynamic renaming, user-configurable device names, random string generation, or mimicking hardware device identifiers (e.g., "Integrated Camera", "Logitech HD Webcam") is strictly prohibited.
- **Hardware IDs and GUIDs**: Hardware IDs and device category GUIDs registered in the Windows registry or Media Foundation runtime must clearly identify ReelCam as a virtual software device. Spoofing real USB hardware vendor/product IDs (VID/PID) is banned.

### 2.2 Transparency and Visibility
- **No Stealth or Cloaking**: The application must never attempt to hide its existence from the operating system, task management utilities, or the user.
- **Mandatory UI Presence**:
  - The main application window must be visible in the Windows taskbar when open.
  - A persistent notification area (tray) icon must be displayed whenever ReelCam background components or streaming pipelines are active.
  - The tray icon must explicitly indicate playback state (e.g., Idle vs. Streaming).
  - The process must run under `reelcam_ui.exe` and `reelcam_core.exe` with transparent process names, proper application metadata, and valid file descriptions. Rootkit techniques, process injection, process hollowing, or self-hiding processes are grounds for immediate project revocation.

### 2.3 Anti-Evasion & Anti-Tampering Prohibitions
- **No Client Inspection Bypasses**: The software must never probe, hook, tamper with, or circumvent detection logic implemented by client applications (e.g., Google Meet, Microsoft Teams, Zoom, Discord, proctoring software, or web browsers).
- **Graceful Error Handling for Blocked Devices**: If a third-party application or browser blocks or disables virtual cameras (via platform policy, Windows Virtual Camera isolation, or custom driver checks), ReelCam must never attempt to bypass the block. Instead, ReelCam must detect device enumeration failures and display a clear, honest error message to the user:
  > *"Virtual camera access is restricted or blocked by the destination application or system policy."*

### 2.4 Synthetic Media and Manipulation Prohibitions
- **No Generative Manipulation**: ReelCam operates strictly as a video playback and passthrough stream engine. 
- **Forbidden Features**:
  - Real-time facial replacement (deepfakes).
  - Real-time facial reenactment or motion transfer.
  - Automated lip-sync generation.
  - Voice morphing or synthetic biometric generation.
- All frames delivered by ReelCam must originate directly from the user-selected local video file, static image, or real physical webcam passthrough, subject only to linear scaling, letterboxing/cropping, color conversion, or approved static watermark badges.

### 2.5 Pre-Recorded Badge
- **Default-On Visual Indicator**: ReelCam ships with a subtle visual corner badge ("Pre-recorded") burned into the top-right or bottom-right corner of the video frame.
- **Factory Setting**: This badge must be enabled by default upon installation. While users may toggle the badge off in settings for professional broadcast presentations, the default configuration for any fresh profile must be `BadgeEnabled = true`.

### 2.6 Automated Rejection Criteria
Any pull request or commit that introduces the following will be rejected immediately:
1. Code designed to spoof hardware camera signatures or disguise virtual camera flags.
2. Code that suppresses tray icons, taskbar presence, or process visibility.
3. Code that hooks external process memory, injects DLLs, or circumvents virtual camera isolation policies.
4. Generative AI face manipulation or voice spoofing dependencies.

---

## 3. C++20 Core & Native Architecture Rules

All native core components (`src/core/`, `src/mfsource/`, `src/ipc/`) must adhere to modern C++20 standards and deterministic real-time media pipeline principles.

### 3.1 Language Standards & Toolchain
- **Standard**: C++20 (`/std:c++20` on MSVC).
- **Compiler**: Visual Studio 2022 (MSVC toolset v143 or newer).
- **Warnings as Errors**: `/W4 /WX` must be enabled on all targets. No compiler warnings are permitted in mainline code.

### 3.2 Memory Management & Resource Safety (RAII)
- **Zero Raw Pointers for Ownership**: Raw pointers (`T*`) are strictly non-owning and may only be used as transient observational parameters (`const T*` or `T&`).
- **Smart Pointers**:
  - Use `std::unique_ptr<T>` for exclusive ownership.
  - Use `std::shared_ptr<T>` only when multi-threaded shared ownership is structurally required (e.g., buffer pools).
  - Never use raw `new`, `delete`, `malloc`, or `free`.
- **COM Pointers**:
  - Always use `Microsoft::WRL::ComPtr<T>` for managing COM interfaces (Media Foundation, DirectX 11, Core Audio).
  - Never call `.AddRef()` or `.Release()` manually on COM interfaces unless implementing custom `IUnknown` reference counters inside COM object implementations.

### 3.3 Strict HRESULT and Error Handling
- **No Swallowed Errors**: Every COM or Win32 call returning an `HRESULT` or `BOOL` must be verified.
- **Standard Macro / Exception Pattern**:
  - Use a standardized macro or helper function: `HR_CHECK(hr)` or `THROW_IF_FAILED(hr)`.
  - In COM methods returning `HRESULT`, catch internal exceptions at the COM perimeter and translate them to standard `HRESULT` return codes (`E_FAIL`, `MF_E_*`, `E_INVALIDARG`).
  ```cpp
  // Compliant COM method perimeter
  IFACEMETHODIMP ReelCamMediaSource::Start(
      IMFPresentationDescriptor* pPresentationDescriptor,
      const GUID* pguidTimeFormat,
      const PROPVARIANT* pvarStartPosition) noexcept
  {
      try
      {
          HR_CHECK(ValidateState());
          HR_CHECK(InternalStart(pPresentationDescriptor, pvarStartPosition));
          return S_OK;
      }
      catch (const wil::ResultException& ex)
      {
          LOG_ERROR("ReelCamMediaSource::Start failed: hr=0x{:08X}", ex.GetErrorCode());
          return ex.GetErrorCode();
      }
      catch (...)
      {
          LOG_ERROR("ReelCamMediaSource::Start failed with unknown exception");
          return E_FAIL;
      }
  }
  ```

### 3.4 Real-Time Media Pipeline Constraints
- **Zero Allocations on the Hot Path**:
  - The sample delivery thread (`IMFMediaStream::RequestSample` response path) must perform **zero** heap allocations (`malloc`, `new`, `std::vector::push_back`, `std::string` allocations).
  - All Direct3D 11 textures, Media Foundation media samples (`IMFSample`), and buffer descriptors must be pre-allocated in a fixed-size ring buffer / sample pool during stream initialization.
- **Non-Blocking Streaming Thread**:
  - The sample request delivery loop must never block on disk I/O, network sockets, IPC named pipes, or long-running synchronization primitives.
  - Thread execution time per sample delivery must remain under **1.0 millisecond**.
  - If the frame queue is starved (decoder falling behind), the stream must push the last known frame or an idle fallback frame immediately with the updated timestamp rather than blocking the downstream camera consumer.
- **No `Sleep`-Based Timing**:
  - Never use `Sleep()`, `std::this_thread::sleep_for()`, or busy spin-loops for frame rate regulation or pacing.
  - Use high-resolution multimedia clocks, waitable timers via `CreateWaitableTimerEx2` with `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`, or Media Foundation presentation clocks (`IMFPresentationClock`).

### 3.5 Naming & Formatting Conventions
- **Files**: Header files use `.h` or `.hpp`; source files use `.cpp`. File names must match the primary class in `PascalCase` (e.g., `FramePacer.h`, `FramePacer.cpp`).
- **Header Guards**: `#pragma once` is mandatory at the beginning of every header.
- **Classes / Structs / Enums / Concepts**: `PascalCase` (e.g., `VirtualCameraSource`, `PlaybackState`).
- **Methods & Functions**: `PascalCase` (e.g., `InitializePipeline()`, `DeliverSample()`).
- **Variables**:
  - Local variables & function parameters: `camelCase` (e.g., `frameDuration`, `sampleTimestamp`).
  - Private / Protected class members: prefixed with `m_` in `camelCase` (e.g., `m_samplePool`, `m_dxgiDeviceManager`).
  - Constants & Enum values: `UPPER_SNAKE_CASE` (e.g., `MAX_FRAME_BUFFER_COUNT`, `DEFAULT_FRAME_RATE`).
- **Namespaces**: `PascalCase` matching the folder hierarchy:
  - `ReelCam::Core`
  - `ReelCam::MFSource`
  - `ReelCam::IPC`
  - `ReelCam::Common`

### 3.6 Structured Logging vs. Standard Output
- Standard I/O output (`printf`, `std::cout`, `std::cerr`, `puts`) is strictly prohibited in production builds.
- Direct calls to `OutputDebugStringA/W` must not be used directly; all diagnostic traces must flow through `ReelCam::Common::Logger`.
- Logging must support log levels (`TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`) and include timestamps, thread IDs, and source file locations.
- Hot-path frame logging must only execute under `TRACE` level and must compile out or short-circuit before string formatting.

---

## 4. C# & WinUI 3 Desktop Application Rules

The user interface layer (`src/ui/`) is built using **C# 12 / .NET 8+** and **WinUI 3 (Windows App SDK)**.

### 4.1 Runtime, SDK & Language Target
- **Framework**: .NET 8 or newer (`net8.0-windows10.0.22000.0` target framework moniker).
- **Windows App SDK**: Version 1.5 or newer.
- **Null Safety**: Nullable reference types must be enabled project-wide:
  ```xml
  <PropertyGroup>
    <Nullable>enable</Nullable>
    <TreatWarningsAsErrors>true</TreatWarningsAsErrors>
    <EnforceCodeStyleInBuild>true</EnforceCodeStyleInBuild>
  </PropertyGroup>
  ```
  Code with unhandled nullability warnings (`CS8600`, `CS8602`, `CS8618`) will fail the build.

### 4.2 Architecture: Strict MVVM
- **Model-View-ViewModel (MVVM)** is mandatory.
- Use the **CommunityToolkit.Mvvm** library:
  - ViewModels must inherit from `ObservableObject`.
  - Use `[ObservableProperty]` for bindable properties.
  - Use `[RelayCommand]` for UI commands.
- **Zero Business Logic in Code-Behind**:
  - Code-behind files (`*.xaml.cs`) must contain **only** view-specific layout initialization, Win32 window handle interop (e.g., setting window backdrops, app window size, title bar customizations), or direct element-to-element animations.
  - No playback state evaluation, IPC dispatching, settings persistence, or media metadata parsing may reside inside a `.xaml.cs` file.

### 4.3 Asynchronous Execution & Thread Safety
- **Asynchronous Everywhere**: All I/O, IPC messages, and file operations must use `async`/`await`.
- **No Sync-Over-Async**: Never call `.Result`, `.Wait()`, or `.GetAwaiter().GetResult()` on tasks. Blocking the UI thread results in application freezes and deadlocks.
- **Dispatcher Handling**: ViewModel callbacks originating from background IPC threads must marshal updates to the UI thread via `DispatcherQueue.TryEnqueue()` before updating observable collections or UI-bound properties.

### 4.4 Data Binding & XAML Standards
- **Strongly Typed Bindings**: Use `x:Bind` exclusively. Reflection-based `{Binding}` is forbidden due to performance overhead and lack of compile-time verification.
- **Explicit Binding Modes**: Always specify `Mode=OneWay` or `Mode=TwoWay` explicitly when using `x:Bind`.
- **Resource Management**: All brushes, strings, styles, and control templates must reside in `ResourceDictionary` files. Hardcoded color hex codes or inline dimensions must not be scattered across XAML files.

### 4.5 C# Code Style & Naming
- **Classes, Records, Structs, Methods, Properties, Events**: `PascalCase`.
- **Local Variables & Parameters**: `camelCase`.
- **Private Fields**: `_camelCase` (e.g., `_ipcClient`, `_playbackState`).
- **Interfaces**: Prefixed with `I` in `PascalCase` (e.g., `IIpcConnectionService`, `IMediaPlayerService`).
- **Namespaces**: Must align with folder structure:
  - `ReelCam.UI.Views`
  - `ReelCam.UI.ViewModels`
  - `ReelCam.UI.Services`
  - `ReelCam.UI.Models`

---

## 5. Inter-Process Communication (IPC) Rules

Communication between the UI process (`reelcam_ui.exe`) and the background native core engine (`reelcam_core.exe`) is conducted via a local Windows Named Pipe.

### 5.1 Named Pipe Transport Specifications
- **Pipe Name**: `\\.\pipe\ReelCam-Control-{UserSID}` where `{UserSID}` is the Security Identifier of the active logged-in user. Global or static cross-user pipe names are prohibited.
- **Security Descriptors**: The pipe server must be created with an explicit Discretionary Access Control List (DACL) that grants read/write permissions **only** to the current user token and Local System. Medium and High integrity processes within the same user session may connect; other users are rejected.
- **Directionality**: Bi-directional (`PIPE_ACCESS_DUPLEX`) in message transmission mode (`PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE`).

### 5.2 Envelope Protocol & Serialization
All IPC payloads must be encoded as UTF-8 JSON objects conforming to the unified schema defined in `Schema.md`.
```json
{
  "protocolVersion": "1.0.0",
  "messageId": "c3a9f05a-e7c6-4d7a-8f47-2c93d9a18df4",
  "correlationId": null,
  "type": "Request",
  "command": "Playback.Play",
  "payload": {
    "sourceFilePath": "C:\\Videos\\demo.mp4",
    "loop": true
  },
  "timestampUtc": "2026-11-18T14:49:00.000Z"
}
```

### 5.3 Request, Response, and Notification Semantics
1. **Request/Response Pairs**:
   - Every request issued by the UI contains a unique `messageId` (UUID v4).
   - The native core must return a response containing the exact same `correlationId = request.messageId`.
   - The UI must maintain a pending request map with a standard **5.0-second timeout**. If no response is received within 5 seconds, the request promise must be faulted with an `IpcTimeoutException`.
2. **State Push Notifications**:
   - The native core autonomously emits state notifications (`type: "Notification"`, `correlationId: null`) when pipeline events occur (e.g., `StreamStarted`, `StreamStopped`, `FrameDropped`, `DeviceStateChanged`).
   - The UI must treat notifications as idempotent updates to current state representations.

### 5.4 Reconnection and Resilience
- If the core process terminates or the pipe disconnects, the UI client must transition its state to `Disconnected` and initiate an exponential backoff reconnect sequence (initial delay 500ms, doubling up to max 5000ms).
- All inflight requests upon pipe disconnection must be rejected immediately to trigger appropriate UI error notifications.

---

## 6. Design, UX & Accessibility Rules

ReelCam must feel like an organic, native element of Windows 11.

### 6.1 Windowing & Workflows
- **Single-Window Core**: The application must run within a single, cohesive primary window. Avoid launching detached floating dialogs or multi-window configuration tools.
- **Window Resizing & Bounds**:
  - Minimum supported window size: `800 x 600` epx.
  - Default initial window size: `1000 x 700` epx.
  - The window bounds and state (maximized/normal) must be remembered and restored across sessions.

### 6.2 Visual Styling & Materials
- **Windows 11 Fluent Design**:
  - The window backdrop must utilize **Mica** (or Mica Alt) material.
  - Apply standard Windows 11 control corner radii (8px for primary containers, 4px for buttons and form fields).
  - Use `Segoe UI Variable` as the primary typographic family, utilizing Variable Display, Text, and Small styles appropriately.
- **Theme Support**:
  - The application must support both Dark and Light themes.
  - Default behavior: dynamically inherit the system-wide Windows theme setting (`Application.Current.RequestedTheme = ApplicationTheme.Default`).
  - Provide a user toggle in Settings to force Light or Dark mode.

### 6.3 Accessibility Requirements
- **Automation Properties**: Every interactive UI control (`Button`, `Slider`, `ToggleSwitch`, `TextBox`, `ListView`) must have an explicit `AutomationProperties.Name` and, where applicable, `AutomationProperties.HelpText`.
- **Keyboard Navigation**:
  - The full application workflow must be operable using keyboard navigation alone (Tab, Shift+Tab, Arrow keys, Enter, Space).
  - Visual focus indicators (`FocusVisualPrimaryBrush`, `FocusVisualSecondaryBrush`) must remain visible and unobstructed.
- **High Contrast Mode**: All UI views must be tested and verified against Windows High Contrast / Contrast Themes (Aquatic, Desert, Dusk, Night Sky). Brushes must adapt to system theme resource dictionaries without illegible text-background collisions.

---

## 7. Testing & Quality Assurance Rules

Quality in ReelCam is verified through multi-tier automated test suites. Manual testing alone is insufficient.

### 7.1 Test Suites & Boundaries
1. **Unit Tests (`tests/unit/`)**:
   - **C++ Native**: GoogleTest (`gtest`). Tests cover:
     - Frame pacing math, presentation timestamp conversions, and clock drift compensation.
     - Ring buffer concurrency, push/pull semantics, and memory boundaries.
     - Aspect ratio calculations (letterbox, pillarbox, stretch, crop coordinates).
     - Color space matrix conversions (NV12, YUY2, RGB32).
     - JSON protocol serializers, deserializers, and schema validation.
   - **C# Managed**: xUnit + FluentAssertions + Moq. Tests cover:
     - ViewModel state machine transitions.
     - Settings persistence and validation logic.
     - IPC message parsing and event dispatching.
     - Hotkey registry triggers.
2. **Integration Tests (`tests/integration/`)**:
   - Verification of the Media Foundation Virtual Camera COM DLL registration.
   - Named pipe handshakes, concurrent messaging stress tests, and connection recovery tests.
   - Media decoding pipeline tests using local test files.

### 7.2 Test Media Constraints
- Synthetic test video files are stored in `tests/media/`.
- **File Constraints**:
  - Test clips must be small (under 10 MB each, duration 2–5 seconds).
  - Codecs included: H.264 (`test_1080p30_h264.mp4`), VP9 (`test_720p60_vp9.webm`), AV1 (`test_1080p30_av1.mkv`).
  - All test media must be royalty-free, CC0, or synthetically generated via tools/scripts (e.g. synthetic test charts with embedded frame counters).

### 7.3 Environmental Independence
- **No Physical Hardware Dependency**: Tests must execute successfully on headless CI runners lacking physical camera hardware, audio endpoints, or discrete GPUs.
- **Mocking & Synthetic Fallback**:
  - The webcam passthrough feature must be backed by a mock camera source during automated tests.
  - DirectX 11 tests must run using the WARP software rasterizer (`D3D_DRIVER_TYPE_WARP`) on CI runners lacking dedicated GPU hardware.
- **No Paid APIs or Network Dependencies**: No unit or integration test may depend on active internet access, external cloud services, or paid third-party APIs.

---

## 8. File & Folder Conventions

The workspace must strictly adhere to the designated project hierarchy. Files must not be placed at arbitrary root locations.

### 8.1 Directory Structure
```
reelcam/
  PRD.md
  Techspec.md
  Appflow.md
  Schema.md
  ImplementationPlan.md
  Tracker.md
  Rules.md
  Security.md
  Structure.md
  src/
    core/             # Native playback engine, decoder, frame pacer
    mfsource/         # Media Foundation virtual camera COM DLL
    ipc/              # Named-pipe message serialization & protocol definitions
    ui/               # WinUI 3 desktop application (.NET 8)
    common/           # Shared C++ logging, math, Win32 utilities
  installer/          # MSIX packaging assets, manifest, WiX project
  tests/
    unit/             # Native (gtest) and managed (xUnit) unit tests
    integration/      # End-to-end pipe, COM registration, and pipeline tests
    media/            # Deterministic, lightweight media test assets
  tools/              # Automation scripts, test clip generators, signing helpers
  .github/workflows/  # CI/CD pipelines
```

### 8.2 File Organization Rules
- **One Class per File**: Each C++ header/source pair and each C# source file must encapsulate exactly one primary class, interface, or struct.
- **Generated Files Are Sacred**: Files generated by build systems, compilers, or source generators (e.g., `*.g.cs`, `*.g.h`, `vcpkg_installed/`, `build/`, `bin/`, `obj/`) must **never** be manually edited or checked into Git.
- **Path Separation**: Always use forward slashes (`/`) in scripts, CMake files, and documentation paths to ensure cross-shell compatibility.

---

## 9. Documentation Standards

Code without documentation is considered incomplete and unmergeable.

### 9.1 Source Code Documentation
- **C++ Public APIs**: Use Doxygen format (`///`) for all public classes, methods, and structures. Describe parameter contracts, return values, expected thread-context, and possible `HRESULT` error codes.
  ```cpp
  /// <summary>
  /// Configures the active frame pacing clock with target frame rates and dimensions.
  /// </summary>
  /// <param name="frameRate">Target playback frames per second.</param>
  /// <param name="dimensions">Output frame resolution in pixels.</param>
  /// <returns>S_OK on successful configuration; MF_E_INVALIDMEDIATYPE on unsupported formats.</returns>
  ```
- **C# Public APIs**: Use standard XML documentation comments (`/// <summary>`) on all public and internal interfaces, ViewModels, and services.

### 9.2 Architecture & Project Document Sync
Whenever an architectural choice, schema field, or workflow changes:
1. Update `Techspec.md`, `Schema.md`, `Appflow.md`, or `Security.md` **in the same commit** or pull request as the code change.
2. Update `Tracker.md` to reflect milestone, task, and bug-fix progress.
3. Code changes that contradict documented architecture will be blocked in review.

---

## 10. Build Systems, Packaging & CI/CD Rules

### 10.1 Build Systems
- **Native Components**: Built using **CMake** (version 3.25+) with `CMakePresets.json`. Generates Visual Studio 2022 project files.
- **Managed Components**: Built using **MSBuild** / `dotnet build` (.NET 8 SDK).
- **Native Dependency Management**: **vcpkg** in manifest mode (`vcpkg.json`). Never commit precompiled binary dependencies (`.dll`, `.lib`) to the repository.

### 10.2 Compiler Warning Enforcement
- C++ builds must pass with `/WX` (Warnings as Errors) under warning level `/W4`.
- C# builds must pass with `<TreatWarningsAsErrors>true</TreatWarningsAsErrors>`.
- Any compiler warning on CI breaks the build immediately.

### 10.3 Packaging & Deployment
- The primary deployment package is a signed **MSIX** package.
- Desktop bridge packaging must declare the required virtual camera extensions (`windows.virtualCamera`) supported on Windows 11 Build 22000+.
- All binaries (`.dll`, `.exe`) must be code-signed. For local developer builds, a self-signed test certificate created via `tools/New-DevCert.ps1` is permitted and installed to the local machine root store.

### 10.4 Continuous Integration (GitHub Actions)
- The CI pipeline executes on every pull request targeting `main`:
  1. **Lint & Format**: Runs `clang-format` on C++ and `dotnet format --verify-no-changes` on C#.
  2. **Build Matrix**: Compiles `Debug` and `Release` configurations for `x64` (and `ARM64` where configured).
  3. **Unit Tests**: Runs all native GoogleTest and managed xUnit tests.
  4. **Packaging Check**: Verifies MSIX manifest schema and package generation.
  5. **Static Analysis**: Runs MSVC Code Analysis (`/analyze`) and Roslyn analyzers.

---

## 11. Git & Version Control Rules

### 11.1 Commit Message Format
All commits must follow the **Conventional Commits** specification:
```
<type>(<scope>): <short summary>

[optional body]

[optional footer(s)]
```
- **Allowed Types**:
  - `feat`: A new feature for the user or system.
  - `fix`: A bug fix.
  - `docs`: Documentation updates only.
  - `refactor`: Code change that neither fixes a bug nor adds a feature.
  - `perf`: A code change that improves performance.
  - `test`: Adding or correcting tests.
  - `chore`: Changes to build process, auxiliary tools, or dependencies.
  - `ci`: Changes to CI/CD workflows and configuration scripts.
- **Allowed Scopes**: `core`, `mfsource`, `ui`, `ipc`, `installer`, `common`, `deps`, `docs`.
- **Example**: `feat(mfsource): implement D3D11 texture sharing for sample delivery`

### 11.2 Branching & Merging Policy
- The `main` branch is permanently protected. Direct pushes are disabled.
- **No Force-Pushes**: `git push --force` is strictly forbidden on `main` and release branches.
- Feature branches must use clear prefixes: `feature/<name>`, `bugfix/<issue-id>-<name>`, `chore/<task>`.
- Merges into `main` must occur via Pull Requests with a requirement of passing CI checks and at least one code review sign-off.

### 11.3 Repository Cleanliness (.gitignore)
- Never commit binary outputs (`.exe`, `.dll`, `.obj`, `.pdb`, `.msix`).
- Never commit user-specific IDE settings (`.vs/`, `.vscode/`, `*.suo`, `*.user`).
- Never commit local media files outside of the minimal files in `tests/media/`.

---

## 12. AI Agent Operating Rules & Behavior Standards

When AI coding assistants or automated agents operate on the ReelCam codebase, the following behavioral protocols are non-negotiable:

### 12.1 Read Before Edit
- An agent must inspect and read relevant existing files, headers, and specifications before proposing or writing modifications.
- Do not make assumptions about existing function signatures, namespace hierarchies, or IPC command strings; confirm them in code and `Schema.md`.

### 12.2 Guardrail Inviolability
- Under no circumstances may an agent modify, soften, or remove the Product Integrity Guardrails defined in Section 2.
- If asked by a user to implement feature disguising, device name randomization, stealth mode, or deepfake/face-swapping, the agent must refuse and cite Section 2 of this document.

### 12.3 Verification Before Completion
- Never declare a feature or bug fix completed without running corresponding verification commands or automated test suites.
- When compilation or tests cannot be run directly in the environment, the agent must clearly state what verification was performed and what steps the user must execute.

### 12.4 Tracker & Documentation Synchronization
- When completing a task, the agent is responsible for updating `Tracker.md`, marking the corresponding task status as complete, and recording any new open items or technical debt discovered during execution.

### 12.5 Scope Discipline
- Do not introduce unapproved third-party dependencies or deprecated architectures (e.g., do not attempt to add DirectShow filters or FFmpeg libraries when Media Foundation source reader and `MFCreateVirtualCamera` have been explicitly confirmed).
- Focus strictly on the assigned task without refactoring unrelated modules.
