param([Parameter(Mandatory = $true)][string]$Script, [string[]]$Ids = @(), [switch]$Preview, [switch]$NoGates,
      [string[]]$Extra = @())
# Build assets with headless Blender, gate them, optionally render the in-engine preview.
#   powershell -NoProfile -File art\team\vehicles\batch03\run.ps1 build_cars.py parked_car,parked_wagon -Preview
$root = 'S:\Repos\trick-or-treat'
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
$path = Join-Path $PSScriptRoot $Script
$Ids = @($Ids | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
Push-Location $root
$argsList = @('--background', '--factory-startup', '--python', $path, '--') + $Ids + $Extra
$out = & $blender @argsList 2>&1
$out | Where-Object { $_ -match 'ASSET|STATS|BOUNDS|Error|Traceback|line \d+|Exception|WARN' } | ForEach-Object { "$_" }
if (-not $NoGates -and $Ids.Count) { python art/tools/gates.py @Ids }
if ($Preview) { foreach ($id in $Ids) { powershell -NoProfile -File tools\asset_preview.ps1 -Id $id } }
Pop-Location
