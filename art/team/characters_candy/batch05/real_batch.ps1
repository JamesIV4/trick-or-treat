# Lane P real-play batch: for each costume record the ledge hang + climb, a fence vault, trampoline
# bounces, the ride-on car (mount, drive, hop, stop, dismount) and the misc scene (pause, fall-out
# respawn), plus the in-play costume change once, and cut 400 px frame strips of each
# (art\review\showcase\wip_p\real\<scene>_<id>.mp4 / _strip.png). Run detached; log real_batch.log.
#   real_batch.ps1 [-Ids kid_witch,...] [-Scenes hang,vault,tramp,car,misc,swap] [-Tag ''] [-V1]
param([string[]]$Ids = @('dragon_kid','kid_ghost','kid_cat','kid_witch','kid_skeleton','kid_pumpkin'),
      [string[]]$Scenes = @('hang','vault','tramp','car','misc','swap'), [string]$Tag = '', [switch]$V1)
$Ids = @($Ids | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Scenes = @($Scenes | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$rp = "$PSScriptRoot\real_play.ps1"
$d = (Resolve-Path "$PSScriptRoot\..\..\..\review\showcase\wip_p").Path + '\real'
New-Item -ItemType Directory -Force $d | Out-Null
function Strip($name, $from, $vf) {
    & ffmpeg -hide_banner -loglevel error -y -sseof $from -i "$d\$name.mp4" -vf $vf -frames:v 1 "$d\${name}_strip.png"
}
$crop = 'crop=iw*0.46:ih*0.86:iw*0.27:ih*0.07,scale=400:-1'
foreach ($id in $Ids) {
    $s = "$Tag$id"
    if ($Scenes -contains 'hang') {
        & $rp -Name "hang_$s" -Costume $id -Scene cam -Secs 0 -Cam '3.0,1.35,145,38,1,0.85' -GameArgs '--ledge-test=0','--ledge-side' -V1:$V1
        Strip "hang_$s" -2.4 "fps=12,$crop,tile=8x4"
    }
    if ($Scenes -contains 'hangside') {
        # Just above the lip, out from the wall and to the side: the hands on the lip, the head over it.
        & $rp -Name "hangside_$s" -Costume $id -Scene cam -Secs 0 -Cam '2.0,1.25,140,40,1,1.0' -GameArgs '--ledge-test=0','--ledge-side' -V1:$V1
        Strip "hangside_$s" -2.4 "fps=12,$crop,tile=8x4"
    }
    if ($Scenes -contains 'hangtop') {
        # From above the roof in front of the kid: the head over the lip and the wand hand on it.
        & $rp -Name "hangtop_$s" -Costume $id -Scene cam -Secs 0 -Cam '2.0,2.4,35,45,1,1.0' -GameArgs '--ledge-test=0','--ledge-side' -V1:$V1
        Strip "hangtop_$s" -2.4 "fps=12,$crop,tile=8x4"
    }
    if ($Scenes -contains 'vault') {
        & $rp -Name "vault_$s" -Costume $id -Scene cam -Secs 0 -Cam '4.0,1.3,100,40,1,0.6' -GameArgs '--mantle-test=1' -V1:$V1
        Strip "vault_$s" -4.5 "fps=6,$crop,tile=9x3"
    }
    if ($Scenes -contains 'tramp') {
        & $rp -Name "tramp_$s" -Costume $id -Scene cam -Secs 7 -Seed 69 -Cam '4.0,0.8,60,40,1,0.5' -GameArgs '--capture-view=prop:trampoline_large','--drop-test' -V1:$V1
        Strip "tramp_$s" -5 "fps=6,$crop,tile=10x3"
    }
    if ($Scenes -contains 'car') {
        & $rp -Name "car_$s" -Costume $id -Scene car -Secs 12.5 -Cam '5.5,2.0,50,40,1,0.6' -GameArgs '--give-car' -V1:$V1
        Strip "car_$s" -12 "fps=4,$crop,tile=8x6"
    }
    if ($Scenes -contains 'misc') {
        & $rp -Name "misc_$s" -Costume $id -Scene misc -Secs 9 -Cam '3.6,1.0,60,40,1,0.55' -V1:$V1
        Strip "misc_$s" -8.5 "fps=6,$crop,tile=9x6"
    }
}
if ($Scenes -contains 'rolls') {
    # Final fixes 2: the roll's ground guard on uneven ground (a kerb, a slope down / up / across), trace on (TRUE= the
    # lowest point above the real ground under it). Low side view.
    foreach ($id in $Ids) {
        $env:TOT_RIG_DEBUG = '1'
        try { & $rp -Name "rolls_$Tag$id" -Costume $id -Scene rolls -Secs 11.5 -Cam '3.2,0.55,90,40,1,0.45' -V1:$V1 }
        finally { Remove-Item Env:TOT_RIG_DEBUG -ErrorAction SilentlyContinue }
        Strip "rolls_$Tag$id" -11 "fps=6,$crop,tile=11x6"
    }
}
if ($Scenes -contains 'swap') {
    & $rp -Name "swap_${Tag}all" -Scene swap -Secs 33 -Cam '3.8,1.1,40,40,1,0.55' -V1:$V1
    Strip "swap_${Tag}all" -32 "fps=3,$crop,tile=10x10"
}
'real_batch done'
