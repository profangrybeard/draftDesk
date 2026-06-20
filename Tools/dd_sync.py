"""dd_sync — the marker -> geometry sync (Stage A). The heart of threshold-first authoring:
the author drags ADraftDeskThreshold markers in the editor, then runs this, and the geometry
rebuilds around where the markers landed.

  python dd_sync.py            # syncs the example layout (dd_castle)
  python dd_sync.py my_layout  # syncs a layout module you wrote (my_layout.py at this folder)

Per connection (matched to its link by LABEL, never by proximity):
  * SLIDE   — Position = the along-wall component of (marker - seed); the axis field picks along/perp.
  * RESIZE  — Width/Height from the marker; a 0 means "unspecified" -> keep the layout/generator default.
  * DELETE  — interior link -> MERGE (Kind=Open at full shared-wall width, the wall dissolves);
              entry/exterior link -> REFUSED (deleting it would break the one-entry invariant, R1).
  * off-wall (perpendicular) drag -> slide along only; perpendicular reshape is Stage B (not built).
The ENGINE clamps every opening onto the shared wall, so we pass the RAW offset and only REPORT
which markers sit past the wall (those want a Stage B reshape).
"""
import importlib
import json
import os
import sys

import ddrun
import dd_config as C

layout_mod = sys.argv[1] if len(sys.argv) > 1 else "dd_castle"
L = importlib.import_module(layout_mod).L
seed = L.threshold_points()

markers = ddrun.run("sandbox/read_markers.py")["markers"]
mby = {m["label"]: m for m in markers}

# Effective opening sizes for the "past wall" report: a 0 width means "use the generator default",
# so size the door by that default, not by 0. Live values read from the Spec; fall back to defaults.
META = {"doorWidth": 240.0, "doorHeight": 200.0, "corridorWidth": 200.0}
try:
    d = ddrun.run("sandbox/read_door.py")
    for k in ("doorWidth", "doorHeight", "corridorWidth"):
        if d.get(k):
            META[k] = d[k]
except Exception:
    pass
def default_w(kind):
    return META["doorWidth"] if kind in ("Doorway", "Window") else META["corridorWidth"]

moved, merged, off_wall, past_wall, kept_entry = [], [], [], [], []
for s in seed:
    link = L.links[s["i"]]
    m = mby.get(s["label"])
    if m is None:                              # marker deleted
        if link.get("bIsEntry") or link.get("RoomB", -1) < 0:
            kept_entry.append(s["label"])      # entry/exterior: no neighbour to merge into (R1) -> keep
            continue
        link["Kind"] = "Open"                  # interior deletion -> MERGE the two spaces
        link["Width"] = max(0.0, s["overlap"] - 2 * L.wall)
        link["Height"] = 0.0
        merged.append(s["label"])
        continue
    along = "y" if s["axis"] == 0 else "x"     # axis 0 = constant-X wall -> slide in Y; else slide in X
    perp = "x" if s["axis"] == 0 else "y"
    pos = m[along] - s[along]
    width = m["width"] if m["width"] > 0 else s["width"]
    height = m["height"] if m["height"] > 0 else s["height"]
    eff = width if width > 0 else default_w(m["kind"])
    limit = max(0.0, s["overlap"] / 2.0 - eff / 2.0)
    if abs(pos) > limit + 1:
        past_wall.append((s["label"], round(pos), round(max(-limit, min(limit, pos)))))
    if abs(m[perp] - s[perp]) > 5:
        off_wall.append(s["label"])
    if abs(pos) > 2 or m["kind"] != s["kind"] or abs(width - s["width"]) > 1:
        moved.append((s["label"], round(pos)))
    link["Position"] = pos
    link["Kind"] = m["kind"]
    link["Width"] = width
    link["Height"] = height

L.write_apply("_apply.py", gen=C.GEN)
result = ddrun.run("_apply.py")
print("apply:", result)
print("moved:", moved)
print("merged (deleted -> open):", merged)
print("off-wall (perpendicular drag; reshape is Stage B):", off_wall)
print("past wall (label, requested, engine-clamps-to) -> wants Stage B reshape:", past_wall)
print("kept (entry/exterior deletion refused -> R1 one-entry invariant):", kept_entry)
