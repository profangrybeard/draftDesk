"""ddrun.py — run a UE ProgrammaticToolset script against the in-editor draftDesk MCP server.

One source of config (dd_config.py), one runner. Before sending, it substitutes
{{GEN}} / {{SPEC}} / {{THRESH}} in the script text from dd_config, so UE-sandbox scripts (which
can't import anything) still get the project paths from the single config file.

Shell:   python ddrun.py sandbox/read_door.py
Code:    import ddrun;  data = ddrun.run("sandbox/read_door.py")   # -> the dict your run() returned

Transport: shells out to `curl` (Windows 11 ships curl.exe in System32; git-bash also has it).
We use curl, not Python's urllib, because this MCP server streams the tool result as
text/event-stream and urllib does not drain that stream — curl does. The editor must be open
(the server is only alive then); a missing session id almost always means it's closed.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

import dd_config as C

_BASE = ["curl", "-sS", "--max-time", "180",
         "-H", "Content-Type: application/json",
         "-H", "Accept: application/json, text/event-stream"]


def _parse_result(resp):
    s = resp.strip()
    if s.startswith("{"):
        d = json.loads(s)
    else:
        datas = re.findall(r"^data:\s*(.*)$", s, re.M)   # SSE framing: last data: line is the payload
        if not datas:
            raise RuntimeError(f"ddrun: empty/unparseable MCP response:\n{resp[:500]}")
        d = json.loads(datas[-1])
    if "error" in d:
        raise RuntimeError(json.dumps(d["error"], indent=2))
    content = d.get("result", {}).get("content")
    texts = [it.get("text", "") for it in content] if isinstance(content, list) else []
    raw = texts[-1] if texts else "{}"
    parsed = json.loads(raw)
    rv = parsed["returnValue"] if isinstance(parsed, dict) and "returnValue" in parsed else parsed
    return json.loads(rv) if isinstance(rv, str) else rv


def run_text(script_text, substitute=True):
    """Run raw script text; return the dict your run() returned (returnValue unwrapped + parsed)."""
    if substitute:
        script_text = (script_text.replace("{{GEN}}", C.GEN)
                       .replace("{{SPEC}}", C.SPEC)
                       .replace("{{THRESH}}", C.THRESH))
    init = {"jsonrpc": "2.0", "id": 1, "method": "initialize",
            "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                       "clientInfo": {"name": "ddrun", "version": "0"}}}
    call = {"jsonrpc": "2.0", "id": 2, "method": "tools/call",
            "params": {"name": "call_tool", "arguments": {
                "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
                "tool_name": "execute_tool_script", "arguments": {"script": script_text}}}}
    with tempfile.TemporaryDirectory() as td:
        hdr = os.path.join(td, "h.txt")
        req = os.path.join(td, "req.json")
        r = subprocess.run(_BASE + ["-D", hdr, "-o", os.devnull, "-X", "POST", C.MCP_URL, "-d", json.dumps(init)],
                           capture_output=True, text=True)
        sid = None
        if os.path.exists(hdr):
            for line in open(hdr, encoding="utf-8", errors="replace"):
                if line.lower().startswith("mcp-session-id:"):
                    sid = line.split(":", 1)[1].strip()
        if not sid:
            raise SystemExit(f"ddrun: no MCP session from {C.MCP_URL} — is the Unreal editor open?\n{r.stderr}")
        sess = _BASE + ["-H", "Mcp-Session-Id: " + sid]
        subprocess.run(sess + ["-o", os.devnull, "-X", "POST", C.MCP_URL,
                               "-d", json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"})],
                       capture_output=True, text=True)
        with open(req, "w", encoding="utf-8") as f:
            f.write(json.dumps(call))
        resp = subprocess.run(sess + ["-X", "POST", C.MCP_URL, "--data-binary", "@" + req],
                              capture_output=True, text=True).stdout
    return _parse_result(resp)


def run(path, substitute=True):
    """Run a script file; return the dict your run() returned."""
    with open(path, encoding="utf-8") as f:
        return run_text(f.read(), substitute)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("usage: python ddrun.py <script.py>")
    print(json.dumps(run(sys.argv[1]), indent=1))
