# Feedback: RtPbrSurvey Host Bring-Up

Date: 2026-07-16

## Summary

TankSandbox reached first RtPbrSurvey-backed startup, but two runtime packaging gaps caused misleading failures:

1. `D3D12CreateDevice` failed for the RTX 2080 Ti with `D3D12_ERROR_INVALID_REDIST`.
2. `ReadDataFromFile` failed while loading `shaders_DebugLine_VSMain.cso`.

Both were caused by the Tank CMake host not reproducing runtime outputs that the RtPbrSurvey MSBuild project normally creates.

## What Happened

`D3D12GetDebugInterface` and adapter creation worked in `C:\work\RtPbrSurvey-work`, so the PC environment was not the root cause.

The Tank executable had the D3D12 Agility SDK exports:

- `D3D12SDKVersion`
- `D3D12SDKPath`

However, `D3D12SDKPath` points to `.\D3D12\`, and `build\Debug\D3D12\` did not contain the required redist DLLs.

After copying:

- `D3D12Core.dll`
- `D3D12SDKLayers.dll`

the RTX 2080 Ti was selected correctly.

The shader failure had the same pattern. RtPbrSurvey's MSBuild project compiles HLSL custom build items into `.cso` files beside the executable, but the CMake host did not yet run those shader build steps. Adding shader compilation to the RtPbrSurvey CMake target produced the expected `.cso` files in `build\Debug`.

## Confirmed Root Causes

- Tank CMake build did not copy D3D12 Agility SDK runtime files into the executable output folder.
- Tank CMake build did not compile RtPbrSurvey HLSL files into runtime `.cso` files.
- The first investigation looked at adapter selection before comparing the known-good RtPbrSurvey output folder against the Tank output folder.

## Fixes Applied

- Added CMake post-build copy for the D3D12 Agility SDK runtime DLLs.
- Added RtPbrSurvey CMake shader compilation outputs for the shader `.cso` files expected at runtime.
- Added adapter diagnostics to show adapter names and `D3D12CreateDevice` HRESULTs.
- Added WARP command-line support in TankSandbox for fallback testing.

## Prevention Checklist

Before debugging renderer internals, compare the Tank executable output folder with the known-good RtPbrSurvey output folder.

Required runtime items include:

- `D3D12/D3D12Core.dll`
- `D3D12/D3D12SDKLayers.dll`
- `*.cso` shader outputs
- `dxcompiler.dll` and `dxil.dll` if runtime shader compilation or tooling requires them
- `WinPixEventRuntime.dll` if linked by the build

When `D3D12CreateDevice` fails:

- Log the adapter name, vendor id, device id, flags, and HRESULT.
- Decode the HRESULT before changing adapter enumeration.
- If the HRESULT is `D3D12_ERROR_INVALID_REDIST`, inspect Agility SDK packaging first.

When `ReadDataFromFile` fails for `.cso`:

- Confirm the expected filename.
- Confirm the file exists next to the executable.
- Confirm CMake has a build rule for the corresponding HLSL entry point.

## Follow-Up

The temporary adapter diagnostics are useful during bring-up, but should later become either a controlled debug option or be removed before the renderer integration is considered stable.

## API Usability Feedback

`Engine::SceneBuilder::AddInstance(world, materialId)` is easy to misuse because the expected matrix convention is not obvious from the function name or signature.

Existing RtPbrSurvey scene code commonly stores transposed matrices in `InstanceData.world`, for example:

```cpp
XMStoreFloat4x4(&instance.world, XMMatrixTranspose(transform));
```

However, Tank-side host code naturally tends to pass a DirectXMath world matrix directly:

```cpp
builder.AddInstance(XMMatrixTranslation(0.0f, 0.5f, 0.0f), materialId);
```

If `SceneBuilder::AddInstance()` expects the caller to already provide the renderer/storage convention, this should be made explicit. Otherwise, external hosts can create apparently valid scenes that render nothing or render in unexpected positions.

This also makes host code unnecessarily verbose. Requiring every caller to wrap ordinary transforms with `XMMatrixTranspose(...)` spreads renderer storage details into application code and makes simple scene setup harder to read.

Possible improvements:

- Document the expected matrix convention directly on `SceneBuilder::AddInstance()`.
- Rename or split the API to make the convention explicit, such as `AddInstanceStorageMatrix()` versus `AddInstanceWorldMatrix()`.
- Prefer having `SceneBuilder::AddInstance()` accept a normal DirectXMath world matrix and perform any required transpose/storage conversion internally.
- Add a small host integration sample that shows the correct matrix path for fixed cube/sphere primitives.

Recommendation:

For external host usability, `SceneBuilder` should accept normal world matrices at its public boundary and hide renderer storage details internally. If changing behavior would break existing scenes, add a new explicit helper and deprecate the ambiguous path gradually.
