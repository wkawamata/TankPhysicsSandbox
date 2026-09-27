# OpenCode Rules

### Coordination

- OpenCode coordination scripts are currently located under `C:\work\RtPbrSurvey-agents`.
- That folder is a communication and coordination folder for another project.
- Do not treat `C:\work\RtPbrSurvey-agents` as part of this project's workspace.
- For Tank Physics Sandbox, keep the source workspace as `C:\work\TankPhysicsSandbox`.
- If this project needs its own OpenCode task folders, use a separate coordination folder such as `C:\work\TankPhysicsSandbox-agents`.
- Be explicit when reporting paths:
  - `workspace`: the source checkout being edited.
  - `coordination folder`: the external folder used for OpenCode task handoff, reports, and logs.

### Documents

- Documents written by OpenCode are stored under `Docs/opencode/`.
- This includes investigation reports, request documents, and integration notes.
- Keep this convention so that first-party and AI-generated documents are clearly separated.

### Document Rules

- `Docs/` (except `Docs/opencode/`) is readonly for OpenCode. Do not edit or delete files under `Docs/` outside of `Docs/opencode/`.
- `Docs/roadmap/` contains human-written roadmap and goal documents.
- OpenCode reads these as readonly inputs.
- OpenCode's analysis or working copies go under `Docs/opencode/roadmap/`.
- Do not edit files under `Docs/roadmap/` from OpenCode.

- `Docs/rtpbrsurvey-requests/` contains Codex-written requests to RtPbrSurvey.
- OpenCode reads these as readonly inputs.
- OpenCode mirrors them under `Docs/opencode/rtpbrsurvey-requests/` and appends annotations.
- Do not edit files under `Docs/rtpbrsurvey-requests/` from OpenCode.
