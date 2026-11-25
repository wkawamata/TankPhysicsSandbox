# RtPbrSurvey comparison, 2026-11-26

Tank host source revision: `9f26c9d`. Baseline: existing Release executable in
`TankPhysicsSandbox-2`, with RtPbrSurvey `a0a07a873122c52827f7768229ff422443463b5f`.
Updated: rebuilt Release executable in `TankPhysicsSandbox`, with RtPbrSurvey
`1a375fb93960f63572262bac8dd9479692a93f97`.

Both ran Box Drop on the same machine with the included renderer configuration,
VSync enabled, and shadows and reflections enabled. Each run used 120 warmup
frames and 600 measured frames. The numbered JSON files are the original
benchmark outputs in run order.

| Renderer CPU elapsed | Baseline | Updated |
| --- | ---: | ---: |
| Mean of run averages | 16.22 ms | 15.93 ms |
| Median of run P95 | 18.25 ms | 16.74 ms |
| Median of run P99 | 19.66 ms | 19.56 ms |

The mean decreased by 0.29 ms (1.8%). Renderer CPU elapsed includes waits.
The benchmark covers Box Drop with VSync and reports renderer CPU elapsed.

Run from the updated checkout with both Release builds available. Use a new
output directory for each comparison.

```powershell
./scripts/benchmark-rtpbrsurvey-comparison.ps1 `
    -BaselineExecutable ../TankPhysicsSandbox-2/build/Release/TankSandbox.exe `
    -UpdatedExecutable ./build/Release/TankSandbox.exe `
    -RendererConfig ./Docs/benchmarks/2026-11-26-rtpbrsurvey/renderer_debug.json `
    -OutputDirectory ./build/Benchmarks/RtPbrSurvey
```
