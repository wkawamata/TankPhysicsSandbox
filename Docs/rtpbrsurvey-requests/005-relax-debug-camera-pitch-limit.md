# Request 005: Evaluate Relaxing the Debug Camera Pitch Limit

Status: requested

Tank Physics Sandbox currently matches the RtPbrSurvey Arcball limit of
`1.4 rad` (approximately `80.214 degrees`) for its Look Down control. This
avoids a visible angle change when switching from a Tank camera to
`RtPbrSurvey::DebugCameraController`.

## Current Constraint

`DebugCameraController` defines:

```cpp
static constexpr float kObjectViewerPitchLimit = 1.4f;
```

The Arcball pitch calculation uses a right axis derived from world Y and the
look direction. The axis becomes undefined at exactly 90 degrees, but the
current 80.214-degree limit appears conservative rather than a numerical
boundary.

## Requested Investigation

- Evaluate relaxing the public Debug Camera pitch range to 88 degrees.
- Preserve world-Y horizontal orbit behavior.
- Preserve stable vertical orbit behavior near the new limit.
- Do not allow the exact 90-degree singularity.
- Add automated tests for horizontal and vertical drag at ordinary and
  near-limit pitch angles.
- Verify that switching into Arcball mode preserves an 88-degree camera pose
  without snapping.

## Preferred Contract

The exact API is open to upstream design. Either of these approaches is
acceptable:

- Raise the internal constant to 88 degrees with suitable tests.
- Expose a host-configurable pitch limit, clamped below 90 degrees.

Tank-specific camera concepts should not be added to RtPbrSurvey.

## Tank Workaround

Until upstream behavior changes, Tank limits Look Down to the same `1.4 rad`
value used by RtPbrSurvey. After an upstream change is merged, Tank can raise
its limit and extend its Camera Control tests to 88 degrees.
