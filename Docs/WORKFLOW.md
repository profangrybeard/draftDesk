# draftDesk — Workflow

How to operate the kit end to end on a fresh machine: prerequisites, the build cycle, and the
threshold-first authoring loop. The runnable harness is in [`../Tools/`](../Tools) (see its
[README](../Tools/README.md) for the per-command reference).

## Prerequisites

A fully set-up machine: UE 5.8, a host project with the plugin built, the in-editor MCP server
enabled (`127.0.0.1:8000/mcp`, alive only while the editor is open), the scene objects placed
(generator + spec + nav volume + PlayerStart), and the harness configured (`Tools/dd_config.py` →
`GEN`/`SPEC`; run scripts from inside `Tools/`). **From a fresh machine, do [SETUP.md](SETUP.md)
first** — it covers all of the above from zero.

## The two cycles

**Code cycle (when you change C++):** edit → **close the editor** → `Build.bat …Editor Win64
Development` → reopen → drive. (The DLL is locked while the editor is open; see
[RESISTANCE.md](RESISTANCE.md).)

**Authoring cycle (no rebuild):** the generator rebuilds live on every property change, so authoring
is just: push a layout / drag markers → sync → screenshot. No editor restart.

## The threshold-first authoring loop

This is the core loop, matching the [philosophy](GOALS.md): *plan connections, fill spaces.*

1. **Dictate or sketch a layout.** Describe it in LD vocabulary; compose it with the `Layout`
   library ([`Tools/dd_author.py`](../Tools/dd_author.py)) — `room`, `corridor`, `east_of`/
   `north_of` (auto-abutment), `link`, `entry`, `exterior`, `stair`, `box`/`cover`/`pillar`/`dais`.
   [`Tools/dd_castle.py`](../Tools/dd_castle.py) is a full worked example — copy it as a template.
2. **Push it.** `python dd_castle.py` (or `python -c "import my_layout, ddrun; …"`) writes the
   `Custom` payload and applies it via `ddrun`. The greybox builds instantly and the nav re-fits.
3. **Seed thresholds.** `python dd_seedmarkers.py [layout]` spawns one movable `ADraftDeskThreshold`
   marker per connection, at its resolved location.
4. **Drag.** In the editor, move the markers to where the connections *should* be. (Delete one to
   merge two spaces; deleting an entry is refused.)
5. **Sync.** `python dd_sync.py [layout]` reads the moved markers and rebuilds the geometry around
   them — slide (along-wall position), resize (width/height), merge (deleted interior link). It
   prints what moved, what merged, and which markers sit *past the wall* (those want a Stage B
   reshape, not yet built). The engine clamps every opening onto its wall, so nothing falls off.
6. **Look.** `python dd_cap.py out.png [cx cy radius]` saves a top-down screenshot (whole generator,
   or a framed region) without dumping base64 to the terminal.

Re-run 4–6 as many times as you like; the sync always rebuilds from the canonical layout baseline +
the current marker positions, so it's idempotent.

## Verifying the engine (clamp / robustness)

`python dd_stress.py <scenario>` drives extreme values straight into a link (no markers) to prove
the durable clamp: `slidefar` / `slideneg` (Position = ±100000 → door clamps flush to the wall end),
`toowide` (Width = 100000 → fills the shared wall, no overflow), `mergeuneq`, `reset`. Pair with
`dd_cap.py` to eyeball the result.

## Where things live

- **Engine:** `Source/DraftDesk/` — headers in `Public/`, sources in `Private/`:
  `DraftDeskMetrics.h` (schema), `DraftDeskGenerator.h` + `Private/DraftDeskGenerator.cpp`
  (build-on-edit actor + the whole pipeline), `DraftDeskThreshold.h/.cpp` (the movable marker
  actor), `DraftDeskStatics.h/.cpp` (locomotion apply), `DraftDeskLayout.h` (enums + USTRUCTs).
- **Harness:** `Tools/` — see [Tools/README.md](../Tools/README.md).
- **Reference:** `Docs/` — [GOALS](GOALS.md), [RULES](RULES.md), [RESISTANCE](RESISTANCE.md),
  [LD_RULES](LD_RULES.md), [LD_Metrics.json](LD_Metrics.json).
