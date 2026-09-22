# Request 008: Robust Screenshot Request Queue

Date: 2026-09-22

## Context

Tank Physics Sandbox exposes renderer screenshot capture through both the
`Screen Shot` button and the F12 hotkey. Both paths call the same Tank host
method, which forwards an `RtPbrSurvey::ScreenshotRequest` to
`SceneRenderer::RequestScreenshot()`.

In a Debug build, pressing F12 can stop execution with a breakpoint. The
renderer currently keeps queued requests in `m_screenshotRequests` while a GPU
readback is represented by `m_pendingScreenshotCapture`. The screenshot pass
contains these assertions:

```cpp
assert(!m_screenshotRequests.empty());
assert(!m_pendingScreenshotCapture.has_value());
```

The latter is not a valid host-visible invariant when multiple requests can be
queued while a prior readback is pending. The public API accepts requests in a
deque, so a host must be able to submit more than one request without causing a
debugbreak.

## Requested Behavior

- Treat screenshot requests as a FIFO queue.
- Permit a new request while the preceding GPU readback is pending.
- Submit at most one readback capture per frame / pending readback, then start
  the next queued request after the preceding result is complete.
- Do not assert or debugbreak for ordinary repeated host requests.
- Preserve a result for every accepted request, including failure results.
- Keep the existing `SceneRenderer::RequestScreenshot()` and
  `ConsumeScreenshotResult()` public API source-compatible.

Tank must not need distinct debounce logic for F12, a UI button, or automated
roll-test capture. Host-side rate limiting can remain an optional policy, but
must not be required for renderer correctness.

## Verification

- Add a focused regression test for multiple queued screenshot requests.
- Exercise a request that arrives while the preceding readback is pending.
- Verify FIFO result ordering and that no assertion or debugbreak occurs.
- Build the CMake `RtPbrSurvey::SceneRenderer` target and the standalone app.
- Keep the change backend-neutral and free of Tank-specific behavior.
