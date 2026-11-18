param(
    [string]$Name = 'walk',           # output: <Name>.log in this folder
    [int]$Seed = 101,
    [int]$Secs = 100,                 # --fly-test duration (15 m/s east from the home spawn)
    [int]$Fps = 60,                   # frame-rate cap (--stream-fps) so the walk has real idle time
    [string[]]$Extra = @(),           # more game args, e.g. --no-preload, --stream-probe, --stream-verify=<file>
    [switch]$NoBuild,
    [switch]$Windowed,                # off-screen windowed run (real renderer) through tools\godot_bg.ps1
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# Area preloading: build the native extension and run the streaming walk (--fly-test) headless, under the
# shared Godot lock; the locked DLL is moved aside, never deleted. The game prints STREAM_STATS lines on exit.
#   art\team\world\preload\walk.ps1 -Name before -Extra --no-preload
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = $PSScriptRoot
. "$root\tools\godot_lock.ps1"
. "$root\tools\godot_bg.ps1"
$lock = Enter-GodotLock
try {
    if (-not $NoBuild) {
        $env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
        $dll = "$root\game\native\totnative.windows.template_debug.x86_64.dll"
        if (Test-Path $dll) {
            try { $s = [IO.File]::Open($dll, 'Open', 'ReadWrite', 'None'); $s.Dispose() }
            catch { Rename-Item $dll ("totnative.windows.template_debug.x86_64.dll.old{0}.dll" -f (Get-Date -Format 'HHmmss')) }
        }
        Push-Location "$root\native"
        & scons platform=windows use_mingw=yes mingw_prefix= target=template_debug custom_api_file=extension_api.json -j8 *> "$out\build.log"
        $code = $LASTEXITCODE
        Pop-Location
        if ($code -ne 0) {
            Select-String -Path "$out\build.log" -Pattern 'error' | Select-Object -First 30 | ForEach-Object { $_.Line }
            throw 'native build failed'
        }
        Write-Output 'build ok'
    }
    $log = Join-Path $out "$Name.log"
    $gameArgs = @("--seed=$Seed", "--fly-test=$Secs", "--stream-fps=$Fps", '--no-lightning') + $Extra
    $t0 = Get-Date
    if ($Windowed) {
        $code = Start-GodotBackground -Godot $Godot -Arguments (@('--path', "$root\game", '--') + $gameArgs) -Log $log -TimeoutSec ($Secs + 240)
    } else {
        & $Godot --headless --path "$root\game" -- @gameArgs *> $log
        $code = $LASTEXITCODE
    }
    Write-Output ("godot exit {0} in {1:N1} s" -f $code, ((Get-Date) - $t0).TotalSeconds)
    Select-String -LiteralPath $log -Pattern 'STREAM_|Loaded |Attached |Preload|SCRIPT ERROR|ERROR:' | Select-Object -First 80 | ForEach-Object { $_.Line }
} finally {
    Exit-GodotLock $lock
}
