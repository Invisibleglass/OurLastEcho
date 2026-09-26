param(
    [Parameter(Mandatory)] [string]$Script,     # e.g. test_climb_live.py
    [Parameter(Mandatory)] [string]$Marker,     # e.g. ECHO_CLIMB_TEST
    [int]$TimeoutSec = 400
)
# Runs one of the gameplay live tests (Milestones 1-4) in a fresh 2-player listen-server PIE session on
# Lvl_SpiritPath: stops PIE, switches PIE to the gameplay setup, starts PIE, closes the Message Log, runs the test
& "$PSScriptRoot\mcp.ps1" -Tool StopPIE -Toolset EditorToolset.EditorAppToolset -ArgsJson '{}' | Out-Null
Start-Sleep 2
& "$PSScriptRoot\pie_mode.ps1" -Mode gameplay | Out-Null
& "$PSScriptRoot\mcp.ps1" -Tool StartPIE -Toolset EditorToolset.EditorAppToolset -ArgsJson '{"options":{"bSimulate":false,"playMode":"PlayMode_InViewPort","warmupSeconds":5}}' | Out-Null
Start-Sleep 6
& "$PSScriptRoot\close_message_log.ps1" | Out-Null
& "$PSScriptRoot\ue_py.ps1" -Script $Script -Marker $Marker -DonePattern "(PASS|FAIL \()" -TimeoutSec $TimeoutSec
