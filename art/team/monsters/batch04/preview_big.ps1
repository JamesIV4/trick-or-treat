param(
    [Parameter(Mandatory = $true)][string]$Id,
    [string]$Out = '',
    [int]$Scale = 2,
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# tools\asset_preview.ps1 at a larger window (default 3200x1800), then one crop per quadrant, so a
# small character can be judged close up:
#   art\team\monsters\batch04\preview_big.ps1 -Id monster_skeleton
#   -> art\review\engine\<id>_big.png and <id>_big_q0.png .. _q3.png (night front, night back, studio front, studio side)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
. "$root\tools\godot_lock.ps1"
if (-not $Out) { $Out = "$root\art\review\engine\${Id}_big.png" }
$catalog = Get-Content "$root\art\asset_catalog.json" -Raw | ConvertFrom-Json
$asset = $catalog.assets | Where-Object { $_.id -eq $Id }
if (-not $asset) { throw "Unknown asset id '$Id'" }
$res = "res://assets/models/$($asset.path).gltf"
$lock = Enter-GodotLock
try {
    & $Godot --headless --import --path "$root\game" *> $null
    & $Godot --path "$root\game" --resolution "$(1600 * $Scale)x$(900 * $Scale)" -- "--preview=$res" "--capture=$Out" *> "$Out.log"
} finally {
    Exit-GodotLock $lock
}
if (-not (Test-Path $Out)) { Write-Output "FAILED (see $Out.log)"; exit 1 }
python -c "
import sys
from PIL import Image
p = sys.argv[1]
im = Image.open(p)
w, h = im.size
for k in range(4):
    x0, y0 = (k % 2) * w // 2, (k // 2) * h // 2
    q = im.crop((x0, y0, x0 + w // 2, y0 + h // 2))
    # the character stands in the middle third of its quadrant
    q = q.crop((w // 8, 0, w // 2 - w // 8, h // 2))
    q.save(p[:-4] + '_q%d.png' % k)
print(p)
" $Out
