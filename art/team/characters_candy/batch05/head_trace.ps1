# Records a head-motion trace in real play (the self-playing bot collecting candy) and measures it.
#   head_trace.ps1 -Name before [-Seconds 40] [-NoBuild] [-Fps 0]
# Builds the debug DLL (unless -NoBuild), runs the game with TOT_RIG_DEBUG set under the Godot lock,
# and writes art\review\showcase\wip_p\head\<Name>.log, <Name>.txt (numbers) and <Name>.png (plot).
# -Fps 60 runs with --fixed-fps 60; 0 (default) runs at the game's own frame rate.
param([string]$Name = 'trace', [int]$Seconds = 40, [switch]$NoBuild, [int]$Fps = 0, [int]$Seed = 101)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = "$root\art\review\showcase\wip_p\head"
New-Item -ItemType Directory -Force $out | Out-Null
if (-not $NoBuild) {
    $r = & "$PSScriptRoot\build_native.ps1" -Log 'build_lead.log'
    $r | Select-Object -Last 12
    if (-not ($r -match 'BUILD OK')) { Write-Output 'head_trace: build failed'; exit 1 }
}
. "$root\tools\godot_lock.ps1"
$lock = Enter-GodotLock
try {
    $env:TOT_RIG_DEBUG = '1'
    $godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
    $ga = @('--path', "$root\game", '--resolution', '1280x720')
    if ($Fps -gt 0) { $ga += @('--fixed-fps', "$Fps") }
    $ga += @('--', "--seed=$Seed", "--autopilot=$Seconds")
    . "$root\tools\godot_bg.ps1"   # off-screen, muted, no focus taken
    [void](Start-GodotBackground -Godot $godot -Arguments $ga -Log "$out\$Name.log" -TimeoutSec ($Seconds + 90))
} finally {
    Remove-Item Env:TOT_RIG_DEBUG -ErrorAction SilentlyContinue
    Exit-GodotLock $lock
}
python "$PSScriptRoot\head_smooth.py" "$out\$Name.log" --plot "$out\$Name.png" --label $Name | Tee-Object -FilePath "$out\$Name.txt"
