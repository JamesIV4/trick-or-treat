param([string[]]$Ids = @('trampoline_large', 'trampoline_mini'), [switch]$Preview)
# Build the trampolines with headless Blender, gate them, optionally render the in-engine previews.
#   powershell -NoProfile -File art\team\play_props\batch03\run.ps1 trampoline_mini -Preview
$root = 'S:\Repos\trick-or-treat'
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
$Ids = @($Ids | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
Push-Location $root
$out = & $blender --background --factory-startup --python "$PSScriptRoot\build_trampolines.py" -- @Ids 2>&1
$out | Where-Object { $_ -match 'ASSET|Error|Traceback|line \d+|Exception' } | ForEach-Object { "$_" }
python art/tools/gates.py @Ids
if ($Preview) { foreach ($id in $Ids) { powershell -NoProfile -File tools\asset_preview.ps1 -Id $id } }
Pop-Location
