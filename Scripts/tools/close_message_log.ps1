# Closes the Message Log window that opens at every PIE start. It floats over the middle of the viewport, so it
# covers CaptureEditorImage screenshots and catches the front-end test's mouse clicks on centred dialogs.
$s = & "$PSScriptRoot\mcp.ps1" -Tool Snapshot -Toolset SlateInspectorToolset.SlateInspectorToolset -ArgsJson '{"ref":"","maxDepth":1}'
$window = [regex]::Match($s, 'window \\"Message Log\\" \[[^\]]*\] \[ref=(w\d+)\]').Groups[1].Value
if (-not $window) { "no Message Log window"; exit 0 }
$t = & "$PSScriptRoot\mcp.ps1" -Tool Snapshot -Toolset SlateInspectorToolset.SlateInspectorToolset -ArgsJson ('{"ref":"' + $window + '","maxDepth":40}')
# The snapshot comes back as JSON text (escaped quotes); the window's own title-bar Close button
$button = [regex]::Match($t, 'button \\"Close\\" \[[^\]]*\] \[ref=(b\d+)\]').Groups[1].Value
if (-not $button) { "close button not found"; exit 1 }
& "$PSScriptRoot\mcp.ps1" -Tool Click -Toolset SlateInspectorToolset.SlateInspectorToolset -ArgsJson ('{"ref":"' + $button + '"}') | Out-Null
"closed the Message Log"
