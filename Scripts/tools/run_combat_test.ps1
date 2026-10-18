param(
    [string]$Mode = "normal",          # normal | host_saraa | travel
    [string]$NetEmu = "",              # "" | average | bad
    [int]$TimeoutSec = 600
)
# Milestone 8: runs Scripts/test_combat_live.py in a fresh 2-player listen-server PIE session on CombatTest:
# stops PIE, sets the gameplay PIE setup, opens CombatTest, (swaps roles for host_saraa), starts PIE, closes the
# Message Log, then runs the test with its options and waits for PASS / FAIL.
$Project = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& "$PSScriptRoot\mcp.ps1" -Tool StopPIE -Toolset EditorToolset.EditorAppToolset -ArgsJson '{}' | Out-Null
Start-Sleep 2
& "$PSScriptRoot\pie_mode.ps1" -Mode gameplay | Out-Null
& "$PSScriptRoot\ue_cmd.ps1" -Command ("Echo.HostPlaysSaraa " + $(if ($Mode -eq "host_saraa") { "1" } else { "0" })) | Out-Null

$Temp = [IO.Path]::GetTempPath()
$Open = Join-Path $Temp "echo_open_combattest.py"
[IO.File]::WriteAllText($Open, "import unreal`nunreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Echo/Maps/CombatTest')`nunreal.log('ECHO_OPENED: ' + unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name())`n", (New-Object Text.UTF8Encoding $false))
& "$PSScriptRoot\ue_py.ps1" -Script $Open -Marker ECHO_OPENED -TimeoutSec 60 | Out-Null

& "$PSScriptRoot\mcp.ps1" -Tool StartPIE -Toolset EditorToolset.EditorAppToolset -ArgsJson '{"options":{"bSimulate":false,"playMode":"PlayMode_InViewPort","warmupSeconds":5}}' | Out-Null
Start-Sleep 6
& "$PSScriptRoot\close_message_log.ps1" | Out-Null

$Emu = switch ($NetEmu) {
    "average" { '["NetEmulation.PktLag 60", "NetEmulation.PktLagVariance 10", "NetEmulation.PktLoss 1"]' }
    "bad"     { '["NetEmulation.PktLag 150", "NetEmulation.PktLagVariance 30", "NetEmulation.PktLoss 5"]' }
    default   { '[]' }
}
$Script = Join-Path $Temp "echo_combat_run.py"
$Body = "ECHO_COMBAT_MODE = '$Mode'`nECHO_COMBAT_NETEMU = $Emu`n" + (Get-Content (Join-Path $Project "Scripts\test_combat_live.py") -Raw)
[IO.File]::WriteAllText($Script, $Body, (New-Object Text.UTF8Encoding $false))
& "$PSScriptRoot\ue_py.ps1" -Script $Script -Marker ECHO_COMBAT_TEST -DonePattern "(PASS|FAIL \()" -TimeoutSec $TimeoutSec
