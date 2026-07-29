# Request 003: Host Camera Up Vector for Exact Top-Down Views

Status: requested

Tank Physics Sandbox needs an exact top-down orthographic camera as one of three
host-managed camera profiles.

## Context

`Engine::CameraState` exposes position, gaze point, projection, FOV, orthographic
height, and clip planes. However, RtPbrSurvey currently builds the view matrix
with a fixed world-up vector:

```cpp
XMMatrixLookAtLH(eye, at, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
```

For an exact top-down camera, the view direction is parallel to the fixed
world-up vector. This makes the LookAt basis degenerate. `CameraState::rot`
does not currently provide a usable host-facing way to select the camera up
direction.

Tank could introduce a small horizontal offset, but that would only create an
approximately top-down camera and would not satisfy orthographic inspection
requirements.

## Requested Outcome

Expose a backend-neutral host-facing camera orientation contract that supports
an exact vertical view. One possible API is:

```cpp
struct CameraState
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 gazePoint;
    DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
    // Existing projection and clip fields remain.
};
```

The exact upstream API shape is open. A quaternion/orientation-based contract
is also acceptable if it is the preferred long-term camera representation.

The renderer should use the host-provided orientation consistently for:

- View and inverse-view matrices.
- Camera up/right/forward constants.
- Motion-vector and previous-camera calculations.
- RayQuery and alternate render views.
- Temporal-history reset detection when orientation changes.

## Compatibility

- Existing hosts and scene files should retain the current `{ 0, 1, 0 }`
  behavior when the new field is absent.
- Existing perspective and arcball camera behavior must remain unchanged.
- The public API should remain backend-neutral.
- Camera persistence remains host-owned for Tank; this request does not require
  RtPbrSurvey to own Tank camera-profile files.

## Acceptance Criteria

1. An external `SceneRenderer` host can configure:

   ```cpp
   camera.pos = { 0.0f, 20.0f, 0.0f };
   camera.gazePoint = { 0.0f, 0.0f, 0.0f };
   camera.up = { 0.0f, 0.0f, 1.0f };
   camera.projection = Engine::CameraProjection::Orthographic;
   ```

2. The resulting view is finite, stable, and exactly top-down.
3. Switching between perspective chase, top-down orthographic, and debug
   cameras does not leave stale temporal history.
4. RtPbrSurvey standalone and external-host CMake builds continue to pass.

## Tank Follow-Up

After upstream merge, Tank will:

- Update the RtPbrSurvey submodule.
- Store three camera profiles in memory: Chase, Top Ortho, and Debug.
- Save/load all three profiles from a Tank-owned JSON file.
- Provide profile switching and per-profile fine tuning in the Tank Camera UI.
