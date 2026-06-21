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
# The blocking grid: prefer the live Spec value, else the layout's own snap, else 50. Markers should be
# dragged onto this grid; whatever they land on, we round to it (precision, not artistic control).
GX = GY = GZ = float(getattr(L, "snap", 50.0) or 50.0)
try:
    d = ddrun.run("sandbox/read_door.py")
    for k in ("doorWidth", "doorHeight", "corridorWidth"):
        if d.get(k):
            META[k] = d[k]
    g = d.get("gridSnap")
    if isinstance(g, dict):
        GX, GY, GZ = float(g.get("x") or GX), float(g.get("y") or GY), float(g.get("z") or GZ)
except Exception:
    pass
def default_w(kind):
    return META["doorWidth"] if kind in ("Doorway", "Window") else META["corridorWidth"]
def snap(v, step):
    return round(v / step) * step if step > 0 else v

# Echo the grid the engine actually enforces (the Spec's GridSnap), up front — and flag drift if the
# layout was authored on a different one (markers always round to the live Spec grid, not the layout's).
print(f"engine grid (Spec.GridSnap): {GX:g}x{GY:g}x{GZ:g} cm")
_ls = float(getattr(L, "snap", 0) or 0)
if _ls and (abs(GX - _ls) > 1 or abs(GY - _ls) > 1):
    print(f"  ! layout authored on {_ls:g} but the Spec grid is {GX:g}x{GY:g} — markers round to the Spec grid.")

moved, merged, off_wall, past_wall, kept_entry, off_grid = [], [], [], [], [], []
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
    gstep = GY if s["axis"] == 0 else GX       # the grid along the slide axis
    raw_pos = m[along] - s[along]
    pos = snap(raw_pos, gstep)                 # round the marker onto the grid (the engine snaps too)
    raw_w = m["width"] if m["width"] > 0 else 0.0
    width = snap(raw_w, gstep) if raw_w > 0 else s["width"]
    raw_h = m["height"] if m["height"] > 0 else 0.0
    height = snap(raw_h, GZ) if raw_h > 0 else s["height"]
    # report EACH dimension the marker had off the grid (we round it on) so the designer sees exactly
    # what was nudged onto the grid: position (the drag) and any width/height resize. A 0 ("use the
    # default") never flags.
    rounded = []
    if abs(raw_pos - pos) > 1:
        rounded.append(f"pos {round(raw_pos)}->{round(pos)}")
    if raw_w > 0 and abs(raw_w - width) > 1:
        rounded.append(f"w {round(raw_w)}->{round(width)}")
    if raw_h > 0 and abs(raw_h - height) > 1:
        rounded.append(f"h {round(raw_h)}->{round(height)}")
    if rounded:
        off_grid.append((s["label"], rounded))
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
print("off-grid (rounded onto the grid):", off_grid)
print("off-wall (perpendicular drag; reshape is Stage B):", off_wall)
print("past wall (label, requested, engine-clamps-to) -> wants Stage B reshape:", past_wall)
print("kept (entry/exterior deletion refused -> R1 one-entry invariant):", kept_entry)
