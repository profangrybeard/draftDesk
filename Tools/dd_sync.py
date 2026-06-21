"""dd_sync — marker -> geometry sync. The heart of threshold-first authoring: the author drags
ADraftDeskThreshold markers, runs this, and the layout updates around where the markers landed,
rebuilds, and re-runs the nav gate.

  python dd_sync.py            # sync the example layout (dd_castle)
  python dd_sync.py my_layout

Per connection (matched to its threshold by LABEL, never proximity):
  * SLIDE   (Stage A) — Position = the along-wall offset of the dragged marker.
  * RESIZE  (Stage A) — Width/Height from the marker (0 = keep the layout/generator default).
  * MERGE   (Stage A) — a deleted interior marker -> Kind=Passage at full width (the wall dissolves);
            a deleted entry/exterior marker is REFUSED (R1 one-entry invariant).
  * RESHAPE (Stage B) — a marker dragged PERPENDICULAR off its wall MOVES the wall: both abutting
            rooms' facing edges follow it (shared-face-follows-both), keeping the one-cell gap. Gated:
            the move is reverted if FaceConnection no longer resolves a positive overlap or a room goes
            degenerate. Geometry is re-applied and the NAV gate re-runs, so a reshape only sticks if the
            layout stays watertight + walkable.
"""
import importlib
import json
import sys

import ddrun
import dd_config as C
import dd_anchor
import dd_navcheck
from dd_navcheck import _shift

layout_mod = sys.argv[1] if len(sys.argv) > 1 else "dd_castle"
L = importlib.import_module(layout_mod).L
seeds = dd_anchor.seeds(L)
dx, dy, _ = _shift(L)
T = L.wall
GRID = float(getattr(L, "snap", 50.0) or 50.0)


def snap(v):
    return round(v / GRID) * GRID if GRID > 0 else v


# --- read the placed markers: label + world transform + Kind/Width/Height ---
READ = '''import json
FIND = "editor_toolset.toolsets.scene.SceneTools.find_actors"
XF = "editor_toolset.toolsets.actor.ActorTools.get_actor_transform"
GET = "editor_toolset.toolsets.object.ObjectTools.get_properties"
CLS = "{{THRESH}}"
def run():
    acts = execute_tool(FIND, json.dumps({"name": "", "tag": "", "collision_channels": [],
        "actor_type": {"refPath": CLS}}))["returnValue"]
    out = []
    for a in acts:
        ref = a["refPath"]
        xf = execute_tool(XF, json.dumps({"actor": {"refPath": ref}}))["returnValue"]
        pr = execute_tool(GET, json.dumps({"instance": {"refPath": ref},
            "properties": ["Label", "Kind", "Width", "Height"]}))["returnValue"]
        pj = json.loads(pr) if isinstance(pr, str) else pr
        out.append({"label": pj["Label"] if "Label" in pj else "",
                    "x": xf["location"]["x"], "y": xf["location"]["y"], "z": xf["location"]["z"],
                    "kind": pj["Kind"] if "Kind" in pj else "",
                    "width": pj["Width"] if "Width" in pj else 0,
                    "height": pj["Height"] if "Height" in pj else 0})
    return {"markers": out}
'''
markers = ddrun.run_text(READ)["markers"]
mby = {m["label"]: m for m in markers}


def reshape(seed, perp):
    """Move the shared wall by `perp` (both rooms' facing edges follow). Returns True if it holds."""
    t = L.thresholds[seed["i"]]
    a, b = t["RoomA"], t["RoomB"]
    if b < 0:
        return False                                   # exterior: no neighbour to co-move (Stage B is interior)
    idx = seed["axis"]                                 # 0 = const-X wall -> move X edges; 1 -> move Y edges
    ra, rb = L.rooms[a], L.rooms[b]
    save = (ra["Min"], ra["Max"], rb["Min"], rb["Max"])
    P = seed["plane"] + perp                           # new wall centreline (local)

    def set_edge(r, key, value):
        pt = list(r[key]); pt[idx] = value; r[key] = (pt[0], pt[1])

    a_low = (ra["Min"][idx] + ra["Max"][idx]) < (rb["Min"][idx] + rb["Max"][idx])
    if a_low:                                          # A on the - side: A.max and B.min straddle P
        set_edge(ra, "Max", P - T / 2); set_edge(rb, "Min", P + T / 2)
    else:
        set_edge(ra, "Min", P + T / 2); set_edge(rb, "Max", P - T / 2)

    s2, rooms2 = dd_anchor._shell(L)                   # gate: facing still resolves + rooms non-degenerate
    fc = s2.face_connection(a, b)
    ok = (fc is not None and (fc[4] - fc[3]) > 1
          and rooms2[a].W() > 1 and rooms2[a].D() > 1 and rooms2[b].W() > 1 and rooms2[b].D() > 1)
    if not ok:
        ra["Min"], ra["Max"], rb["Min"], rb["Max"] = save
    return ok


def default_w(kind):
    return 240.0 if kind in ("Doorway", "Window") else 200.0


moved, resized, merged, reshaped, kept, rejected, past_wall = [], [], [], [], [], [], []
for s in seeds:
    t = L.thresholds[s["i"]]
    m = mby.get(s["label"])
    if m is None:                                      # marker deleted
        if t["bIsEntry"] or t["RoomB"] < 0:
            kept.append(s["label"]); continue          # entry/exterior: R1 -> refuse
        t["Kind"] = "Passage"; t["Width"] = max(0.0, (s["hi"] - s["lo"]) - 2 * T); t["Height"] = 0.0
        merged.append(s["label"]); continue

    lx, ly = m["x"] + dx, m["y"] + dy                  # marker world -> local
    position, perp = dd_anchor.project(s, lx, ly)

    # Stage B (perpendicular -> move the wall) is INTERIOR only; the entry never grows its wall (R1).
    if t["RoomB"] >= 0 and not t["bIsEntry"] and abs(perp) > GRID * 0.5:
        if reshape(s, snap(perp)):
            reshaped.append((s["label"], round(snap(perp))))
        else:
            rejected.append((s["label"], round(perp)))

    # Clamp the slide onto the wall span: a door can't slide off its wall, and the ENTRY must stay on
    # its wall or the origin/PlayerStart strands off the navmesh. Dragging past the end = wanting a
    # longer wall (an along-axis reshape, not built) -> clamp + report.
    weff = t["Width"] if t["Width"] > 0 else default_w(t["Kind"])
    limit = max(0.0, (s["hi"] - s["lo"]) / 2.0 - weff / 2.0)
    clamped = max(-limit, min(limit, position))
    if abs(position - clamped) > 1:
        past_wall.append((s["label"], round(position), round(clamped)))
    t["Position"] = snap(clamped)                      # Stage A: slide (clamped to the wall)
    if m["kind"]:
        t["Kind"] = m["kind"]
    if m["width"] and m["width"] > 0:
        t["Width"] = snap(m["width"]); resized.append((s["label"], round(t["Width"])))
    if m["height"] and m["height"] > 0:
        t["Height"] = m["height"]
    if abs(position) > 2:
        moved.append((s["label"], round(snap(position))))

print("moved (slide):", moved)
print("resized:", resized)
print("reshaped (Stage B — wall moved):", reshaped)
print("reshape REJECTED (would break the connection — reverted):", rejected)
print("past wall (label, dragged-to, clamped-to — wants a longer wall, not slide):", past_wall)
print("merged (deleted -> passage):", merged)
print("kept (entry/exterior deletion refused, R1):", kept)

L.write_apply("_apply.py", gen=C.GEN)
print("apply:", ddrun.run("_apply.py"))
print()
dd_navcheck.check_connections(L)   # the gate: every connection still traversable
print()
dd_navcheck.check(L)

import dd_save                          # persist the synced layout + markers so the drag survives a restart
print("saved:", dd_save.save())
