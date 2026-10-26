param(
    [string[]]$Keys = @(),        # knot_a,knot_b,knot_c,root_mass (all when empty)
    [switch]$NoPreview,
    [switch]$Sheet               # also render the Blender review sheets
)
# Build thorn-vine assets with Blender, then gate + engine-preview each one.
#   powershell -NoProfile -File art\team\vines\batch03\go.ps1 -Keys knot_a,root_mass
$Keys = @($Keys | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
if (-not $Keys) { $Keys = @('knot_a', 'knot_b', 'knot_c', 'root_mass') }
$root = 'S:\Repos\trick-or-treat'
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
$py = "$root\art\team\vines\batch03\build_vines.py"
$flags = @(); if (-not $Sheet) { $flags += '--nosheet' }
$out = & $blender --background --factory-startup --python $py -- @Keys @flags 2>&1
$out | Select-String -Pattern 'RESULT|spines|Error|Traceback|WARN|File "' | ForEach-Object { $_.Line }
$ids = @($Keys | ForEach-Object { if ($_ -eq 'root_mass') { 'thorn_root_mass' } else { "thorn_$_" } })
Push-Location $root
python art/tools/gates.py @ids
if (-not $NoPreview) {
    foreach ($id in $ids) { powershell -NoProfile -File tools\asset_preview.ps1 -Id $id }
}
Pop-Location
