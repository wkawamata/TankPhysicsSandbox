param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../build'),
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$buildPath = (Resolve-Path -LiteralPath $BuildDirectory).Path
$exe = Join-Path $buildPath 'Debug/TankSandbox.exe'
$output = Join-Path $buildPath ('CaptureSmoke/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output -Force | Out-Null

function Invoke-CaptureCase([string]$Name, [string[]]$Options, [bool]$ShouldSucceed) {
    $casePath = Join-Path $output $Name
    New-Item -ItemType Directory -Path $casePath | Out-Null
    $arguments = @('--scene', 'box-drop', '-CaptureSessionOutputDir', ('"' + $casePath + '"'),
        '-CaptureSessionBaseName', $Name, '-CaptureSessionClock', 'fixed-step',
        '-CaptureSessionWarmupFrames', '3', '-ExitAfterCapture',
        '-LogToFile', ('"' + (Join-Path $casePath 'capture.log') + '"')) + $Options
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $buildPath -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        throw "$Name timed out. Output retained at $casePath"
    }
    if (($process.ExitCode -eq 0) -ne $ShouldSucceed) {
        throw "$Name returned unexpected exit code $($process.ExitCode). See $casePath"
    }
    if ($ShouldSucceed -and (Select-String -LiteralPath (Join-Path $casePath 'capture.log') -Pattern '\[(ERROR|CORRUPTION)\]')) {
        throw "$Name reported a D3D12 error. See $casePath"
    }
    Write-Output "$Name exit=$($process.ExitCode)"
    return $casePath
}

function Assert-Png([string]$Path, [int]$Width, [int]$Height) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ([BitConverter]::ToString($bytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A') { throw "Invalid PNG: $Path" }
    $actualWidth = ([int]$bytes[16] -shl 24) -bor ([int]$bytes[17] -shl 16) -bor ([int]$bytes[18] -shl 8) -bor $bytes[19]
    $actualHeight = ([int]$bytes[20] -shl 24) -bor ([int]$bytes[21] -shl 16) -bor ([int]$bytes[22] -shl 8) -bor $bytes[23]
    if ($actualWidth -ne $Width -or $actualHeight -ne $Height) { throw "Unexpected PNG dimensions: ${actualWidth}x${actualHeight}" }
}

$result = @(Invoke-CaptureCase 'full' @('--scene', 'tracked-vehicle', '-CaptureSessionFrames', '2') $true)
$full = $result[-1]
Assert-Png (Join-Path $full 'full_000000.png') 1920 1080
Assert-Png (Join-Path $full 'full_000001.png') 1920 1080
if ((Get-ChildItem -LiteralPath $full -Filter '*.png').Count -ne 2) { throw 'Unexpected full-frame count' }
$trace = Select-String -LiteralPath (Join-Path $full 'capture.log') -Pattern '\[CAPTURE_FRAME\].*simulation=([0-9.]+)'
if ($trace.Count -ne 2) { throw 'Missing capture timing trace' }
$times = @($trace | ForEach-Object { [double]::Parse($_.Matches[0].Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture) })
if ([Math]::Abs(($times[1] - $times[0]) - 1.0 / 60.0) -gt 0.000001) { throw 'Readback wait advanced the physics clock' }

$result = @(Invoke-CaptureCase 'roi' @('-CaptureSessionFrames', '2', '-CaptureSessionRoi', '100', '80', '317', '239') $true)
$roi = $result[-1]
Assert-Png (Join-Path $roi 'roi_000000.png') 317 239
Assert-Png (Join-Path $roi 'roi_000001.png') 317 239

$result = @(Invoke-CaptureCase 'duration' @('-CaptureSessionFps', '30', '-CaptureSessionDurationSeconds', '0.09', '-CaptureSessionWarmupFrames', '90') $true)
$duration = $result[-1]
if ((Get-ChildItem -LiteralPath $duration -Filter '*.png').Count -ne 3) { throw 'Duration should exclude warmup and save three frames' }

$result = @(Invoke-CaptureCase 'exr' @('-CaptureSessionFormat', 'exr', '-CaptureSessionFrames', '2') $true)
$exr = $result[-1]
foreach ($index in 0..1) {
    $bytes = [IO.File]::ReadAllBytes((Join-Path $exr ('exr_{0:D6}.exr' -f $index)))
    if ([BitConverter]::ToString($bytes[0..3]) -ne '76-2F-31-01') { throw 'Invalid EXR magic' }
}

Invoke-CaptureCase 'unsupported' @('-CaptureSessionFormat', 'gif', '-CaptureSessionFrames', '1') $false
Invoke-CaptureCase 'invalid-fps' @('-CaptureSessionFps', '0', '-CaptureSessionFrames', '1') $false
Invoke-CaptureCase 'invalid-roi' @('-CaptureSessionFrames', '1', '-CaptureSessionRoi', '1900', '1000', '320', '240') $false
Write-Output "Capture smoke passed. Artifacts: $output"
