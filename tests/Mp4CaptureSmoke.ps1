param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../build'),
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$buildPath = (Resolve-Path -LiteralPath $BuildDirectory).Path
$exe = Join-Path $buildPath 'Debug/TankSandbox.exe'
$inspector = Join-Path $buildPath 'External/RtPbrSurvey/Debug/RtPbrSurvey.Mp4EncoderTests.exe'
$output = Join-Path $buildPath ('Mp4CaptureSmoke/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output -Force | Out-Null

function Invoke-Mp4Case([string]$Name, [string[]]$Options, [int]$Frames = 3) {
    $casePath = Join-Path $output $Name
    New-Item -ItemType Directory -Path $casePath -Force | Out-Null
    $arguments = @('--scene', 'box-drop', '-CaptureSessionFormat', 'mp4',
        '-CaptureSessionOutputDir', ('"' + $casePath + '"'), '-CaptureSessionBaseName', $Name,
        '-CaptureSessionClock', 'fixed-step',
        '-CaptureSessionWarmupFrames', '3', '-ExitAfterCapture',
        '-LogToFile', ('"' + (Join-Path $casePath 'capture.log') + '"')) + $Options
    if ($Frames -gt 0) { $arguments += @('-CaptureSessionFrames', "$Frames") }
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $buildPath -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        throw "$Name timed out. Output retained at $casePath"
    }
    if ($process.ExitCode -ne 0) { throw "$Name failed with exit code $($process.ExitCode). See $casePath" }
    if (Select-String -LiteralPath (Join-Path $casePath 'capture.log') -Pattern '\[(ERROR|CORRUPTION)\]') {
        throw "$Name reported a D3D12 error. See $casePath"
    }
    return $casePath
}

function Assert-Mp4([string]$Path, [int]$Width, [int]$Height, [int]$Fps) {
    & $inspector $Path $Width $Height 3 $Fps
    if ($LASTEXITCODE -ne 0) { throw "MP4 decode validation failed: $Path" }
}

$full = Invoke-Mp4Case 'full' @('-CaptureSessionFps', '60')
Assert-Mp4 (Join-Path $full 'full.mp4') 1920 1080 60

$roiOptions = @('-CaptureSessionFps', '30', '-CaptureSessionMp4BitrateMbps', '8',
    '-CaptureSessionSubfolder', 'takes/center', '-CaptureSessionRoi', '100', '80', '317', '239')
$roi = Invoke-Mp4Case 'roi' $roiOptions
$first = Join-Path $roi 'takes/center/roi.mp4'
Assert-Mp4 $first 318 240 30
$firstHash = (Get-FileHash -LiteralPath $first).Hash
$null = Invoke-Mp4Case 'roi' $roiOptions
Assert-Mp4 (Join-Path $roi 'takes/center/roi_000001.mp4') 318 240 30
if ((Get-FileHash -LiteralPath $first).Hash -ne $firstHash) { throw 'The earlier MP4 was overwritten' }

$realtime = Invoke-Mp4Case 'realtime' @('-CaptureSessionClock', 'real-time',
    '-CaptureSessionFps', '60', '-CaptureSessionDurationSeconds', '0.75') 0
& $inspector --duration (Join-Path $realtime 'realtime.mp4') 1920 1080 60 0.75
if ($LASTEXITCODE -ne 0) { throw 'Real-time MP4 playback duration differs from capture duration' }

Write-Output "MP4 GPU smoke checks passed. Output retained at $output"
