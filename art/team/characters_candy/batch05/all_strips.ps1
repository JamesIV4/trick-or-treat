param([string[]]$Ids = @('dragon_kid'), [string]$After = 'art\review\showcase\wip_p', [string[]]$Beats = @())
$spec = [ordered]@{
  turntable = @(0, 359, 16); walk = @(40, 100, 16); run = @(60, 105, 16); stop_idle = @(0, 60, 16); jump = @(20, 95, 16)
  double_jump_glide = @(5, 235, 24); spin = @(2, 77, 16); roll = @(30, 75, 16); ground_pound = @(24, 114, 16)
  fire_cast = @(16, 91, 16); hurt = @(76, 166, 16); candy_celebrate = @(16, 119, 16); idle = @(0, 89, 8)
}
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
foreach ($id in $Ids) { foreach ($b in $spec.Keys) {
  if ($Beats.Count -gt 0 -and $Beats -notcontains $b) { continue }
  $s = $spec[$b]
  python "$PSScriptRoot\strips.py" $id $b "$root\$After" --crop --from $s[0] --to $s[1] --n $s[2] --cols 8
} }
