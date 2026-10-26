param(
    [Parameter(Mandatory = $true)][string[]]$Ids,
    [double]$Distance = 15.0,
    [double]$Fov = 50.0
)
# Simulates how the night preview tile reads at a gameplay distance: the preview camera sits at
# radius / tan(fov/2) * 1.25 from the bounds centre; the tile is shrunk by preview_dist / Distance
# (render is 1600x900, tile 800x450) and blown back up with nearest neighbour into one strip.
Add-Type -AssemblyName System.Drawing
$Ids = @($Ids | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$root = 'S:\Repos\trick-or-treat'
$out = "$root\art\team\decor_ground\batch03\_far\far_$($Ids -join '_').png"
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
$tiles = @()
foreach ($id in $Ids) {
    $gltf = Get-Content "$root\game\assets\models\props\$id.gltf" -Raw | ConvertFrom-Json
    $lo = @(1e9, 1e9, 1e9); $hi = @(-1e9, -1e9, -1e9)
    foreach ($acc in $gltf.accessors) { if ($acc.type -eq 'VEC3' -and $acc.min -and $acc.max -and $acc.min.Count -eq 3) {
        # only POSITION accessors carry min/max in Blender exports
        for ($k = 0; $k -lt 3; $k++) { $lo[$k] = [Math]::Min($lo[$k], $acc.min[$k]); $hi[$k] = [Math]::Max($hi[$k], $acc.max[$k]) } } }
    $diag = [Math]::Sqrt(($hi[0] - $lo[0]) * ($hi[0] - $lo[0]) + ($hi[1] - $lo[1]) * ($hi[1] - $lo[1]) + ($hi[2] - $lo[2]) * ($hi[2] - $lo[2]))
    $radius = [Math]::Max($diag * 0.5, 0.5)
    $dist = $radius / [Math]::Tan($Fov * 0.5 * [Math]::PI / 180) * 1.25
    $f = [Math]::Min(1.0, $dist / $Distance)
    $src = [System.Drawing.Bitmap]::FromFile("$root\art\review\engine\$id.png")
    $tile = $src.Clone((New-Object System.Drawing.Rectangle 0, 0, 800, 450), $src.PixelFormat)
    $w = [int][Math]::Max(8, 1600 * $f); $h = [int][Math]::Max(5, 900 * $f)
    $small = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($small); $g.InterpolationMode = 'HighQualityBicubic'
    $g.DrawImage($tile, 0, 0, $w, $h); $g.Dispose()
    # crop the centre 1/3 so the asset fills the strip cell, then enlarge x(4) nearest
    $cw = [int]($w / 2.5); $ch = [int]($h / 2.0)
    $crop = $small.Clone((New-Object System.Drawing.Rectangle ([int](($w - $cw) / 2)), ([int](($h - $ch) / 2)), $cw, $ch), $small.PixelFormat)
    $tiles += , @($crop, $id, $f)
    $src.Dispose()
}
$cell = 320
$strip = New-Object System.Drawing.Bitmap ($cell * $tiles.Count), $cell
$g = [System.Drawing.Graphics]::FromImage($strip)
$g.InterpolationMode = 'NearestNeighbor'; $g.PixelOffsetMode = 'Half'
$i = 0
foreach ($t in $tiles) {
    $c = $t[0]; $s = [Math]::Min($cell / $c.Width, $cell / $c.Height)
    $g.DrawImage($c, $i * $cell, 0, [int]($c.Width * $s), [int]($c.Height * $s))
    $g.DrawString("$($t[1]) x$([Math]::Round($t[2],3))", (New-Object System.Drawing.Font 'Arial', 9), [System.Drawing.Brushes]::White, $i * $cell + 4, 4)
    $i++
}
$g.Dispose()
$strip.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output $out

