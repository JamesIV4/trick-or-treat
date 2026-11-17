# A frame strip of any stretch of a showcase recording, with a free crop (strips.py crops the middle
# of the picture only). Frames are movie frames (beats.json start_frame + frame in the beat).
#   p3_strip.ps1 -From 570 -To 600 -Step 2 -Crop 'iw*0.5:ih*0.4:iw*0.25:ih*0.5' -Name walk_feet
# writes <Dir>\strips\<Id>_<Name>.png. -Before reads art\review\showcase\before instead.
param([string]$Id = 'dragon_kid', [int]$From = 0, [int]$To = 60, [int]$Step = 2, [string]$Crop = 'iw*0.6:ih*0.6:iw*0.2:ih*0.22',
      [int]$Cols = 8, [int]$Width = 320, [string]$Name = 'custom', [string]$Dir = 'art\review\showcase\wip_p', [switch]$Before)
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$src = if ($Before) { "$root\art\review\showcase\before\$Id.mp4" } else { "$root\$Dir\$Id.mp4" }
$n = [int][math]::Floor(($To - $From) / $Step) + 1
$rows = [int][math]::Ceiling($n / $Cols)
New-Item -ItemType Directory -Force "$root\$Dir\strips" | Out-Null
$out = "$root\$Dir\strips\${Id}_$Name.png"
& ffmpeg -hide_banner -loglevel error -y -i $src -vf "select='between(n\,$From\,$To)*not(mod(n-$From\,$Step))',crop=$Crop,scale=${Width}:-1,tile=${Cols}x$rows" -vsync 0 -frames:v 1 $out
$out
