# House goals: the one in-game look, an off-screen windowed capture of the play view (the area panel, top right).
#   art\team\world\houses\hud_capture.ps1
param([int]$Seed = 101, [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe')
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"
$lock = Enter-GodotLock
try {
    $png = Join-Path $PSScriptRoot "hud_$Seed.png"
    $code = Start-GodotBackground -Godot $Godot -Arguments @('--path', "$root\game", '--', "--seed=$Seed", "--capture=$png", '--no-lightning') -Log "$PSScriptRoot\hud_capture.log" -TimeoutSec 300
    Write-Output ("capture exit {0}: {1}" -f $code, (Test-Path $png))
} finally {
    Exit-GodotLock $lock
}
