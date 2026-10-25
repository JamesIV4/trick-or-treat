# Dot-source this, then wrap Godot runs:  $lock = Enter-GodotLock; try { ... } finally { Exit-GodotLock $lock }
# Only one Godot import/run touches the project at a time (several art workers preview in parallel).
function Enter-GodotLock {
    param([int]$TimeoutSeconds = 900)
    $path = Join-Path (Resolve-Path "$PSScriptRoot\..") '.godot_lock'
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ($true) {
        try {
            $stream = [IO.File]::Open($path, 'OpenOrCreate', 'ReadWrite', 'None')
            return $stream
        } catch {
            if ((Get-Date) -gt $deadline) { throw "Timed out waiting for the Godot lock ($path)" }
            Start-Sleep -Milliseconds 700
        }
    }
}
function Exit-GodotLock {
    param($Stream)
    if ($Stream) { $Stream.Dispose() }
}
