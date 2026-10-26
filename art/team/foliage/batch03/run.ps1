param([Parameter(Mandatory = $true)][string]$Script, [string[]]$Ids = @(), [switch]$Preview, [switch]$NoGates)
# Build foliage assets with headless Blender, gate them, optionally render the in-engine preview.
#   powershell -NoProfile -File art\team\foliage\batch03\run.ps1 build_maples.py tree_maple_red -Preview
$root = 'S:\Repos\trick-or-treat'
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
$path = Join-Path $PSScriptRoot $Script
$Ids = @($Ids | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
Push-Location $root
$argsList = @('--background', '--factory-startup', '--python', $path)
if ($Ids.Count) { $argsList += '--'; $argsList += $Ids }
$out = & $blender @argsList 2>&1
$out | Where-Object { $_ -match 'ASSET|INFO|Error|Traceback|line \d+|Exception|WARN' } | ForEach-Object { "$_" }
if (-not $NoGates -and $Ids.Count) { python art/tools/gates.py @Ids }
if ($Preview) { foreach ($id in $Ids) { powershell -NoProfile -File tools\asset_preview.ps1 -Id $id } }
Pop-Location
