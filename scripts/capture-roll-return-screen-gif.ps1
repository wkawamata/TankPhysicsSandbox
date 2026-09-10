param(
    [string]$Configuration = "Debug",
    [string]$OutputPath = "build/Artifacts/rolling-return-at-75.gif",
    [ValidateSet(-1, 1)]
    [int]$Sign = -1,
    [ValidateRange(1.0, 3.0)]
    [double]$RoiScale = 1.5,
    [int]$Frames = 110
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$artifact = Join-Path $build "Artifacts/rolling-return-at-75-capture"
$log = Join-Path $build "Artifacts/rolling-return-at-75.log"
$exe = Join-Path $build "$Configuration/TankSandbox.exe"
$python = "C:\Users\wkawa\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"

cmake --build $build --config $Configuration --target TankSandbox
if (Test-Path -LiteralPath $artifact) { Remove-Item -LiteralPath $artifact -Recurse -Force }

Start-Process -FilePath $exe -WorkingDirectory $build -Wait -ArgumentList @(
    "--scene", "tracked-vehicle",
    "--roll-capture-dir", $artifact,
    "--roll-capture-sign", $Sign,
    "--roll-capture-return-at-decision-angle",
    "--roll-capture-frames", $Frames,
    "--roll-capture-interval", "2",
    "-LogToFile", $log
)

& $python (Join-Path $root "scripts/render_roll_capture_gif.py") `
    --single-dir $artifact `
    --output (Join-Path $root $OutputPath) --roi-scale $RoiScale
