param(
    [string]$Configuration = "Debug",
    [string]$OutputPath = "build/Artifacts/rolling-input-test.gif",
    [ValidateRange(1.0, 10.0)]
    [double]$RoiScale = 1.5
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $root "build"
$tracePath = Join-Path $buildDirectory "Artifacts/rolling-input-test.csv"
$gifPath = Join-Path $root $OutputPath
$python = "C:\Users\wkawa\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"

cmake --build $buildDirectory --config $Configuration --target TrackedVehicleRollGifTrace
& (Join-Path $buildDirectory "$Configuration/TrackedVehicleRollGifTrace.exe") --output $tracePath
& $python (Join-Path $root "scripts/render_roll_gif.py") --input $tracePath --output $gifPath --roi-scale $RoiScale

Write-Host "GIF generated: $gifPath"
