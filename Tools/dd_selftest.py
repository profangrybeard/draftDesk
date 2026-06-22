"""dd_selftest.py — the iterative drag-loop acceptance, by query (no screenshots, no human). Simulates the
author by moving/deleting ADraftDeskThreshold markers via MCP, runs SyncDrags, and asserts the gate stays
GREEN after each step of a SEQUENCE. Run from a reconciled-green baseline (dd_castle.py + dd_reconcile.py):

    python dd_selftest.py

Steps: SLIDE an interior door along its wall (must FOLD + hold + green); IDEMPOTENT re-run (no fold);
ENTRY slide (must be REFUSED — no layout float, peers don't move); DELETE an interior marker (must MERGE
to a passage + green). The engine ReconcileSerial must advance once per SyncDrags (proves the loop closed).

NOTE: the test sequence lives under main() / `if __name__ == "__main__"`, so importing this module for its
helpers (snapshot, move, remove, ...) does NOT run the test.
"""
import json
import math
import ddrun
import dd_drag
import dd_gate
import dd_castle

L = dd_castle.L

# ---- MCP helpers (engine truth) ---------------------------------------------
_READ = '''import json
FIND="editor_toolset.toolsets.scene.SceneTools.find_actors"
XF="editor_toolset.toolsets.actor.ActorTools.get_actor_transform"
GET="editor_toolset.toolsets.object.ObjectTools.get_properties"
CLS="{{THRESH}}"
GEN="{{GEN}}"
def run():
    acts=execute_tool(FIND,json.dumps({"name":"","tag":"","collision_channels":[],"actor_type":{"refPath":CLS}}))["returnValue"]
    out=[]
    for a in acts:
        ref=a["refPath"]
        xf=execute_tool(XF,json.dumps({"actor":{"refPath":ref}}))["returnValue"]
        pr=execute_tool(GET,json.dumps({"instance":{"refPath":ref},"properties":["Label","SourceThreshold"]}))["returnValue"]
        pj=json.loads(pr) if isinstance(pr,str) else pr
        out.append({"ref":ref,"label":str(pj.get("Label","")),"thr":pj.get("SourceThreshold",-1),
                    "x":xf["location"]["x"],"y":xf["location"]["y"],"z":xf["location"]["z"]})
    gp=execute_tool(GET,json.dumps({"instance":{"refPath":GEN},"properties":["ReconcileSerial","Openings"]}))["returnValue"]
    gj=json.loads(gp) if isinstance(gp,str) else gp
    return {"markers":out,"serial":gj.get("ReconcileSerial",-1),"openings":gj.get("Openings",[])}
'''


def snapshot():
    return ddrun.run_text(_READ)


def _of(o, *ks):
    for k in ks:
        if isinstance(o, dict) and k in o:
            return o[k]
    return None


def axis_of(openings, thr):
    for o in openings:
        if _of(o, "SourceThreshold", "sourceThreshold") == thr:
            return _of(o, "Axis", "axis")
    return None


def opening_pos(openings):
    """SourceThreshold -> (x, y) of the engine opening."""
    out = {}
    for o in openings:
        st = _of(o, "SourceThreshold", "sourceThreshold")
        p = _of(o, "Position", "position") or {}
        if st is not None:
            out[st] = (p.get("x", 0.0), p.get("y", 0.0))
    return out


def move(ref, x, y, z):
    s = '''import json
XF="editor_toolset.toolsets.actor.ActorTools.set_actor_transform"
def run():
    execute_tool(XF, json.dumps({"actor":{"refPath":%s},"xform":{"location":{"x":%f,"y":%f,"z":%f},
        "rotation":{"pitch":0,"yaw":0,"roll":0},"scale":{"x":1,"y":1,"z":1}}}))
    return {"ok":1}
''' % (json.dumps(ref), x, y, z)
    ddrun.run_text(s, substitute=False)


def remove(ref):
    s = '''import json
RM="editor_toolset.toolsets.scene.SceneTools.remove_from_scene"
def run():
    execute_tool(RM, json.dumps({"actor":{"refPath":%s}}))
    return {"ok":1}
''' % json.dumps(ref)
    ddrun.run_text(s, substitute=False)


def find(markers, label):
    for m in markers:
        if m["label"] == label:
            return m
    return None


def main():
    results = []

    def check(step, cond, detail=""):
        results.append((step, cond, detail))
        print(f"  [{'PASS' if cond else 'FAIL'}] {step}{(' - ' + detail) if detail else ''}")

    print("=== DRAG-LOOP SELF-TEST (simulated drags, green-by-query) ===")
    snap0 = snapshot()
    print(f"baseline: {len(snap0['markers'])} markers, serial={snap0['serial']}")

    # 1) SLIDE an interior door along its wall by +120, must FOLD and the marker must HOLD.
    door = next((m for m in snap0["markers"] if "-" in m["label"] and m["label"] != "entry" and m["thr"] >= 0), None)
    ax = axis_of(snap0["openings"], door["thr"]) if door else None
    print(f"\n[1] SLIDE  {door['label']} (thr {door['thr']}, axis {ax})")
    DV = 120.0
    nx, ny = (door["x"], door["y"] + DV) if ax == 0 else (door["x"] + DV, door["y"])
    move(door["ref"], nx, ny, door["z"])
    rep = dd_drag.sync()
    snap1 = snapshot()
    d2 = find(snap1["markers"], door["label"])
    held = d2 and math.hypot(d2["x"] - nx, d2["y"] - ny) < 5.0
    check("1.slide folded", (rep or {}).get("folded", 0) >= 1, f"report={rep}")
    check("1.marker held at drag", bool(held), (f"want=({nx:.0f},{ny:.0f}) got=({d2['x']:.0f},{d2['y']:.0f})" if d2 else "marker gone"))
    check("1.serial advanced", snap1["serial"] == snap0["serial"] + 1, f"{snap0['serial']}->{snap1['serial']}")
    check("1.gate green", dd_gate.gate(L))

    # 2) IDEMPOTENT: SyncDrags again, no new drag -> zero folds.
    print("\n[2] IDEMPOTENT re-sync")
    rep2 = dd_drag.sync()
    check("2.no fold", (rep2 or {}).get("folded", 0) == 0, f"report={rep2}")

    # 3) ENTRY slide must be REFUSED (no layout float: a peer marker must NOT move).
    print("\n[3] ENTRY slide refused")
    snapA = snapshot()
    entry = find(snapA["markers"], "entry")
    peer = find(snapA["markers"], door["label"])
    move(entry["ref"], entry["x"], entry["y"] + 150.0, entry["z"])
    dd_drag.sync()
    snapB = snapshot()
    peerB = find(snapB["markers"], door["label"])
    peer_still = peerB and math.hypot(peerB["x"] - peer["x"], peerB["y"] - peer["y"]) < 5.0
    check("3.peer did not move (no origin float)", bool(peer_still),
          (f"{(round(peer['x']),round(peer['y']))}->{(round(peerB['x']),round(peerB['y']))}" if peerB else "peer gone"))
    check("3.gate green", dd_gate.gate(L))

    # 4) DELETE an interior marker -> wall dissolves to a Passage, gate stays green.
    print("\n[4] DELETE interior marker -> merge to passage")
    snapC = snapshot()
    victim = next((m for m in snapC["markers"] if m["label"] != "entry" and m["label"] != door["label"]
                   and "-" in m["label"] and m["thr"] >= 0), None)
    remove(victim["ref"])
    rep4 = dd_drag.sync()
    check("4.merged", (rep4 or {}).get("merged", 0) >= 1, f"report={rep4}")
    check("4.gate green", dd_gate.gate(L))

    n_ok = sum(1 for _, c, _ in results if c)
    print(f"\n==== SELF-TEST: {n_ok}/{len(results)} checks pass ====")
    return n_ok == len(results)


if __name__ == "__main__":
    raise SystemExit(0 if main() else 1)
