param(
    [string]$Version = "3.5.262"
)

$ErrorActionPreference = "Stop"

$workspace = Split-Path -Parent $PSScriptRoot
$packageName = "Microsoft.GameInput.$Version"
$packageDirectory = Join-Path $workspace "packages\$packageName"
$packageArchive = Join-Path $workspace "$packageName.nupkg"

if (Test-Path (Join-Path $packageDirectory "native\include\v0\GameInput.h"))
{
    Write-Host "GameInput package already restored: $packageDirectory"
    exit 0
}

Write-Host "Downloading Microsoft.GameInput $Version..."
Invoke-WebRequest `
    -Uri "https://www.nuget.org/api/v2/package/Microsoft.GameInput/$Version" `
    -OutFile $packageArchive

New-Item -ItemType Directory -Force -Path $packageDirectory | Out-Null
tar -xf $packageArchive -C $packageDirectory

Write-Host "GameInput package restored: $packageDirectory"
