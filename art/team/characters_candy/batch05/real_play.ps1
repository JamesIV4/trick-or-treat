# Lane P real-play review: records the kid in a normal game start with the movie writer (60 fps), in any
# costume, with a review camera, for the states the showcase recorder does not show. Uses the review
# switches in native/src/play/dragon_controller.cpp (TOT_RIG_COSTUME, TOT_RIG_REVIEW, TOT_RIG_CAM); the
# game's own review modes (--ledge-test, --mantle-test, --drop-test, --give-car) are passed in -GameArgs.
#   real_play.ps1 -Name hang_kid_witch -Costume kid_witch -Scene cam -Secs 0 -Cam '2.6,0.6,150,45,1,0.6' -GameArgs '--ledge-test=0','--ledge-side'
# Output: art\review\showcase\wip_p\real\<Name>.mp4 and <Name>.log. The window opens off-screen, muted
# (tools\godot_bg.ps1). Build the DLL first (build_native.ps1); this does not build.
param(
    [Parameter(Mandatory)][string]$Name,
    [string]$Costume = '',
    [string]$Scene = 'cam',
    [float]$Secs = 10,
    [string]$Cam = '',
    [string[]]$GameArgs = @(),
    [int]$Seed = 101,
    [switch]$V1,
    [switch]$KeepAvi
)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = "$root\art\review\showcase\wip_p\real"
New-Item -ItemType Directory -Force $out | Out-Null
$godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
$avi = "$out\$Name.avi"; $mp4 = "$out\$Name.mp4"; $log = "$out\$Name.log"
Remove-Item $avi, $mp4 -ErrorAction SilentlyContinue
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"
$lock = Enter-GodotLock
try {
    if ($Costume) { $env:TOT_RIG_COSTUME = $Costume }
    $env:TOT_RIG_REVIEW = "$Scene,$Secs"
    if ($Cam) { $env:TOT_RIG_CAM = $Cam }
    $ga = @('--path', "$root\game", '--windowed', '--fixed-fps', '60', '--write-movie', $avi, '--', "--seed=$Seed") + $GameArgs
    if ($V1) { $ga += '--anim-v1' }
    $code = Start-GodotBackground -Godot $godot -Arguments $ga -Log $log -TimeoutSec 240
} finally {
    Remove-Item Env:TOT_RIG_COSTUME, Env:TOT_RIG_REVIEW, Env:TOT_RIG_CAM -ErrorAction SilentlyContinue
    Exit-GodotLock $lock
}
if (Test-Path $avi) {
    & ffmpeg -hide_banner -loglevel error -y -i $avi -an -c:v libx264 -preset fast -crf 18 -pix_fmt yuv420p $mp4
    if (-not $KeepAvi -and (Test-Path $mp4)) { Remove-Item $avi }
}
$errs = @(Select-String -Path $log -Pattern 'SCRIPT ERROR|ERROR:|Crash|crash' | Select-Object -First 6 | ForEach-Object { $_.Line })
$info = @(Select-String -Path $log -Pattern 'RIGREVIEW|RIGSWAP|LEDGE_|MANTLE|DROP_TEST|PROP_VIEW|BOT_DONE|BOT t=' | ForEach-Object { $_.Line })
"$Name : exit $code, mp4 $(Test-Path $mp4)"
$info
if ($errs.Count) { 'errors:'; $errs }
