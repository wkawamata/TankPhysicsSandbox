# Performance Meter

The Performance Meter window appears with the tool UI in game, editor and menu.
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
