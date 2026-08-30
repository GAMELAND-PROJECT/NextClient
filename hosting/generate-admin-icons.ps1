param(
    [string]$Source = "$PSScriptRoot/../nextclient/launcher/src/next_launcher/assets/app_icon.ico",
    [string]$Destination = "$PSScriptRoot/admin"
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

foreach ($size in @(192, 512)) {
    $icon = [System.Drawing.Icon]::new($Source, $size, $size)
    $sourceBitmap = $icon.ToBitmap()
    $bitmap = [System.Drawing.Bitmap]::new($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.DrawImage($sourceBitmap, [System.Drawing.Rectangle]::new(0, 0, $size, $size))
        $output = Join-Path $Destination "icon-$size.png"
        $bitmap.Save($output, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $sourceBitmap.Dispose()
        $bitmap.Dispose()
        $icon.Dispose()
    }
}
