# Phase 2 Physics Test Scenes

Date: 2026-07-17

## Goal

Add physics verification scenes that can run from both the UI application and a headless CLI.

The UI application should start at a top ImGui menu. From that menu, the user can enter physics validation scenes. Pressing `ESC` from a validation scene should return to the top menu.

## Architecture

Physics validation must stay independent from rendering.

The shared test flow should be:

```text
BoxDropTest
  -> owns or drives Jolt state
  -> returns BoxDropState snapshots
```

The CLI and UI should both consume the same test object:

```text
TankPhysicsCli
  -> BoxDropTest

TankSandboxApp
  -> BoxDropTest
  -> SceneRenderer visualization
```

Rendering should only visualize state snapshots. Physics code must not depend on RtPbrSurvey.

## First Scene

The first validation scene is `box-drop`.

It will use:

- A static floor body.
- A dynamic box body.
- Fixed timestep updates.
- A final PASS/FAIL condition based on the box settling above the floor.

This is intentionally simpler than tank motion. It validates allocator setup, Jolt type registration, body creation, gravity, collision, sleep state, and the physics-to-rendering state path.

## Initial CLI Shape

Expected command:

```powershell
TankPhysicsCli.exe --test box-drop --steps 300 --dt 0.0166667
```

Expected eventual output:

```text
test=box-drop step=300 t=5.000 box.y=0.500 sleeping=true
PASS box-drop
```

## Step Boundary

This first increment only adds:

1. This design note.
2. A rendering-independent Physics module skeleton.
3. A CLI executable target that can select the `box-drop` test path.

The actual Jolt box-drop simulation and UI scene transition will be added in later increments.

## Step 4 Result

`BoxDropTest` now owns a minimal Jolt world internally while keeping Jolt types out of its public header.

Implemented behavior:

- Initializes the Jolt allocator, factory, and type registry once per process.
- Creates a static floor body.
- Creates a dynamic box body at the configured initial height.
- Advances the physics world with a fixed caller-provided timestep.
- Returns `BoxDropState` snapshots with position, rotation, elapsed time, step index, and sleep state.
- Keeps physics independent from RtPbrSurvey and rendering code.

CLI verification:

```powershell
build\Debug\TankPhysicsCli.exe --test box-drop --steps 300 --dt 0.0166667
```

Observed result:

```text
test=box-drop step=300 t=5.00001 box.y=0.48 sleeping=true
PASS box-drop final_y=0.48
```

The PASS range is currently intentionally tolerant (`0.45 <= final_y <= 0.65`) so this test verifies the integration path without becoming sensitive to small solver/contact-margin differences.

## Step 5 Result

`TankSandboxApp` now has an application mode skeleton:

- `TopMenu`
- `PhysicsBoxDrop`

The app starts in `TopMenu`. The top menu exposes a `Box Drop` button, which switches to `PhysicsBoxDrop`. Pressing `ESC` from `PhysicsBoxDrop` returns to `TopMenu`.

The actual physics-driven rendering for `PhysicsBoxDrop` is intentionally left for Step 6.

## Step 6 Result

`PhysicsBoxDrop` now runs the same `BoxDropTest` used by `TankPhysicsCli` and visualizes its state through RtPbrSurvey.

Implemented behavior:

- Builds the floor and box rendering instances once when entering the mode.
- Advances physics at a fixed timestep and updates only the box transform each frame.
- Reloads scene resources only when the Box Drop scene is created.
- Sets the visible instance count explicitly after switching from the one-instance Top Menu scene to the two-instance Box Drop scene.
- Shows step, elapsed time, box height, sleeping state, and a Reset button in ImGui.
- Keeps all RtPbrSurvey and DirectXMath usage in the application layer; `BoxDropTest` remains rendering-independent.

The explicit display-instance update is required because scene reload preserves the previous scene's visible instance count. Without it, switching from the one-instance Top Menu scene displayed only the floor and hid the second instance.

## Step 7 Verification

Verified on 2026-07-18:

```powershell
scripts\build.bat TankPhysicsCli
build\Debug\TankPhysicsCli.exe --test box-drop --steps 300 --dt 0.0166667
scripts\build.bat TankSandbox
```

Observed CLI result:

```text
test=box-drop step=300 t=5.00001 box.y=0.48 sleeping=true
PASS box-drop final_y=0.48
```

Both targets built successfully. Final visual confirmation of the falling box and Reset behavior is deferred until the UI can be inspected.

## Automated Regression Test

The CLI box-drop scenario is registered with CTest as `TankPhysics.BoxDrop`. The test runs the same 300-step simulation and requires `PASS box-drop` in the process output.

```powershell
ctest --test-dir build -C Debug --output-on-failure
```
