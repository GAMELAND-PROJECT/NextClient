[CmdletBinding()]
param(
    [string]$SourceRoot = 'F:\CS 1.6 - AllClient',
    [string]$BuildDirectory = 'build\vs2022',
    [string]$Compiler = 'C:\Program Files\Inno Setup 7\ISCC.exe'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (-not [IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot $BuildDirectory
}
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$cache = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt')
$installSetting = @($cache | Where-Object { $_ -match '^NEXTCLIENT_INSTALL_DIR:PATH=' })
if ($installSetting.Count -ne 1) { throw 'Configure NEXTCLIENT_INSTALL_DIR before packaging.' }
$installRoot = (Resolve-Path -LiteralPath ($installSetting[0] -replace '^[^=]+=', '')).Path
if ($installRoot -ne $SourceRoot) {
    throw "Build deploys to '$installRoot', but installer packages '$SourceRoot'. Configure matching paths first."
}
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) { throw "Missing Inno Setup compiler: $Compiler" }

# BUILD_ALL builds and deploys every runtime component and the matching assets.
# Compiling the ISS alone used to silently distribute yesterday's game binaries.
& rtk proxy cmake --build $BuildDirectory --config Release --target BUILD_ALL --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Client build/deployment failed; installer was not created.' }

$binaryMap = [ordered]@{
    'cstrike.exe' = 'cstrike.exe'
    'next_engine_mini.dll' = 'next_engine_mini.dll'
    'FileSystem_Proxy.dll' = 'FileSystem_Proxy.dll'
    'cstrike\cl_dlls\client_mini.dll' = 'cstrike\cl_dlls\client_mini.dll'
    'cstrike\cl_dlls\GameUI.dll' = 'cstrike\cl_dlls\GameUI.dll'
    'steam_api.dll' = 'steam_api.dll'
    'nitro_api2.dll' = 'nitro_api2.dll'
    'vgui2.dll' = 'vgui2.dll'
}
$binaryRoot = Join-Path $repoRoot 'out\bin\Release'
$hashes = [ordered]@{}
foreach ($entry in $binaryMap.GetEnumerator()) {
    $builtHash = (Get-FileHash -LiteralPath (Join-Path $binaryRoot $entry.Key) -Algorithm SHA256).Hash
    $installedHash = (Get-FileHash -LiteralPath (Join-Path $SourceRoot $entry.Value) -Algorithm SHA256).Hash
    if ($builtHash -ne $installedHash) { throw "Stale deployment: $($entry.Value)" }
    $hashes[$entry.Value] = $installedHash
}

& rtk proxy $Compiler "/DSourceRoot=$SourceRoot" (Join-Path $PSScriptRoot 'Allclient.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
$setupPath = Join-Path $PSScriptRoot 'output\Allclient-Setup.exe'
$manifest = [ordered]@{
    createdUtc = [DateTime]::UtcNow.ToString('o')
    sourceRoot = $SourceRoot
    installerSha256 = (Get-FileHash -LiteralPath $setupPath -Algorithm SHA256).Hash
    binaries = $hashes
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$setupPath.manifest.json" -Encoding UTF8
Write-Host "Verified installer: $setupPath"
