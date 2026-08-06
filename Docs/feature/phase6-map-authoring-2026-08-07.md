# Phase 6: Map Authoring

Date: 2026-08-07

## Purpose

Tank Physics Sandbox maps are data-driven validation scenes shared by the UI and headless CLI. Physics owns the map definition; rendering only visualizes the same primitive data.

Map files live in `Config/Maps/` and use versioned JSON. The current schema version is `3`.

## Coordinate System

- Units are meters.
- Positive Y is up.
- A spawn yaw of zero faces positive Z.
- `position` is the center of a box or triangular prism.
- `yawRadians` rotates a primitive or spawn pose around positive Y.

## Document Fields

- `version`: Schema version. New files must use `3`.
- `name`: Non-empty display name shown by the UI and CLI.
- `environment`: Floor, friction, grid, and generated-obstacle settings.
- `spawn.position`: Initial tank position as `[x, y, z]`.
- `spawn.yawRadians`: Initial tank heading in radians.
- `primitives`: Array of static collision and display primitives.

Versions 1 and 2 remain readable for compatibility. Version 3 adds the explicit spawn pose.

## Primitive Types

### Box

Use `shape: "box"`. The `size` array contains full X, Y, and Z dimensions.

### Triangular Prism

Use `shape: "triangularPrism"`. The `size` array contains full width, height, and length. It is intended for ramps and wedges.

### Height Field

Use `shape: "heightField"` and provide:

- `sampleCount`: Number of samples on each side, from 2 through 256.
- `cellSizeM`: Horizontal spacing between adjacent samples.
- `heights`: Row-major array containing exactly `sampleCount * sampleCount` finite heights.

The height-field geometry is controlled by these three fields. Its `size` field is retained for the common primitive schema.

## Friction

Each primitive has a `friction` value from `0.0` through `2.0`. Debug rendering uses these categories:

- Blue: friction below `0.45`.
- Green: friction from `0.45` through `0.79`.
- Red: friction `0.80` or higher.

The environment floor has its own `floorFriction` setting and is stored separately from tank settings.

## Validation

Validate every checked-in map after editing or adding a JSON file:

```powershell
build\Debug\TankPhysicsCli.exe --test maps --map-directory Config\Maps
```

Run a deterministic traversal check when a map has a specific mobility goal:

```powershell
build\Debug\TankPhysicsCli.exe --test map --map Config\Maps\mobility_course.json --settle-steps 180 --steps 480 --dt 0.0166667 --throttle 1 --min-forward-distance 45 --min-final-y 0
```

`TankPhysics.AllMapFilesCli` runs the directory validation through CTest. Course-specific CTest entries should be added only when the expected traversal distance or height is stable and meaningful.

## Authoring Rules

- Keep maps deterministic and small enough for repeatable physics tests.
- Use primitive friction colors to make surface changes visible.
- Place the spawn above clear ground and leave settling space.
- Add one obstacle concept per focused map; use a combined course only for integration validation.
- Keep physics definitions independent from renderer APIs and assets.
