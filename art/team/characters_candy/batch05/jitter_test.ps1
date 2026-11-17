# Runs the on-screen jitter test (the kid walks then runs across the gameplay camera's view) and
# measures it.   jitter_test.ps1 -Name rig [-Seconds 8] [-V1] [-Fps 0] [-MaxFps 0] [-Dir 0] [-NoBuild]
#   -NoSmooth  turn off drawing between physics ticks (the old behaviour), for comparison
#   -V1      use the old animation (--anim-v1) for comparison
#   -Fps N   run with --fixed-fps N (every frame is exactly 1/N s of game time)
#   -MaxFps N  cap the real frame rate with --max-fps N (0: the game's own behaviour)
# Output: art\review\showcase\wip_p\jitter\<Name>.log / .txt / .png
param([string]$Name = 'rig', [int]$Seconds = 8, [switch]$V1, [int]$Fps = 0, [int]$MaxFps = 0, [float]$Dir = 0, [switch]$NoBuild, [switch]$NoSmooth, [int]$Seed = 101)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = "$root\art\review\showcase\wip_p\jitter"
New-Item -ItemType Directory -Force $out | Out-Null
if (-not $NoBuild) {
    $r = & "$PSScriptRoot\build_native.ps1" -Log 'build_lead.log'
    $r | Select-Object -Last 12
    if (-not ($r -match 'BUILD OK')) { Write-Output 'jitter_test: build failed'; exit 1 }
}
. "$root\tools\godot_lock.ps1"
$lock = Enter-GodotLock
try {
    $env:TOT_JITTER_TEST = "$Seconds"
    $env:TOT_JITTER_DIR = "$Dir"
    if ($NoSmooth) { $env:TOT_NO_TICK_SMOOTH = '1' }
    $godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
    $ga = @('--path', "$root\game", '--resolution', '1920x1080')
    if ($Fps -gt 0) { $ga += @('--fixed-fps', "$Fps") }
    if ($MaxFps -gt 0) { $ga += @('--max-fps', "$MaxFps") }
    $ga += @('--', "--seed=$Seed")
    if ($V1) { $ga += '--anim-v1' }
    . "$root\tools\godot_bg.ps1"   # off-screen, muted, no focus taken
    [void](Start-GodotBackground -Godot $godot -Arguments $ga -Log "$out\$Name.log" -TimeoutSec ($Seconds + 60))
} finally {
    Remove-Item Env:TOT_JITTER_TEST, Env:TOT_JITTER_DIR, Env:TOT_NO_TICK_SMOOTH -ErrorAction SilentlyContinue
    Exit-GodotLock $lock
}
python "$PSScriptRoot\jitter.py" "$out\$Name.log" --plot "$out\$Name.png" --label $Name | Tee-Object -FilePath "$out\$Name.txt"
