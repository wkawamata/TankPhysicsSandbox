# Request 009: Group DLSS Debug Controls

Date: 2026-09-22

## Context

`SceneRendererDebugUi::DrawContents()` currently displays the `DLSS SR` and
`DLSS RR` sections as consecutive top-level content in the host's Render
Settings scrolling region.

Tank needs these related renderer controls to be collapsed together so they do
not consume vertical space when DLSS tuning is not active.

## Requested UI Change

- Add a `DLSS` `ImGui::CollapsingHeader`.
- Place the complete existing `DLSS SR` and `DLSS RR` UI sections inside it.
- Keep the existing availability, backend, status, simple/detail mode, and
  settings controls unchanged.
- Preserve all current renderer behavior and settings persistence.
- Keep this renderer-owned and backend-neutral; do not add Tank-specific UI
  state or names.

The default expanded/collapsed policy may follow the existing Debug UI style.

## Verification

- Build the CMake `RtPbrSurvey::SceneRenderer` target and standalone app.
- Verify the embedded `DrawContents()` path has a single `DLSS` header and
  both SR and RR controls remain accessible when it is expanded.
- Confirm the existing standalone `Draw()` path remains usable.
