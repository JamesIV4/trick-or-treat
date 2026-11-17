param(
    [string[]]$Keys = @('skeleton'),
    [switch]$NoPreview,     # skip the engine preview
    [switch]$NoSheet,       # skip the Blender review and pose sheets (faster)
    [switch]$Pieces         # print the triangle count of every piece
)
# Build v2 monsters with Blender, then catalog, gates, rig verifier and the engine preview.
#   powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys skeleton
# Outputs: game\assets\models\monsters\monster_<key>.gltf, rig_spec\monster_<key>.json,
#   art\review\sheets\monster_<key>.png and monster_<key>_poses.png, art\review\engine\monster_<key>.png
$Keys = @($Keys | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
$py = "$PSScriptRoot\build_monsters2.py"
$flags = @()
if ($NoSheet) { $flags += '--nosheet' }
if ($Pieces) { $flags += '--pieces' }
$out = & $blender --background --factory-startup --python $py -- @Keys @flags 2>&1
$out | Select-String -Pattern 'RESULT|PARTS|SHADE|Error|Traceback|WARN|File "|pose sheet|^   ' | ForEach-Object { $_.Line }
$ids = @($Keys | ForEach-Object { "monster_$_" })
Push-Location $root
python art\team\monsters\batch04\catalog_update2.py @ids
python art\tools\gates.py @ids
python art\team\monsters\batch04\verify_rig2.py @ids
python art\tools\publish_catalog.py | Select-String -Pattern 'published|FAIL' | ForEach-Object { $_.Line }
if (-not $NoPreview) {
    foreach ($id in $ids) { powershell -NoProfile -File tools\asset_preview.ps1 -Id $id }
}
Pop-Location
