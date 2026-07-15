# OpenCode Working Copy: Two-Week Goals

Source: `Docs/roadmap/2026-07-15-two-week-goals.md`

## Summary

Two-week window: 2026-07-15 to 2026-07-29.

### Goal 1: Stabilize The Tank Project Foundation
- Keep CMake/C++20/VS2022 buildable.
- Keep docs organized under `Docs/`.

### Goal 2: Prepare RtPbrSurvey As The Test Rendering Backend
- Keep submodule updated.
- Track requests under `Docs/opencode/rtpbrsurvey-requests/`.
- Resolve CMake consumption path for `SceneRenderer`.

### Goal 3: Build The Minimal Renderer Host Proof
- Tank owns window and renderer lifetime.
- Render fixed cube/sphere scene through `SceneRenderer`.
- Add Tank ImGui window through `SetToolUiHandler()`.
- No Jolt in this proof.

### Goal 4: Define Physics/Rendering Data Boundaries
- Draft renderer-agnostic tank state snapshot.
- Keep DX12 types out of physics data.

### Goal 5: Prepare Jolt Integration
- Decide submodule/FetchContent/package manager.
- Start with world, ground body, one dynamic body.

## OpenCode Notes

- The `Docs/rtpbrsurvey-requests/` path in the original document should now read `Docs/opencode/rtpbrsurvey-requests/`.
- Goal 3 depends on Goal 2 being resolved first (CMake consumption path).
- Goal 5 can proceed in parallel with Goals 2-3.
