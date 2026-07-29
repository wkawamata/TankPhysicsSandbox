# Request 004: Host Cylinder Primitive and Per-Instance Mesh Selection

Status: requested

Tank Physics Sandbox needs a low-poly cylinder primitive for road-wheel
visualization. A 16-sided cylinder is sufficient for the current debug model.

## Context

`Engine::SceneBuilder` currently provides `AppendCube()` and `AppendSphere()`,
but all appended geometry is stored in one shared `SceneMesh`. Every scene
instance therefore renders that same combined mesh.

Tank currently uses one appended cube for the floor, hull, tracks, debug
markers, and obstacles. Calling a future `AppendCylinder()` on the same builder
would append cylinder geometry to the shared mesh and make every existing
instance render both the cube and cylinder.

Tank can approximate each wheel using 16 cube segments, but that requires
hundreds of instances and does not generalize to other host applications.

## Requested Outcome

Provide a backend-neutral SceneBuilder contract that supports:

- Creating a low-poly cylinder with a host-selected radial segment count.
- Selecting a mesh or primitive per scene instance.
- Using cube, sphere, cylinder, and loaded mesh geometry in the same scene.
- Preserving ordinary DirectXMath world-matrix input for instances.
- Preserving per-instance material selection.
- Remaining usable from the `RtPbrSurvey::SceneRenderer` CMake target.

One possible shape is:

```cpp
using SceneMeshId = uint32_t;

SceneMeshId SceneBuilder::AddCube(float size);
SceneMeshId SceneBuilder::AddSphere(float radius, int stacks, int slices);
SceneMeshId SceneBuilder::AddCylinder(
    float radius,
    float height,
    int radialSegments);

void SceneBuilder::AddInstance(
    SceneMeshId meshId,
    DirectX::FXMMATRIX world,
    uint32_t materialId);
```

The exact API may differ. The important requirement is that adding cylinder
geometry must not change the geometry rendered by existing cube instances.

## Cylinder Convention

- Cylinder axis should be explicitly documented.
- Tank prefers a unit cylinder whose axis can be transformed normally by the
  host; no Tank-specific axis or wheel API is required.
- End caps are required.
- Flat side normals are acceptable for a 16-sided debug wheel.
- UV generation is useful but not required for the first increment.

## Boundaries

- Do not add Tank, track, wheel, or Jolt-specific concepts to RtPbrSurvey.
- Tank owns wheel dimensions, transforms, animation, and physics.
- RtPbrSurvey owns reusable scene geometry and renderer resource handling.
- Tank will update its submodule only after this work is merged to upstream
  `main`.

## Validation Request

- Build the standalone RtPbrSurvey app and the CMake host target.
- Add a test scene or host probe containing cube and cylinder instances
  simultaneously.
- Verify that each instance renders only its selected mesh.
- Verify a 16-sided capped cylinder with non-uniform world scaling.
