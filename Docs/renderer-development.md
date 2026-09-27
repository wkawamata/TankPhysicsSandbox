# Renderer Development

- When integrating RtPbrSurvey from CMake, verify runtime assets before debugging renderer code:
  - `D3D12/D3D12Core.dll`
  - `D3D12/D3D12SDKLayers.dll`
  - required `*.cso` shader files next to the executable
  - required runtime DLLs such as `dxcompiler.dll`, `dxil.dll`, and `WinPixEventRuntime.dll` when used.
- If `D3D12CreateDevice` returns `D3D12_ERROR_INVALID_REDIST`, compare the Tank executable output folder with the working RtPbrSurvey output folder before changing adapter-selection code.
- If `ReadDataFromFile` fails for a shader `.cso`, check shader generation/copy rules before changing renderer resource-loading code.
- Prefer `scripts/run.bat` for local GUI verification because it selects the CMake build directory as the working directory while forwarding command-line arguments.
- Direct executable launches remain supported. When saved development settings under `build/Config` are expected, explicitly use the CMake build directory as the working directory. Otherwise renderer settings can fall back to defaults and camera slots will not load.
- For Visual Studio launches, preserve `VS_DEBUGGER_WORKING_DIRECTORY` as `${CMAKE_BINARY_DIR}` in CMake.
