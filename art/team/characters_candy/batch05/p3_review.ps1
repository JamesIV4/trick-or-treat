# P3 review run (detached-friendly): build, record the ids normally into wip_p, then optionally one
# recording per idle fidget kind (TOT_RIG_FIDGET) into wip_p\fid<kind>, the ledge hang / climb strip,
# and the sole trace report. Everything is logged to wip_p\review.log.
#   p3_review.ps1 -Ids dragon_kid -Fidgets 0,2,3 -Hang -Trace
param([string[]]$Ids = @('dragon_kid'), [int[]]$Fidgets = @(), [switch]$Hang, [switch]$Trace, [switch]$NoBuild, [switch]$NoMain)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
Set-Location $root
$wip = "$root\art\review\showcase\wip_p"
$log = "$wip\review.log"
"review start $(Get-Date -Format s)" | Out-File -Encoding utf8 $log
if (-not $NoBuild) { & "$PSScriptRoot\build_native.ps1" *>&1 | Out-File -Append -Encoding utf8 $log }
if (-not $NoMain) {
    if ($Trace) { $env:TOT_RIG_DEBUG = '1' }
    & "$root\tools\showcase.ps1" -Ids $Ids -Out art\review\showcase\wip_p *>&1 | Out-File -Append -Encoding utf8 $log
    Remove-Item Env:TOT_RIG_DEBUG -ErrorAction SilentlyContinue
    python tools\showcase_sheets.py art\review\showcase\wip_p @Ids *>&1 | Out-File -Append -Encoding utf8 $log
    & "$PSScriptRoot\all_strips.ps1" -Ids $Ids *>&1 | Out-File -Append -Encoding utf8 $log
    if ($Trace) { python "$PSScriptRoot\p3_slide.py" "$wip\$($Ids[0]).log" *>&1 | Out-File -Append -Encoding utf8 $log }
}
foreach ($k in $Fidgets) {
    $env:TOT_RIG_FIDGET = "$k"
    & "$root\tools\showcase.ps1" -Ids $Ids[0] -Out "art\review\showcase\wip_p\fid$k" *>&1 | Out-File -Append -Encoding utf8 $log
    Remove-Item Env:TOT_RIG_FIDGET -ErrorAction SilentlyContinue
    # the idle moments seen from the front: the stop, the wait before the hit, the last beat
    & "$PSScriptRoot\p3_strip.ps1" -Id $Ids[0] -Dir "art\review\showcase\wip_p\fid$k" -From 836 -To 898 -Step 2 -Cols 16 -Width 118 -Name fid_stop *>&1 | Out-File -Append -Encoding utf8 $log
    & "$PSScriptRoot\p3_strip.ps1" -Id $Ids[0] -Dir "art\review\showcase\wip_p\fid$k" -From 1656 -To 1724 -Step 2 -Cols 18 -Width 105 -Name fid_wait *>&1 | Out-File -Append -Encoding utf8 $log
    & "$PSScriptRoot\p3_strip.ps1" -Id $Ids[0] -Dir "art\review\showcase\wip_p\fid$k" -From 1940 -To 2029 -Step 3 -Cols 15 -Width 126 -Name fid_idle *>&1 | Out-File -Append -Encoding utf8 $log
}
if ($Hang) { & "$PSScriptRoot\p3_hang.ps1" *>&1 | Out-File -Append -Encoding utf8 $log }
"review done $(Get-Date -Format s)" | Out-File -Append -Encoding utf8 $log
