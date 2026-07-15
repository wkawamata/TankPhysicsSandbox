# RtPbrSurvey Host Integration Notes

## Status

`External/RtPbrSurvey` has been updated to a commit that includes the reusable renderer layer:

- `RtPbrSurvey::SceneRenderer`
- `Engine::SceneBuilder`
- `RtPbrSurvey::DebugCameraController`
- `SceneRenderer::SetToolUiHandler()`

The Tank repository now has a minimal CMake application shell in `Source/TankSandbox/Main.cpp`.

## Current Boundary

Do not wire Jolt or tank physics into this step.

The first rendering proof should be a host-owned scene with fixed primitives:

1. The Tank app owns the Win32 window and `GraphicsDevice`.
2. The Tank app constructs `RtPbrSurvey::SceneRenderer`.
3. The Tank app creates a fixed scene with `Engine::SceneBuilder`.
4. The Tank app passes a Tank tool window through `SceneRenderer::SetToolUiHandler()`.
5. Physics later exports a renderer-agnostic state snapshot consumed by this host layer.

## Integration Blocker

`RtPbrSurvey` is still shaped as a Visual Studio/MSBuild application project, not a CMake-consumable static library.

Because Tank Physics Sandbox uses CMake, the next clean increment should avoid copying a large source list into Tank. Prefer one of these small changes:

- Add a CMake target or static library boundary to `RtPbrSurvey` for `SceneRenderer` host consumption.
- Add a dedicated Tank host executable target that deliberately consumes the existing RtPbrSurvey project shape.

If the renderer API feels awkward from the Tank side, document the requirement here instead of adding Tank-specific APIs inside `RtPbrSurvey`.

## Expected Host Flow

Pseudo-flow for the eventual Tank host app:

```cpp
GraphicsDevice graphicsDevice;

GraphicsDeviceDesc deviceDesc = {};
deviceDesc.hwnd = hwnd;
deviceDesc.swapChainWidth = width;
deviceDesc.swapChainHeight = height;
deviceDesc.bufferCount = RtPbrSurveyEngine::kSwapChainBufferCount;
deviceDesc.swapChainFormat = RtPbrSurveyEngine::kSwapChainFormat;
graphicsDevice.Initialize(deviceDesc);

RtPbrSurvey::SceneRenderer renderer(graphicsDevice);
renderer.Initialize(width, height);

Engine::SceneBuilder builder;
const uint32_t materialId = builder.AddSolidColorMaterial(180, 180, 180, 255);
builder.AppendCube(1.0f, materialId);
builder.AddInstance(DirectX::XMMatrixIdentity(), materialId);

renderer.SetScene(builder.GetScene());
renderer.ReloadSceneResources(builder.GetScene());
renderer.SetToolUiHandler([]()
{
    ImGui::Begin("Tank");
    ImGui::TextUnformatted("Tank sandbox renderer proof");
    ImGui::End();
});
```
