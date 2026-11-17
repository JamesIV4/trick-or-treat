# Lane P real-play checks with the existing tools (no build; build first):
#   head traces with candy (witch, skeleton, pumpkin + the dragon for today's reference),
#   on-screen judder (kid_ghost hooded, kid_witch soft parts; uncapped and 60 fps; dragon reference),
#   the legacy --anim-v1 showcase of all six v2 models, and 45 s of autopilot per costume (frame rate).
# Everything goes under the Godot lock and through tools\godot_bg.ps1. Run detached; log real_checks.log.
param([string[]]$Parts = @('head','jitter','v1','auto'), [string]$WaitFor = '')
$Parts = @($Parts | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
if ($WaitFor) { while (-not (Select-String -Path $WaitFor -Pattern 'real_batch done' -Quiet)) { Start-Sleep -Seconds 10 } }
$six = @('dragon_kid','kid_ghost','kid_cat','kid_witch','kid_skeleton','kid_pumpkin')
if ($Parts -contains 'head') {
    foreach ($p in @(@('witch_head','kid_witch'), @('skeleton_head','kid_skeleton'), @('pumpkin_head','kid_pumpkin'), @('dragon_head_now','dragon_kid'))) {
        "== head $($p[0])"
        & "$PSScriptRoot\head_showcase.ps1" -Name $p[0] -Id $p[1] -Candy 4 -NoBuild
    }
}
if ($Parts -contains 'jitter') {
    foreach ($c in 'kid_ghost','kid_witch','dragon_kid') {
        $env:TOT_RIG_COSTUME = $c
        "== jitter $c uncapped"; & "$PSScriptRoot\jitter_test.ps1" -Name "${c}_free" -NoBuild | Select-Object -First 12
        "== jitter $c 60 fps"; & "$PSScriptRoot\jitter_test.ps1" -Name "${c}_60" -Fps 60 -NoBuild | Select-Object -First 12
        Remove-Item Env:TOT_RIG_COSTUME -ErrorAction SilentlyContinue
    }
}
if ($Parts -contains 'v1') {
    "== anim-v1 showcase, all six"
    & "$PSScriptRoot\showcase_v1.ps1" -Ids ($six -join ',') -NoBuild -Out "$root\art\review\showcase\wip_p\v1check2"
}
if ($Parts -contains 'auto') {
    . "$root\tools\godot_lock.ps1"
    . "$root\tools\godot_bg.ps1"
    $godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
    $out = "$root\art\review\showcase\wip_p\real"
    foreach ($c in $six) {
        "== autopilot $c"
        $lock = Enter-GodotLock
        try {
            $env:TOT_RIG_COSTUME = $c
            [void](Start-GodotBackground -Godot $godot -Log "$out\auto_$c.log" -TimeoutSec 150 -Arguments @('--path', "$root\game", '--windowed', '--resolution', '1600x900', '--', '--seed=101', '--autopilot=45'))
        } finally { Remove-Item Env:TOT_RIG_COSTUME -ErrorAction SilentlyContinue; Exit-GodotLock $lock }
        Select-String -Path "$out\auto_$c.log" -Pattern 'RIGREVIEW|BOT t=|BOT_DONE|SCRIPT ERROR|ERROR: .*(kid|rig|Kid)' | ForEach-Object { $_.Line }
    }
}
'real_checks done'
