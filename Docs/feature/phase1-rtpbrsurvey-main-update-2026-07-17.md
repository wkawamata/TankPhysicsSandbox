# Phase 1 RtPbrSurvey Main Update

Date: 2026-07-17

Branch: `main`

## Summary

Tank updated `External/RtPbrSurvey` to the latest RtPbrSurvey `origin/main` and verified that the upstream CMake renderer target can build the Tank host app.

Pinned RtPbrSurvey commit:

- `dcd54a3 Fix SceneRenderer CMake host integration`

## Tank Changes

- Link `TankSandbox` against `RtPbrSurvey::SceneRenderer`.
- Use the Win32 Tank app entry point from `src/main.cpp`.
- Keep `External/RtPbrSurvey` clean at the pinned commit.
- Keep Tank compilation on `/permissive` while RtPbrSurvey public headers still expose MSVC extension patterns such as taking the address of `CD3DX12_*` temporaries.

## Upstream Fixes Consumed

Two RtPbrSurvey CMake integration gaps were found while validating `a64338c` and then fixed upstream in `dcd54a3`.

1. `Renderer/StreamlineAdapter.cpp` is referenced by `TemporalUpscalerSupport.cpp` but is not included in the upstream `RtPbrSurvey.SceneRenderer` CMake source list.
2. `rtpbrsurvey_copy_runtime_files(TankSandbox)` is not safe to call from the parent Tank CMake scope because the helper resolves some RtPbrSurvey paths from the caller scope.

Tank no longer carries workarounds for these issues. The current Tank CMake path uses:

```cmake
target_link_libraries(TankSandbox PRIVATE RtPbrSurvey::SceneRenderer)
rtpbrsurvey_copy_runtime_files(TankSandbox)
```

## Verification

Local configure used the existing RtPbrSurvey package cache:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S . -B build -DRTPBRSURVEY_PACKAGES_DIR=C:/work/RtPbrSurvey-work/packages
```

Build command:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target TankSandbox
```

Result:

- Configure succeeded.
- `TankSandbox` Debug build succeeded.
- `TankSandbox.exe` was launched briefly with `-LogToFile`.
- D3D12 InfoQueue was obtained successfully.
- No startup D3D12 error was logged during the short launch check.
- `External/RtPbrSurvey` remained clean.

## Upstream Feedback

The missing `StreamlineAdapter.cpp` source and parent-scope runtime copy helper issue were reported to the RtPbrSurvey coordination thread and fixed in `dcd54a3`.

## Next Step

Commit and push the Tank-side submodule pointer and CMake cleanup after review.
