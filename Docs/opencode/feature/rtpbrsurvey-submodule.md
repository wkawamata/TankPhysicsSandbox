# RtPbrSurvey Submodule Investigation

## Purpose

Evaluate using `RtPbrSurvey` as a Git submodule for the Tank Physics Sandbox test rendering system.

- Repository: https://github.com/wkawamata/RtPbrSurvey.git
- Local reference clone: `C:\work\RtPbrSurvey-work`
- Reference clone rule: read-only. Do not edit `C:\work\RtPbrSurvey-work` for this investigation.
- Reference commit checked: `e5d8e576700d2f6b5eff8f2684c64f5252d054b5`

## Current RtPbrSurvey Shape

`RtPbrSurvey` is a standalone DirectX 12 rendering survey application. It already has:

- DirectX 12 PBR rendering.
- glTF scene loading and sample scenes.
- Debug visualization modes.
- ImGui-based runtime controls.
- CLI automation for debug-layer logging and scene selection.
- A separated layout including `App/`, `Engine/`, `Renderer/`, `Scene/`, `Shaders/`, `Shared/`, `Platform/`, `Ui/`, and `Assets/`.

The project is currently Visual Studio/MSBuild based:

- `RtPbrSurvey.sln`
- `RtPbrSurvey.vcxproj`
- `Restore-NuGet.ps1`
- `packages.config`
- `vcpkg.json`

## Fit For Tank Physics Sandbox

Using `RtPbrSurvey` as a submodule is useful for early visual verification because it already provides a rich DirectX 12 renderer, camera controls, debug UI, and scene infrastructure.

The main value is to avoid building a renderer before the tank physics behavior is understood.

## Architectural Boundary

Tank Physics Sandbox must keep physics independent from rendering.

Recommended dependency direction:

- `TankPhysics`: owns Jolt integration, `TankController`, tuning data, and simulation state.
- `TankSandboxApp`: owns the executable loop and connects physics to a renderer.
- `RtPbrSurvey` submodule: provides optional test rendering and debug visualization.

Do not let the physics module include or depend on `RtPbrSurvey` headers.

The bridge should be data-oriented:

- Physics exports tank transforms, wheel/suspension state, contact state, and debug primitives.
- Rendering consumes snapshots of that state.
- JSON tuning belongs to the physics/sandbox layer, not to the renderer.

## Submodule Placement

Recommended path:

```text
External/RtPbrSurvey
```

Rationale:

- Keeps third-party or external project code outside first-party source directories.
- Makes it clear that edits inside the submodule affect a separate repository.
- Leaves room for other dependencies such as Jolt under `External/`.

Possible command when ready:

```powershell
git submodule add https://github.com/wkawamata/RtPbrSurvey.git External/RtPbrSurvey
```

Do not run this until the repository has a committed baseline and the intended branch/commit policy is clear.

## Integration Options

### Option A: Loose Submodule Reference

Keep `RtPbrSurvey` as a standalone submodule and build it separately.

Pros:

- Lowest risk.
- No immediate CMake/MSBuild integration problem.
- Keeps Tank Physics Sandbox focused on Jolt and physics first.

Cons:

- Manual data bridge at first.
- Less convenient one-command build.

Recommended for the first phase.

### Option B: Sandbox App Uses RtPbrSurvey Internals

Add a Tank-specific app or scene inside the submodule that visualizes tank state.

Pros:

- Fast path to useful rendering.
- Reuses existing ImGui, camera, scene, and D3D12 systems.

Cons:

- Requires changes to the `RtPbrSurvey` repository.
- May blur ownership unless the bridge is kept narrow.
- Requires careful submodule commit management.

Use after the physics module has a stable minimal state snapshot API.

### Option C: Full CMake Integration

Convert or wrap `RtPbrSurvey` so it participates in the Tank Physics Sandbox CMake build.

Pros:

- Cleaner long-term build story.

Cons:

- High early cost.
- Risks mixing rendering build-system work with physics development.
- `RtPbrSurvey` is currently MSBuild-oriented.

Not recommended for the initial phase.

## Recommendation

Use `RtPbrSurvey` as a submodule, but initially treat it as a separate test-rendering application.

Start with Option A:

1. Build Tank Physics Sandbox as a CMake/Jolt project.
2. Define a small renderer-agnostic tank state snapshot.
3. Add `RtPbrSurvey` as `External/RtPbrSurvey` after the baseline is committed.
4. Create a narrow bridge or sample-export path for visualization.
5. Only then consider modifying `RtPbrSurvey` to host a tank visualization scene.

## Open Questions

- Should Tank Physics Sandbox pin the submodule to `RtPbrSurvey` `main`, or should it use a dedicated branch for tank visualization changes?
- Should the first bridge be file-based JSON snapshots, in-process C++ structs, or a small debug IPC stream?
- Should `RtPbrSurvey` changes live directly in the submodule repository, or should they first be prototyped in a branch of `C:\work\RtPbrSurvey-work`?

## Guardrails

- Do not edit `C:\work\RtPbrSurvey-work` while using it as a reference clone.
- Do not make `Physics` depend on `Rendering`.
- Do not make Jolt depend on DirectX 12 types.
- Keep submodule changes separate from Tank Physics Sandbox changes.
- Keep build-system changes separate from rendering feature changes.
