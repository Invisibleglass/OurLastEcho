param([Parameter(Mandatory)] [string]$Command)
# Types a console command into the open editor's status-bar console (the textbox after the "Cmd" label)
$py = @'
import json, re
CMD = __CMD__
def run():
    snap = execute_tool('SlateInspectorToolset.SlateInspectorToolset.Snapshot', json.dumps({'ref': '', 'maxDepth': 60}))['returnValue']
    lines = snap.split('\n')
    ref = None
    for i, l in enumerate(lines):
        if 'text "Cmd"' in l:
            for l2 in lines[i:i + 6]:
                m = re.search(r'textbox .*\[ref=(\w+)\]', l2)
                if m:
                    ref = m.group(1)
                    break
        if ref:
            break
    if not ref:
        return {'error': 'console textbox not found'}
    execute_tool('SlateInspectorToolset.SlateInspectorToolset.Click', json.dumps({'ref': ref}))
    ok = execute_tool('SlateInspectorToolset.SlateInspectorToolset.Type', json.dumps({'ref': ref, 'text': CMD, 'submit': True}))['returnValue']
    return {'ref': ref, 'typed': ok}
'@
$py = $py.Replace("__CMD__", ($Command | ConvertTo-Json -Compress))
$argsJson = @{ script = $py } | ConvertTo-Json -Compress
& "$PSScriptRoot\mcp.ps1" -Tool execute_tool_script -Toolset editor_toolset.toolsets.programmatic.ProgrammaticToolset -ArgsJson $argsJson
