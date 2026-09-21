# =========================================================================
# Build-Release.ps1 - Automated Release & Patch Builder
# Usage:
#   .\Build-Release.ps1
#   .\Build-Release.ps1 -Version "0.0.2"
#   .\Build-Release.ps1 -Version "0.0.2" -PatchOnly
#   .\Build-Release.ps1 -Version "0.0.2" -InstallerOnly
# =========================================================================
param(
    [string]$Version = "0.0.1",
    [string]$Tag = "GAMELAND",
    [string]$BaseGameDir = "F:\CS 1.6 - AllClient",
    [switch]$PatchOnly,
    [switch]$InstallerOnly
)

$ErrorActionPreference = "Stop"

function Write-Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "    [OK] $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "    [WARN] $msg" -ForegroundColor Yellow }

$rootDir = $PSScriptRoot
$outDir  = Join-Path $rootDir "out_release"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# ---- Step 1: Compile binaries via CMake ----
Write-Step "Building target binaries (BUILD_ALL)..."
$installStage = Join-Path $rootDir "install"
New-Item -ItemType Directory -Force -Path $installStage | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $installStage "cstrike\cl_dlls") | Out-Null

cmake --build --preset vs2022-release --target BUILD_ALL -- /nodeReuse:false /v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed!"
}
Write-OK "Binaries compiled successfully."

# Write metadata
Set-Content -LiteralPath (Join-Path $installStage "version.txt") -Value $Version -Encoding ascii -NoNewline
@(
    "Client tag: $Tag"
    "Version: $Version"
    "Built locally: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
) | Set-Content -LiteralPath (Join-Path $installStage "build-info.txt") -Encoding utf8
Copy-Item -LiteralPath (Join-Path $rootDir "pinned_servers.txt") -Destination (Join-Path $installStage "pinned_servers.txt") -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $rootDir "mix_servers.txt") -Destination (Join-Path $installStage "mix_servers.txt") -Force -ErrorAction SilentlyContinue

# ---- Step 2: Build lightweight patch ZIP (for web panel upload) ----
if (-not $InstallerOnly) {
    Write-Step "Packaging lightweight client patch ZIP..."
    $patchStage = Join-Path $env:TEMP "allclient-local-patch"
    Remove-Item -LiteralPath $patchStage -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $patchStage | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $patchStage "cstrike\cl_dlls") | Out-Null

    $outBin = Join-Path $rootDir "out\bin\Release"
    $allclientDir = "F:\Allclient"

    $binaries = @(
        "Allclient.exe",
        "cstrike.exe",
        "updater.exe",
        "FileSystem_Proxy.dll",
        "next_engine_mini.dll",
        "nitro_api2.dll",
        "steam_api.dll",
        "vgui2.dll",
        "pinned_servers.txt",
        "mix_servers.txt",
        "build-info.txt",
        "version.txt",
        "allclient-install.ini"
    )

    foreach ($bin in $binaries) {
        $src = Join-Path $installStage $bin
        if (-not (Test-Path -LiteralPath $src -PathType Leaf)) {
            $src = Join-Path $outBin $bin
        }
        if (-not (Test-Path -LiteralPath $src -PathType Leaf)) {
            $src = Join-Path $allclientDir $bin
        }
        if (Test-Path -LiteralPath $src -PathType Leaf) {
            Copy-Item -LiteralPath $src -Destination (Join-Path $patchStage $bin) -Force
        }
    }

    $clDlls = @("client_mini.dll", "GameUI.dll")
    foreach ($dll in $clDlls) {
        $src = Join-Path $installStage "cstrike\cl_dlls\$dll"
        if (-not (Test-Path -LiteralPath $src -PathType Leaf)) {
            $src = Join-Path $outBin "cstrike\cl_dlls\$dll"
        }
        if (-not (Test-Path -LiteralPath $src -PathType Leaf)) {
            $src = Join-Path $allclientDir "cstrike\cl_dlls\$dll"
        }
        if (Test-Path -LiteralPath $src -PathType Leaf) {
            Copy-Item -LiteralPath $src -Destination (Join-Path $patchStage "cstrike\cl_dlls\$dll") -Force
        }
    }

    $versionedPatchZip = Join-Path $outDir "Allclient-v$Version-Patch.zip"
    $latestPatchZip    = Join-Path $outDir "Allclient-Patch.zip"

    Remove-Item -LiteralPath $versionedPatchZip -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $latestPatchZip -Force -ErrorAction SilentlyContinue

    Compress-Archive -Path (Join-Path $patchStage "*") -DestinationPath $versionedPatchZip -Force
    Copy-Item -LiteralPath $versionedPatchZip -Destination $latestPatchZip -Force

    $patchMb = [math]::Round((Get-Item -LiteralPath $versionedPatchZip).Length / 1MB, 2)
    Write-OK "Patch ZIP created successfully ($patchMb MB):"
    Write-Host "    -> $versionedPatchZip" -ForegroundColor Yellow
    Write-Host "    -> $latestPatchZip" -ForegroundColor Yellow
}

# ---- Step 3: Build Smart Universal Installer (Allclient-Setup.exe) ----
if (-not $PatchOnly) {
    Write-Step "Compiling smart installer with Inno Setup..."
    $iscc = "C:\Program Files\Inno Setup 7\ISCC.exe"
    if (-not (Test-Path -LiteralPath $iscc -PathType Leaf)) {
        $iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    }

    if (-not (Test-Path -LiteralPath $iscc -PathType Leaf)) {
        Write-Warn "Inno Setup compiler (ISCC.exe) not found on system. Skipping installer."
    } else {
        $sourceRoot = $BaseGameDir
        if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
            Write-Warn "Base game directory '$sourceRoot' not found. Using install stage folder."
            $sourceRoot = $installStage
        }

        $issScript = Join-Path $rootDir "installer\Allclient.iss"
        $compiledExe = Join-Path $rootDir "installer\output\Allclient-Setup.exe"
        $finalExe = Join-Path $outDir "Allclient-Setup.exe"

        & $iscc "/DSourceRoot=$sourceRoot" "/DAppVersion=$Version" "/DBuildTag=$Tag" $issScript
        if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $compiledExe -PathType Leaf)) {
            Copy-Item -LiteralPath $compiledExe -Destination $finalExe -Force
            $installerMb = [math]::Round((Get-Item -LiteralPath $finalExe).Length / 1MB, 1)
            Write-OK "Smart Installer created successfully ($installerMb MB):"
            Write-Host "    -> $finalExe" -ForegroundColor Green
        } else {
            throw "Inno Setup compilation failed."
        }
    }
}

Write-Host "`n========================================================" -ForegroundColor Green
Write-Host " Done! Release files are located in:" -ForegroundColor Green
Write-Host " $outDir" -ForegroundColor Yellow
Write-Host "========================================================`n" -ForegroundColor Green
