<#
.SYNOPSIS
    Automated CS 1.6 Demo to MP4 Video Renderer for Allclient / NextClient.
.DESCRIPTION
    Runs an isolated render session of cstrike.exe at fixed 60 FPS,
    captures frames and audio, encodes to high-quality H.264/AAC MP4 via FFmpeg,
    cleans up temp files, and opens the resulting video in Explorer.
#>

param(
    [Parameter(Mandatory=$true)][string]$DemoPath,
    [string]$OutputDir = "",
    [int]$Fps = 60,
    [string]$FfmpegPath = ""
)

$ErrorActionPreference = "Stop"

# Set process priority to BelowNormal so live gameplay or other apps are never impacted
try {
    [System.Diagnostics.Process]::GetCurrentProcess().PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
} catch {}

$scriptDir = $PSScriptRoot
if (-not $scriptDir) { $scriptDir = (Get-Location).Path }
$rootDir = $scriptDir
$cstrikeDir = Join-Path $rootDir "cstrike"

if (-not (Test-Path $cstrikeDir)) {
    # If script is run from a subfolder
    $rootDir = Split-Path -Parent $scriptDir
    $cstrikeDir = Join-Path $rootDir "cstrike"
}

if (-not $OutputDir) {
    $OutputDir = Join-Path $cstrikeDir "videos"
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$demoFileName = [System.IO.Path]::GetFileName($DemoPath)
$demoBaseName = [System.IO.Path]::GetFileNameWithoutExtension($DemoPath)
$outputMp4 = Join-Path $OutputDir "$demoBaseName.mp4"

# 1. Locate FFmpeg
if (-not $FfmpegPath -or -not (Test-Path $FfmpegPath)) {
    if (Test-Path (Join-Path $rootDir "ffmpeg.exe")) {
        $FfmpegPath = Join-Path $rootDir "ffmpeg.exe"
    } elseif (Test-Path (Join-Path $cstrikeDir "ffmpeg.exe")) {
        $FfmpegPath = Join-Path $cstrikeDir "ffmpeg.exe"
    } else {
        $cmd = Get-Command "ffmpeg.exe" -ErrorAction SilentlyContinue
        if ($cmd) { $FfmpegPath = $cmd.Source }
    }
}

if (-not $FfmpegPath) {
    Write-Host "[ERROR] ffmpeg.exe was not found. Please place ffmpeg.exe in the game folder." -ForegroundColor Red
    exit 1
}

Write-Host "==> Starting Demo to MP4 Render Pipeline" -ForegroundColor Cyan
Write-Host "    Demo:       $demoFileName"
Write-Host "    Framerate:  $Fps FPS (Fixed Mathematical Time)"
Write-Host "    Output:     $outputMp4"

# Ensure the demo is inside cstrike folder
$cstrikeDemoPath = Join-Path $cstrikeDir $demoFileName
if ($DemoPath -ne $cstrikeDemoPath) {
    Copy-Item -Path $DemoPath -Destination $cstrikeDemoPath -Force
}

# Unique temporary prefix for frames
$tmpPrefix = "dem2vid_" + (Get-Random -Minimum 1000 -Maximum 9999)
$tmpWav = Join-Path $cstrikeDir "$tmpPrefix.wav"

$cstrikeExe = Join-Path $rootDir "cstrike.exe"
if (-not (Test-Path $cstrikeExe)) {
    Write-Host "[ERROR] cstrike.exe not found at $cstrikeExe" -ForegroundColor Red
    exit 1
}

# 2. Launch cstrike in isolated demo render mode
$launchArgs = "-game cstrike -demorender +viewdemo `"$demoFileName`" +startmovie `"$tmpPrefix`" $Fps"
Write-Host "==> Launching isolated engine session..." -ForegroundColor Yellow

$proc = Start-Process -FilePath $cstrikeExe -ArgumentList $launchArgs -PassThru -WindowStyle Minimized
try {
    $proc.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
} catch {}

# Wait for playback and render to complete
$proc.WaitForExit()
Write-Host "==> Engine playback completed." -ForegroundColor Green

# 3. Check for rendered frames
$frames = Get-ChildItem -Path $cstrikeDir -Filter "$tmpPrefix*.bmp"
if ($frames.Count -eq 0) {
    Write-Host "[WARN] No frames were captured. Demo might have been cancelled." -ForegroundColor Yellow
    exit 0
}

Write-Host "==> Captured $($frames.Count) frames. Encoding to MP4 via FFmpeg..." -ForegroundColor Cyan

# 4. Encode to MP4 with pristine visual quality (H.264 High Profile, CRF 18, AAC audio)
$inputFrames = Join-Path $cstrikeDir "$tmpPrefix%04d.bmp"

$ffmpegArgs = @(
    "-y",
    "-framerate", "$Fps",
    "-i", $inputFrames
)

if (Test-Path $tmpWav) {
    $ffmpegArgs += @("-i", $tmpWav, "-c:a", "aac", "-b:a", "192k")
}

$ffmpegArgs += @(
    "-c:v", "libx264",
    "-preset", "medium",
    "-crf", "18",
    "-pix_fmt", "yuv420p",
    $outputMp4
)

$ffProc = Start-Process -FilePath $FfmpegPath -ArgumentList $ffmpegArgs -PassThru -NoNewWindow -Wait
try {
    $ffProc.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
} catch {}

# 5. Cleanup temporary raw frames and wav
Write-Host "==> Cleaning up temporary frame files..." -ForegroundColor Gray
Remove-Item -Path (Join-Path $cstrikeDir "$tmpPrefix*.bmp") -Force -ErrorAction SilentlyContinue
Remove-Item -Path $tmpWav -Force -ErrorAction SilentlyContinue

if (Test-Path $outputMp4) {
    Write-Host "`n[SUCCESS] Video rendered successfully: $outputMp4" -ForegroundColor Green
    Start-Process "explorer.exe" -ArgumentList "/select,`"$outputMp4`""
} else {
    Write-Host "[ERROR] Output MP4 was not created." -ForegroundColor Red
}
