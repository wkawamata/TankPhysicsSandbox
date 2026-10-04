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
    New-Item -ItemType Directory -Path $casePath -Force | Out-Null
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

function Assert-GifRepeat([byte[]]$Bytes, [int]$ExpectedRepeatCount) {
    $applicationOffset = [Text.Encoding]::ASCII.GetString($Bytes).IndexOf('NETSCAPE2.0')
    if ($ExpectedRepeatCount -lt 0) {
        if ($applicationOffset -ge 0) { throw 'Play-once GIF should omit the repeat extension' }
        return
    }
    if ($applicationOffset -lt 0) { throw 'GIF repeat extension is missing' }
    $dataOffset = $applicationOffset + 11
    if ($Bytes[$dataOffset] -ne 3 -or $Bytes[$dataOffset + 1] -ne 1) { throw 'GIF repeat extension is malformed' }
    $actualRepeatCount = [int]$Bytes[$dataOffset + 2] -bor ([int]$Bytes[$dataOffset + 3] -shl 8)
    if ($actualRepeatCount -ne $ExpectedRepeatCount) { throw "Unexpected GIF repeat count: $actualRepeatCount" }
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

$result = @(Invoke-CaptureCase 'gif' @('-CaptureSessionFormat', 'gif', '-CaptureSessionFrames', '2') $true)
$gif = $result[-1]
$gifPath = Join-Path $gif 'gif.gif'
$gifBytes = [IO.File]::ReadAllBytes($gifPath)
if ([Text.Encoding]::ASCII.GetString($gifBytes, 0, 6) -notin @('GIF87a', 'GIF89a')) { throw 'Invalid GIF header' }
Assert-GifRepeat $gifBytes 0
Add-Type -AssemblyName System.Drawing
$gifImage = [Drawing.Image]::FromFile($gifPath)
try {
    if ($gifImage.GetFrameCount([Drawing.Imaging.FrameDimension]::Time) -ne 2) { throw 'GIF should contain two animation frames' }
}
finally {
    $gifImage.Dispose()
}

$gifSettingsOptions = @('-CaptureSessionFormat', 'gif', '-CaptureSessionFrames', '2',
    '-CaptureSessionSubfolder', 'takes/center', '-CaptureSessionRoi', '810', '390', '300', '300')
$gifSettings = Join-Path $output 'gif-settings/takes/center'
$previousGifHashes = @{}
foreach ($repeatCase in @(
    @{ Repeat = 'none'; Count = -1; File = 'gif-settings.gif' },
    @{ Repeat = '3'; Count = 3; File = 'gif-settings_000001.gif' },
    @{ Repeat = 'infinite'; Count = 0; File = 'gif-settings_000002.gif' }
)) {
    $null = Invoke-CaptureCase 'gif-settings' ($gifSettingsOptions + @('-CaptureSessionGifRepeat', $repeatCase.Repeat)) $true
    $gifSettingsPath = Join-Path $gifSettings $repeatCase.File
    Assert-GifRepeat ([IO.File]::ReadAllBytes($gifSettingsPath)) $repeatCase.Count
    foreach ($existingGifPath in $previousGifHashes.Keys) {
        if ((Get-FileHash -LiteralPath $existingGifPath).Hash -ne $previousGifHashes[$existingGifPath]) {
            throw "Existing GIF was overwritten: $existingGifPath"
        }
    }
    $previousGifHashes[$gifSettingsPath] = (Get-FileHash -LiteralPath $gifSettingsPath).Hash
}
if (@(Get-ChildItem -LiteralPath $gifSettings -Filter '*.gif').Count -ne 3) { throw 'Expected three distinct GIF takes' }

Invoke-CaptureCase 'invalid-fps' @('-CaptureSessionFps', '0', '-CaptureSessionFrames', '1') $false
Invoke-CaptureCase 'invalid-roi' @('-CaptureSessionFrames', '1', '-CaptureSessionRoi', '1900', '1000', '320', '240') $false
Write-Output "Capture smoke passed. Artifacts: $output"
