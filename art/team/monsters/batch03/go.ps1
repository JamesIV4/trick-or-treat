param(
    [Parameter(Mandatory = $true)][string]$Script,   # monsters | nests
    [string[]]$Keys = @(),
    [switch]$NoPreview,
    [switch]$Lift          # floating assets: export raised for the preview, then re-export normally
)
# Build some assets with Blender, then gate + engine-preview each one.
#   powershell -NoProfile -File art\team\monsters\batch03\go.ps1 -Script monsters -Keys pumpkin,bat
$Keys = @($Keys | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$root = 'S:\Repos\trick-or-treat'
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
$py = "$root\art\team\monsters\batch03\build_$Script.py"
$out = & $blender --background --factory-startup --python $py -- @Keys --nosheet 2>&1
$out | Select-String -Pattern 'RESULT|Error|Traceback|WARN|File "' | ForEach-Object { $_.Line }
$prefix = if ($Script -eq 'monsters') { 'monster_' } else { 'nest_' }
$ids = @($Keys | ForEach-Object { "$prefix$_" })
Push-Location $root
python art/tools/gates.py @ids
if (-not $NoPreview) {
    if ($Lift) {
        & $blender --background --factory-startup --python $py -- @Keys --nosheet --lift 2>&1 | Select-String -Pattern 'WARN|Traceback' | ForEach-Object { $_.Line }
    }
    foreach ($id in $ids) { powershell -NoProfile -File tools\asset_preview.ps1 -Id $id }
    if ($Lift) {
        & $blender --background --factory-startup --python $py -- @Keys --nosheet 2>&1 | Select-String -Pattern 'RESULT|Traceback' | ForEach-Object { $_.Line }
        powershell -NoProfile -Command "& { Set-Location '$root'; python art/tools/gates.py $($ids -join ' ') }"
    }
}
Pop-Location
