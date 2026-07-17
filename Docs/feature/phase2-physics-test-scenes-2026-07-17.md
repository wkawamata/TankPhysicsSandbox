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
