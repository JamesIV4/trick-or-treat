param([string]$Out = '')
# Engine preview of the three parked cars side by side (gameplay-distance comparison).
# Temporarily writes the lineup over props/parked_car, previews it, then rebuilds parked_car.
$root = 'S:\Repos\trick-or-treat'
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
if (-not $Out) { $Out = Join-Path $PSScriptRoot 'lineup_engine.png' }
Push-Location $root
$gltf = "$root\game\assets\models\props\parked_car.gltf"
& $blender --background --factory-startup --python "$PSScriptRoot\lineup.py" -- $gltf 2>&1 | Where-Object { $_ -match 'LINEUP|Error|Traceback' } | ForEach-Object { "$_" }
powershell -NoProfile -File tools\asset_preview.ps1 -Id parked_car -Out $Out
& $blender --background --factory-startup --python "$PSScriptRoot\build_cars.py" -- parked_car 2>&1 | Where-Object { $_ -match 'ASSET|Error|Traceback' } | ForEach-Object { "$_" }
Pop-Location
