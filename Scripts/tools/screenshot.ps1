param([Parameter(Mandatory)] [string]$Out, [string]$Window = "")
# Saves a JPEG of the editor (Bat's / the first PIE player's view), or of a window whose title contains $Window
# (e.g. "Client 1" for the second PIE player)
$tmp = Join-Path ([IO.Path]::GetTempPath()) "ourlastecho_capture.txt"
if ($Window) {
    $s = & "$PSScriptRoot\mcp.ps1" -Tool Snapshot -Toolset SlateInspectorToolset.SlateInspectorToolset -ArgsJson '{"ref":"","maxDepth":1}'
    $ref = [regex]::Match($s, 'window \\"[^\]*' + [regex]::Escape($Window) + '[^\]*\\" \[[^\]]*\] \[ref=(w\d+)\]').Groups[1].Value
    if (-not $ref) { "window '$Window' not found"; exit 1 }
    & "$PSScriptRoot\mcp.ps1" -Tool Screenshot -Toolset SlateInspectorToolset.SlateInspectorToolset -ArgsJson ('{"ref":"' + $ref + '"}') | Out-File $tmp -Encoding utf8
} else {
    & "$PSScriptRoot\mcp.ps1" -Tool CaptureEditorImage -Toolset EditorToolset.EditorAppToolset -ArgsJson '{}' | Out-File $tmp -Encoding utf8
}
& "$PSScriptRoot\decode_img.ps1" -In $tmp -Out $Out
