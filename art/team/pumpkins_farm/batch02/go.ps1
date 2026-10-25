param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string[]]$Ids = @(),
    [switch]$NoPreview
)
# Build assets with a batch02 script, run the gates and the in-engine preview for each id.
#   powershell -NoProfile -File art\team\pumpkins_farm\batch02\go.ps1 -Script build_pumpkins.py -Ids pumpkin_round
$Ids = @($Ids | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$root = 'S:\Repos\trick-or-treat'
Set-Location $root
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
$py = "art/team/pumpkins_farm/batch02/$Script"
$args2 = @('--background', '--factory-startup', '--python', $py)
if ($Ids.Count -gt 0) { $args2 += '--'; $args2 += $Ids }
$log = & $blender @args2 2>&1 | Out-String
$log -split "`n" | Where-Object { $_ -match 'RESULT|Error|WARN|Traceback|File "|line \d+' } | ForEach-Object { $_.TrimEnd() }
$built = @([regex]::Matches($log, 'RESULT (\S+)') | ForEach-Object { $_.Groups[1].Value })
if ($built.Count -eq 0) { Write-Output 'NOTHING BUILT'; exit 1 }
python art/tools/gates.py @built
if (-not $NoPreview) {
    foreach ($id in $built) { powershell -NoProfile -File tools\asset_preview.ps1 -Id $id | Out-Null }
}
