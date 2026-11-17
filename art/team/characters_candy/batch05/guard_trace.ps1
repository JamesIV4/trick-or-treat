# Final-fix review (2026-11-16): records the showcase with the rig trace on (TOT_RIG_DEBUG) for one or more
# costumes and measures, per clip frame, how far the head's parts and the whole kid are above the road (roll, ground
# pound, hurt) and how the ghost's sheet swings on the stop (guard_trace.py).
#   guard_trace.ps1 -Name base -Ids kid_ghost,kid_witch [-NoBuild]
# Output: art\review\showcase\wip_p\guard\<Name>\ (clips, logs) and <Name>.txt beside it. Run it detached for more
# than two ids (about a minute per id). Pass the ids from inside PowerShell (an array), not through Start-Process.
param([string]$Name = 'base', [string[]]$Ids = @('kid_ghost'), [switch]$NoBuild)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = "$root\art\review\showcase\wip_p\guard"
$ids = @($Ids | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
New-Item -ItemType Directory -Force "$out\$Name" | Out-Null
if (-not $NoBuild) {
    $r = & "$PSScriptRoot\build_native.ps1" -Log 'build_p.log'
    if (-not ($r -match 'BUILD OK')) { $r | Select-Object -Last 12; Write-Output 'guard_trace: build failed'; exit 1 }
}
$env:TOT_RIG_DEBUG = '1'
try {
    & "$root\tools\showcase.ps1" -Ids $ids -Out "$out\$Name" -NoBuild *> "$out\$Name\run.log"
} finally {
    Remove-Item Env:TOT_RIG_DEBUG -ErrorAction SilentlyContinue
}
Get-Content "$out\$Name\run.log" -Tail 3
python "$PSScriptRoot\guard_trace.py" "$out\$Name" @ids | Tee-Object -FilePath "$out\$Name.txt"
