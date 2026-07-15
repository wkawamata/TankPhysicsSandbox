# RtPbrSurvey Host Integration Report

Date: 2026-07-16

Commit: `79e6b40 Update RtPbrSurvey host integration`

Branch: `feature/rtpbrsurvey-submodule`

## Summary

TankSandbox was updated to consume the latest RtPbrSurvey host-facing renderer fixes and to keep the minimal renderer proof buildable.

The Tank app now owns the host-side application shell, creates a fixed `SceneBuilder` cube scene, initializes `RtPbrSurvey::SceneRenderer`, reloads scene resources, and renders the scene without manually forcing `SetDisplayInstanceCount(1)` after reload.

## Changes

- Updated `External/RtPbrSurvey` submodule pointer to `07b2f5b Preserve visible instance count on scene reload`.
- Removed Tank-side reliance on `SetDisplayInstanceCount(1)` after `ReloadSceneResources()`.
- Kept the Tank scene setup on the revised RtPbrSurvey contract:

```cpp
m_sceneRenderer.SetScene(builder.GetScene());
m_sceneRenderer.ReloadSceneResources(builder.GetScene());
```

- Added Tank CMake integration pieces needed by the host app:
  - DirectX headers package lookup.
  - Generated include directory exposure for forwarded DirectX headers.
  - RtPbrSurvey asset copy into the Tank executable output folder.
- Added D3D12 debug log capture through `-LogToFile`.
- Added feedback notes for the earlier `SceneBuilder::AddInstance()` matrix convention problem.

## RtPbrSurvey Feedback Accepted

Two Tank-side usability issues were reported upstream and accepted in RtPbrSurvey:

1. `SceneBuilder::AddInstance()` should accept ordinary DirectXMath world matrices at the public API boundary.
2. `ReloadSceneResources()` should not leave visible instance count at zero by default after rebuilding scene resources.

RtPbrSurvey now preserves or initializes visible instance count during scene reload, clamped to the new scene capacity. This makes the host-side first scene path substantially less surprising.

## Verification

Verified locally:

- `TankSandbox` Debug CMake build succeeded.
- `TankSandbox.exe` was launched briefly with `-LogToFile`.
- D3D12 InfoQueue was obtained successfully.
- No startup D3D12 error was logged during the short launch check.
- `git diff --check` passed after CRLF normalization.

Build command:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target TankSandbox
```

## Remaining Issues

- `External/RtPbrSurvey` still contains local Tank-side CMake integration and C++ compatibility edits inside the submodule working tree.
- These local submodule edits are required for the current Tank CMake path, but they are not represented by the parent repository commit except as a dirty submodule state.
- A cleaner follow-up should move the reusable CMake target support into RtPbrSurvey upstream, then update the submodule pointer again.
- `Docs/opencode` contains OpenCode-owned changes and was intentionally left untouched by Codex.
- Temporary debug logging and adapter diagnostics should eventually be reviewed and either kept behind explicit diagnostic switches or removed.

## Recommended Next Step

Make the RtPbrSurvey CMake-consumable library target an upstream feature instead of a Tank-local submodule edit. After that, Tank should be able to reference a clean submodule commit without carrying local source changes under `External/RtPbrSurvey`.
