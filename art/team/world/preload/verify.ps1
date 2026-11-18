param(
    [int]$Seed = 101,
    [int]$Secs = 200,
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# Area preloading parity: the same streaming walk (--fly-test) with --no-preload and preloaded (up to 4 plans at
# once over a 1.1 km preload radius, so a shared-state race would show), both under ONE hold of the Godot lock so they run the same
# binary; every settlement attached is fingerprinted (--stream-verify). Then verify_compare.py.
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
. "$root\tools\godot_lock.ps1"
$lock = Enter-GodotLock
try {
    foreach ($run in @(@{ n = 'verify_sync'; x = @('--no-preload') }, @{ n = 'verify_pre'; x = @('--preload-tasks=4', '--preload-cache=12', '--preload-dist=1100') })) {
        $dump = Join-Path $PSScriptRoot "$($run.n)_dump.jsonl"
        if (Test-Path $dump) { Remove-Item $dump }
        $gameArgs = @("--seed=$Seed", "--fly-test=$Secs", '--stream-fps=60', '--no-lightning', "--stream-verify=$dump") + $run.x
        $t0 = Get-Date
        & $Godot --headless --path "$root\game" -- @gameArgs *> (Join-Path $PSScriptRoot "$($run.n).log")
        Write-Output ("{0}: godot exit {1} in {2:N1} s" -f $run.n, $LASTEXITCODE, ((Get-Date) - $t0).TotalSeconds)
    }
} finally {
    Exit-GodotLock $lock
}
& python "$PSScriptRoot\verify_compare.py" verify_sync verify_pre
