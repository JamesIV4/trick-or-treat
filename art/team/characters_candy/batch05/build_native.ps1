param([string]$Log = "build_p.log")
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
. "$root\tools\godot_lock.ps1"
$lock = Enter-GodotLock
try {
    $env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
    $dll = "$root\game\native\totnative.windows.template_debug.x86_64.dll"
    if (Test-Path $dll) {
        try { $s = [IO.File]::Open($dll, 'Open', 'ReadWrite', 'None'); $s.Dispose() }
        catch { Rename-Item $dll ("totnative.windows.template_debug.x86_64.dll.old{0}.dll" -f (Get-Date -Format 'HHmmss')) }
    }
    Push-Location "$root\native"
    & scons platform=windows use_mingw=yes mingw_prefix= target=template_debug custom_api_file=extension_api.json -j8 *> $Log
    $code = $LASTEXITCODE
    Pop-Location
    if ($code -ne 0) {
        Select-String -Path "$root\native\$Log" -Pattern 'error|Error' -Context 0,3 | Select-Object -First 30 | ForEach-Object { $_.Line; $_.Context.PostContext }
        Write-Output "BUILD FAILED ($code)"
    } else { Write-Output "BUILD OK" }
} finally { Exit-GodotLock $lock }
