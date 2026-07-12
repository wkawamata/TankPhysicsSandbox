# Tank Physics Sandbox

## Project Overview

Tank Physics Sandbox is a physics-system development project for a future tank game.

The priority is not gameplay. The first goal is to build realistic and convincing vehicle behavior.

The project is intended to be migrated into a custom DirectX 12 engine later, so physics and rendering must remain fully separated.

## Development Policy

- Use Jolt Physics.
- Use C++20.
- Use CMake.
- Target Visual Studio 2022.
- Target Windows.
- Actively use Codex and OpenCode.
- Build the project through small incremental changes.
- Keep the project buildable at all times.
- Strictly use CRLF line endings for project text and source files.

## Architecture

- Physics must not depend on Rendering.
- Design around `TankController`.
- Tuning parameters should be editable through JSON.
- Debug rendering belongs in a separate module.
- Structure the code so it can later be ported to Godot and to a custom engine.

## Initial Investigation

Consider adding `RtPbrSurvey` as a Git submodule and using it as a test rendering system while modifying it as needed:

- Repository: https://github.com/wkawamata/RtPbrSurvey.git

## Initial Goal

Build a Tank Sandbox using Jolt's official `TrackedVehicleController`.

The initial sandbox should support:

- Driving on flat ground.
- Turning left and right.
- Pivot turning in place.

## AI Rules

- Do not make large changes at once.
- Explain why each change is made.
- Add tests.
- Keep refactoring separate from functional changes.
- Do not leave build errors behind.
- Ask questions instead of guessing when something is unclear.

## OpenCode Coordination

- OpenCode coordination scripts are currently located under `C:\work\RtPbrSurvey-agents`.
- That folder is a communication and coordination folder for another project.
- Do not treat `C:\work\RtPbrSurvey-agents` as part of this project's workspace.
- For Tank Physics Sandbox, keep the source workspace as `C:\work\TankPhysicsSandbox`.
- If this project needs its own OpenCode task folders, use a separate coordination folder such as `C:\work\TankPhysicsSandbox-agents`.
- Be explicit when reporting paths:
  - `workspace`: the source checkout being edited.
  - `coordination folder`: the external folder used for OpenCode task handoff, reports, and logs.

## Roadmap

1. Introduce Jolt.
2. Build the Tank Sandbox.
3. Add suspension.
4. Add track control.
5. Test steps and slopes.
6. Add terrain friction.
7. Add recoil.
8. Add turret behavior.
9. Add track animation.
