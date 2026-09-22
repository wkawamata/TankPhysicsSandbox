# DLSS startup regression check

## Cause and fix

DLSS host initialization was introduced in commit 9fd8c70 (2026-08-11).
Commit 0d7bfb1 (2026-09-04) removed it while adapting renderer startup.
Restore SceneRenderer::ConfigureGraphicsDevice before GraphicsDevice::Initialize.
This initializes Streamline before device creation and registers the D3D device.
The saved temporalUpscaler.enabled flag alone cannot perform that initialization.

## Manual regression test (RTX / Streamline runtime required)

1. Build the Debug TankSandbox target, then launch scripts/run.bat so that
   build/Config/renderer_debug.json is loaded from the correct working directory.
2. Open Render Settings > DLSS > DLSS SR and switch to the detailed display.
3. Confirm the backend is Streamline and the availability/status reports Available.
4. Enable DLSS SR and select Performance. Confirm an upscaled output is produced,
   the render resolution is lower than the output resolution, and the scene renders
   without corruption. Keep Deferred rendering and the final Light Pass view selected.
5. Toggle DLSS SR off/on and resize the window. Confirm rendering remains stable.
6. Save, restart using scripts/run.bat, and confirm DLSS SR remains enabled and available.

Do not enable experimental Ray Reconstruction as part of this SR regression test.
The user performs the interactive checks; they have not been run by Codex.
