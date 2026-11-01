# Performance Meter

The Performance Meter window appears with the tool UI in game, editor and menu.
Use Sub Windows in the Tank Sandbox start menu or Tracked Vehicle panel to turn it
on or off. The visibility is saved with the UI layout and supports Undo/Redo.
Collapse its title bar to save space; left + right mouse buttons hide all tool UI.
It is a read-only diagnostic view and does not edit saved settings or simulation.

The meter keeps the last 300 frames and refreshes its summary every 15 frames.
Last, average, nearest-rank P95 and peak are milliseconds in that rolling window.
Changing application modes clears the history, so menu/editor/game do not mix.
Frame FPS uses the interval between host ticks, including the event loop outside
OnIdle. Host elapsed measures OnIdle; it includes synchronization waits and is not
process CPU utilization. The approximate difference is shown as outside-host time.
The two budget bars use a fixed 16.67 ms (60 FPS) reference.

## CPU/host intervals

Exclusive rows add up to host elapsed. Sorted averages identify both the largest
host interval and the largest Tank interval (excluding the renderer call).

- Input / gamepad: focus/input polling, device mapping and camera-button commands.
- Physics / gameplay: vehicle/enemy/projectile simulation, Jolt update and worker
  completion, state copies and physics-step logging. It is not pure Jolt CPU time.
- Scene / renderer sync: runtime scene preparation, transforms, projectile/debug
  lines, materials/instances and renderer SetScene. Paused/no-step frames do not
  reuse the previous physics measurement.
- Camera / mode management: follow/transition/cache updates and other mode-step work.
- UI / editor: building all ImGui windows, editor actions and viewport preparation.
- Renderer / submit / waits: RtPbr RunFrame, including resource resize, recording,
  submitting, synchronization and Present/VSync waits. This row is not GPU time.
- Capture / logging: capture scheduling/polling, screenshots and debug-log draining.
- Other host work: unassigned bookkeeping, including benchmark sample processing.

The existing Tracked Vehicle headline and Frame Timing labels explicitly identify
the older renderer-only counter. Overall FPS and host total are in this meter.

## GPU timeline

GPU pass times come from RtPbr completed timestamp queries. They are delayed by
in-flight frames; the host does not add a wait or read back unfinished GPU work.
Cumulative timestamps are converted to adjacent differences, then equal pass names
are aggregated. The final unnamed interval is shown as GPU tail.
Missing/invalid query sets are unavailable, not zero-time samples. A pass absent
from an otherwise valid frame contributes zero to its rolling average.

CPU/host and GPU execute concurrently and must not be summed. Present or GPU waits
can dominate host time without indicating expensive Tank work. Use the GPU pass
rows to locate rendering cost, and the Tank rows to locate simulation/UI/scene cost.
This is a coarse elapsed-time meter, not per-thread CPU utilization or a complete
GPU/driver profiler. The meter itself has small sampling/UI overhead; summary
aggregation follows the measured host interval and is reflected in frame intervals.

PerformanceMeterTests verifies timestamp differencing, duplicate passes, invalid
query sets, independent CPU/GPU timelines, exclusive CPU totals, rolling eviction,
P95, missing-pass averages and history reset. All project files use CRLF.

GUI verification (Debug, Hybrid + DLSS Quality) confirmed FPS/host/GPU bars,
CPU interval ranking, DLSS and other GPU pass rows, and zero physics latest time
while paused. The D3D12 debug log contained no errors (startup COMMON-state
warnings remained). Manual UI layout settings were restored after validation.

## Renderer CPU breakdown

The Renderer CPU breakdown header splits the Renderer / submit / waits row into
four exclusive CPU elapsed intervals. Each row has rolling Last/Avg/P95/Peak and a
share of the instrumented renderer total. This is a nested breakdown: do not add
these rows to the host table or to GPU times.

- Update / housekeeping: frame update, instance/constants/descriptors, resource
  resize and HDR setup, capture readback and cleanup, excluding explicit waits.
- Record / submit: PopulateCommandList, renderer pass recording (including DLSS
  SDK CPU calls and ImGui command recording), command submission and history commit.
- GPU fence wait: time inside explicit fence-wait calls, including normal frame
  rotation, resize FlushGpu and diagnostic readbacks. It measures CPU blocked in
  the API, not GPU execution. Waits nested in other intervals are subtracted there.
- Present: the Present API call, including any VSync/compositor/driver blocking.

The engine exposes a completed CPU snapshot directly, without obtaining GPU data
or querying DLSS settings. The full RunFrame boundary is measured, including resize
work that the older renderer-only counter excludes. Tiny host/facade call-boundary
cost can make the host's Renderer row slightly larger than the instrumented total.
Driver/SDK waits hidden inside update/recording APIs remain in those categories;
only explicit fence waits and Present are separated. No new waits are introduced.

RendererCpuTimingTests uses deterministic timestamps to check nested waits,
phase resumption, exact totals, frame reset and inactive initialization/shutdown.
PerformanceMeterTests also verifies renderer rolling statistics, missing snapshots,
zero-wait frames and separation from the host total.

Renderer CPU GUI validation confirmed all four rows, rolling statistics and matching
host renderer-call/instrumented totals. In that Debug validation session the largest
interval was Record / submit, followed by GPU fence wait; Present was small. These
are local validation observations, not a reproduction of a user's prior capture.
The D3D12 log had no errors. Original UI JSON settings were restored afterwards.

## Dump a report

The DumpToOutput button at the upper right of Performance Meter refreshes the
rolling statistics, writes a line-by-line report to the Output log (category
Performance) and copies the complete plain-text report to the clipboard. Paste
it directly into a chat for diagnosis. Output can be shown from Sub Windows or
with F9; its filters still apply. If a console is attached, the same report is
also written to stdout. Dumping does not clear previous log entries.

The report includes application mode, paused state, display map name, build type,
GPU adapter, rendering path/view, VSync, shadow/reflection and DLSS SR status,
window/viewport/render dimensions, sample counts, frame time, host CPU breakdown,
renderer CPU breakdown, and completed GPU pass timings. It omits filesystem paths.
The CPU, renderer, and GPU timelines overlap as described above; GPU query data is
delayed. The reported settings are current while rolling samples may include
earlier settings or startup; collect 300 new frames after changing settings.
If samples are unavailable, the report marks them unavailable, not zero.
The dump is a diagnostic output action and does not edit the scene or settings.
