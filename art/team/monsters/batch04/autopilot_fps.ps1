# Frame rate in real play: `--seed=101 --autopilot=<s>` at 1600x900 (off-screen, muted, under the Godot lock), the
# same run the earlier monster rounds quoted (M1 181-189 fps, M2 169-196, M3 163-198). Prints the bot's fps lines
# and their min / median / max.   autopilot_fps.ps1 [-Seconds 45] [-Name ap]
param([int]$Seconds = 45, [string]$Name = 'ap')
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = "$root\art\review\showcase\wip_m\real"
New-Item -ItemType Directory -Force $out | Out-Null
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"
$godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
$log = "$out\$Name.log"
$lock = Enter-GodotLock
try {
    $code = Start-GodotBackground -Godot $godot -Log $log -TimeoutSec ($Seconds + 180) -Arguments @('--path', "$root\game", '--resolution', '1600x900', '--', '--seed=101', "--autopilot=$Seconds")
} finally { Exit-GodotLock $lock }
$fps = @(Select-String -Path $log -Pattern 'fps=([\d.]+)' | ForEach-Object { [double]$_.Matches[-1].Groups[1].Value } | Where-Object { $_ -gt 0 })
Select-String -Path $log -Pattern 'BOT_DONE|SCRIPT ERROR|Crash' | Select-Object -First 4 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(200, $_.Line.Length)) }
if ($fps.Count) {
    $s = $fps | Sort-Object
    "exit $code; fps samples $($fps.Count): min $($s[0]) median $($s[[int]($s.Count / 2)]) max $($s[-1]); all: $($fps -join ' ')"
} else { "exit $code; no fps lines" }
