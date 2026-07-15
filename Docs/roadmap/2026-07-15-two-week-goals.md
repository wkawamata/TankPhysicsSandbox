# Two-Week Goals: 2026-07-15 to 2026-07-29

## Purpose

Define the broad goals for the next two weeks of Tank Physics Sandbox development.

The main objective is to build a stable foundation for renderer-host integration and Jolt-based tank physics work without mixing physics, rendering, and build-system changes into one large step.

## Goal 1: Stabilize The Tank Project Foundation

- Keep the repository buildable with CMake, C++20, Visual Studio 2022, and Windows.
- Keep CRLF line endings for project text and source files.
- Keep project documentation organized under `Docs/`.
- Keep changes small and commit at clean boundaries.

Expected outcome:

- The minimal `TankSandbox` app remains buildable.
- README and docs describe the current project shape clearly.
- New work has obvious places to live.

## Goal 2: Prepare RtPbrSurvey As The Test Rendering Backend

- Keep `External/RtPbrSurvey` updated deliberately as a submodule.
- Track Tank-side requests to RtPbrSurvey under `Docs/rtpbrsurvey-requests/`.
- Resolve or document the CMake consumption path for `RtPbrSurvey::SceneRenderer`.
- Avoid adding Tank-specific APIs inside RtPbrSurvey.

Expected outcome:

- Tank has a clear path to construct `GraphicsDevice`, `RtPbrSurvey::SceneRenderer`, and a fixed `Engine::SceneBuilder` scene.
- Any renderer API friction is documented as a request before making RtPbrSurvey changes.

## Goal 3: Build The Minimal Renderer Host Proof

- Create a host app shape where Tank owns the window and renderer lifetime.
- Render a fixed cube/sphere scene through `SceneRenderer`.
- Add a small Tank ImGui window through `SceneRenderer::SetToolUiHandler()` or the cleanest host UI callback.
- Keep Jolt and tank physics out of this proof.

Expected outcome:

- A minimal Tank-hosted rendering proof runs independently of physics.
- The rendering boundary is clear enough for later tank state visualization.

## Goal 4: Define Physics/Rendering Data Boundaries

- Draft the first renderer-agnostic tank state snapshot.
- Keep DirectX 12 types out of physics-facing data.
- Keep Jolt types out of rendering-facing data where practical.
- Define debug primitive needs for early tank visualization.

Expected outcome:

- `TankController` has a clear intended responsibility.
- Rendering can consume exported state without depending on physics internals.

## Goal 5: Prepare Jolt Integration

- Decide how Jolt should enter the project: submodule, FetchContent, or package manager.
- Add only the smallest Jolt integration needed for a buildable physics proof.
- Start with a world, ground body, and one dynamic body before tracked vehicle work.

Expected outcome:

- Jolt integration direction is chosen and documented.
- The project is ready for `TrackedVehicleController` work without destabilizing the renderer proof.

## Out Of Scope For This Two-Week Window

- Full tank gameplay.
- Turret, recoil, track animation, and terrain friction.
- Large RtPbrSurvey refactors from the Tank repository.
- Deep Jolt `TrackedVehicleController` tuning before renderer and project boundaries are stable.

## Success Criteria

By 2026-07-29, the project should have:

- A clean, documented CMake app foundation.
- A deliberate RtPbrSurvey submodule update/request workflow.
- A minimal renderer host proof or a precisely documented blocker.
- A first draft of the physics/rendering state boundary.
- A documented plan for Jolt integration.
