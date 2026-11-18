param(
    [Parameter(Mandatory)][string]$Name,     # output: <Name>.log in this folder
    [string[]]$GameArgs = @(),               # game args (after --)
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# Area preloading: one headless game run under the shared Godot lock (no build; see walk.ps1 for that).
#   art\team\world\preload\run.ps1 -Name state -GameArgs --seed=101,--stream-state-test,--stream-fps=60
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
. "$root\tools\godot_lock.ps1"
$lock = Enter-GodotLock
try {
    $log = Join-Path $PSScriptRoot "$Name.log"
    $t0 = Get-Date
    & $Godot --headless --path "$root\game" -- @GameArgs *> $log
    Write-Output ("{0}: godot exit {1} in {2:N1} s" -f $Name, $LASTEXITCODE, ((Get-Date) - $t0).TotalSeconds)
} finally {
    Exit-GodotLock $lock
}
