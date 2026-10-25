param(
    [int[]]$Seeds = @(101),
    [string[]]$Views = @('play'),
    [string]$Out = "$PSScriptRoot\..\art\review\captures",
    [int]$Frames = 200,
    [switch]$NoBuild,
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# Builds the native extension (unless -NoBuild) and captures in-engine screenshots:
#   tools\review.ps1 -Seeds 101,102 -Views play,overview,lot:3
# Everything runs under the shared Godot lock (art workers preview assets in parallel, and a
# running Godot holds the extension DLL).
$root = Resolve-Path "$PSScriptRoot\.."
New-Item -ItemType Directory -Force $Out | Out-Null
$ErrorActionPreference = 'Continue'
. "$PSScriptRoot\godot_lock.ps1"
$lock = Enter-GodotLock
try {
    if (-not $NoBuild) {
        $env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
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
    foreach ($seed in $Seeds) {
        foreach ($view in $Views) {
            $name = "seed$seed`_$($view -replace ':', '')"
            $png = Join-Path (Resolve-Path $Out) "$name.png"
            & $Godot --path "$root\game" --resolution 1600x900 -- "--seed=$seed" "--capture=$png" "--capture-frames=$Frames" "--capture-view=$view" *> "$Out\$name.log"
            if (Test-Path $png) { Write-Output $png } else { Write-Output "FAILED $name (see $Out\$name.log)" }
        }
    }
} finally {
    Exit-GodotLock $lock
}
