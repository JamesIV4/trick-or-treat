param(
    [Parameter(Mandatory = $true)][string]$Id,
    [string]$Out = '',
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# In-engine preview of one catalog asset with the real textured toon shader and lighting:
#   tools\asset_preview.ps1 -Id pumpkin_round      -> art\review\engine\pumpkin_round.png
# Top row: night lighting (moon + warm porch light). Bottom row: neutral studio light for textures.
# Imports new/changed models first. Safe to run from several workers at once (serialised by a lock).
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot\.."
. "$PSScriptRoot\godot_lock.ps1"
$catalog = Get-Content "$root\art\asset_catalog.json" -Raw | ConvertFrom-Json
$asset = $catalog.assets | Where-Object { $_.id -eq $Id }
if (-not $asset) { throw "Unknown asset id '$Id'" }
$res = "res://assets/models/$($asset.path).gltf"
if (-not (Test-Path "$root\game\assets\models\$($asset.path).gltf")) { throw "Not exported yet: $res" }
if (-not $Out) { New-Item -ItemType Directory -Force "$root\art\review\engine" | Out-Null; $Out = "$root\art\review\engine\$Id.png" }
$Out = [IO.Path]::GetFullPath($Out)
$ErrorActionPreference = 'Continue'
$lock = Enter-GodotLock
try {
    & $Godot --headless --import --path "$root\game" *> $null
    & $Godot --path "$root\game" --resolution 1600x900 -- "--preview=$res" "--capture=$Out" *> "$Out.log"
} finally {
    Exit-GodotLock $lock
}
if (Test-Path $Out) { Write-Output $Out } else { Write-Output "FAILED (see $Out.log)" }
