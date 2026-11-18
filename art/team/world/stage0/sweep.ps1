param(
    [string]$Name = 'sweep',          # output files: <Name>.log, <Name>.jsonl (SWEEP lines), <Name>_dump.jsonl (with -Dump)
    [int]$Count = 50,
    [int]$Seed = 101,
    [switch]$NoBuild,
    [switch]$Dump,                    # also write the per-seed layout dump (--sweep-dump, Stage 0 and later)
    [string[]]$Extra = @(),           # more game args (Stage 4c), e.g. --sweep-geo=<dir>
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# World rework, Stage 0: build the native extension and run the headless generator sweep
#   art\team\world\stage0\sweep.ps1 -Name baseline
# Everything under the shared Godot lock; the locked DLL is moved aside, never deleted.
$ErrorActionPreference = 'Continue'
$root = Resolve-Path "$PSScriptRoot\..\..\..\.."
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
        & scons platform=windows use_mingw=yes mingw_prefix= target=template_debug custom_api_file=extension_api.json -j8 *> build.log
        $code = $LASTEXITCODE
        Pop-Location
        if ($code -ne 0) {
            Select-String -Path "$root\native\build.log" -Pattern 'error' | Select-Object -First 30 | ForEach-Object { $_.Line }
            throw 'native build failed'
        }
        Write-Output 'build ok'
    }
    $log = Join-Path $out "$Name.log"
    $gameArgs = @("--sweep=$Count", "--seed=$Seed")
    if ($Dump) { $gameArgs += "--sweep-dump=$(Join-Path $out "$Name`_dump.jsonl")" }
    $gameArgs += $Extra
    # The generator reads the asset catalog (prop sizes, published assets): note it, so runs compare like for like.
    Write-Output ('catalog ' + (Get-FileHash "$root\game\assets\catalog.json" -Algorithm SHA256).Hash.Substring(0, 16))
    $t0 = Get-Date
    & $Godot --headless --path "$root\game" -- @gameArgs *> $log
    Write-Output ("godot exit {0} in {1:N1} s" -f $LASTEXITCODE, ((Get-Date) - $t0).TotalSeconds)
    $lines = @(Select-String -LiteralPath $log -Pattern '^SWEEP \{' | ForEach-Object { $_.Line.Substring(6) })
    [IO.File]::WriteAllLines((Join-Path $out "$Name.jsonl"), [string[]]$lines, (New-Object Text.UTF8Encoding $false))
    Write-Output ("{0} SWEEP lines" -f $lines.Count)
    Select-String -LiteralPath $log -Pattern 'SWEEP_DONE|ERROR|SCRIPT ERROR' | Select-Object -First 10 | ForEach-Object { $_.Line }
} finally {
    Exit-GodotLock $lock
}
