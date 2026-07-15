# Task

Objective:

Develop a Win32 host application that uses RtPbrSurvey's SceneRenderer layer for rendering.

Inputs:

- Workspace: `C:\work\TankPhysicsSandbox`
- Report: `C:\work\RtPbrSurvey-agents\tank-scene-renderer-host-app\report.md`

Scope:

- In scope:
- Out of scope:

Expected output:

- Update `report.md` as you go.
- End with `Status: done` or `Status: blocked`.
- The final non-empty line of `report.md` must be exactly `Status: done` or exactly `Status: blocked`.
- Do not append anything after the final `Status:` line.

Rules:

- Keep changes small and reviewable.
- Do not change branch history.
- Do not commit, push, merge, reset, checkout, or switch branches unless explicitly requested.


Detailed scope:

- In scope:
  - Inspect `C:\work\TankPhysicsSandbox` only.
  - Develop a minimal Win32 host app that owns the window and renderer lifetime.
  - Use `RtPbrSurvey::SceneRenderer` for rendering.
  - Use `Engine::SceneBuilder` to build a fixed cube/sphere scene.
  - Use `SceneRenderer::SetToolUiHandler()` for Tank ImGui windows.
  - Keep Jolt and vehicle physics out of the first renderer proof.
- Out of scope:
  - Do not edit files outside the workspace.
  - Do not update submodules unless explicitly requested.
  - Do not commit, push, merge, reset, checkout, or switch branches unless explicitly requested.

Expected report details:

- Workspace inspected: `C:\work\TankPhysicsSandbox`.
- Coordination folder/report path: this task folder under `C:\work\RtPbrSurvey-agents`.
- Current git branch and short status.
- Step-by-step implementation progress.
- Files created or modified.
- Build and runtime verification results.
- Final non-empty line exactly `Status: done` or `Status: blocked`.

Rules:

- Preserve UTF-8/CRLF if you touch only text files.
- Do not create mojibake.
