param(
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe',
    [string]$MinGW = 'C:\msys64\mingw64\bin',
    [string]$OutputDirectory = "$PSScriptRoot\..\builds\windows",
    [int]$Jobs = 8
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..").Path
$project = Join-Path $root 'game'
$compiler = Join-Path $MinGW 'g++.exe'
$windres = Join-Path $MinGW 'windres.exe'
foreach ($required in @($Godot, $compiler, $windres)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required tool not found: $required" }
}
$version = & $Godot --version
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^4\.7\.' -or $version -match '\.mono\.') {
    throw "Use the Godot 4.7 standard build for the release (found: $version)."
}

$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$stage = Join-Path $tempRoot ('TrickOrTreat-build-' + [guid]::NewGuid().ToString('N'))
$stage = [IO.Path]::GetFullPath($stage)
if (-not $stage.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Split-Path $stage -Leaf).StartsWith('TrickOrTreat-build-')) {
    throw "Unsafe build staging path: $stage"
}
New-Item -ItemType Directory -Path $stage | Out-Null
$temporaryOutput = $null
$backupOutput = $null
$lock = $null
try {
    . "$PSScriptRoot\godot_lock.ps1"
    $lock = Enter-GodotLock
    Write-Host 'Building the release native extension...'
    & "$root\native\BuildNative.ps1" -Godot $Godot -MinGW $MinGW -Target template_release -Jobs $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Native release build failed.' }

    Write-Host 'Importing assets and exporting the game...'
    & $Godot --headless --path $project --import
    if ($LASTEXITCODE -ne 0) { throw 'Godot asset import failed.' }
    $export = Join-Path $stage 'export'
    New-Item -ItemType Directory -Path $export | Out-Null
    $gameExe = Join-Path $export 'Game.exe'
    $dllName = 'totnative.windows.template_release.x86_64.dll'
    $gameDll = Join-Path $export $dllName
    & $Godot --headless --path $project --export-release 'Windows Desktop' $gameExe
    if ($LASTEXITCODE -ne 0) { throw 'Godot export failed.' }
    foreach ($required in @($gameExe, $gameDll)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Export is missing $required" }
    }
    $unexpected = @(Get-ChildItem -LiteralPath $export -Force | Where-Object { $_.Name -notin @('Game.exe', $dllName) })
    if ($unexpected.Count -ne 0) {
        throw "The export has additional files that the single-file launcher does not package: $($unexpected.Name -join ', ')"
    }

    Write-Host 'Building the single-file launcher...'
    $rc = Join-Path $stage 'launcher.rc'
    $iconObject = Join-Path $stage 'launcher-icon.o'
    $icon = (Join-Path $project 'icon.ico').Replace('\', '/')
    Set-Content -LiteralPath $rc -Value "1 ICON `"$icon`"" -Encoding Ascii
    & $windres --input $rc --output $iconObject --output-format coff
    if ($LASTEXITCODE -ne 0) { throw 'Could not build the launcher icon.' }
    $launcher = Join-Path $stage 'launcher.exe'
    & $compiler -std=c++17 -O2 -municode -mwindows -static -static-libgcc -static-libstdc++ `
        (Join-Path $PSScriptRoot 'windows_single_file_launcher.cpp') $iconObject -o $launcher
    if ($LASTEXITCODE -ne 0) { throw 'Could not compile the single-file launcher.' }

    $candidate = Join-Path $stage 'standalone.exe'
    $stream = [IO.File]::Open($candidate, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        foreach ($inputPath in @($launcher, $gameExe, $gameDll)) {
            $inputStream = [IO.File]::OpenRead($inputPath)
            try { $inputStream.CopyTo($stream) } finally { $inputStream.Dispose() }
        }
        foreach ($value in @([uint64](Get-Item -LiteralPath $gameExe).Length, [uint64](Get-Item -LiteralPath $gameDll).Length)) {
            $bytes = [BitConverter]::GetBytes($value)
            if (-not [BitConverter]::IsLittleEndian) { [Array]::Reverse($bytes) }
            $stream.Write($bytes, 0, $bytes.Length)
        }
        $marker = [Text.Encoding]::ASCII.GetBytes('TOTPKG01')
        $stream.Write($marker, 0, $marker.Length)
    } finally {
        $stream.Dispose()
    }

    # Run the actual shareable EXE from an otherwise empty directory. The sweep
    # exercises the native classes, the catalog, imported models, and both world types.
    Write-Host 'Checking the standalone EXE in isolation...'
    $smoke = Join-Path $stage 'smoke'
    New-Item -ItemType Directory -Path $smoke | Out-Null
    $standalone = Join-Path $smoke 'Halloween - Trick or Treat!.exe'
    Copy-Item -LiteralPath $candidate -Destination $standalone
    $log = Join-Path $stage 'smoke test.log'
    $arguments = "--headless --quit-after 300 --log-file `"$log`" -- --sweep=2 --seed=101"
    # Do not let this machine's MinGW installation hide missing runtime DLLs.
    $previousPath = $env:PATH
    try {
        $env:PATH = "$(Join-Path $env:SystemRoot 'System32');$env:SystemRoot"
        $process = Start-Process -FilePath $standalone -ArgumentList $arguments -WorkingDirectory $smoke -PassThru -WindowStyle Hidden
    } finally {
        $env:PATH = $previousPath
    }
    if (-not $process.WaitForExit(60000)) {
        & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $process.Id /T /F | Out-Null
        throw 'The standalone EXE did not complete its smoke test in 60 seconds.'
    }
    if (-not (Test-Path -LiteralPath $log)) { throw 'The standalone EXE produced no smoke-test log.' }
    $logText = Get-Content -LiteralPath $log -Raw
    if ($process.ExitCode -ne 0 -or $logText -notmatch 'SWEEP_DONE seeds=2 openness_failures=0' -or $logText -match '(?m)^ERROR:') {
        throw "Standalone smoke test failed (exit $($process.ExitCode)):`n$logText"
    }
    $expected = @( (Get-Content -LiteralPath (Join-Path $project 'assets\catalog.json') -Raw | ConvertFrom-Json).assets |
        Where-Object { $_.published -ne $false } ).Count
    $sweeps = @(Select-String -LiteralPath $log -Pattern '^SWEEP \{' | ForEach-Object { $_.Line.Substring(6) | ConvertFrom-Json })
    if ($sweeps.Count -ne 2 -or @($sweeps | Where-Object { $_.assets_published -ne $expected }).Count -ne 0) {
        throw "Standalone smoke test did not load all $expected published models:`n$logText"
    }
    Write-Host "Standalone smoke test passed: two world types and $expected published models."

    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $outputDirectoryPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
    $destination = Join-Path $outputDirectoryPath 'Halloween - Trick or Treat!.exe'
    $temporaryOutput = Join-Path $outputDirectoryPath ('.release-' + [guid]::NewGuid().ToString('N') + '.tmp')
    [IO.File]::Copy($candidate, $temporaryOutput)
    if (Test-Path -LiteralPath $destination -PathType Leaf) {
        $backupOutput = Join-Path $outputDirectoryPath ('.previous-' + [guid]::NewGuid().ToString('N') + '.tmp')
        [IO.File]::Replace($temporaryOutput, $destination, $backupOutput)
    } else {
        [IO.File]::Move($temporaryOutput, $destination)
    }
    $temporaryOutput = $null
    if ($backupOutput) {
        Remove-Item -LiteralPath $backupOutput -Force
        $backupOutput = $null
    }
    Write-Host "Share this one file: $destination"
} finally {
    if ($lock) { Exit-GodotLock $lock }
    if ($temporaryOutput -and (Test-Path -LiteralPath $temporaryOutput -PathType Leaf)) {
        Remove-Item -LiteralPath $temporaryOutput -Force
    }
    if ($backupOutput -and (Test-Path -LiteralPath $backupOutput -PathType Leaf) -and
            (Test-Path -LiteralPath $destination -PathType Leaf)) {
        Remove-Item -LiteralPath $backupOutput -Force
    }
    if (Test-Path -LiteralPath $stage -PathType Container) {
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
}
