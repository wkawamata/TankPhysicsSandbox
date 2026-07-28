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
- D3D12 renderer host integration complete: procedural cube scene rendered through `RtPbrSurvey::SceneRenderer`.

## Prerequisites

- Visual Studio 2022 (with "C++ CMake tools for Windows" component)
- Windows SDK 10.0+
- vcpkg at `C:\dev\vcpkg`
- RtPbrSurvey NuGet packages restored under `C:\work\RtPbrSurvey-work\packages`

## Build

Clone with submodules, then use the project scripts to configure and build:

```powershell
git submodule update --init --recursive
.\scripts\Restore-GameInput.ps1
.\scripts\configure.bat
.\scripts\build.bat TankSandbox
```

`configure.bat` uses the Visual Studio bundled CMake executable, configures the
`build` directory with the vcpkg toolchain, and refreshes stale CMake cache
entries. If the RtPbrSurvey NuGet packages are stored elsewhere, pass their
directory as the first argument:

```powershell
.\scripts\configure.bat C:/path/to/RtPbrSurvey/packages
```

The generated solution is `build\TankPhysicsSandbox.sln`. After opening it in
Visual Studio, right-click the `TankSandbox` project in Solution Explorer and
select **Set as Startup Project**.

## Run

```powershell
.\build\Debug\TankSandbox.exe
```

The D3D12 renderer version (default) launches a window with a green background and a red cube.
The headless Jolt physics demo can be selected by adding `-Warp` and adjusting the main entry point.

## Gamepad Input

Gamepad support is being developed with Microsoft GameInput while keeping the
platform API separate from tank physics. The controls use the left stick for
driving, steering, and pivot turns. Standard-mapped controllers use the A
button for braking; raw-controller fallback currently uses button 3. Keyboard
controls remain available.

The `Tracked Vehicle` debug window reports the connected device name, button
count, axis count, switch count, and current left-stick values. Run
`scripts\Restore-GameInput.ps1` before configuring to restore the Microsoft
GameInput SDK used by the Windows input layer.

## Documentation

- [Project and agent rules](AGENTS.md)
- [Feature documents](Docs/feature/)
