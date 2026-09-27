# Tank Physics Sandbox

Develop convincing tank physics before gameplay, with portability to Godot and a custom DirectX 12 engine.

## Development

- Use Jolt Physics, C++20, CMake, Visual Studio 2022, and Windows.
- Strictly use CRLF line endings for project text and source files.
- Make small incremental changes, explain why, add tests, and keep the project buildable.
- Keep refactoring separate from functional changes.
- Ask questions instead of guessing when something is unclear.

## Architecture

- Physics must not depend on Rendering; keep debug rendering in a separate module.
- Design around `TankController` and keep tuning parameters editable through JSON.

## Documentation and Task-Specific Rules

- Codex must not edit or delete `Docs/opencode/`; write Codex-authored documents elsewhere.
- For RtPbrSurvey integration, renderer troubleshooting, GUI verification, or CMake runtime/deployment changes, read `Docs/renderer-development.md`.
- For goal or roadmap planning, read `Docs/project-roadmap.md` and relevant human-written documents under `Docs/roadmap/`.
- Read `OPENCODE_RULES.md` only when the user explicitly instructs you to use OpenCode.
