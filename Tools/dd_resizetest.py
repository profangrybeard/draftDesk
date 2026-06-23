"""dd_resizetest.py — SCALE-gizmo room resize acceptance by query. Scale a room handle and the room's
footprint resizes about its centre, with BOTH walls landing on the grid (so the authored frame the gate
reads stays identical to the engine's built frame -- the cross-frame corruption the adversarial pass
found). The scale is consumed (reset to 1), so a second sync re-resizes nothing. Run with the castle
loaded + reconciled-green:

    python dd_resizetest.py
"""
import json
import ddrun
import dd_drag
import dd_gate
import dd_engine
from dd_roomtest import read_handles


def move_scale(ref, x, y, z, sx, sy, sz):
    s = ('import json\n'
         'XF="editor_toolset.toolsets.actor.ActorTools.set_actor_transform"\n'
         'def run():\n'
         f'    execute_tool(XF, json.dumps({{"actor":{{"refPath":{json.dumps(ref)}}},"xform":{{'
         f'"location":{{"x":{x},"y":{y},"z":{z}}},"rotation":{{"pitch":0,"yaw":0,"roll":0}},'
         f'"scale":{{"x":{sx},"y":{sy},"z":{sz}}}}}}}))\n'
         '    return {"ok":1}\n')
    ddrun.run_text(s, substitute=False)


def _ongrid(v, g=50.0):
    m = abs(v) % g
    return m < 0.01 or abs(m - g) < 0.01


def main():
    results = []

    def check(s, c, d=""):
        results.append(c); print(f"  [{'PASS' if c else 'FAIL'}] {s}{(' - ' + d) if d else ''}")

    print("=== RESIZE (scale gizmo) SELF-TEST ===")
    L = dd_engine.layout()
    entry = next((t["RoomA"] for t in L.thresholds if t["bIsEntry"]), -1)
    R = next((i for i in range(len(L.rooms)) if i != entry), 0)
    r0 = L.rooms[R]
    w0, d0 = r0["Max"][0] - r0["Min"][0], r0["Max"][1] - r0["Min"][1]
    print(f"resize room {R}: W={w0:.0f} D={d0:.0f}  -> scale 1.3x about centre")

    h = next((x for x in read_handles() if x["ri"] == R), None)
    if not h:
        print(f"  no handle for room {R}"); return False

    move_scale(h["ref"], h["x"], h["y"], h["z"], 1.3, 1.3, 1.0)
    rep = dd_drag.sync()
    check("1.report RoomResized >= 1", (rep or {}).get("roomResized", 0) >= 1, f"roomResized={(rep or {}).get('roomResized')}")

    L2 = dd_engine.layout()
    r2 = L2.rooms[R]
    w2, d2 = r2["Max"][0] - r2["Min"][0], r2["Max"][1] - r2["Min"][1]
    check("2.W/D grew about centre", w2 > w0 and d2 > d0, f"{w0:.0f}x{d0:.0f} -> {w2:.0f}x{d2:.0f}")
    faces = (r2["Min"][0], r2["Max"][0], r2["Min"][1], r2["Max"][1])
    check("3.all 4 faces ON-GRID (snap-faces fix, no cross-frame drift)", all(_ongrid(v) for v in faces),
          f"min/max=({faces[0]:.0f},{faces[2]:.0f})..({faces[1]:.0f},{faces[3]:.0f})")

    # 2nd sync with NO new scale -> nothing re-resizes (the scale was consumed / reset to 1)
    rep2 = dd_drag.sync()
    check("4.idempotent (scale consumed, no phantom resize)", (rep2 or {}).get("roomResized", 0) == 0,
          f"roomResized={(rep2 or {}).get('roomResized')}")

    n = sum(1 for c in results if c)
    print(f"\n==== RESIZE: {n}/{len(results)} checks pass ====")
    return n == len(results)


if __name__ == "__main__":
    raise SystemExit(0 if main() else 1)
