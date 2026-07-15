# Report

Task: jolt-physics-investigation

Status: running

Progress: Session created. Sharing existing knowledge about Jolt Physics integration.

---

# Jolt Physics Integration Investigation

## Workspace Inspected

`C:\work\TankPhysicsSandbox`

## Coordination Folder

`C:\work\RtPbrSurvey-agents\jolt-physics-investigation`

## Current State

- Branch: `feature/rtpbrsurvey-submodule`, clean.
- No Jolt integration exists yet.
- No first-party source files (no CMakeLists.txt, no .cpp/.h outside External/).
- RtPbrSurvey submodule is up to date at `f90591d`.

## Existing Knowledge

### Project Requirements (from AGENTS.md)

- Use Jolt Physics.
- Use C++20, CMake, VS2022, Windows.
- Physics must not depend on Rendering.
- Design around `TankController`.
- Initial goal: `TrackedVehicleController` for driving, turning, pivot turning.

### Two-Week Goals (from Docs/roadmap/2026-07-15-two-week-goals.md)

- Goal 5: Prepare Jolt Integration
  - Decide how Jolt enters the project: submodule, FetchContent, or package manager.
  - Add only the smallest integration needed for a buildable physics proof.
  - Start with a world, ground body, and one dynamic body before tracked vehicle work.

### Integration Options

1. **Git Submodule** — `External/JoltPhysics` submodule pointing to Jolt repo.
   - Pros: Full control, explicit version pinning.
   - Cons: Manual CMake setup needed.

2. **CMake FetchContent** — Download at configure time.
   - Pros: No submodule management.
   - Cons: Network dependency at configure time.

3. **Package Manager (vcpkg)** — If Jolt is available.
   - Pros: Integrated with existing vcpkg usage.
   - Cons: May lag behind latest Jolt release.

### Minimal Integration Steps (Proposed)

1. Add Jolt as submodule under `External/JoltPhysics`.
2. Create CMakeLists.txt with Jolt as a static library target.
3. Create minimal `src/main.cpp` with:
   - Jolt factory/allocator initialization.
   - PhysicsSystem creation.
   - Static ground body (plane or box).
   - One dynamic cube body.
   - Simple step loop (no rendering yet).
4. Verify build and runtime behavior.

### Jolt API Basics (Reference)

- `Jolt::Factory` — Register types.
- `Jolt::TempAllocatorImpl` / `Jolt::JobSystemThreadPool` — Threading.
- `Jolt::PhysicsSystem` — Core simulation.
- `Jolt::BodyInterface` — Create/manipulate bodies.
- `Jolt::BodyCreationSettings` — Configure body shape, type, motion.
- `Jolt::Shape::Static` vs `Jolt::Shape::Dynamic` — Body types.
- `Jolt::Vehicle::TrackedVehicleController` — Future tank vehicle.

### Risks

- CMake integration with Jolt may need tuning (Jolt uses its own CMakeLists.txt).
- Jolt's build options (e.g., `USE_SIMD`, `USE_AVX2`) need to match project settings.
- Thread safety: Jolt uses jobs, must integrate with project's threading model.
- No existing CMakeLists.txt in Tank project — must be created from scratch.

## Research-Only Statement

This was a research-only task. No source files were edited. No submodules were updated. No commits, pushes, or branch changes were made.

Status: done
