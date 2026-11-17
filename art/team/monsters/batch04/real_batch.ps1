# Runs several real_test.ps1 scenarios one after another (each under the Godot lock, off-screen, muted) and
# prints each one's state timeline (real_trace.py). Scenarios separated by '/', each as real_test.ps1 arguments:
#   real_batch.ps1 -Runs "-Name pk_street -Type pumpkin / -Name sp_leap -Type spider -Leap"
# Long batches: run detached with a log (Start-Process powershell ... -RedirectStandardOutput).
param([Parameter(Mandatory)][string]$Runs)
$list = $Runs -split '/' | ForEach-Object { $_.Trim() } | Where-Object { $_ }
$here = $PSScriptRoot
$root = (Resolve-Path "$here\..\..\..\..").Path
foreach ($r in $list) {
    $t = Get-Date
    $out = Invoke-Expression "& '$here\real_test.ps1' $r" # collect it all: never cut the run's output short
    $out | Where-Object { $_ -match '^\w+: exit|^movie:|MTEST (hit|slope|steps)' } | ForEach-Object { $_ }
    if ($r -match '-Name\s+(\S+)') {
        python "$here\real_trace.py" "$root\art\review\showcase\wip_m\real\$($Matches[1]).log"
    }
    "  ({0:n0} s)" -f ((Get-Date) - $t).TotalSeconds
}
