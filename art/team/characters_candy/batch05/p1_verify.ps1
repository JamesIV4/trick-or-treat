# Lane P chunk P1 verification: build, record all six costumes, sheets, the --anim-v1 check, a hang still.
param([string[]]$Ids = @('dragon_kid','kid_ghost','kid_cat','kid_witch','kid_skeleton','kid_pumpkin'), [switch]$SkipV1, [switch]$SkipHang)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = "$root\art\review\showcase\wip_p"
$godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
New-Item -ItemType Directory -Force $out | Out-Null
"== build ==";  & "$PSScriptRoot\build_native.ps1"
"== showcase =="; & "$root\tools\showcase.ps1" -Ids $Ids -Out $out
"== sheets =="; python "$root\tools\showcase_sheets.py" $out @Ids
if (-not $SkipV1) {
  "== anim-v1 check (dragon_kid) =="
  & "$PSScriptRoot\showcase_v1.ps1" -Ids dragon_kid -NoBuild -Out "$out\v1check"
  # frame-for-frame against the BEFORE recording (deterministic recorder): mean PSNR of 200 frames
  foreach ($pair in @(@("$out\v1check\dragon_kid.mp4", 'v1 vs BEFORE'), @("$out\dragon_kid.mp4", 'rig vs BEFORE'))) {
    $line = & ffmpeg -hide_banner -i "$root\art\review\showcase\before\dragon_kid.mp4" -i $pair[0] -lavfi "[0:v]select='not(mod(n\,10))'[a];[1:v]select='not(mod(n\,10))'[b];[a][b]psnr" -vsync 0 -f null - 2>&1 | Select-String 'PSNR'
    "$($pair[1]): $line"
  }
}
if (-not $SkipHang) {
  "== hang still =="
  . "$root\tools\godot_lock.ps1"
  . "$root\tools\godot_bg.ps1"
  $lock = Enter-GodotLock
  try {
    foreach ($lot in 0..5) {
      $png = "$out\hang_lot$lot.png"
      [void](Start-GodotBackground -Godot $godot -Log "$out\hang_lot$lot.log" -Arguments @('--path', "$root\game", '--resolution', '1600x900', '--', '--seed=101', "--ledge-test=$lot", '--ledge-side', "--capture=$png", '--capture-frames=100000'))
      $grab = Select-String -Path "$out\hang_lot$lot.log" -Pattern 'LEDGE_' | ForEach-Object { $_.Line }
      "lot $lot : $($grab -join ' | ')"
      if (Test-Path $png) { break }
    }
  } finally { Exit-GodotLock $lock }
}
"== autopilot (real gameplay, 45 s of bot play) =="
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"   # windows open off-screen, muted, without taking focus
$lock = Enter-GodotLock
try {
  [void](Start-GodotBackground -Godot $godot -Log "$out\autopilot.log" -Arguments @('--path', "$root\game", '--resolution', '1600x900', '--', '--seed=101', '--autopilot=45', "--capture=$out\autopilot.png", '--capture-frames=100000'))
  Select-String -Path "$out\autopilot.log" -Pattern 'BOT_DONE|ERROR|SCRIPT ERROR|Crash|crash' | Select-Object -First 8 | ForEach-Object { $_.Line }
} finally { Exit-GodotLock $lock }
"== done =="
