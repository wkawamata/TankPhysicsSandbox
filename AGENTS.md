# Tank Physics Sandbox

Develop convincing tank physics before gameplay, with portability to Godot and a custom DirectX 12 engine.

## Development

- Use Jolt Physics, C++20, CMake, Visual Studio 2022, and Windows.
- Strictly use CRLF line endings for project text and source files.
- Make small incremental changes, keep the project buildable, and briefly explain significant decisions.
- Add or update tests for behavior changes; scale verification to the change.
- Keep refactoring separate from functional changes.
- Investigate first; ask only when unresolved ambiguity affects requirements or scope.

## GUI Undo/Redo

- New or changed GUI operations in Editor and Game that edit data or settings must support Undo/Redo by default; use the existing command history and group continuous edits into one command. See `Docs/undo-history.md`.
- When Undo/Redo is deferred, record the operation, code location, reason and remaining work in `Docs/undo-backlog.md` in the same change, and mention the deferral in the completion report. Deferral does not itself require separate approval; follow the user's requested scope.
- Consult that backlog when touching a listed operation; remove its entry when support is implemented and verified. File writes/deletion and simulation rewind are intentional exclusions unless explicitly requested; do not list them as deferred work.

## Architecture

- Physics must not depend on Rendering; keep debug rendering in a separate module.
- Design around `TankController` and keep tuning parameters editable through JSON.

## Documentation and Task-Specific Rules

- Codex must not edit or delete `Docs/opencode/`; write Codex-authored documents elsewhere.
- For RtPbrSurvey integration, renderer troubleshooting, GUI verification, or CMake runtime/deployment changes, read `Docs/renderer-development.md`.
- For planning, consult relevant human-written documents under `Docs/roadmap/` and `Docs/project-roadmap.md`; the latter records initial goals, not current progress.
- Read `OPENCODE_RULES.md` only when the user explicitly instructs you to use OpenCode.
