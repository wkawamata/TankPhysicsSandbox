# Requests To RtPbrSurvey

## Request 001: CMake-Consumable SceneRenderer Target

Status: draft

Tank Physics Sandbox wants to consume `RtPbrSurvey::SceneRenderer` from a CMake-based host application without copying RtPbrSurvey source lists into the Tank repository.

### Context

Tank Physics Sandbox uses:

- C++20
- CMake
- Visual Studio 2022
- Windows
- `External/RtPbrSurvey` as a Git submodule

RtPbrSurvey currently exposes useful host-side runtime types:

- `RtPbrSurvey::SceneRenderer`
- `Engine::SceneBuilder`
- `RtPbrSurvey::DebugCameraController`
- `SceneRenderer::SetToolUiHandler()`

However, RtPbrSurvey is still primarily shaped as a Visual Studio/MSBuild application project. That makes it awkward for the Tank CMake app to link only the reusable renderer layer.

### Requested Outcome

Provide a small, documented way for an external CMake host to consume the renderer layer.

Possible shapes:

- A CMake target for a reusable RtPbrSurvey runtime/static library.
- A documented source-list include file for CMake hosts.
- A small host integration sample showing how to construct `GraphicsDevice`, `SceneRenderer`, and a scene built by `SceneBuilder`.

The preferred direction is a real CMake target if it can be added without disrupting the existing Visual Studio workflow.

### Non-Goals

- Do not rename `SceneRenderer` to a backend-specific name.
- Do not add Tank-specific APIs to RtPbrSurvey.
- Do not require Jolt or Tank physics concepts in RtPbrSurvey.
- Do not force Tank Physics Sandbox to depend on RtPbrSurvey from the physics layer.

### Tank-Side Use Case

The first Tank host proof should:

- Own the Win32 window and `GraphicsDevice`.
- Construct `RtPbrSurvey::SceneRenderer`.
- Build a fixed cube/sphere scene with `Engine::SceneBuilder`.
- Add a small Tank ImGui window through `SceneRenderer::SetToolUiHandler()`.
- Keep Jolt and vehicle physics out of the first renderer proof.
