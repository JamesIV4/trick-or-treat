param(
    [int[]]$Seeds = @(101, 102, 103, 104),
    [string[]]$Views = @('yard3c', 'column3c', 'roofs3c'),
    [string]$Godot = 'C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe'
)
# Stage 3c: headless dry run of the capture views (no window, no PNG): prints each view's pick (YARD3C_VIEW,
# COLUMN3C_VIEW, ROOFS3C_VIEW) per world seed, so the one windowed capture round can pick its seed. Under the lock.
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
. "$root\tools\godot_lock.ps1"
$lock = Enter-GodotLock
try {
    foreach ($s in $Seeds) {
        foreach ($v in $Views) {
            $log = Join-Path $PSScriptRoot "dry3c_$($s)_$v.log"
            & $Godot --headless --path "$root\game" --quit-after 400 -- "--seed=$s" "--capture-view=$v" '--no-lightning' *> $log
            $hit = Select-String -LiteralPath $log -Pattern 'YARD3C_VIEW|COLUMN3C_VIEW|ROOFS3C_VIEW' | Select-Object -First 1
            Write-Output ("seed {0} {1}: {2}" -f $s, $v, $(if ($hit) { $hit.Line } else { '(no pick)' }))
        }
    }
} finally {
    Exit-GodotLock $lock
}
