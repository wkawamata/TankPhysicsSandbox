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

## Request 002: Host-Consumable SceneRenderer Debug UI

Status: requested

Tank Physics Sandbox wants to display RtPbrSurvey renderer diagnostics alongside its own ImGui physics tools.

### Context

RtPbrSurvey currently implements its `Debug` window as `App::DrawDebugUi(RtPbrSurveyApp&, const UiFrameContext&)`. That function depends on `RtPbrSurveyApp` private state, scene selection, debug camera state, and app-owned material settings, so an external `SceneRenderer` host cannot call it.

`SceneRenderer::SetToolUiHandler()` provides the opposite direction: it lets a host inject its own UI, but it does not expose RtPbrSurvey's renderer debug UI to the host.

### Requested Outcome

Extract a host-consumable renderer debug UI layer with a backend-neutral public name, for example:

```cpp
RtPbrSurvey::SceneRendererDebugUi::Draw(
    RtPbrSurvey::SceneRenderer& renderer,
    Engine::Scene& scene);
```

The exact API shape is open to upstream design. It should use `SceneRenderer` public APIs where practical and avoid requiring an instance of `RtPbrSurveyApp`.

The first reusable subset should cover renderer-owned diagnostics and controls such as:

- Frame index and CPU frame time.
- Ray tracing support and tier.
- Temporal upscaler backend and status.
- Back-buffer clear color.
- Render view mode.
- Shadow settings.
- Hybrid reflection settings.
- Tone mapping controls.

App-specific controls may remain in `RtPbrSurveyApp`, including scene selection, app camera mode and speed, loaded-scene metadata, close-scene behavior, and app-specific save/reset workflows.

### Integration Expectation

Tank should be able to draw both windows in the same ImGui frame:

```cpp
DrawTankPhysicsUi();
rendererDebugUi.Draw(sceneRenderer, scene);
```

The reusable debug UI should be part of the upstream CMake `RtPbrSurvey::SceneRenderer` consumption path, or exposed through a separate upstream target that links cleanly beside it.

### Non-Goals

- Do not add Tank physics concepts to RtPbrSurvey.
- Do not expose `RtPbrSurveyApp` private fields merely to make the existing function callable.
- Do not require the external host to construct the full RtPbrSurvey application.
- Do not move scene selection or Tank-specific UI into the renderer layer.
