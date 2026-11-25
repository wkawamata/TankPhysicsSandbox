param(
    [Parameter(Mandatory = $true)]
    [string]$BaselineExecutable,
    [Parameter(Mandatory = $true)]
    [string]$UpdatedExecutable,
    [Parameter(Mandatory = $true)]
    [string]$RendererConfig,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$variants = @{
    baseline = (Resolve-Path -LiteralPath $BaselineExecutable).Path
    updated = (Resolve-Path -LiteralPath $UpdatedExecutable).Path
}
$config = (Resolve-Path -LiteralPath $RendererConfig).Path
$outputRoot = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
$sequence = 'baseline','updated','updated','baseline','baseline','updated'
for ($i = 0; $i -lt $sequence.Count; $i++) {
    $variant = $sequence[$i]
    $run = Join-Path $outputRoot ('{0:D2}-{1}' -f ($i+1), $variant)
    New-Item -ItemType Directory -Path (Join-Path $run 'Config') -Force | Out-Null
    Copy-Item -LiteralPath $config -Destination (Join-Path $run 'Config/renderer_debug.json')
    $result = Join-Path $run 'result.json'
    $arguments = @('--scene','box-drop','--benchmark-frames','600','--benchmark-output',('"{0}"' -f $result),'--benchmark-shadows-on','--benchmark-reflections-on')
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $variants[$variant] -ArgumentList $arguments -WorkingDirectory $run -PassThru -RedirectStandardError (Join-Path $run 'stderr.log') -RedirectStandardOutput (Join-Path $run 'stdout.log')
    if (!$process.WaitForExit(90000)) {
        Stop-Process -Id $process.Id
        throw "Timed out: $variant"
    }
    $timer.Stop()
    if ($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $result)) {
        throw "Benchmark failed: $variant, exit $($process.ExitCode), logs: $run"
    }
    $metrics = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
    if ($metrics.frames -ne 600 -or $metrics.averageCpuFrameTimeMs -le 0) {
        throw "Invalid measurement: $result"
    }
    Write-Output ('{0}: avg={1} p95={2} p99={3} peak={4} ms; process elapsed={5:F2} s' -f $variant,$metrics.averageCpuFrameTimeMs,$metrics.p95CpuFrameTimeMs,$metrics.p99CpuFrameTimeMs,$metrics.maximumCpuFrameTimeMs,$timer.Elapsed.TotalSeconds)
}
