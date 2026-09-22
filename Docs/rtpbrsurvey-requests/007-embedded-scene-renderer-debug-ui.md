# Request 007: Embedded SceneRenderer Debug UI

Date: 2026-09-22

## Context

Tank Physics Sandbox currently displays these two independent host windows:

- `Renderer Settings`: Tank-owned Save / Load / Reset and screenshot capture commands.
- `RtPbrSurvey Debug`: renderer-owned diagnostics and controls through
  `SceneRendererDebugUi::Draw()`.

For repeated vehicle tuning, this wastes screen space and separates controls that
belong to one renderer-settings workflow. Tank needs one host-owned
`Render Settings` window with a non-scrolling command header and a scrolling
renderer-debug content region.

## Requested Host-Public Contract

Keep the existing standalone window API source-compatible:

```cpp
SceneRendererDebugUi::Draw(renderer, open, windowName, environment);
```

Add an embedding entry point that does not call `ImGui::Begin()` or
`ImGui::End()`:

```cpp
SceneRendererDebugUi::DrawContents(
    SceneRenderer& renderer,
    EnvironmentMappingUiState* environment = nullptr);
```

Equivalent backend-neutral naming is acceptable.

The host will own the window and child scrolling region. It will use the
already-public `SceneRenderer::GetUiFrameContext()` to show `FrameIndex` and
`CPU Frame` in its fixed header.

## UI Requirements

- `DrawContents()` must be safe to call inside a host `ImGui::BeginChild()`.
- Do not create an additional top-level `RtPbrSurvey Debug` window in that path.
- Preserve the existing standalone `Draw()` behavior for RtPbrSurvey itself.
- Keep renderer-owned controls, including RenderGraph window behavior, available
  from the embedded content path.
- Move the `Temporal Upscaler` availability/backend/status text from the general
  frame summary into the `DLSS SR` section.
- Move the `DLSS Ray Reconstruction` availability/backend/status text from the
  general frame summary into the `DLSS RR` section.
- Keep Ray Tracing capability diagnostics renderer-owned; it may remain at the
  top of the scrolling renderer content.

## Ownership Boundaries

RtPbrSurvey owns renderer debug controls and their state. Tank owns the
`Render Settings` window, its fixed Save / Load / Reset / Capture commands,
its visibility toggle, and layout.

Do not add Tank-specific names, file paths, physics concepts, or UI layout
policy to RtPbrSurvey.

## Verification

- Add a focused UI/API test where practical, or document why ImGui interaction
  remains a manual verification.
- Build `RtPbrSurvey::SceneRenderer` through CMake and the standalone app.
- Confirm the existing standalone window remains usable.
- Confirm an external host can place `DrawContents()` inside an ImGui child
  region without a second renderer-debug window.
