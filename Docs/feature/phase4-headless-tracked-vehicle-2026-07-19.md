# Phase 4 Headless Tracked Vehicle

Date: 2026-07-19

## Goal

Create the first headless Jolt `TrackedVehicleController` proof on a flat floor while preserving the rendering-independent `TankController` boundary.

The first proof validates vehicle construction and stable simulation. Forward movement, steering, and pivot-turn acceptance criteria will be added as separate increments.

## Ownership

```text
TrackedVehicleTest
  -> owns PhysicsWorld and flat floor
  -> owns TankController

TankController
  -> owns tank body and VehicleConstraint
  -> registers and removes the vehicle step listener
  -> exports TankState

PhysicsWorld
  -> owns Jolt runtime, PhysicsSystem, allocator, and job system
```

The test object must destroy `TankController` before destroying `PhysicsWorld`.

## First Proof

The first implementation increment will:

- Create a flat static floor.
- Create one dynamic box-shaped tank body.
- Add a Jolt `VehicleConstraint` using `TrackedVehicleControllerSettings`.
- Add two tracks with a small, symmetric wheel set.
- Run with neutral driver input for a fixed number of steps.
- Assert that position and rotation remain finite and that the body settles above the floor.
- Expose the result through CTest only; no UI scene is added yet.

## Input Mapping

Jolt's tracked controller accepts a forward input plus non-zero left and right track ratios. The official sample uses:

- Straight: forward `1`, ratios `(1, 1)`.
- Left steering: forward `1`, ratios `(0.6, 1)`.
- Right steering: forward `1`, ratios `(1, 0.6)`.
- Left pivot: forward `1`, ratios `(-1, 1)`.
- Right pivot: forward `1`, ratios `(1, -1)`.

`TankInput.leftTrack` and `TankInput.rightTrack` represent normalized requested track motion, not raw Jolt ratios. Conversion to valid Jolt driver input belongs inside `TankController` and will be tested before movement assertions are introduced.

## Out Of Scope

- Rendering and ImGui integration.
- Keyboard or gamepad input.
- JSON tuning.
- Turret, barrel, recoil, or track animation.
- Slope, step, and terrain-friction tests.
- Performance tuning or final vehicle dimensions.

## Verification

The increment must keep all existing tests passing and add a dedicated headless tracked-vehicle construction test. Both `TankPhysicsCli` and `TankSandbox` must remain buildable.
