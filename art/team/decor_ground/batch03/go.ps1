param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string[]]$Ids = @(),
    [switch]$Preview
)
# Build with a batch03 script (headless Blender), print RESULT lines, gate the built ids, and
# optionally run the in-engine preview for each.
#   powershell -NoProfile -File art\team\decor_ground\batch03\go.ps1 -Script build_mums.py -Ids mums_orange -Preview
$Ids = @($Ids | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$root = 'S:\Repos\trick-or-treat'
Set-Location $root
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
$py = "art/team/decor_ground/batch03/$Script"
$a = @('--background', '--factory-startup', '--python', $py)
if ($Ids.Count -gt 0) { $a += '--'; $a += $Ids }
$log = & $blender @a 2>&1 | Out-String
$log -split "`n" | Where-Object { $_ -match 'RESULT|Error|WARN|Traceback|File "|line \d+' } | ForEach-Object { $_.TrimEnd() }
$built = @([regex]::Matches($log, 'RESULT (\S+)') | ForEach-Object { $_.Groups[1].Value })
if ($built.Count -eq 0) { Write-Output 'NOTHING BUILT'; exit 1 }
python art/tools/gates.py @built
if ($Preview) {
    foreach ($id in $built) { powershell -NoProfile -File tools\asset_preview.ps1 -Id $id }
}
