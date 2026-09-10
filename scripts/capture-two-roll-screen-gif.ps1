param(
    [string]$Configuration = "Debug",
    [string]$OutputPath = "build/Artifacts/rolling-two-consecutive.gif",
    [ValidateSet(-1, 1)]
    [int]$Sign = -1,
    [ValidateRange(1.0, 3.0)]
    [double]$RoiScale = 1.5,
    [int]$Frames = 150
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$artifact = Join-Path $build "Artifacts/rolling-two-consecutive-capture"
$exe = Join-Path $build "$Configuration/TankSandbox.exe"
$python = "C:\Users\wkawa\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"

cmake --build $build --config $Configuration --target TankSandbox
if (Test-Path -LiteralPath $artifact) { Remove-Item -LiteralPath $artifact -Recurse -Force }

Start-Process -FilePath $exe -WorkingDirectory $build -Wait -ArgumentList @(
    "--scene", "tracked-vehicle",
    "--roll-capture-dir", $artifact,
    "--roll-capture-sign", $Sign,
    "--roll-capture-roll-count", "2",
    "--roll-capture-frames", $Frames,
    "--roll-capture-interval", "2"
)

& $python (Join-Path $root "scripts/render_roll_capture_gif.py") `
    --single-dir $artifact `
    --output (Join-Path $root $OutputPath) --roi-scale $RoiScale
