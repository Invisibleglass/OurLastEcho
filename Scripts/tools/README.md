# Editor-driving helpers (PowerShell)

Used when Claude Code's own connection to the editor's Unreal MCP server is down (e.g. the session started
before the editor did). They talk to the same server, `http://127.0.0.1:8000/mcp`, directly.

- `mcp.ps1 -Tool <short name> -Toolset <toolset> -ArgsJson '<json>'` calls one MCP tool (via the `call_tool` meta-tool)
  and prints its text result. Keeps the MCP session id in `%TEMP%\ourlastecho_mcp_session.txt` and re-initialises
  if the editor restarted.
- `ue_cmd.ps1 -Command '<console command>'` types a console command into the open editor's status-bar console
  (found fresh each time via `SlateInspectorToolset`).
- `ue_py.ps1 -Script <name in Scripts/ or full path> [-Marker ECHO_X -DonePattern '(PASS|FAIL)']` runs a Python script
  in the open editor with `py exec(open(...).read())` and waits for/prints its tagged log lines.
- `decode_img.ps1 -In <tool output file> -Out <jpg>` decodes a base64 screenshot from `CaptureViewport`,
  `CaptureEditorImage` or `SlateInspectorToolset.Screenshot` output.

Call them with `&` from a PowerShell prompt/script. Nesting `powershell -File ... -ArgsJson '{...}'` inside another shell strips the JSON's quotes.

Example, running the Milestone 3 test:
```
powershell -File Scripts/tools/mcp.ps1 -Tool StartPIE -Toolset EditorToolset.EditorAppToolset -ArgsJson '{"options":{"bSimulate":false,"playMode":"PlayMode_InViewPort","warmupSeconds":3}}'
powershell -File Scripts/tools/ue_py.ps1 -Script test_climb_live.py -Marker ECHO_CLIMB_TEST -DonePattern "(PASS|FAIL \()"
```
