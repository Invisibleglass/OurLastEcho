# Two real game processes (not Play-In-Editor) host and join over LAN (Online Subsystem Null), ready up and start;
# runs Scripts/test_lan_standalone.py in each and prints both logs' ECHO_LAN lines. The editor must be closed.
# Usage:  powershell -File Scripts/run_lan_test.ps1   (two small game windows open; takes about 2 minutes)
# The games run windowed at the Low preset: with -nullrhi nothing is drawn and CommonUI screens never finish their
# transitions, and two full-quality copies overload a 6 GB GPU.

$Project = Split-Path -Parent $PSScriptRoot
$Editor = "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
# The `py` console command can't take a path with spaces, so run copies from TEMP
$Temp = [IO.Path]::GetTempPath().TrimEnd('\').Replace('\', '/')
$Games = @()
foreach ($Role in "host", "guest") {
    $Script = "$Temp/echo_lan_$Role.py"
    $Body = "ECHO_LAN_ROLE = '$Role'`n" + (Get-Content "$PSScriptRoot\test_lan_standalone.py" -Raw)
    [IO.File]::WriteAllText($Script, $Body, (New-Object Text.UTF8Encoding $false))
    $Log = "$Temp/echo_lan_$Role.log"
    Remove-Item $Log -ErrorAction SilentlyContinue
    $Display = @("-windowed", "-ResX=960", "-ResY=540")
    $Games += Start-Process $Editor -PassThru -ArgumentList (@("`"$Project\OurLastEcho.uproject`"", "-game") + $Display +
        @("-nosound", "-unattended", "-nosplash", "`"-ExecCmds=scalability 0, py $Script`"", "-abslog=$Log"))
}
foreach ($Game in $Games) {
    $Game | Wait-Process -Timeout 240 -ErrorAction SilentlyContinue
    if (-not $Game.HasExited) { Stop-Process $Game -Force; Write-Output "a game timed out" }
}
foreach ($Role in "host", "guest") {
    Select-String "$Temp/echo_lan_$Role.log" -Pattern "ECHO_LAN|LogPython: Error|Traceback" | ForEach-Object { $_.Line -replace '^\[.*?\]\[.*?\]LogPython: ', '' }
}
