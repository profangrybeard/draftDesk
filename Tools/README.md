# draftDesk — Tools (authoring harness)

The Python harness that drives the in-editor `ADraftDeskGenerator` over the MCP server: compose a
layout, push it, seed/read movable threshold markers, sync geometry to them, screenshot, and
stress-test. This is the operational layer of [threshold-first authoring](../Docs/GOALS.md); the
full loop is in [../Docs/WORKFLOW.md](../Docs/WORKFLOW.md).

## Setup (once per project/PC)

From a fresh machine, do **[../Docs/SETUP.md](../Docs/SETUP.md)** first (UE, host project, the
in-editor MCP server, scene objects). Then, with the editor open and the MCP server live
(`127.0.0.1:8000/mcp`):

1. **Run from inside this `Tools/` directory** — every script imports its siblings (`dd_config`,
   `ddrun`, `dd_author`), so the cwd must be `Tools/` or the imports fail.
2. Need: **Python 3.8+** (system, not the UE-sandbox one), **no pip packages** (stdlib only), and
   **real `curl`** on PATH (Windows 11 built-in, or git-bash). ⚠️ PowerShell aliases `curl` to
   `Invoke-WebRequest` — use cmd/git-bash or ensure `curl.exe` resolves.
3. **Edit [`dd_config.py`](dd_config.py)** — set `GEN` and `SPEC` for your project (the committed
   values are the author's example paths and **must be replaced**). That's the only file you change.
   Find your generator path with:
   ```
   python ddrun.py sandbox/find_generator.py
   ```

## Files

| File | What it is |
|---|---|
| `dd_config.py` | **The one config you edit** — `GEN`, `SPEC` (per project) + stable `THRESH`, `MCP_URL`. |
| `ddrun.py` | The transport. Substitutes `{{GEN}}`/`{{SPEC}}`/`{{THRESH}}` into a sandbox script and runs it via curl. Import `ddrun.run(path)` / `ddrun.run_text(text)`, or `python ddrun.py <script>`. |
| `dd_author.py` | The `Layout` library — compose rooms/links/stairs/boxes, emit the `Custom` payload, `write_apply()`. |
| `dd_castle.py` | Worked example layout (Castle Chorrol). `import` it for `.L`; run it to apply. **Copy as a template.** |
| `dd_sync.py` | **Stage A sync** — read moved markers → rebuild geometry (slide/resize/merge). The core loop. |
| `dd_seedmarkers.py` | Spawn one movable marker per connection. |
| `dd_genrepair.py` | Backfill markers whose width/height read back 0. |
| `dd_stress.py` | Drive extreme values into a link to verify the engine clamp. |
| `dd_cap.py` | Top-down screenshot to a PNG (whole generator or a framed region). |
| `sandbox/` | UE-sandbox scripts (run via `ddrun`): `find_generator`, `read_markers`, `read_door`. |

Generated/transient files (`_apply.py`, `*.png`, `__pycache__/`) are git-ignored.

## Quickstart — the loop

```bash
python dd_castle.py                 # build + apply the example castle (nav re-fits)
python dd_seedmarkers.py            # spawn a movable marker at every connection
#   ... drag the markers in the editor; delete one to merge two spaces ...
python dd_sync.py                   # rebuild geometry around the moved markers
python dd_cap.py out.png            # screenshot the result
```

Use your own layout by passing its module name: `python dd_sync.py my_layout` (with `my_layout.py`
in this folder, modeled on `dd_castle.py`).

## Notes

- **System-python vs UE-sandbox.** Scripts here run in *system* Python and may import freely. Files
  in `sandbox/` are shipped *as text* to the editor's restricted Python (no `.get()`; imports limited
  to json/re/math/datetime/copy/time) — they get config via `{{…}}` placeholders, not `import`.
- **Casing.** `set_properties` keys are case-insensitive (`width` and `Width` both apply); reads
  come back PascalCase for top-level props, lowercase for nested struct fields. See
  [../Docs/RULES.md](../Docs/RULES.md).
- **Gotchas & recovery:** [../Docs/RESISTANCE.md](../Docs/RESISTANCE.md).
