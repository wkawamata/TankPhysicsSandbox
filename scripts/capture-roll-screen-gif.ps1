param(
    [string]$Configuration = "Debug",
    [string]$OutputPath = "build/Artifacts/rolling-screen-test.gif",
    [ValidateRange(1.0, 3.0)]
    [double]$RoiScale = 1.5,
    [int]$Frames = 90
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$artifact = Join-Path $build "Artifacts/rolling-screen-capture"
$exe = Join-Path $build "$Configuration/TankSandbox.exe"
$python = "C:\Users\wkawa\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"

cmake --build $build --config $Configuration --target TankSandbox
foreach ($sign in @(-1, 1)) {
    $dir = Join-Path $artifact ("roll_{0:+#;-#}" -f $sign)
    if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
    Start-Process -FilePath $exe -WorkingDirectory $build -Wait -ArgumentList @(
        "--scene", "tracked-vehicle",
        "--roll-capture-dir", $dir,
        "--roll-capture-sign", $sign,
        "--roll-capture-frames", $Frames,
        "--roll-capture-interval", "2"
    )
}

& $python (Join-Path $root "scripts/render_roll_capture_gif.py") `
    --negative-dir (Join-Path $artifact "roll_-1") `
    --positive-dir (Join-Path $artifact "roll_+1") `
    --output (Join-Path $root $OutputPath) --roi-scale $RoiScale
