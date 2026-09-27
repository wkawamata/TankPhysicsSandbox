# Tank capture sessions

Tank uses RtPbrSurvey's shared CaptureSession, CLI configuration conversion, and
CaptureSessionUi. Physics remains host-owned and has no renderer dependency.

## GUI

Open **Render Settings > Capture Session**. Select the output folder, base name,
PNG or EXR, full frame or numeric ROI, FPS, warmup, and frame/duration limit.
Use **Start Capture Session** / **Stop Capture Session**. Stop finishes the
pending save before completing. Closing the window also drains an active session.
The panel shows accepted/saved/dropped counts, last output path, and errors.

PNG contains the final composed image including UI. EXR contains linear HDR scene
color before tone mapping and excludes UI. GIF/MP4 are visibly unavailable until
their shared encoders arrive. Mouse ROI selection is not implemented.

F12 and the existing Screen Shot button remain single-PNG operations. They cannot
be used during a capture session. Starting a session is disabled while legacy
screenshots, rolling capture, or benchmark automation are pending.

## CLI

Run from the repository root. `scripts/run.bat` uses the build directory as the
working directory, so relative output paths are relative to `build/`.

```bat
scripts\run.bat --scene tracked-vehicle -CaptureSessionOutputDir Screenshots\tank -CaptureSessionBaseName tank -CaptureSessionFormat png -CaptureSessionFrames 60 -CaptureSessionFps 30 -CaptureSessionWarmupFrames 30 -CaptureSessionClock fixed-step -ExitAfterCapture
```

```bat
scripts\run.bat --scene tracked-vehicle -CaptureSessionOutputDir Screenshots\hdr -CaptureSessionBaseName suspension -CaptureSessionFormat exr -CaptureSessionRoi 100 200 640 480 -CaptureSessionDurationSeconds 2 -CaptureSessionFps 30 -CaptureSessionClock fixed-step -ExitAfterCapture
```

The scene defaults to tracked-vehicle for session CLI requests. `--scene box-drop`
is also supported. Filenames are `tank_000000.png`, `tank_000001.png`, etc.
Choose a fresh folder/base name for each run; existing matching files can be
overwritten. A frame limit or duration is required. Duration excludes warmup.
If both limits are specified, capture ends at whichever is reached first.

`-ExitAfterCapture` (or Tank's `--quit-after-capture`) exits after saving completes.
Invalid CLI settings or failed output return a nonzero process exit code.
`run.bat` launches asynchronously; automation that needs the application exit code
must launch `build/Debug/TankSandbox.exe` directly with `build/` as its working
directory and wait for it. Do not combine session CLI with Tank's legacy
`--capture-after-frames`, `--roll-capture-dir`, or benchmark automation.

## Timing

Fixed-step capture uses the host's simulation clock. Tank's physics step is 1/60 s;
use 60 FPS or a divisor such as 30 FPS for evenly spaced distinct physics states.
Physics and camera stepping pause while a capture readback is pending, while
rendering and result polling continue. No blocking GPU wait is added. Pausing the
vehicle physics also pauses this capture clock; Stop remains available.
For scenes without physics, the host clock advances at 60 Hz per allowed tick.

Real-time capture uses elapsed wall time. A busy encoder/readback can miss
deadlines; the shared panel reports the dropped count.

## Dependencies and validation

The renderer submodule includes the shared core/UI PRs and integration contract
fixes merged into RtPbrSurvey main at `8968337` (PRs #78, #79, and #80).
TinyEXR is supplied
through Tank's vcpkg manifest; its license is BSD-3-Clause. With the current
`x64-windows` triplet, its transitive dependency also requires `miniz.dll` next to
the executable (deployed by the shared CMake runtime helper). Renderer DLLs/shaders
are copied through the existing RtPbrSurvey CMake integration.

Run the opt-in GPU smoke test after a Debug build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/CaptureSessionSmoke.ps1
```

It launches the application with the build working directory and checks PNG
dimensions, ordered PNG/EXR outputs, ROI, warmup/duration, fixed-step capture
timestamps, and failure exit codes. Artifacts are retained in a unique
`build/CaptureSmoke/` folder for inspection.

Verified on 2026-09-28: Tank Debug build succeeded, and all seven GPU smoke cases
passed (full PNG, unaligned ROI PNG, duration, EXR, unsupported format, invalid
FPS, and out-of-bounds ROI). Successful cases reported no D3D12 errors. Two
60 FPS fixed-step captures were exactly 1/60 s apart despite readback spanning
multiple render frames. GUI checks covered start/stop, PNG and EXR selection,
full frame and 317x239 ROI, and closing during recording. Closing drained the
pending save and completed with zero dropped frames. The shared UI keeps the
Start/Stop controls stable while status text changes.
