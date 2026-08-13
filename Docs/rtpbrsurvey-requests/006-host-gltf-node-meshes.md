# Request 006: Host glTF Node Meshes

Date: 2026-08-13

## Context

Tank Physics Sandbox now tracks this authored model:

`Assets/TankModels/TankModel-2026-08-13-v001.glb`

The GLB scene contains three named mesh nodes:

- `Body`
- `Cannon`
- `Side`

Tank needs to load the asset once during application startup, then add each named part as a separate scene instance. Each part must be independently visible while remaining aligned with the physics dummy model.

The current `SceneBuilder::AddGltfMesh(path)` flattens the complete default scene into one `SceneMeshId`, so the host cannot toggle named parts independently.

## Requested Host-Public Contract

A minimal shape could be:

```cpp
struct GltfSceneAsset;

std::optional<GltfSceneAsset> LoadGltfSceneAsset(const std::string& path);
std::vector<std::string> GetGltfMeshNodeNames(const GltfSceneAsset& asset);

std::optional<SceneMeshId> SceneBuilder::AddGltfNodeMesh(
    const GltfSceneAsset& asset,
    const std::string& nodeName);
```

Equivalent backend-neutral naming is acceptable.

## Requirements

- Preserve node transforms when extracting a node mesh.
- Preserve glTF materials and textures using the same global remapping rules as `AddGltfMesh()`.
- Permit one CPU-side asset load followed by multiple node mesh additions.
- Return a clear failure for missing or duplicate node names.
- Keep existing `AddGltfMesh(path)` behavior source compatible.
- Do not add Tank-specific names or policies to RtPbrSurvey.
- Add tests using a multi-node glTF/GLB fixture.
- Document object lifetime and whether the loaded asset can be released after the meshes are added to `SceneBuilder`.

## Tank Integration After Merge

1. Load the GLB during `TankSandboxApp` initialization, before entering a physics scene.
2. Build separate `SceneMeshId` values for `Body`, `Cannon`, and `Side`.
3. Add one scene instance per part and apply the tracked vehicle body transform each frame.
4. Add independent UI checkboxes for Dummy, Body, Cannon, and Side.
5. Keep Physics independent from Rendering.

