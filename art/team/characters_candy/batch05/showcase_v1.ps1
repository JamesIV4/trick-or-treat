# COPY of tools\showcase.ps1 with one change: the game is started with --anim-v1 (the animation before KidRig),
# to prove the legacy path still matches the BEFORE recording. Lane P, batch05. Not the before/after recorder.
param(
    [string[]]$Ids = @('all'),
    [string]$Out = "$PSScriptRoot\..\..\..\review\showcase\wip_p\v1check",
    [switch]$NoBuild,
    [int]$Width = 960,
    [int]$Height = 1080,
    [switch]$KeepAvi,
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# Character showcase recorder: one deterministic clip per character (6 player costumes, 10 monsters),
# the same beats on the same frames every run, so a "before" and an "after" line up frame for frame.
#   tools\showcase.ps1 -Ids all -Out art\review\showcase\before
#   tools\showcase.ps1 -Ids kid_cat,monster_bat -Out art\review\showcase\after -NoBuild
# Per id it writes into -Out:
#   <id>.mp4          H.264, yuv420p, crf 16, 60 fps, Width x Height, starting on the first beat's first frame
#   <id>.beats.json   the ordered beats (name, start_frame, end_frame inclusive, fps) and the still frames
#   <id>_front.png, <id>_back.png, <id>_face.png   stills taken from the mp4 at fixed frames
#   <id>.log          the game's output for that run
# The director is native/src/play/festival_showcase.cpp (--showcase=<id>). Everything runs under the
# shared Godot lock. Exit code 0 only if every id produced a clip of the right size and length.
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$all = @('dragon_kid', 'kid_ghost', 'kid_cat', 'kid_witch', 'kid_skeleton', 'kid_pumpkin',
    'monster_pumpkin', 'monster_bat', 'monster_skeleton', 'monster_ghost', 'monster_gremlin',
    'monster_mummy', 'monster_slime', 'monster_scarecrow', 'monster_spider', 'monster_toadstool')
$list = @($Ids | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($list.Count -eq 0 -or $list -contains 'all') { $list = $all }
foreach ($id in $list) { if ($all -notcontains $id) { throw "Unknown showcase id '$id' (known: $($all -join ', '))" } }
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$started = Get-Date
$failed = @()
$override = "$root\game\override.cfg"
$marker = 'tools/showcase.ps1 temporary window size.'
$wrote = $false
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"
$lock = Enter-GodotLock
try {
    if (-not $NoBuild) {
        $env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
        # The editor (or a running game) holds the extension DLL: move a locked one aside, never delete it.
        $dll = "$root\game\native\totnative.windows.template_debug.x86_64.dll"
        if (Test-Path $dll) {
            try { $s = [IO.File]::Open($dll, 'Open', 'ReadWrite', 'None'); $s.Dispose() }
            catch { Rename-Item $dll ("totnative.windows.template_debug.x86_64.dll.old{0}.dll" -f (Get-Date -Format 'HHmmss')) }
        }
        Push-Location "$root\native"
        & scons platform=windows use_mingw=yes mingw_prefix= target=template_debug custom_api_file=extension_api.json -j8 *> build.log
        $code = $LASTEXITCODE
        Pop-Location
        if ($code -ne 0) {
            Select-String -Path "$root\native\build.log" -Pattern 'error' | Select-Object -First 20 | ForEach-Object { $_.Line }
            throw 'native build failed'
        }
    }
    & $Godot --headless --import --path "$root\game" *> "$Out\import.log"
    # Movie Maker mode records at the project's window size whatever --resolution says, and scales
    # every frame to it. So the project's window size is overridden for these runs only, with a
    # temporary game\override.cfg (Godot reads it on top of project.godot), removed in the finally.
    if (Test-Path $override) {
        if (-not (Select-String -Path $override -SimpleMatch $marker -Quiet)) { throw "game\override.cfg exists and is not the showcase's: move it away first" }
        Remove-Item $override
    }
    Set-Content -Path $override -Encoding ascii -Value @(
        "; $marker Written for a recording and removed when it ends. Safe to delete.",
        '[display]', '',
        "window/size/window_width_override=$Width",
        "window/size/window_height_override=$Height",
        'window/size/mode=0')
    $wrote = $true
    foreach ($id in $list) {
        $t0 = Get-Date
        $avi = Join-Path $Out "$id.avi"
        $mp4 = Join-Path $Out "$id.mp4"
        $json = Join-Path $Out "$id.beats.json"
        $log = Join-Path $Out "$id.log"
        Remove-Item $avi, $mp4, $json, (Join-Path $Out "${id}_front.png"), (Join-Path $Out "${id}_back.png"), (Join-Path $Out "${id}_face.png") -ErrorAction SilentlyContinue
        # --showcase-size: the showcase checks its window really has this size before the first beat.
        # Off-screen, muted, no focus taken (tools\godot_bg.ps1, decision 16); the same frames either way.
        [void](Start-GodotBackground -Godot $Godot -Log $log -Arguments @('--path', "$root\game", '--fixed-fps', '60', '--write-movie', $avi, '--', "--showcase=$id", "--showcase-out=$Out", "--showcase-size=${Width}x${Height}", '--anim-v1'))
        if (-not (Test-Path $json) -or -not (Test-Path $avi)) {
            Write-Output "FAILED $id : no recording (see $log)"
            $failed += $id
            continue
        }
        $beats = Get-Content $json -Raw | ConvertFrom-Json
        $hint = [int]$beats.movie_start_hint
        $total = [int]$beats.total_frames
        # Where the first beat starts in the movie. The engine's frame count is only a hint: the showcase
        # ends its lead-in with a run of plain magenta frames, and the clip starts on the frame after the
        # last of them. Frames are picked by number with filters, decoding from the start of the .avi:
        # seeking with -ss lands a frame early or late now and then.
        $lo = [Math]::Max(0, $hint - 26)
        $raw = Join-Path $Out "$id.sync.raw"
        & ffmpeg -hide_banner -loglevel error -y -i $avi -an -vf "select='between(n\,$lo\,$($lo + 35))',scale=8:8" -vsync 0 -frames:v 36 -f rawvideo -pix_fmt rgb24 $raw 2>$null
        $bytes = if (Test-Path $raw) { [IO.File]::ReadAllBytes($raw) } else { [byte[]]@() }
        Remove-Item $raw -ErrorAction SilentlyContinue
        $seen = [int][Math]::Floor($bytes.Length / 192)
        $last = -1; $count = 0
        for ($k = 0; $k -lt $seen; $k++) {
            $r = 0; $g = 0; $b = 0
            for ($p = 0; $p -lt 64; $p++) { $r += $bytes[$k * 192 + $p * 3]; $g += $bytes[$k * 192 + $p * 3 + 1]; $b += $bytes[$k * 192 + $p * 3 + 2] }
            if ($r / 64 -gt 180 -and $g / 64 -lt 90 -and $b / 64 -gt 180) { $last = $k; $count++ }
        }
        if ($count -ne [int]$beats.sync_marker_frames -or $last -ge $seen - 1) {
            Write-Output "FAILED $id : sync marker not found near movie frame $hint ($count magenta frames in $seen; the .avi is kept; see $log)"
            $failed += $id
            continue
        }
        $first = $lo + $last + 1
        # Cut the lead-in (loading screen, settling, the marker) and anything after the last beat.
        # MJPEG is full-range BT.601; the mp4 is ordinary limited-range BT.709 yuv420p, tagged as such.
        $vf = "trim=start_frame=${first}:end_frame=$($first + $total),setpts=PTS-STARTPTS," +
            'scale=in_range=full:out_range=limited:in_color_matrix=bt601:out_color_matrix=bt709,format=yuv420p'
        & ffmpeg -hide_banner -loglevel error -y -i $avi -frames:v $total -an -vf $vf -c:v libx264 -preset medium -crf 16 -pix_fmt yuv420p `
            -color_range tv -colorspace bt709 -color_primaries bt709 -color_trc bt709 -r 60 -movflags +faststart $mp4 *> (Join-Path $Out "$id.ffmpeg.log")
        $probe = & ffprobe -v error -select_streams v:0 -count_frames -show_entries stream=width,height,pix_fmt,r_frame_rate,nb_read_frames -of csv=p=0 $mp4
        $parts = "$probe".Trim() -split ','   # width,height,pix_fmt,r_frame_rate,nb_read_frames
        $ok = (Test-Path $mp4) -and $parts.Count -ge 5 -and [int]$parts[0] -eq $Width -and [int]$parts[1] -eq $Height -and $parts[2] -eq 'yuv420p' -and $parts[3] -eq '60/1' -and [int]$parts[4] -eq $total
        if (-not $ok) {
            Write-Output "FAILED $id : mp4 is '$probe', expected ${Width}x${Height} yuv420p 60/1 with $total frames (the .avi is kept; see $log)"
            $failed += $id
            continue
        }
        if (-not $KeepAvi) { Remove-Item $avi -ErrorAction SilentlyContinue }
        Remove-Item (Join-Path $Out "$id.ffmpeg.log") -ErrorAction SilentlyContinue
        foreach ($still in 'front', 'back', 'face') {
            $n = [int]$beats.stills.$still
            & ffmpeg -hide_banner -loglevel error -y -i $mp4 -vf "select=eq(n\,$n)" -vframes 1 (Join-Path $Out "${id}_$still.png") 2>$null
        }
        Write-Output ("OK {0} : {1}x{2} {3}, {4} frames ({5:n1} s), {6} beats, starts at movie frame {7} (hint {8}), took {9:n0} s" -f $id, $parts[0], $parts[1], $parts[2], $parts[4], ($total / 60.0), $beats.beats.Count, $first, $hint, ((Get-Date) - $t0).TotalSeconds)
    }
} finally {
    if ($wrote -and (Test-Path $override)) { Remove-Item $override -ErrorAction SilentlyContinue }
    Exit-GodotLock $lock
}
Write-Output ("showcase: {0} of {1} ids recorded into {2} in {3:n1} min" -f ($list.Count - $failed.Count), $list.Count, $Out, ((Get-Date) - $started).TotalMinutes)
if ($failed.Count -gt 0) { Write-Output ("failed: " + ($failed -join ', ')); exit 1 }
exit 0
