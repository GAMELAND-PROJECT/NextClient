param([string]$LocalRuntimeDirectory = 'F:\')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$destination = Join-Path $PSScriptRoot 'runtime'
New-Item -ItemType Directory -Force -Path $destination | Out-Null

$sources = @{
    'vcredist2010_x86.exe' = @(
        'https://download.microsoft.com/download/1/6/5/165255e7-1014-4d0a-b094-b6a430a6bffc/vcredist_x86.exe'
    )
    'vcredist2010_x64.exe' = @(
        'https://download.microsoft.com/download/1/6/5/165255e7-1014-4d0a-b094-b6a430a6bffc/vcredist_x64.exe'
    )
    'vc_redist.x86.exe' = @(
        'https://aka.ms/vs/17/release/vc_redist.x86.exe',
        'https://download.visualstudio.microsoft.com/download/pr/2a71910f-4fb9-4c5d-8ab7-440d36e8a2c9/33E7DD4605E613F770E6C72A961B3D935A86033D/vc_redist.x86.exe'
    )
    'vc_redist.x64.exe' = @(
        'https://aka.ms/vs/17/release/vc_redist.x64.exe',
        'https://download.visualstudio.microsoft.com/download/pr/2a71910f-4fb9-4c5d-8ab7-440d36e8a2c9/99AAAD8D5AE3F8065420DDF8FD3F6FAFF61708E4/vc_redist.x64.exe'
    )
}

function Get-TrustedRuntime {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Urls,
        [Parameter(Mandatory = $true)][string]$Target
    )

    $temp = "$Target.download"
    Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue

    foreach ($url in $Urls) {
        for ($attempt = 1; $attempt -le 4; ++$attempt) {
            try {
                Write-Host "Downloading $Name from $url (attempt $attempt of 4)..."
                Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
                & curl.exe --fail --location --retry 4 --connect-timeout 30 --max-time 600 --output $temp $url
                if ($LASTEXITCODE -ne 0) {
                    Write-Warning "curl exited with $LASTEXITCODE; trying Invoke-WebRequest fallback."
                    Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
                    Invoke-WebRequest -Uri $url -OutFile $temp -UseBasicParsing -TimeoutSec 600
                }
                if (-not (Test-Path -LiteralPath $temp -PathType Leaf) -or (Get-Item -LiteralPath $temp).Length -lt 1MB) {
                    throw "downloaded file is missing or unexpectedly small"
                }

                Move-Item -LiteralPath $temp -Destination $Target -Force
                return
            } catch {
                Write-Warning "Download failed for ${Name}: $($_.Exception.Message)"
                Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
                if ($attempt -lt 4) {
                    Start-Sleep -Seconds (5 * $attempt)
                }
            }
        }
    }

    throw "Runtime download failed after all mirrors: $Name"
}

foreach ($name in $sources.Keys) {
    $target = Join-Path $destination $name
    if (-not (Test-Path -LiteralPath $target)) {
        $local = Join-Path $LocalRuntimeDirectory $name
        if (Test-Path -LiteralPath $local) {
            Copy-Item -LiteralPath $local -Destination $target
        } else {
            Get-TrustedRuntime -Name $name -Urls $sources[$name] -Target $target
        }
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $target
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        throw "Invalid Microsoft signature: $target. Replace this file before building."
    }
    Write-Host "$name verified: $((Get-Item -LiteralPath $target).VersionInfo.FileVersion)"
}
