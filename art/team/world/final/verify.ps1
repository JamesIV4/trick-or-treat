param([string]$Name = 'ship_final')
# Ship: the whole headless verification on one build, in order (each step takes the shared Godot lock itself):
# sweep A (seeds 101-124) with the build, sweep B (125-150), both rerun -NoBuild for determinism, the house door test
# (seed 101, seed 103), and the cine / showcase / save checks. Logs next to each tool; a summary here.
$ErrorActionPreference = 'Continue'
$s0 = "$PSScriptRoot\..\stage0"
& "$s0\sweep.ps1" -Name "$Name`_a" -Count 24 -Seed 101 -Dump 2>&1 | Select-String 'build|SWEEP lines|exit|failed'
& "$s0\sweep.ps1" -Name "$Name`_b" -Count 26 -Seed 125 -Dump -NoBuild 2>&1 | Select-String 'SWEEP lines|exit'
& "$s0\sweep.ps1" -Name "$Name`_a_rerun" -Count 24 -Seed 101 -Dump -NoBuild 2>&1 | Select-String 'SWEEP lines|exit'
& "$s0\sweep.ps1" -Name "$Name`_b_rerun" -Count 26 -Seed 125 -Dump -NoBuild 2>&1 | Select-String 'SWEEP lines|exit'
& "$PSScriptRoot\..\houses\door_test.ps1" -Name "$Name`_door" -NoBuild 2>&1 | Select-String 'exit|summary|done|FAIL' | Select-Object -First 12
& "$PSScriptRoot\..\houses\door_test.ps1" -Name "$Name`_door_s103" -Seed 103 -NoBuild 2>&1 | Select-String 'exit|summary|done|FAIL' | Select-Object -First 12
& "$PSScriptRoot\headless_checks.ps1" -Name $Name -ShowcaseIds dragon_kid, monster_bat 2>&1
Write-Output 'VERIFY DONE'
