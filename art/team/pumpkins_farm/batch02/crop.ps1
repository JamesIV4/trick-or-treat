param(
    [Parameter(Mandatory = $true)][string]$Id,
    [int]$X = 0, [int]$Y = 0, [int]$W = 800, [int]$H = 450, [double]$Scale = 2.0, [string]$Tag = 'crop'
)
# Crops a region of an engine preview (1600x900 sheet coordinates) and enlarges it for close review.
Add-Type -AssemblyName System.Drawing
$root = 'S:\Repos\trick-or-treat'
$src = [System.Drawing.Bitmap]::FromFile("$root\art\review\engine\$Id.png")
$c = $src.Clone((New-Object System.Drawing.Rectangle $X, $Y, $W, $H), $src.PixelFormat)
$out = New-Object System.Drawing.Bitmap ([int]($W * $Scale)), ([int]($H * $Scale))
$g = [System.Drawing.Graphics]::FromImage($out); $g.InterpolationMode = 'HighQualityBicubic'
$g.DrawImage($c, 0, 0, $out.Width, $out.Height); $g.Dispose()
$path = "$root\art\team\pumpkins_farm\batch02\_far\${Id}_$Tag.png"
$out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
$src.Dispose()
Write-Output $path
