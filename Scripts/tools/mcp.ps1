# Usage: powershell -File Scripts/tools/mcp.ps1 -Tool StartPIE -Toolset EditorToolset.EditorAppToolset -ArgsJson '{...}'
param(
    [Parameter(Mandatory)] [string]$Tool,        # short tool name, e.g. StartPIE
    [string]$Toolset = "",                        # e.g. EditorToolset.EditorAppToolset
    [string]$ArgsJson = "{}",
    [int]$TimeoutSec = 280
)
# Minimal Streamable-HTTP MCP client for the Unreal editor's MCP server (used when the Claude Code
# session's own connection to it is down). Calls the meta-tool call_tool and prints the result text.
$Url = "http://127.0.0.1:8000/mcp"
$SessionFile = Join-Path ([IO.Path]::GetTempPath()) "ourlastecho_mcp_session.txt"
$Headers = @{ "Accept" = "application/json, text/event-stream" }

function Invoke-Rpc($Body, $Session) {
    $h = $Headers.Clone()
    if ($Session) { $h["Mcp-Session-Id"] = $Session }
    $json = $Body | ConvertTo-Json -Depth 50 -Compress
    $resp = Invoke-WebRequest -Uri $Url -Method Post -Headers $h -ContentType "application/json" -Body ([Text.Encoding]::UTF8.GetBytes($json)) -TimeoutSec $TimeoutSec -UseBasicParsing
    return $resp
}

function Get-Session {
    if (Test-Path $SessionFile) { return (Get-Content $SessionFile -Raw).Trim() }
    $init = @{ jsonrpc = "2.0"; id = 1; method = "initialize"; params = @{ protocolVersion = "2025-06-18"; capabilities = @{}; clientInfo = @{ name = "claude-code-direct"; version = "1.0" } } }
    $r = Invoke-Rpc $init $null
    $sid = $r.Headers["Mcp-Session-Id"]
    if ($sid -is [array]) { $sid = $sid[0] }
    Invoke-Rpc @{ jsonrpc = "2.0"; method = "notifications/initialized" } $sid | Out-Null
    Set-Content $SessionFile $sid -NoNewline
    return $sid
}

function Parse-Body([string]$text) {
    # Either plain JSON or SSE ("data: {...}" lines)
    if ($text.TrimStart().StartsWith("{")) { return $text | ConvertFrom-Json }
    $data = ($text -split "`n" | Where-Object { $_ -like "data:*" } | ForEach-Object { $_.Substring(5).Trim() }) -join ""
    return $data | ConvertFrom-Json
}

$arguments = $ArgsJson | ConvertFrom-Json
$params = @{ name = "call_tool"; arguments = @{ tool_name = $Tool; arguments = $arguments } }
if ($Toolset) { $params.arguments.toolset_name = $Toolset }
$call = @{ jsonrpc = "2.0"; id = 2; method = "tools/call"; params = $params }

for ($attempt = 0; $attempt -lt 2; $attempt++) {
    $sid = Get-Session
    try {
        $r = Invoke-Rpc $call $sid
        $body = Parse-Body ([Text.Encoding]::UTF8.GetString($r.RawContentStream.ToArray()))
        if ($body.error) { Write-Output ("RPC ERROR: " + ($body.error | ConvertTo-Json -Depth 10 -Compress)); exit 1 }
        $texts = $body.result.content | Where-Object { $_.type -eq "text" } | ForEach-Object { $_.text }
        if ($body.result.isError) { Write-Output ("TOOL ERROR: " + ($texts -join "`n")); exit 1 }
        Write-Output ($texts -join "`n")
        exit 0
    } catch {
        # Stale session (editor restarted): start a new one once
        if ($attempt -eq 0 -and (Test-Path $SessionFile)) { Remove-Item $SessionFile; continue }
        Write-Output ("HTTP ERROR: " + $_.Exception.Message); exit 1
    }
}

