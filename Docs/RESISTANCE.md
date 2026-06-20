# draftDesk — Resistance

The friction. Every entry is a real trap hit during development, written as **symptom → cause →
fix** so a fresh session on a new PC recovers in minutes instead of rediscovering it. These are the
things that fight you; the design in [RULES.md](RULES.md) exists to defend against most of them.

## Build & editor

- **Build fails with LNK1104 (DLL locked) / "cannot open UnrealEditor-DraftDesk.dll".**
  Cause: the editor (or, after a crash, `CrashReportClientEditor.exe`) holds the plugin DLL.
  Fix: **close the editor before building.** After a crash, `taskkill //PID <pid> //F` the
  `CrashReportClientEditor.exe` (or `UnrealEditor.exe`) that's holding it, then rebuild.
- **The MCP server is unreachable / `ddrun` says "no MCP session — is the editor open?"**
  Cause: the in-editor MCP server (127.0.0.1:8000/mcp) is **only alive while the editor is open**.
  Fix: open the editor (wait for it to finish loading), then retry. This is also why the
  build/verify loop is *close → build → reopen → drive*.
- **Locally-compiled UE DLLs blocked from loading.**
  Cause: Windows **Smart App Control** quarantines unsigned locally-built DLLs.
  Fix: it was turned off on the dev machine. If plugin code builds but won't load, check it.
- **Build command (editor closed):**
  `"<UE>/Engine/Build/BatchFiles/Build.bat" <Project>Editor Win64 Development -Project="<...>.uproject" -WaitMutex`

## MCP / property gotchas

- **A marker's `set_properties` value silently doesn't stick (reads back 0 / the default) later.**
  Cause: **marker write-persistence flakiness** — a `set_properties` on an `ADraftDeskThreshold`
  sometimes reverts across calls (it *does* apply in-call). This (not casing) is what left 14/18
  seeded markers at width 0. NOTE: `set_properties` is **case-insensitive** (`Width` and `width` both
  apply, verified) — do *not* "fix" this by changing key casing; that was a misdiagnosis. Fix:
  `python dd_genrepair.py` backfills marker dims; and `dd_sync` already treats a 0 as "unspecified"
  and falls back to the layout/metric default, so geometry is correct regardless.
- **A property read comes back with the "wrong" casing / a key-lookup misses.**
  Cause: `get_properties` OUTPUT is **PascalCase for top-level** UPROPERTYs but
  **first-char-lowercase for nested struct fields**. Fix: read top-level as `Width`, nested metric
  fields as `doorWidth`.
- **Reading `Metrics` off the generator errors.**
  Cause: metrics live in the **Spec data asset**, not on the generator. Fix: read `generator.Spec`,
  then the Spec's `Metrics`.
- **Editor crash: "Array index out of bounds: 0 into an array of size 0" on apply.**
  Cause: `set_properties` applies arrays one at a time, each triggering a rebuild, so Links can be
  set while Rooms is momentarily empty. Fix: the fixed **apply order** (clear connections → clear
  rooms → set rooms → set links) + bounds-checked link indices. `write_apply()` already does this.
- **Spurious "broken threshold / unresolved link" warnings during an apply.**
  Cause: the same transient (links present before rooms). It cried wolf and misled debugging. Fix:
  the apply order eliminates it; to read the *true* count, force one clean rebuild and measure the
  delta. (Now moot: markers are loud-but-rare via the inline `CarveOpenings` warning.)

## Transport (the `ddrun` runner)

- **A tool call returns an empty / unparseable response via Python urllib.**
  Cause: this MCP server streams the tool result as `text/event-stream`, and `urllib` does **not**
  drain that stream. Fix: `ddrun.py` shells out to **`curl`** (Windows 11 ships `curl.exe`;
  git-bash also has it), which handles the SSE. Don't "simplify" it back to urllib.
- **A sandbox script errors with `NameError: null` / `name 'true' is not defined`.**
  Cause: you injected a value into the sandbox script text with `json.dumps(...)`, which emits JSON
  literals (`null`/`true`/`false`) that aren't valid Python. Fix: inject Python literals with
  `repr(...)` (or `json.loads` *inside* the sandbox script). `dd_cap.py` uses `repr(region)`.
- **The UE-sandbox Python is restricted.** No `.get()` on the tool-result dicts; imports limited to
  `json`/`re`/`math`/`datetime`/`copy`/`time`. Write sandbox scripts with `["key"] if "key" in d
  else default`, not `d.get(...)`. (System-python scripts have no such limits.)

## Geometry / authoring

- **A door is silently walled off (the viability-killer).**
  Cause (historical): the old carve resolved one wall by a strict 1cm abutment check; widen a room
  10cm and the door sealed. Fix: the **connection guarantee** (tolerant `FaceConnection` +
  carve-both-walls). A declared link can no longer become a solid wall. Don't reintroduce a strict
  abutment gate.
- **A door cuts through the top of the wall.**
  Cause: a `DoorHeight` taller than the wall `Height` (saw a corrupted `doorHeight = 465` against
  300 walls). Fix: keep door height under the wall height; the lintel clamp (`Height ≤ WallH−40`)
  guards non-full-clear openings. Metrics can carry junk from prior sessions — sanity-check them.
- **A door slid past the end of its wall / a too-wide door overflows.**
  Cause: an extreme authored Position/Width. Not a bug anymore — the **opening clamp** keeps it on
  the shared wall (clamps Position, caps Width to the overlap). `dd_sync` reports such a marker as
  "past wall → wants Stage B reshape" rather than moving the wall (Stage B isn't built).
- **A double wall or a thin sliver between two rooms.**
  Cause: a missing or wrong abutment gap (must be exactly `T`). Fix: use `east_of`/`north_of` (they
  place the gap), or hand-place with `A.Max + T == B.Min`.
- **Deleting an entry marker would strand the level.**
  Cause: an entry/exterior link has no neighbour to merge into; dropping it breaks the one-entry
  invariant (R1) and re-anchors the origin. Fix: `dd_sync` **refuses** to merge-delete a
  `bIsEntry`/exterior link and reports it under `kept`.

## Recovery quick-reference

| Symptom | First thing to try |
|---|---|
| Build won't link | Close editor; `taskkill` CrashReportClientEditor; rebuild |
| `ddrun` can't connect | Is the editor open and finished loading? Is the MCP server plugin enabled? |
| `ddrun` errors in PowerShell | `curl` is aliased to Invoke-WebRequest — use cmd/git-bash or real `curl.exe` |
| `ImportError: dd_config` | Run from inside the `Tools/` directory |
| Editor crashed on apply | Use `write_apply()` order; it's already correct in the harness |
| Door missing/blocked | Confirm the link exists; the engine guarantees a carve — check RoomA/RoomB indices |
| Markers read back 0 | `python dd_genrepair.py` (write-persistence flakiness, not casing) |
