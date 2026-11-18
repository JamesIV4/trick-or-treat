param([string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe')
# Ship: the one in-game capture round, all under one hold of the shared Godot lock. Every window off-screen, muted,
# no focus (tools\godot_bg.ps1), --no-lens. Writes art\team\world\final\<name>.png (+ .log).
#   hood overview (world 101 home, Acorn Court), town overview (world 101's large cross town),
#   two cine stills: the trailer's house-cleared shot (lights) and the town shot (both retargeted at ship).
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"
$lock = Enter-GodotLock
try {
    $shots = @(
        @{ n = 'ship_hood_overview'; a = @('--seed=101', '--capture-view=overview') },
        @{ n = 'ship_town_overview'; a = @('--seed=101', '--capture-view=atover:279.3,760.1', '--overview-dist=70') }
    )
    foreach ($s in $shots) {
        $png = Join-Path $PSScriptRoot "$($s.n).png"
        $gameArgs = $s.a + @("--capture=$png", '--capture-frames=240', '--no-lens', '--no-lightning')
        $code = Start-GodotBackground -Godot $Godot -Arguments (@('--path', "$root\game", '--') + $gameArgs) -Log (Join-Path $PSScriptRoot "$($s.n).log") -TimeoutSec 300
        Write-Output ("{0}: exit {1}, png {2}" -f $s.n, $code, (Test-Path $png))
    }
    $cine = Join-Path $PSScriptRoot 'cine'
    New-Item -ItemType Directory -Force $cine | Out-Null
    foreach ($shot in 'lights', 'town') {
        $code = Start-GodotBackground -Godot $Godot -Arguments @('--path', "$root\game", '--', "--cine=$shot", "--cine-out=$cine", '--cine-clean', '--cine-check', '--no-lens', '--no-lightning') -Log (Join-Path $PSScriptRoot "ship_cine_$shot.log") -TimeoutSec 300
        Write-Output ("cine {0}: exit {1}" -f $shot, $code)
    }
    Get-ChildItem $cine -Filter *.png | ForEach-Object { Write-Output ("  " + $_.FullName) }
} finally {
    Exit-GodotLock $lock
}
