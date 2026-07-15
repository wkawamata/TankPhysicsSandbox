# Task

Objective:

Investigate Jolt Physics integration into Tank Physics Sandbox.

Inputs:

- Workspace: `C:\work\TankPhysicsSandbox`
- Report: `C:\work\RtPbrSurvey-agents\jolt-physics-investigation\report.md`

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
  - Research Jolt Physics library integration options (submodule, FetchContent, package manager).
  - Identify minimal Jolt integration needed for a buildable physics proof (world, ground body, one dynamic body).
  - Review Jolt's `TrackedVehicleController` API for future tank sandbox use.
  - Document recommended integration path and risks.
- Out of scope:
  - Do not edit any source files.
  - Do not update submodules.
  - Do not build.
  - Do not commit, push, merge, reset, checkout, or switch branches.

Expected report details:

- Workspace inspected: `C:\work\TankPhysicsSandbox`.
- Coordination folder/report path: this task folder under `C:\work\RtPbrSurvey-agents`.
- Jolt Physics integration options and recommendation.
- Minimal integration steps for first physics proof.
- Risks and blockers.
- State that this was research-only and source files were not edited.
- Final non-empty line exactly `Status: done` or `Status: blocked`.

Rules:

- Research-only. Do not edit source files.
- Preserve UTF-8/CRLF if you touch only `report.md`.
- Do not create mojibake.
