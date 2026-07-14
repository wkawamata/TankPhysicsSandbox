# Tank Physics Sandbox

[日本語版](README.ja.md)

Tank Physics Sandbox is a C++20 physics sandbox for developing realistic tank vehicle behavior.

The project focuses on convincing vehicle physics before gameplay. Physics and rendering are kept separated so the simulation can later move to a custom DirectX 12 engine or another host.

## Goals

- Build a tank sandbox using Jolt Physics.
- Keep physics independent from rendering.
- Use `TankController` as the center of the vehicle design.
- Make tuning data editable through JSON.
- Use `RtPbrSurvey` as an optional DirectX 12 test rendering system.

## Current Status

- Minimal CMake application shell exists.
- `RtPbrSurvey` is included as a Git submodule under `External/RtPbrSurvey`.
- Renderer host integration is being investigated before Jolt vehicle work begins.

## Build

Configure and build with Visual Studio 2022 CMake:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target TankSandbox
```

If `cmake` is not on `PATH`, use the Visual Studio bundled CMake executable.

## Documentation

- [Project and agent rules](AGENTS.md)
- [Feature documents](Docs/feature/)
