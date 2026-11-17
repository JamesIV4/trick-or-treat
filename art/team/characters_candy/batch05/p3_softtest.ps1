# Engine test of the soft_* / bob_* parts (see p3_softtest.py): backs up the game's dragon_kid model,
# exports the test variant over it, records it into wip_p\soft (with a hem ripple forced on), then puts
# the real model back byte for byte. Log: wip_p\soft\softtest.log. If this is interrupted, restore by
# hand from wip_p\soft\backup\ (or rebuild: build_costumes.py -- dragon_kid, then publish_catalog.py).
param([switch]$NoBuild)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
Set-Location $root
$out = "$root\art\review\showcase\wip_p\soft"
$models = "$root\game\assets\models\characters"
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
New-Item -ItemType Directory -Force "$out\backup" | Out-Null
$log = "$out\softtest.log"
"softtest start $(Get-Date -Format s)" | Out-File -Encoding utf8 $log
Copy-Item "$models\dragon_kid.gltf", "$models\dragon_kid.bin" "$out\backup" -Force
$before = (Get-FileHash "$models\dragon_kid.gltf").Hash + (Get-FileHash "$models\dragon_kid.bin").Hash
try {
    if (-not $NoBuild) { & "$PSScriptRoot\build_native.ps1" *>&1 | Out-File -Append -Encoding utf8 $log }
    & $blender --background --factory-startup --python "$PSScriptRoot\p3_softtest.py" *>&1 | Select-String 'SOFTTEST|Error|Traceback|assert' | Out-File -Append -Encoding utf8 $log
    $env:TOT_RIG_HEM = '0.012'
    & "$root\tools\showcase.ps1" -Ids dragon_kid -Out art\review\showcase\wip_p\soft *>&1 | Out-File -Append -Encoding utf8 $log
    Remove-Item Env:TOT_RIG_HEM -ErrorAction SilentlyContinue
    python tools\showcase_sheets.py art\review\showcase\wip_p\soft dragon_kid *>&1 | Out-File -Append -Encoding utf8 $log
} finally {
    Remove-Item Env:TOT_RIG_HEM -ErrorAction SilentlyContinue
    Copy-Item "$out\backup\dragon_kid.gltf", "$out\backup\dragon_kid.bin" $models -Force
    $after = (Get-FileHash "$models\dragon_kid.gltf").Hash + (Get-FileHash "$models\dragon_kid.bin").Hash
    "restored: $($before -eq $after)" | Out-File -Append -Encoding utf8 $log
}
foreach ($b in @(@('walk', 510, 569, 3), @('jump', 918, 992, 3), @('glide', 1070, 1160, 4), @('spin', 1232, 1290, 3), @('hurt', 1736, 1790, 3), @('idle', 1940, 2029, 4))) {
    & "$PSScriptRoot\p3_strip.ps1" -Dir art\review\showcase\wip_p\soft -From $b[1] -To $b[2] -Step $b[3] -Cols 10 -Width 190 -Crop 'iw*0.6:ih*0.6:iw*0.2:ih*0.2' -Name "soft_$($b[0])" *>&1 | Out-File -Append -Encoding utf8 $log
}
"softtest done $(Get-Date -Format s)" | Out-File -Append -Encoding utf8 $log
