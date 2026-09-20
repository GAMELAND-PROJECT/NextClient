# ====================================================
# Build-Test.ps1 - اسکریپت بیلد برای تست توسعه
# استفاده: .\Build-Test.ps1 [-Configure] [-InstallDir "F:\Allclient"]
# ====================================================
param(
    [switch]$Configure,
    [string]$InstallDir = "F:\Allclient"
)

$ErrorActionPreference = "Stop"

function Write-Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "    [OK] $msg" -ForegroundColor Green }

# ---- گام 1: پیکربندی (در صورت نیاز با -Configure) ----
if ($Configure) {
    Write-Step "پیکربندی CMake برای مسیر: $InstallDir"
    cmake --fresh --preset vs2022 "-DNEXTCLIENT_INSTALL_DIR=$InstallDir"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed!" }
    Write-OK "پیکربندی با موفقیت انجام شد."
}

# ---- گام 2: بیلد ----
Write-Step "در حال بیلد کردن..."
cmake --build --preset vs2022-release --target BUILD_ALL -- /nodeReuse:false /v:minimal
if ($LASTEXITCODE -ne 0) { throw "Build failed!" }
Write-OK "بیلد با موفقیت انجام شد."

# ---- گام 3: ساخت gameland_license.dat ----
Write-Step "ساخت فایل gameland_license.dat..."
$licPath = Join-Path $InstallDir "gameland_license.dat"

$key       = "NextClientSecureRC4Key2026!"
$plaintext = "GAMELAND"

$S = [int[]](0..255)
$j = 0
for ($i = 0; $i -lt 256; $i++) {
    $j = ($j + $S[$i] + [byte][char]$key[$i % $key.Length]) % 256
    $tmp = $S[$i]; $S[$i] = $S[$j]; $S[$j] = $tmp
}

$data   = [System.Text.Encoding]::ASCII.GetBytes($plaintext)
$i = 0; $j = 0
$output = [byte[]]::new($data.Length)
for ($n = 0; $n -lt $data.Length; $n++) {
    $i = ($i + 1) % 256
    $j = ($j + $S[$i]) % 256
    $tmp = $S[$i]; $S[$i] = $S[$j]; $S[$j] = $tmp
    $output[$n] = $data[$n] -bxor $S[($S[$i] + $S[$j]) % 256]
}

[System.IO.File]::WriteAllBytes($licPath, $output)
Write-OK "فایل ساخته شد: $licPath"

Write-Host ""
Write-Host "همه چیز آماده است! بازی را از '$InstallDir\cstrike.exe' اجرا کنید." -ForegroundColor Green
Write-Host ""
