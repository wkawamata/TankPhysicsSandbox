param(
    [int]$Frames = 600,
    [string]$OutputDirectory = "build/Benchmarks/FramePacing"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $root "build"
$executable = Join-Path $buildDirectory "Debug/TankSandbox.exe"
$outputRoot = [System.IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))

if (-not (Test-Path -LiteralPath $executable))
{
    throw "TankSandbox.exe was not found. Build the Debug TankSandbox target first."
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

$cases = @(
    @{ Name = "baseline"; Flags = @(
        "--benchmark-track-shoes-on",
        "--benchmark-shadows-on",
        "--benchmark-reflections-on") },
    @{ Name = "track-shoes-off"; Flags = @(
        "--benchmark-track-shoes-off",
        "--benchmark-shadows-on",
        "--benchmark-reflections-on") },
    @{ Name = "shadows-off"; Flags = @(
        "--benchmark-track-shoes-on",
        "--benchmark-shadows-off",
        "--benchmark-reflections-on") },
    @{ Name = "reflections-off"; Flags = @(
        "--benchmark-track-shoes-on",
        "--benchmark-shadows-on",
        "--benchmark-reflections-off") },
    @{ Name = "all-off"; Flags = @(
        "--benchmark-track-shoes-off",
        "--benchmark-shadows-off",
        "--benchmark-reflections-off") }
)

$results = foreach ($case in $cases)
{
    $outputPath = Join-Path $outputRoot ($case.Name + ".json")
    $arguments = @(
        "--scene", "tracked-vehicle",
        "--benchmark-frames", $Frames,
        "--benchmark-output", $outputPath
    ) + $case.Flags

    Write-Host "Running $($case.Name)..."
    $process = Start-Process `
        -FilePath $executable `
        -WorkingDirectory $buildDirectory `
        -ArgumentList $arguments `
        -Wait `
        -PassThru
    if ($process.ExitCode -ne 0)
    {
        throw "$($case.Name) failed with exit code $($process.ExitCode)."
    }
    if (-not (Test-Path -LiteralPath $outputPath))
    {
        throw "$($case.Name) did not produce $outputPath."
    }

    $result = Get-Content -LiteralPath $outputPath -Raw | ConvertFrom-Json
    [pscustomobject]@{
        Case = $case.Name
        AverageMs = $result.averageCpuFrameTimeMs
        P95Ms = $result.p95CpuFrameTimeMs
        P99Ms = $result.p99CpuFrameTimeMs
        MaximumMs = $result.maximumCpuFrameTimeMs
        PhysicsPeakMs = $result.physicsPeakTimeMs
        ScenePeakMs = $result.sceneUpdatePeakTimeMs
    }
}

$summaryPath = Join-Path $outputRoot "summary.csv"
$results | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding utf8
$results | Format-Table -AutoSize
Write-Host "Saved: $summaryPath"
