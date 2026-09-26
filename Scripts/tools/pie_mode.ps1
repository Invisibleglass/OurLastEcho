param(
    [ValidateSet("frontend", "gameplay")] [string]$Mode = "gameplay",
    [int]$Players = 2
)
# Switches Play-In-Editor between the project's two test setups and opens the matching map (stop PIE first):
#   frontend: TitleScreen, $Players players in Standalone net mode (each its own game, like separate copies of the
#             game on one network) for the menus, hosting and joining
#   gameplay: Lvl_SpiritPath, 2 players, Play As Listen Server (the project default) for the gameplay tests
$net = if ($Mode -eq "frontend") { "PIE_Standalone" } else { "PIE_ListenServer" }
$count = if ($Mode -eq "frontend") { $Players } else { 2 }
$values = "{\`"PlayNetMode\`":\`"$net\`",\`"PlayNumberOfClients\`":$count}"
& "$PSScriptRoot\mcp.ps1" -Tool set_properties -Toolset editor_toolset.toolsets.object.ObjectTools -ArgsJson ('{"instance":{"refPath":"/Script/UnrealEd.Default__LevelEditorPlaySettings"},"values":"' + $values.Replace('`', '') + '"}') | Out-Null
$map = if ($Mode -eq "frontend") { "TitleScreen" } else { "Lvl_SpiritPath" }
& "$PSScriptRoot\ue_cmd.ps1" -Command "py ECHO_PIE_MAP='/Game/Echo/Maps/$map'" | Out-Null
& "$PSScriptRoot\ue_py.ps1" -Script pie_mode.py -Marker "ECHO_PIE_MODE|LogPython: Error" -TimeoutSec 90
