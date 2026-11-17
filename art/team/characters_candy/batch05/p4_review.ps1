# P4 review run for one or more costumes (detached-friendly), like p3_review.ps1 but with its own log so
# two Lane P workers do not clobber each other's review.log:
#   p4_review.ps1 -Ids kid_ghost [-Log ghost_review.log] [-NoBuild] [-Fidget 4]
# build, record into wip_p, beat sheets, every beat's strip. -Fidget k also records the first id with
# TOT_RIG_FIDGET=k into wip_p\fid<k> and cuts the idle strips there.
param([string[]]$Ids = @('kid_ghost'), [string]$Log = 'ghost_review.log', [switch]$NoBuild, [int]$Fidget = -1)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
Set-Location $root
$wip = "$root\art\review\showcase\wip_p"
$log = "$wip\$Log"
"review start $(Get-Date -Format s)" | Out-File -Encoding utf8 $log
if (-not $NoBuild) { & "$PSScriptRoot\build_native.ps1" *>&1 | Out-File -Append -Encoding utf8 $log }
& "$root\tools\showcase.ps1" -Ids $Ids -Out art\review\showcase\wip_p *>&1 | Out-File -Append -Encoding utf8 $log
python tools\showcase_sheets.py art\review\showcase\wip_p @Ids *>&1 | Out-File -Append -Encoding utf8 $log
& "$PSScriptRoot\all_strips.ps1" -Ids $Ids *>&1 | Out-File -Append -Encoding utf8 $log
if ($Fidget -ge 0) {
    $env:TOT_RIG_FIDGET = "$Fidget"
    & "$root\tools\showcase.ps1" -Ids $Ids[0] -Out "art\review\showcase\wip_p\fid$Fidget" *>&1 | Out-File -Append -Encoding utf8 $log
    Remove-Item Env:TOT_RIG_FIDGET -ErrorAction SilentlyContinue
    & "$PSScriptRoot\p3_strip.ps1" -Id $Ids[0] -Dir "art\review\showcase\wip_p\fid$Fidget" -From 836 -To 898 -Step 2 -Cols 16 -Width 118 -Name fid_stop *>&1 | Out-File -Append -Encoding utf8 $log
    & "$PSScriptRoot\p3_strip.ps1" -Id $Ids[0] -Dir "art\review\showcase\wip_p\fid$Fidget" -From 1940 -To 2029 -Step 3 -Cols 15 -Width 126 -Name fid_idle *>&1 | Out-File -Append -Encoding utf8 $log
}
"review done $(Get-Date -Format s)" | Out-File -Append -Encoding utf8 $log
