# Request 010: Reflect DLSS Availability in Simple Display

Date: 2026-09-22

## Context

In `SceneRendererDebugUi`, the simple `DLSS SR` display exposes an enable
checkbox and a mode selector. The simple `DLSS RR` display exposes an enable
checkbox.

When either feature is unavailable, those controls are disabled. However, a
persisted requested setting can still make the disabled checkbox appear checked.
This implies the feature is active even though it cannot be enabled, which is
misleading during renderer diagnosis.

## Requested Behavior

- Keep the DLSS SR simple display limited to its enable checkbox and mode
  selector.
- Keep the DLSS RR simple display limited to its enable checkbox.
- When SR is unavailable, show its disabled enable checkbox as unchecked.
- When RR is unavailable, show its disabled enable checkbox as unchecked.
- Do not allow either simple-display checkbox to be changed while unavailable.
- Do not mutate the persisted requested settings merely because availability is
  temporarily false. The UI should present the effective availability state;
  the stored preference may take effect again when the backend becomes
  available.
- Preserve detailed-mode controls, renderer capability detection, public APIs,
  and settings serialization.

## Verification

- Add focused UI-state or helper tests where practical for available and
  unavailable SR/RR states with persisted enabled settings.
- Build the CMake `RtPbrSurvey::SceneRenderer` target and standalone app.
- Confirm `Draw()` and embedded `DrawContents()` have matching behavior.
