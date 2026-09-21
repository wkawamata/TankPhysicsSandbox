param(
    [string]$Version = "3.5.270"
)

$ErrorActionPreference = "Stop"

$workspace = Split-Path -Parent $PSScriptRoot
$packageName = "Microsoft.GameInput.$Version"
$packageDirectory = Join-Path $workspace "packages\$packageName"
$packageArchive = Join-Path $workspace "$packageName.nupkg"
$headerPath = Join-Path $packageDirectory "native\include\GameInput.h"
$libraryPath = Join-Path $packageDirectory "native\lib\x64\GameInput.lib"
$redistMsiPath = Join-Path $packageDirectory "redist\GameInputRedist.msi"
$runtimeDirectory = Join-Path $packageDirectory "redist\x64"
$runtimePath = Join-Path $runtimeDirectory "GameInputRedist.dll"

if (-not (Test-Path $headerPath) -or -not (Test-Path $libraryPath))
{
    Write-Host "Downloading Microsoft.GameInput $Version..."
    Invoke-WebRequest `
        -Uri "https://www.nuget.org/api/v2/package/Microsoft.GameInput/$Version" `
        -OutFile $packageArchive

    New-Item -ItemType Directory -Force -Path $packageDirectory | Out-Null
    tar -xf $packageArchive -C $packageDirectory
}

if (-not (Test-Path $redistMsiPath))
{
    throw "GameInput redistributable MSI was not found: $redistMsiPath"
}

$installedRuntimePath = Join-Path $env:SystemRoot "System32\GameInputRedist.dll"
if (-not (Test-Path $installedRuntimePath))
{
    throw "GameInput runtime is not installed. Install $redistMsiPath, then run this script again."
}

$installedVersion = (Get-Item $installedRuntimePath).VersionInfo.FileVersion
$installedSemanticVersion = [Version]$installedVersion
$requiredSemanticVersion = [Version]$Version
if ($installedSemanticVersion.Major -ne $requiredSemanticVersion.Major -or
    $installedSemanticVersion.Minor -ne $requiredSemanticVersion.Minor -or
    $installedSemanticVersion.Build -ne $requiredSemanticVersion.Build)
{
    throw "Installed GameInput runtime is $installedVersion, but $Version is required. Install $redistMsiPath, then run this script again."
}

New-Item -ItemType Directory -Force -Path $runtimeDirectory | Out-Null
Copy-Item -LiteralPath $installedRuntimePath -Destination $runtimePath -Force

Write-Host "GameInput package restored: $packageDirectory"
Write-Host "Pinned GameInput runtime ${installedVersion}: $runtimePath"
