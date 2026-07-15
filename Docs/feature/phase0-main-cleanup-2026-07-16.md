# Phase 0 Main Cleanup

Date: 2026-07-16

Branch: `main`

## Completed

- Confirmed `feature/rtpbrsurvey-submodule` work was squash-merged into `main`.
- Added `.vscode/` and `vcpkg_installed/` to `.gitignore`.
- Committed the ignore update:
  - `46772a9 Ignore local editor and vcpkg output`
- Pushed `main` to `origin/main`.
- Verified local `HEAD` and `origin/main` both point to `46772a9`.
- Confirmed normal `git status` is clean.
- Confirmed `External/RtPbrSurvey` submodule working tree is currently clean.

## Remaining Follow-Up

The next cleanup target is to make RtPbrSurvey cleanly consumable from Tank without Tank-local submodule edits.

Recommended next task:

1. Ask RtPbrSurvey to add an upstream CMake-consumable static library target for the host renderer layer.
2. Include shader build outputs, generated DirectX header forwarding, assets, and runtime packaging expectations in that upstream target or documented integration path.
3. Update `External/RtPbrSurvey` in Tank to that upstream commit.
4. Verify Tank builds with a clean submodule.

## Notes

`Docs/opencode` remains OpenCode-owned. Codex should not write there.
