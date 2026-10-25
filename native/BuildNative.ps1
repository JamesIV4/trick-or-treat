param(
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe',
    [ValidateSet('template_debug','template_release')][string]$Target = 'template_debug',
    [int]$Jobs = 8,
    [string]$MinGW = 'C:\msys64\mingw64\bin'
)
# Same pinning approach as OpenXcom-Extended-3D: fixed godot-cpp revision, API dumped from the engine.
$ErrorActionPreference = 'Stop'
$nativeRoot = $PSScriptRoot
$bindingsPath = Join-Path $nativeRoot 'third_party\godot-cpp'
$bindingsRevision = '507ed9d840c01a3c5b2a39af8bb4000bfac30bf5'
if (-not (Test-Path -LiteralPath (Join-Path $bindingsPath 'SConstruct'))) {
    & git clone --no-checkout --filter=blob:none https://github.com/godotengine/godot-cpp.git $bindingsPath
    if ($LASTEXITCODE -ne 0) { throw 'godot-cpp download failed' }
    & git -C $bindingsPath checkout $bindingsRevision
    if ($LASTEXITCODE -ne 0) { throw 'godot-cpp checkout failed' }
}
$actualRevision = & git -C $bindingsPath rev-parse HEAD
if ($actualRevision -ne $bindingsRevision) { throw "Unexpected godot-cpp revision: $actualRevision" }
if (-not (Test-Path -LiteralPath $Godot)) { throw 'Supply -Godot with a Godot 4.7 Standard executable.' }
if (Test-Path -LiteralPath $MinGW) { $env:PATH = "$MinGW;$env:PATH" }
Push-Location $nativeRoot
try {
    if (-not (Test-Path -LiteralPath 'extension_api.json')) {
        & $Godot --headless --dump-extension-api
        if ($LASTEXITCODE -ne 0) { throw 'Godot API dump failed' }
    }
    & scons platform=windows use_mingw=yes mingw_prefix= target=$Target custom_api_file=extension_api.json "-j$Jobs"
    if ($LASTEXITCODE -ne 0) { throw 'Native extension build failed' }
} finally {
    Pop-Location
}
