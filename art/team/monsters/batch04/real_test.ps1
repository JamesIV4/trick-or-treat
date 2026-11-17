# Real-play monster review: runs the game with the review switch TOT_MONSTER_TEST (festival_game.cpp) and
# records it with Godot's movie writer, off-screen and muted (tools\godot_bg.ps1), under the Godot lock.
#   real_test.ps1 -Name pk_street -Type pumpkin [-Count 1] [-Seconds 15] [-Scene street|kerb|slope|steps|idle|far]
#                 [-Hit windup:0.5|attack:0.2] [-Lure high] [-Walk 0.3] [-Dist 9] [-Cam 6,0.9,40] [-At 0.3] [-Lot 0]
#                 [-NoMovie] [-NoSmooth] [-Leap] [-MaxFps N] [-Seed 101] [-Res 1920x1080]
# -Leap is a no-op since 2026-11-16: the game itself now keeps the HOP / POUNCE leap velocity (see CHECKPOINT.md).
# Writes art\review\showcase\wip_m\real\<Name>.log (with the MTRACE lines) and, unless -NoMovie, <Name>.mp4
# (60 fps, from about the moment the test starts; the picture's top-left line says the test time, the engine
# frame and the first monster's state). -NoMovie runs uncapped (vsync off) for the on-screen evenness trace.
param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Type, [int]$Count = 1, [float]$Seconds = 15,
    [string]$Scene = 'street', [string]$Hit = '', [string]$Lure = '', [float]$Walk = 0, [float]$Dist = 0, [string]$Cam = '',
    [float]$At = -1, [int]$Lot = 0, [switch]$NoMovie, [switch]$NoSmooth, [switch]$Leap, [int]$MaxFps = 0, [int]$Seed = 101, [string]$Res = '1920x1080')
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = "$root\art\review\showcase\wip_m\real"
New-Item -ItemType Directory -Force $out | Out-Null
$godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
$log = "$out\$Name.log"; $avi = "$out\$Name.avi"; $mp4 = "$out\$Name.mp4"
Remove-Item $avi, $mp4 -ErrorAction SilentlyContinue
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"
$vars = @{ TOT_MONSTER_TEST = "$Type,$Count,$Seconds"; TOT_MONSTER_SCENE = $Scene; TOT_MONSTER_HIT = $Hit; TOT_MONSTER_LURE = $Lure;
    TOT_MONSTER_WALK = $(if ($Walk -gt 0) { "$Walk" } else { '' }); TOT_MONSTER_DIST = $(if ($Dist -gt 0) { "$Dist" } else { '' });
    TOT_MONSTER_CAM = $Cam; TOT_MONSTER_AT = $(if ($At -ge 0) { "$At" } else { '' }); TOT_MONSTER_LOT = "$Lot";
    TOT_NO_TICK_SMOOTH = $(if ($NoSmooth) { '1' } else { '' }); TOT_MONSTER_LEAP = $(if ($Leap) { '1' } else { '' }) }
$lock = Enter-GodotLock
try {
    foreach ($k in $vars.Keys) { if ($vars[$k] -ne '') { Set-Item "Env:$k" $vars[$k] } else { Remove-Item "Env:$k" -ErrorAction SilentlyContinue } }
    $ga = @('--path', "$root\game")
    if (-not $NoMovie) { $ga += @('--fixed-fps', '60', '--write-movie', $avi) }
    if ($MaxFps -gt 0) { $ga += @('--max-fps', "$MaxFps") }
    if ($Res) { $ga += @('--resolution', $Res) }
    $ga += @('--', "--seed=$Seed")
    $code = Start-GodotBackground -Godot $godot -Arguments $ga -Log $log -TimeoutSec ([int]$Seconds + 120)
} finally {
    foreach ($k in $vars.Keys) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }
    Exit-GodotLock $lock
}
$start = Select-String -Path $log -Pattern 'MTEST start .* frame=(\d+)' | Select-Object -First 1
Write-Output ("{0}: exit {1}; {2}" -f $Name, $code, $(if ($start) { $start.Line } else { 'no MTEST start line' }))
Select-String -Path $log -Pattern 'MTEST (spawn|hit|slope|steps)|SCRIPT ERROR|ERROR' | Select-Object -First 12 | ForEach-Object { $_.Line }
if (-not $NoMovie -and (Test-Path $avi)) {
    $f0 = if ($start) { [Math]::Max(0, [int]$start.Matches[0].Groups[1].Value - 2) } else { 0 }
    & ffmpeg -hide_banner -loglevel error -y -i $avi -an -vf "trim=start_frame=${f0},setpts=PTS-STARTPTS,scale=in_range=full:out_range=limited,format=yuv420p" -c:v libx264 -preset medium -crf 18 -r 60 $mp4
    if (Test-Path $mp4) { Remove-Item $avi -ErrorAction SilentlyContinue; Write-Output "movie: $mp4 (from engine frame $f0)" }
}
