# Records the ledge test (run at a roof edge, grab it, hang 45 frames, climb up) as a movie and cuts
# a frame strip of the hang and the climb out of it: <Out>\hang.avi, <Out>\hang_strip.png, plus the
# game's own still of the hang (<Out>\hang.png). Tries lots 0..5 until one has a ledge.
param([string]$Out = 'art\review\showcase\wip_p\hang', [int]$Cols = 8, [string]$Crop = 'iw*0.36:ih*0.56:iw*0.32:ih*0.2')
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = Join-Path $root $Out
$godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
New-Item -ItemType Directory -Force $out | Out-Null
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"   # off-screen, muted, no focus taken (decision 16)
$lock = Enter-GodotLock
try {
    foreach ($lot in 0..5) {
        Remove-Item "$out\hang.avi", "$out\hang.png" -ErrorAction SilentlyContinue
        [void](Start-GodotBackground -Godot $godot -Log "$out\hang.log" -Arguments @('--path', "$root\game", '--resolution', '1280x720', '--write-movie', "$out\hang.avi", '--fixed-fps', '60', '--', '--seed=101', "--ledge-test=$lot", '--ledge-side', "--capture=$out\hang.png", '--capture-frames=100000'))
        $grab = Select-String -Path "$out\hang.log" -Pattern 'LEDGE_' | ForEach-Object { $_.Line }
        "lot $lot : $($grab -join ' | ')"
        if (Test-Path "$out\hang.png") { break }
    }
} finally { Exit-GodotLock $lock }
Select-String -Path "$out\hang.log" -Pattern 'ERROR|SCRIPT ERROR' | Select-Object -First 8 | ForEach-Object { $_.Line }
if (Test-Path "$out\hang.avi") {
    # The last 2.9 s: the jump to the ledge, the hang, the climb.
    & ffmpeg -hide_banner -loglevel error -y -sseof -2.9 -i "$out\hang.avi" -vf "fps=15,crop=$Crop,scale=400:-1,tile=${Cols}x6" -frames:v 1 "$out\hang_strip.png"
    "strip: $out\hang_strip.png"
}
