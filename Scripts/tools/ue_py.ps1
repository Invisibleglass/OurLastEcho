param(
    [Parameter(Mandatory)] [string]$Script,     # file name under the project's Scripts/ folder, or a full path
    [string]$Marker = "",                        # log marker to wait for, e.g. ECHO_PIE
    [string]$DonePattern = "",                   # regex on marker lines that means "finished", e.g. "(PASS|FAIL)"
    [int]$TimeoutSec = 240
)
# Runs a Python script inside the open editor and (optionally) waits for and prints its log lines
$Project = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Log = Join-Path $Project "Saved\Logs\OurLastEcho.log"
$Path = if ([IO.Path]::IsPathRooted($Script)) { $Script } else { Join-Path $Project "Scripts\$Script" }
$Path = $Path -replace '\\', '/'
$startLines = (Get-Content $Log -ErrorAction SilentlyContinue | Measure-Object -Line).Lines
& "$PSScriptRoot\ue_cmd.ps1" -Command "py exec(open(r'$Path').read())" | Out-Null
if (-not $Marker) { exit 0 }
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 1500
    $new = Get-Content $Log | Select-Object -Skip $startLines
    $hits = $new | Where-Object { $_ -match $Marker -or $_ -match "LogPython: Error" }
    if (-not $DonePattern -and $hits) { break }
    if ($DonePattern -and ($hits | Where-Object { $_ -match "$Marker.*$DonePattern" -or $_ -match "LogPython: Error" })) { Start-Sleep -Milliseconds 500; break }
}
$new = Get-Content $Log | Select-Object -Skip $startLines
$new | Where-Object { $_ -match $Marker -or $_ -match "LogPython: Error" } | ForEach-Object { $_ -replace '^\[.*?\]\[.*?\]LogPython: ', '' }

