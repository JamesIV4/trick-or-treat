param(
    [string]$Name = 'ship',                       # logs: <Name>_cine.log, <Name>_showcase_<id>.log, <Name>_save.log
    [string[]]$Runs = @('cine', 'showcase', 'save'),
    [string[]]$CineArgs = @(),                    # extra game args for the cine run (e.g. --cine=town)
    [string[]]$ShowcaseIds = @('dragon_kid', 'monster_bat'),
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# Ship of the world rework: the headless checks of the paths no sweep covers, under the shared Godot lock.
#   cine:     the trailer reel (--cine=reel) with --cine-check: one "CINE check" line per shot (camera under the
#             ground / inside something / kid hidden, and the still), plus each shot's own finder prints.
#   showcase: --showcase=<id> for a kid and a monster (the stage finder on the home street; no recording).
#   save:     --save-version-test (an older save refused in process, nothing written to user://saves).
# No windows, no --import, no recordings.
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
. "$root\tools\godot_lock.ps1"
$lock = Enter-GodotLock
try {
    if ($Runs -contains 'cine') {
        $log = Join-Path $PSScriptRoot "$Name`_cine.log"
        $t0 = Get-Date
        & $Godot --headless --path "$root\game" --fixed-fps 30 -- --cine=reel --cine-check --cine-clean --no-lightning @CineArgs *> $log
        Write-Output ("cine: exit {0} in {1:N0} s" -f $LASTEXITCODE, ((Get-Date) - $t0).TotalSeconds)
    }
    if ($Runs -contains 'showcase') {
        foreach ($id in $ShowcaseIds) {
            $log = Join-Path $PSScriptRoot "$Name`_showcase_$id.log"
            $out = Join-Path $env:TEMP "tot_showcase_$id"
            New-Item -ItemType Directory -Force $out | Out-Null
            $t0 = Get-Date
            & $Godot --headless --path "$root\game" --fixed-fps 60 -- "--showcase=$id" "--showcase-out=$out" *> $log
            Write-Output ("showcase {0}: exit {1} in {2:N0} s" -f $id, $LASTEXITCODE, ((Get-Date) - $t0).TotalSeconds)
        }
    }
    if ($Runs -contains 'save') {
        $log = Join-Path $PSScriptRoot "$Name`_save.log"
        & $Godot --headless --path "$root\game" --quit-after 1200 -- --seed=101 --save-version-test *> $log
        Write-Output ("save test: exit {0}" -f $LASTEXITCODE)
    }
} finally {
    Exit-GodotLock $lock
}
