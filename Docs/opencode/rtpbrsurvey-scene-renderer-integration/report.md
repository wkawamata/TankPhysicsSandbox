# Report

Task: tank-scene-renderer-host-app

Status: queued

---

# Tank SceneRenderer Host App

Develop a Win32 host application that uses RtPbrSurvey's SceneRenderer layer for rendering.

## Workspace Inspected

`C:\work\TankPhysicsSandbox`

## Coordination Folder

`C:\work\RtPbrSurvey-agents\tank-scene-renderer-host-app`

## Current Git Branch and Status

- **Branch:** `feature/rtpbrsurvey-submodule`
- **Status:** clean

## Session Goal

Create a minimal Tank-hosted rendering proof:

- Tank owns the Win32 window and `GraphicsDevice`.
- Construct `RtPbrSurvey::SceneRenderer`.
- Build a fixed cube/sphere scene with `Engine::SceneBuilder`.
- Add a small Tank ImGui window through `SceneRenderer::SetToolUiHandler()`.
- Keep Jolt and vehicle physics out of this first renderer proof.

## Key APIs (from submodule at f90591d)

- `RtPbrSurvey::SceneRenderer` — wraps `RtPbrSurveyEngine`, exposes `Initialize()`, `RunFrame()`, `SetScene()`, `SetToolUiHandler()`.
- `Engine::SceneBuilder` — builds `Scene` with `AppendCube()`, `AppendSphere()`, `AddMaterial()`, `AddInstance()`, `SetCamera()`.
- `RtPbrSurvey::DebugCameraController` — extracted camera input handling.
- `SceneRenderer::SetToolUiHandler(ToolUiHandler)` — registers a `std::function<void()>` callback for tool UI.

## Planned Steps

1. Create minimal CMakeLists.txt and `TankSandboxApp` implementing `Platform::IApplication`.
2. Render a procedural cube/sphere scene through `SceneRenderer`.
3. Add Tank ImGui window through `SetToolUiHandler()`.
4. Verify build and runtime.

Status: queued
