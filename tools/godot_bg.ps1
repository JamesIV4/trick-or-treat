# Runs a windowed Godot (a recording, a capture, a test) without getting in the user's way:
# the window opens far off-screen, sound is off, and keyboard focus goes straight back to the window
# that had it. The user works and plays on this machine while reviews run, so EVERY windowed Godot
# launch by a tool or a worker goes through this (headless runs such as --import do not need it).
#
#   . "$PSScriptRoot\godot_bg.ps1"
#   $code = Start-GodotBackground -Godot $Godot -Arguments @('--path', "$root\game", '--', '--seed=101') -Log "$Out\run.log"
#
# -Arguments are Godot's own arguments (engine flags, then '--', then the game's). The function adds
# '--position' and '--audio-driver Dummy' in front. stdout and stderr both go to -Log (UTF-8).
# Returns the process exit code (or -1 on timeout, after stopping the process it started).
# A movie-writer run records exactly the same frames as on screen: only where the window sits changes.

if (-not ('TotBg.Win' -as [type])) {
    Add-Type -Namespace TotBg -Name Win -MemberDefinition @'
[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
[DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
[DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
[DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
'@
}

function Start-GodotBackground {
    param(
        [Parameter(Mandatory)][string]$Godot,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$Log,
        [int]$TimeoutSec = 900
    )
    $offX = 32000; $offY = 32000   # far outside any desktop
    $all = @('--position', "$offX,$offY", '--audio-driver', 'Dummy') + $Arguments
    # Start-Process joins arguments with spaces: quote the ones that need it.
    $quoted = $all | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }
    $tmpOut = [IO.Path]::GetTempFileName(); $tmpErr = [IO.Path]::GetTempFileName()
    $before = [TotBg.Win]::GetForegroundWindow()
    $p = Start-Process -FilePath $Godot -ArgumentList $quoted -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr -PassThru -WindowStyle Hidden
    try {
        # As soon as the game window exists: make sure it is off-screen, and give focus back. Keep doing it
        # for the first 20 s, not just once: the game may move or resize its window after it first appears
        # (settings.gd centred scripted windows, which put captures on the user's screen for a moment).
        $deadline = (Get-Date).AddSeconds(20)
        $handled = $false
        while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 40
            # Only the process we started and its children (the console exe starts the real one): never
            # a game or editor the user opened themselves.
            $ids = @($p.Id) + @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($p.Id)" -ErrorAction SilentlyContinue | ForEach-Object { $_.ProcessId })
            $wins = @(Get-Process -Id $ids -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero })
            foreach ($w in $wins) {
                # (a window can close between the lookup and here: skip it)
                $h = $w.MainWindowHandle
                if ($null -eq $h -or $h -eq [IntPtr]::Zero) { continue }
                # SWP_NOSIZE 0x1 | SWP_NOZORDER 0x4 | SWP_NOACTIVATE 0x10
                try { [void][TotBg.Win]::SetWindowPos($h, [IntPtr]::Zero, $offX, $offY, 0, 0, 0x15) } catch {}
                $handled = $true
            }
            $fg = [TotBg.Win]::GetForegroundWindow()
            $ours = @($wins | Where-Object { $_.MainWindowHandle -eq $fg }).Count -gt 0
            if ($handled -and $ours -and $before -ne [IntPtr]::Zero -and [TotBg.Win]::IsWindow($before)) {
                # Windows only lets a background process hand focus over right after an input event:
                # a tap of a key no application uses (F15) counts.
                [TotBg.Win]::keybd_event(0x7E, 0, 0, [UIntPtr]::Zero); [TotBg.Win]::keybd_event(0x7E, 0, 2, [UIntPtr]::Zero)
                [void][TotBg.Win]::SetForegroundWindow($before)
            }
        }
        if (-not $p.WaitForExit($TimeoutSec * 1000)) {
            try { $p.Kill() } catch {}
            $code = -1
        } else {
            $p.WaitForExit()   # flush redirected output
            $code = $p.ExitCode
        }
    } finally {
        $text = ''
        foreach ($f in $tmpOut, $tmpErr) {
            if (Test-Path -LiteralPath $f) { $text += [IO.File]::ReadAllText($f); Remove-Item -LiteralPath $f -ErrorAction SilentlyContinue }
        }
        [IO.File]::WriteAllText($Log, $text, (New-Object Text.UTF8Encoding $false))
    }
    return $code
}
