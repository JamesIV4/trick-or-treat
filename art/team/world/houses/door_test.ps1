param(
    [string]$Name = 'door_test',      # output: <Name>.log in this folder
    [int]$Seed = 101,
    [string[]]$Extra = @(),           # more game args, e.g. --no-preload
    [switch]$NoBuild,
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# House goals and candy bug (WORLD_REWORK.md): build the native extension and run --door-test headless under
# the shared Godot lock (a locked DLL is moved aside, never deleted). Prints the DOOR_TEST lines.
#   art\team\world\houses\door_test.ps1 -Name fixed
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$out = $PSScriptRoot
. "$root\tools\godot_lock.ps1"
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
    $gameArgs = @("--seed=$Seed", '--door-test', '--stream-fps=60', '--no-lightning') + $Extra
    $t0 = Get-Date
    & $Godot --headless --path "$root\game" -- @gameArgs *> $log
    Write-Output ("godot exit {0} in {1:N1} s" -f $LASTEXITCODE, ((Get-Date) - $t0).TotalSeconds)
    Select-String -LiteralPath $log -Pattern 'DOOR_TEST (check|summary|home|done|save)|SCRIPT ERROR|ERROR:|Attached ' | Select-Object -First 120 | ForEach-Object { $_.Line }
} finally {
    Exit-GodotLock $lock
}
