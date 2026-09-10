param([string]$LocalRuntimeDirectory = 'F:\')
$ErrorActionPreference = 'Stop'
$destination = Join-Path $PSScriptRoot 'runtime'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$sources = @{
    'vcredist2010_x86.exe' = 'https://download.microsoft.com/download/1/6/5/165255e7-1014-4d0a-b094-b6a430a6bffc/vcredist_x86.exe'
    'vcredist2010_x64.exe' = 'https://download.microsoft.com/download/1/6/5/165255e7-1014-4d0a-b094-b6a430a6bffc/vcredist_x64.exe'
    'vc_redist.x86.exe' = 'https://download.microsoft.com/download/6/d/f/6df3ff94-f7f9-4f0b-838c-a328d1a7d0ee/vc_redist.x86.exe'
    'vc_redist.x64.exe' = 'https://download.microsoft.com/download/6/d/f/6df3ff94-f7f9-4f0b-838c-a328d1a7d0ee/vc_redist.x64.exe'
}
foreach ($name in $sources.Keys) {
    $target = Join-Path $destination $name
    if (-not (Test-Path -LiteralPath $target)) {
        $local = Join-Path $LocalRuntimeDirectory $name
        if (Test-Path -LiteralPath $local) {
            Copy-Item -LiteralPath $local -Destination $target
        } else {
            & curl.exe --fail --location --retry 3 --connect-timeout 20 --max-time 600 --output $target $sources[$name]
            if ($LASTEXITCODE -ne 0) { throw "Runtime download failed: $name" }
        }
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $target
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        throw "Invalid Microsoft signature: $target. Replace this file before building."
    }
    Write-Host "$name verified: $((Get-Item -LiteralPath $target).VersionInfo.FileVersion)"
}
