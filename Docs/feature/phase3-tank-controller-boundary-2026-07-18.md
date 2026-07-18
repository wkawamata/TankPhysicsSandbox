# Phase 3 Tank Controller Boundary

Date: 2026-07-18

## Goal

Define the first rendering-independent boundary for tank simulation before introducing Jolt's `TrackedVehicleController`.

`TankController` will own tank-specific physics behavior. It must not depend on RtPbrSurvey, DirectX 12, ImGui, or DirectXMath.

## Responsibilities

`TankController` is responsible for:

- Creating and owning the tank vehicle bodies and constraints inside an existing physics world.
- Accepting normalized driver input.
- Advancing tank-specific control for a fixed simulation step.
- Exporting a plain-data snapshot for CLI validation and rendering.

`TankController` is not responsible for:

- Creating the application window or renderer.
- Converting state to RtPbrSurvey scene instances.
- Reading keyboard or gamepad input directly.
- Drawing debug geometry.
- Loading tuning JSON in the first increment.

## Input Boundary

The initial input should remain independent from Jolt and platform APIs:

```cpp
struct TankInput
{
    float throttle;
    float steering;
    float leftTrack;
    float rightTrack;
    bool brake;
};
```

The first tracked-vehicle proof should use `leftTrack` and `rightTrack` directly. Higher-level throttle and steering mapping can be added separately.

## Output Boundary

The initial snapshot should contain only plain scalar data:

```cpp
struct TransformState
{
    float position[3];
    float rotation[4];
};

struct TankState
{
    float timeSeconds;
    int stepIndex;
    TransformState body;
    float linearVelocity[3];
    float angularVelocity[3];
    bool sleeping;
};
```

Wheel, suspension, contact, and track animation state will be added only when a consumer needs them.

## Consumer Flow

```text
CLI input or UI input
  -> TankInput
  -> TankController
  -> TankState
  -> CLI assertions or rendering adapter
```

The rendering adapter may convert `TankState` into DirectXMath matrices, but that conversion remains outside `src/Physics/`.

## Implementation Steps

1. Add `TankInput`, `TransformState`, and `TankState` as plain C++ types.
2. Add a compile-safe `TankController` interface without creating vehicle bodies yet.
3. Add unit-level tests for input clamping and initial snapshot state.
4. Add a headless flat-ground tracked-vehicle test.
5. Expose the same test through a Tank Sandbox UI scene.
6. Add forward, steering, and pivot-turn assertions as separate increments.

## First Increment Boundary

The first code increment will add only the plain-data types, a minimal controller lifecycle, and tests. Jolt `TrackedVehicleController`, rendering integration, JSON tuning, and debug drawing remain out of scope until that boundary is verified.

## First Increment Result

The first boundary increment adds:

- Plain `TankInput`, `TransformState`, and `TankState` types.
- A minimal `TankController` lifecycle with normalized input clamping.
- Deterministic initial state and fixed-step bookkeeping.
- A CTest executable covering initial state, input clamping, positive-step behavior, and rejection of a zero timestep.

The controller does not own Jolt objects yet. This keeps the public contract independently testable before tracked-vehicle setup is introduced.
