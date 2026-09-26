param(
    [ValidateSet("title", "online")] [string]$Mode = "title",
    [int]$TimeoutSec = 300
)
# Runs Scripts/test_frontend_live.py in a fresh PIE session: stops PIE, sets the front-end PIE mode (1 player for
# "title", 2 for "online"), starts PIE, closes the Message Log window (it covers the viewport and catches clicks),
# then runs the test and prints its ECHO_FE_TEST lines
$players = if ($Mode -eq "online") { 2 } else { 1 }
& "$PSScriptRoot\mcp.ps1" -Tool StopPIE -Toolset EditorToolset.EditorAppToolset -ArgsJson '{}' | Out-Null
Start-Sleep 2
& "$PSScriptRoot\pie_mode.ps1" -Mode frontend -Players $players | Out-Null
& "$PSScriptRoot\mcp.ps1" -Tool StartPIE -Toolset EditorToolset.EditorAppToolset -ArgsJson '{"options":{"bSimulate":false,"playMode":"PlayMode_InViewPort","warmupSeconds":5}}' | Out-Null
Start-Sleep 4
& "$PSScriptRoot\close_message_log.ps1" | Out-Null
Start-Sleep 1
& "$PSScriptRoot\ue_cmd.ps1" -Command "py ECHO_FE_MODE='$Mode'" | Out-Null
& "$PSScriptRoot\ue_py.ps1" -Script test_frontend_live.py -Marker ECHO_FE_TEST -DonePattern "(PASS|FAIL \()" -TimeoutSec $TimeoutSec
