# Milestone 7: two real game processes play online through Epic Online Services, signed in as two Epic accounts
# with the EOS Developer Authentication Tool (two credentials, by default "Default" and "Default2"; see NOTES.md).
# Runs Scripts/test_eos_standalone.py in each and prints both logs' ECHO_EOS lines. The editor must be closed.
# Usage:  powershell -File Scripts/run_eos_test.ps1 [-NetEmu "average"|"bad"] [-SoakMinutes 30]   (two small windows; ~3 minutes + soak)
param(
    [string]$HostCredential = "Default",    # Dev Auth Tool credential names (host plays Bat, guest Saraa)
    [string]$GuestCredential = "Default2",
    [double]$SoakMinutes = 0,               # > 0: stay in the game this long first (Saraa walks and jumps), checking drift and disconnects
    [string]$NetEmu = ""   # "", "average" (60 ms each way, 1% loss) or "bad" (150 ms, 30 ms jitter, 5% loss)
)

$Project = Split-Path -Parent $PSScriptRoot
$Editor = "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Temp = [IO.Path]::GetTempPath().TrimEnd('\').Replace('\', '/')
$Shared = "$Temp/echo_eos_shared"
Remove-Item $Shared -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $Shared | Out-Null

$Emu = switch ($NetEmu) {
    "average" { "NetEmulation.PktLag 60, NetEmulation.PktLagVariance 10, NetEmulation.PktLoss 1, " }
    "bad"     { "NetEmulation.PktLag 150, NetEmulation.PktLagVariance 30, NetEmulation.PktLoss 5, " }
    default   { "" }
}

$Games = @()
foreach ($Role in "host", "guest") {
    $Script = "$Temp/echo_eos_$Role.py"
    $Body = "ECHO_EOS_ROLE = '$Role'`nECHO_EOS_DIR = r'$Shared'`nECHO_EOS_SOAK = $SoakMinutes`n" + (Get-Content "$PSScriptRoot\test_eos_standalone.py" -Raw)
    [IO.File]::WriteAllText($Script, $Body, (New-Object Text.UTF8Encoding $false))
    $Log = "$Temp/echo_eos_$Role.log"
    Remove-Item $Log -ErrorAction SilentlyContinue
    $Credential = if ($Role -eq "host") { $HostCredential } else { $GuestCredential }
    $Display = @("-windowed", "-ResX=960", "-ResY=540", "-WinX=$(if ($Role -eq 'host') { 20 } else { 990 })", "-WinY=60")
    $Games += Start-Process $Editor -PassThru -ArgumentList (@("`"$Project\OurLastEcho.uproject`"", "-game") + $Display +
        @("-unattended", "-nosplash", "-EchoDevAuth=$Credential", "`"-ExecCmds=scalability 0, ${Emu}py $Script`"", "-abslog=$Log"))
    Start-Sleep 4
}
foreach ($Game in $Games) {
    $Game | Wait-Process -Timeout (720 + [int]($SoakMinutes * 60)) -ErrorAction SilentlyContinue
    if (-not $Game.HasExited) { Stop-Process $Game -Force; Write-Output "a game timed out" }
}
foreach ($Role in "host", "guest") {
    Select-String "$Temp/echo_eos_$Role.log" -Pattern "ECHO_EOS|LogPython: Error|Traceback" | ForEach-Object { $_.Line -replace '^\[.*?\]\[.*?\]LogPython: ', '' }
}

# Memory over the run, from each game's online session log (one network sample every 10 s)
foreach ($Log in Get-ChildItem "$Project\Saved\Logs\OnlineSessions" -Filter 'Online-*.log' | Sort-Object LastWriteTime | Select-Object -Last 2) {
    $mem = Select-String $Log.FullName -Pattern 'memory (\d+) MB' | ForEach-Object { [int]$_.Matches[0].Groups[1].Value }
    if ($mem) { Write-Output ("memory " + $Log.Name + ": first " + $mem[0] + " MB, last " + $mem[-1] + " MB, peak " + ($mem | Measure-Object -Maximum).Maximum + " MB over " + $mem.Count + " samples") }
}
