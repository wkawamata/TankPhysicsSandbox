# Phase 7: Mobility Evaluation Suite

Date: 2026-08-12

## Purpose

Run multiple versioned Tank settings files through the same deterministic map and input sequence. This combines the authored mobility course with headless vehicle telemetry so tuning changes can be compared without rendering.

## Command

```powershell
build\Debug\TankPhysicsCli.exe `
  --test mobility-suite `
  --map Config\Maps\mobility_course.json `
  --tank-settings-directory tests\data\mobility `
  --settle-steps 180 `
  --steps 480 `
  --dt 0.0166667 `
  --throttle 1
```

Input JSON files are evaluated in filename order. Each `RESULT mobility-suite` line reports forward distance, final height, maximum speed, zero-to-ten time, final engine RPM, and gear. The suite fails on invalid input or non-finite physics state. Per-vehicle performance thresholds remain in focused map tests.

## Initial Fixtures

- `high_torque.json`: 900 Nm baseline.
- `low_torque.json`: 600 Nm comparison.

The first measured run produced 93.32 m versus 79.27 m of forward travel. These values are observations, not fixed score thresholds.

## Verification

CTest target: `TankPhysics.MobilityEvaluationSuite`.

