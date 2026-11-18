param(
    [Parameter(Mandatory)][int]$Seed,                 # world seed
    [Parameter(Mandatory)][string[]]$Shots,           # "name|view|extra args (comma separated)", e.g. "4c_grid|atover:120,-340|--overview-dist=60"
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# Stage 4c: the one in-game capture round. Each shot is one off-screen, muted, no-focus game window
# (tools\godot_bg.ps1) under the shared Godot lock, writing art\team\world\stage0\<name>.png (--no-lens).
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"
$lock = Enter-GodotLock
try {
    foreach ($s in $Shots) {
        $p = $s.Split('|')
        $name = $p[0]; $view = $p[1]
        $extra = @(); if ($p.Length -gt 2 -and $p[2]) { $extra = $p[2].Split(',') }
        $png = Join-Path $PSScriptRoot "$name.png"
        $log = Join-Path $PSScriptRoot "$name.log"
        $gameArgs = @("--seed=$Seed", "--capture-view=$view", "--capture=$png", '--capture-frames=240', '--no-lens', '--no-lightning') + $extra
        $code = Start-GodotBackground -Godot $Godot -Arguments (@('--path', "$root\game", '--') + $gameArgs) -Log $log -TimeoutSec 300
        Write-Output ("{0}: exit {1}, png {2}" -f $name, $code, (Test-Path $png))
    }
} finally {
    Exit-GodotLock $lock
}
