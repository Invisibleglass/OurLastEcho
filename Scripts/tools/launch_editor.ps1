param([switch]$Wait)
# Starts the editor on this project. -DDC=NoZenLocalFallback keeps the derived-data cache on plain files: on this
# machine the default local Zen cache server crashed the editor (an assertion in DerivedDataRequestOwner.cpp, inside
# its HTTP code) during shader compiles and PIE starts.
$Project = (Resolve-Path (Join-Path $PSScriptRoot "..\..\OurLastEcho.uproject")).Path
Start-Process "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"$Project`" -DDC=NoZenLocalFallback"
if (-not $Wait) { exit 0 }
# Wait for the MCP server, then turn off background CPU throttling (it resets on every start)
for ($i = 0; $i -lt 90; $i++) {
    Start-Sleep 5
    $r = & "$PSScriptRoot\mcp.ps1" -Tool IsPIERunning -Toolset EditorToolset.EditorAppToolset -ArgsJson '{}' -TimeoutSec 10 2>$null
    if ($r -match "returnValue") { break }
}
& "$PSScriptRoot\mcp.ps1" -Tool set_properties -Toolset editor_toolset.toolsets.object.ObjectTools -ArgsJson '{"instance":{"refPath":"/Script/UnrealEd.Default__EditorPerformanceSettings"},"values":"{\"bThrottleCPUWhenNotForeground\":false}"}' | Out-Null
"editor ready"
