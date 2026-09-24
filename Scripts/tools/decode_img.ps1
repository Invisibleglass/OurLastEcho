param([string]$In, [string]$Out, [int]$Width = 1280)
# Pulls the first base64 image out of an MCP tool-result dump and saves it (downscaled) as JPEG
$raw = Get-Content -Raw $In
$m = [regex]::Match($raw, 'data\\*"\s*:\s*\\*"([A-Za-z0-9+/=]{1000,})')
$bytes = [Convert]::FromBase64String($m.Groups[1].Value)
Add-Type -AssemblyName System.Drawing
$ms = New-Object IO.MemoryStream(, $bytes)
$img = [Drawing.Image]::FromStream($ms)
$w = [Math]::Min($Width, $img.Width); $h = [int]($img.Height * $w / $img.Width)
$bmp = New-Object Drawing.Bitmap $w, $h
$g = [Drawing.Graphics]::FromImage($bmp); $g.InterpolationMode = 'HighQualityBicubic'; $g.DrawImage($img, 0, 0, $w, $h)
$bmp.Save($Out, [Drawing.Imaging.ImageFormat]::Jpeg)
"$($img.Width)x$($img.Height) -> $Out"
$img.Dispose(); $bmp.Dispose()
