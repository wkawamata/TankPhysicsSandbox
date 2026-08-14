param(
    [ValidateSet("Snapshot", "Slot1")]
    [string]$Settings = "Snapshot"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $root "build"
$executable = Join-Path $buildDirectory "Debug\TankPhysicsCli.exe"
$settingsPath = if ($Settings -eq "Slot1") {
    Join-Path $buildDirectory "Config\tank_physics_slot1.json"
} else {
    Join-Path $root "tests\data\tank_mobility.json"
}

if (-not (Test-Path -LiteralPath $executable)) {
    throw "TankPhysicsCli.exe was not found. Run scripts\build.bat TankPhysicsCli first."
}
if (-not (Test-Path -LiteralPath $settingsPath)) {
    throw "Tank settings were not found: $settingsPath"
}

$reportDirectory = Join-Path $buildDirectory "Reports\Mobility"
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$reportPath = Join-Path $reportDirectory "mobility-$($Settings.ToLowerInvariant())-$timestamp.txt"
$commit = (& git -C $root rev-parse --short HEAD 2>$null)
if (-not $commit) {
    $commit = "unknown"
}

$header = @(
    "Tank Physics Sandbox Mobility Evaluation",
    "timestamp=$(Get-Date -Format o)",
    "settings=$Settings",
    "settings_path=$settingsPath",
    "commit=$commit",
    ""
)
$output = & $executable `
    --test mobility-all `
    --tank-settings $settingsPath `
    --settle-steps 180 `
    --dt 0.0166667 2>&1
$exitCode = $LASTEXITCODE
$lines = @($header) + @($output | ForEach-Object { $_.ToString() }) + @("", "exit_code=$exitCode")
[System.IO.File]::WriteAllLines(
    $reportPath,
    $lines,
    [System.Text.UTF8Encoding]::new($false))
$lines | ForEach-Object { Write-Host $_ }
Write-Host "report=$reportPath"
exit $exitCode
