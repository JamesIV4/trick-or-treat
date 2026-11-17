# Records the showcase for one costume with the head trace on, optionally with candy landing in the
# bucket at a fixed rate through every beat, and measures how smoothly the head moves.
#   head_showcase.ps1 -Name walkrun [-Id dragon_kid] [-Candy 4] [-NoBuild]
# Output: art\review\showcase\wip_p\head\<Name>\ (clip, sheets) and <Name>.txt / <Name>.png beside it.
param([string]$Name = 'walkrun', [string]$Id = 'dragon_kid', [float]$Candy = 0, [switch]$NoBuild)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = "$root\art\review\showcase\wip_p\head"
New-Item -ItemType Directory -Force "$out\$Name" | Out-Null
if (-not $NoBuild) {
    $r = & "$PSScriptRoot\build_native.ps1" -Log 'build_lead.log'
    $r | Select-Object -Last 12
    if (-not ($r -match 'BUILD OK')) { Write-Output 'head_showcase: build failed'; exit 1 }
}
$env:TOT_RIG_DEBUG = '1'
if ($Candy -gt 0) { $env:TOT_RIG_CANDY = "$Candy" }
try {
    & "$root\tools\showcase.ps1" -Ids $Id -Out "$out\$Name" -NoBuild *> "$out\$Name\run.log"
} finally {
    Remove-Item Env:TOT_RIG_DEBUG, Env:TOT_RIG_CANDY -ErrorAction SilentlyContinue
}
Get-Content "$out\$Name\run.log" -Tail 2
python "$root\tools\showcase_sheets.py" "$out\$Name" $Id | Select-Object -Last 1
python "$PSScriptRoot\head_smooth.py" "$out\$Name\$Id.log" --plot "$out\$Name.png" --label $Name --beats "$out\$Name\$Id.beats.json" | Tee-Object -FilePath "$out\$Name.txt"
