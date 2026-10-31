param(
    [switch]$SkipBuild,
    [switch]$Preview,
    [string]$Channel = 'windows',
    [string]$Version = ''
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..").Path
$keyPath = Join-Path $root '.itchio-api-key'
$localButler = Join-Path $root 'builds\tools\butler\butler.exe'
$artifact = Join-Path $root 'builds\windows\Halloween - Trick or Treat!.exe'
if ($Channel -notmatch '^[a-z0-9][a-z0-9._-]*$') {
    throw 'Channel must contain only lowercase letters, numbers, dots, underscores or hyphens.'
}
if (-not (Test-Path -LiteralPath $keyPath -PathType Leaf)) {
    throw 'Missing .itchio-api-key in the project root. Put your itch.io API key in that ignored file.'
}
$key = [IO.File]::ReadAllText($keyPath).Trim()
if ([string]::IsNullOrWhiteSpace($key)) { throw 'The .itchio-api-key file is empty.' }
if (Test-Path -LiteralPath $localButler -PathType Leaf) {
    $butler = $localButler
} else {
    $command = Get-Command butler -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $command) { throw 'Butler was not found. Install it or see the README for the local installation path.' }
    $butler = $command.Source
}

if (-not $SkipBuild) {
    & "$PSScriptRoot\build_release.ps1"
}
if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) {
    throw 'The Windows release EXE is missing. Build it first or omit -SkipBuild.'
}

# Push only the single tested EXE, even if other files happen to be in builds/windows.
$stageRoot = Join-Path $root 'builds\itchio-staging'
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null
$stage = Join-Path $stageRoot ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
$stagedExe = Join-Path $stage 'Halloween - Trick or Treat!.exe'
$previousKey = $env:BUTLER_API_KEY
try {
    Copy-Item -LiteralPath $artifact -Destination $stagedExe
    $env:BUTLER_API_KEY = $key
    $target = "kalelreturns89/halloween-trick-or-treat:${Channel}"
    Write-Host "$(if ($Preview) { 'Previewing' } else { 'Pushing' }) $artifact to $target"
    if ($SkipBuild) { Write-Host 'Using the existing EXE; it may not include the latest source changes.' }
    if ($Preview) {
        & $butler push-preview $stage $target
    } else {
        $butlerArgs = @('push', $stage, $target, '--if-changed')
        if (-not [string]::IsNullOrWhiteSpace($Version)) { $butlerArgs += @('--userversion', $Version) }
        & $butler @butlerArgs
    }
    if ($LASTEXITCODE -ne 0) { throw "Butler $(if ($Preview) { 'preview' } else { 'push' }) failed (exit $LASTEXITCODE)." }
} finally {
    $env:BUTLER_API_KEY = $previousKey
    $key = $null
    if (Test-Path -LiteralPath $stagedExe -PathType Leaf) { Remove-Item -LiteralPath $stagedExe -Force }
    if (Test-Path -LiteralPath $stage -PathType Container) { Remove-Item -LiteralPath $stage -Force }
}
