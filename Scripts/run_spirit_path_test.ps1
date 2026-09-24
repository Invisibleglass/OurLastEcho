# Runs Scripts/test_spirit_path.py (end-to-end Milestone 1 test) inside a headless game world and prints its ECHO_TEST lines.
# The editor must be closed. Usage:  powershell -File Scripts/run_spirit_path_test.ps1

$Project = Split-Path -Parent $PSScriptRoot
$Editor = "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

# The `py` console command can't take a path with spaces, so run a copy from TEMP
$Temp = [IO.Path]::GetTempPath().TrimEnd('\') -replace '\\', '/'
Copy-Item "$PSScriptRoot\test_spirit_path.py" "$Temp/echo_spirit_path_test.py" -Force
$Log = "$Temp/echo_spirit_path_test.log"

$Cmds = "EnableCheats, summon /Game/Echo/Blueprints/BP_Saraa.BP_Saraa_C, py $Temp/echo_spirit_path_test.py"
$Game = Start-Process $Editor -PassThru -ArgumentList @(
    "`"$Project\OurLastEcho.uproject`"", "/Game/Echo/Maps/Lvl_SpiritPath",
    "-game", "-nullrhi", "-nosound", "-unattended", "-nosplash",
    "`"-ExecCmds=$Cmds`"", "-abslog=$Log")

$Game | Wait-Process -Timeout 90 -ErrorAction SilentlyContinue
if (-not $Game.HasExited) { Stop-Process $Game -Force; Write-Output "Timed out" }

Select-String $Log -Pattern "ECHO_TEST|LogPython: Error|Traceback" | ForEach-Object { $_.Line -replace '^\[.*?\]\[.*?\]LogPython: ', '' }
