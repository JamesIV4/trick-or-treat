param(
    [Parameter(Mandatory)][string]$Log,
    [int]$Secs = 240,
    [int]$Every = 5
)
# Machine load while a timing run goes: total CPU %, and every Godot / compiler process with its CPU time so
# far (another worker's builds and sweeps run alongside). One line per sample.
$end = (Get-Date).AddSeconds($Secs)
$lines = @()
while ((Get-Date) -lt $end) {
    $cpu = (Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average
    $procs = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(Godot|cc1plus|g\+\+|scons|python|blender)' } |
        ForEach-Object { '{0}#{1}:{2:N0}s' -f $_.ProcessName, $_.Id, $_.CPU })
    $lines += ('{0:HH:mm:ss} cpu={1}% {2}' -f (Get-Date), $cpu, ($procs -join ' '))
    Start-Sleep -Seconds $Every
}
[IO.File]::WriteAllLines($Log, [string[]]$lines)
