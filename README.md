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

- Jolt Physics (`v5.6.0`) integrated as a Git submodule under `External/JoltPhysics`.
- Minimal physics scene running: ground plane + dynamic box (headless, no rendering needed).
- `RtPbrSurvey` included as a Git submodule under `External/RtPbrSurvey` for optional test rendering.

## Prerequisites

- Visual Studio 2022 (with "C++ CMake tools for Windows" component)
- Windows SDK 10.0+

## Build

Clone with submodules and build with CMake:

```powershell
git submodule update --init --recursive
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

If `cmake` is not on `PATH`, use the Visual Studio bundled CMake executable at `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`.

## Run

```powershell
.\build\Debug\TankSandbox.exe
```

The program simulates a box falling onto a ground plane and prints position and velocity at each step until the box settles.

## Documentation

- [Project and agent rules](AGENTS.md)
- [Feature documents](Docs/feature/)
