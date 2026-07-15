# Session Anchored Summary

**Date**: 2026-07-15
**Session**: RtPbrSurvey Scene Renderer Integration (renamed from "Tank Scene Renderer Host App")
**Workspace**: `C:\work\TankPhysicsSandbox`
**Session folder**: `Docs/opencode/rtpbrsurvey-scene-renderer-integration/`
**Build status**: Minimal Win32 EXE builds and runs. RtPbrSurvey integration blocked pending DirectX-Headers fix.

---

## What We Did

### Phase 1: RtPbrSurvey Build Integration (blocked)

Created the full scaffold for integrating `RtPbrSurvey::SceneRenderer` as the rendering layer:

- **`External/RtPbrSurvey/CMakeLists.txt`** (99 lines) -- Static library wrapper for RtPbrSurvey. Excludes app-level sources (Main.cpp, RtPbrSurveyApp.cpp). Uses `find_package(directx-headers CONFIG)` and `find_package(imgui CONFIG)`. Generates forwarding headers to map `include/d3dx12/d3dx12.h` (RtPbrSurvey's NuGet convention) to vcpkg's `<directx/d3dx12.h>` layout.
- **Root `CMakeLists.txt`** -- TankSandbox EXE target linking RtPbrSurvey, plus Win32Application and IApplication from RtPbrSurvey's platform layer.
- **`src/main.cpp`** (9 lines) -- WinMain entry.
- **`src/TankSandboxApp.h`** (41 lines) -- IApplication derived class owning GraphicsDevice, SceneRenderer, ImGuiSystem.
- **`src/TankSandboxApp.cpp`** (124 lines) -- Full SceneRenderer lifecycle: init, scene building (cube+sphere procedural), resize, frame callbacks, ImGui tool UI.
- **`vcpkg.json`** (17 lines) -- Manifest with directx-headers 1.619.1, imgui (dx12+win32 binding), tinygltf, nlohmann-json.

**Build result**: 123 errors. All errors originate in vcpkg's `build/vcpkg_installed/x64-windows/include/directx/d3dx12_core.h` (DirectX-Headers v1.619.1). The header uses pre-C++20 constructs incompatible with MSVC `/std:c++20` two-phase name lookup. This is a known version-mismatch issue: vcpkg's `directx-headers` 1.619.1 provides newer d3dx12 helpers that reference D3D12 SDK types not present in Windows SDK 10.0.26100.0, while the Windows SDK's own D3D12 headers are used instead of the package's.

### Phase 2: Minimal Bootstrap (buildable)

Pivoted to a minimal Win32 EXE with no RtPbrSurvey dependency to prove the CMake/toolchain setup:

- **`src/main.cpp`** (52 lines) -- Simple WinMain that registers a window class, creates a window, runs a message loop. No D3D12, no ImGui.
- **`CMakeLists.txt`** (20 lines) -- Minimal CMake project: C++20, VS2022 generator, vcpkg toolchain, WIN32_EXECUTABLE property set.
- Removed `TankSandboxApp.h` and `TankSandboxApp.cpp`.

**Build result**: **Success**. TankSandbox.exe builds and runs (verified: starts, creates window, stays alive, can be quit with Esc).

### Parallel Work: Jolt Physics Investigation

- Created `Docs/opencode/jolt-physics-investigation/` session.
- Researched Jolt Physics integration: `TrackedVehicleController` API, CMake FetchContent method, WheelSettings TV, suspension modeling.
- Produced knowledge report sharing findings across sessions.

### Documentation Structure

- `Docs/opencode/rtpbrsurvey-scene-renderer-integration/` -- Current session (this one).
- `Docs/opencode/jolt-physics-investigation/` -- Jolt parallel session.
- `Docs/opencode/feature/rtpbrsurvey-submodule.md` -- Submodule investigation report.
- `Docs/opencode/feature/rtpbrsurvey-host-integration.md` -- Host integration investigation report.
- `Docs/opencode/roadmap/` -- OpenCode mirror of `Docs/roadmap/`.
- `Docs/opencode/rtpbrsurvey-requests/` -- OpenCode mirror of `Docs/rtpbrsurvey-requests/`.

---

## Current File Structure

```
C:\work\TankPhysicsSandbox/
  CMakeLists.txt          -- Minimal Win32 EXE (bootstrap)
  vcpkg.json              -- Manifest (directx-headers, imgui, tinygltf, nlohmann-json)
  External/
    RtPbrSurvey/          -- Git submodule at commit f90591d (origin/main)
      CMakeLists.txt      -- Static library wrapper (not used by root build yet)
  src/
    main.cpp              -- Minimal Win32 window app
  Docs/
    opencode/             -- OpenCode session/coordination docs
    roadmap/              -- Human-written roadmap
    rtpbrsurvey-requests/ -- Request 001 (CMake consumable target)
  AGENTS.md               -- Coordination rules for AIs
```

---

## Key Decisions

1. **RtPbrSurvey integration deferred**. The DirectX-Headers v1.619.1 build failure is the current blocker. Fix options:
   - Add `/permissive-` or relax C++20 conformance flags.
   - Patch `d3dx12_core.h` locally.
   - Use a specific tag/version of `directx-headers` that matches the Windows SDK.
   - Switch RtPbrSurvey to use NuGet D3D12 package (its original approach).
   - Use `FetchContent` to pin a compatible DirectX-Headers version.

2. **Build-then-integrate strategy**. Get a buildable CMake project first, then layer on RtPbrSurvey in the next iteration.

3. **Session folders in workspace**. `Docs/opencode/` holds all session docs. Coordination folder is inside the workspace (not `C:\work\RtPbrSurvey-agents`).

4. **AGENTS.md split into AI rules**. Sections: `## AI Rules (All AIs)`, `## OpenCode Rules`, `## Codex Rules`, `## Roadmap`.

---

## Next Steps

1. **Fix DirectX-Headers build error** in RtPbrSurvey static library target. Most promising approaches:
   - Try adding `/permissive-` to the RtPbrSurvey target compile options.
   - Try vcpkg `directx-headers` version without the override (use baseline version).
   - Try using the NuGet `Microsoft.Direct3D.D3D12` package instead of vcpkg's `directx-headers`.
2. **Re-link RtPbrSurvey** into TankSandbox once build passes.
3. **Add Jolt Physics** as a vcpkg dependency and create `TankController` skeleton.
4. **Add suspension/track control** per roadmap.

---

## Status

| Component | Status |
|-----------|--------|
| CMake project scaffold | Done |
| Minimal Win32 EXE | Done (builds and runs) |
| RtPbrSurvey CMake wrapper | Written (builds with 123 errors) |
| SceneRenderer host app | Written (code saved, not in current build) |
| Jolt Physics investigation | Knowledge shared |
| Build green | Partial (bootstrap only) |
