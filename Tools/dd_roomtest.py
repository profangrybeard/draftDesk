"""dd_roomtest.py — room-MOVE acceptance by query (no screenshots, no human). Simulates the author dragging
a ROOM HANDLE via MCP, runs SyncDrags, and asserts the ENGINE-TRUTH gate reacts correctly:

  pull a leaf room far off its only neighbour -> it DETACHES (gap > one wall, the clamped face_connection)
  -> unreachable -> gate RED; drag it back -> RECONNECTS -> gate GREEN.

Proves the translate + the gap-clamped detach + the live route end to end. Run from a reconciled-green
baseline (dd_castle.py + dd_reconcile.py), with the castle level loaded:

    python dd_roomtest.py

NOTE: set_actor_transform does NOT auto-fire the live OnActorMoved->SyncDrags (that is for editor gizmo
drags); this test calls dd_drag.sync() explicitly, exactly like dd_selftest. The test sequence lives under
main()/__main__, so importing for helpers does not run it.
"""
import math
import ddrun
import dd_drag
import dd_gate
import dd_engine
import dd_config
from dd_selftest import move   # reuse the set_actor_transform helper


def read_handles():
    """Live ADraftDeskRoomHandle actors -> [{ref, ri, x, y, z}]."""
    s = ('import json\n'
         'FIND="editor_toolset.toolsets.scene.SceneTools.find_actors"\n'
         'XF="editor_toolset.toolsets.actor.ActorTools.get_actor_transform"\n'
         'GET="editor_toolset.toolsets.object.ObjectTools.get_properties"\n'
         f'CLS="{dd_config.ROOMHANDLE}"\n'
         'def run():\n'
         '    acts=execute_tool(FIND,json.dumps({"name":"","tag":"","collision_channels":[],"actor_type":{"refPath":CLS}}))["returnValue"]\n'
         '    out=[]\n'
         '    for a in acts:\n'
         '        ref=a["refPath"]\n'
         '        xf=execute_tool(XF,json.dumps({"actor":{"refPath":ref}}))["returnValue"]\n'
         '        pr=execute_tool(GET,json.dumps({"instance":{"refPath":ref},"properties":["RoomIndex"]}))["returnValue"]\n'
         '        pj=json.loads(pr) if isinstance(pr,str) else pr\n'
         '        ri=int(pj.get("roomIndex", pj.get("RoomIndex",-1))) if isinstance(pj,dict) else -1\n'
         '        out.append({"ref":ref,"ri":ri,"x":xf["location"]["x"],"y":xf["location"]["y"],"z":xf["location"]["z"]})\n'
         '    return {"handles":out}\n')
    return ddrun.run_text(s, substitute=False)["handles"]


def _center(r):
    return ((r["Min"][0] + r["Max"][0]) / 2.0, (r["Min"][1] + r["Max"][1]) / 2.0)


def main():
    results = []

    def check(step, cond, detail=""):
        results.append(cond)
        print(f"  [{'PASS' if cond else 'FAIL'}] {step}{(' - ' + detail) if detail else ''}")

    print("=== ROOM-MOVE SELF-TEST (drag a room handle, green/red by query) ===")
    L = dd_engine.layout()
    entry = next((t["RoomA"] for t in L.thresholds if t["bIsEntry"]), -1)

    # degree over WALL connections (interior, non-rail); find a non-entry leaf + its one neighbour
    deg, nbr = {}, {}
    for t in L.thresholds:
        a, b = t["RoomA"], t["RoomB"]
        if b is None or b < 0 or t["Kind"] == "Rail":
            continue
        deg[a] = deg.get(a, 0) + 1; deg[b] = deg.get(b, 0) + 1
        nbr.setdefault(a, []).append(b); nbr.setdefault(b, []).append(a)
    leaf = next((r for r in range(len(L.rooms)) if r != entry and deg.get(r, 0) == 1), None)
    if leaf is None:
        print("  no leaf room found"); return False
    nb = nbr[leaf][0]
    orig_kind = next((t["Kind"] for t in L.thresholds
                      if {t["RoomA"], t["RoomB"]} == {leaf, nb} and t["Plane"] == "Vertical"), None)
    lc, nc = _center(L.rooms[leaf]), _center(L.rooms[nb])
    ddx, ddy = lc[0] - nc[0], lc[1] - nc[1]
    mv = ((1000.0 if ddx >= 0 else -1000.0), 0.0) if abs(ddx) >= abs(ddy) else (0.0, (1000.0 if ddy >= 0 else -1000.0))
    print(f"leaf room {leaf} (1 connection -> room {nb}); detach move {mv}")

    h = next((x for x in read_handles() if x["ri"] == leaf), None)
    if h is None:
        print(f"  no handle for room {leaf}"); return False
    home = (h["x"], h["y"], h["z"])

    check("0.baseline gate green", dd_gate.gate_engine())

    # 1) DETACH: pull the leaf far off its only neighbour -> unreachable -> RED
    print(f"\n[1] DETACH room {leaf} (move handle {mv} into the void)")
    move(h["ref"], home[0] + mv[0], home[1] + mv[1], home[2])
    rep = dd_drag.sync()
    check("1.room translated", (rep or {}).get("roomTranslated", 0) >= 1, f"roomTranslated={(rep or {}).get('roomTranslated')}")
    check("1.gate RED (leaf unreachable)", not dd_gate.gate_engine())

    # 2) REATTACH: drag back home -> reconnects -> GREEN
    print(f"\n[2] REATTACH room {leaf} (handle back home)")
    h2 = next((x for x in read_handles() if x["ri"] == leaf), None)
    move(h2["ref"], home[0], home[1], home[2])
    dd_drag.sync()
    check("2.gate GREEN (reconnected)", dd_gate.gate_engine())

    # 3) the restored connection keeps its KIND (dormant-preserved, not dissolved to a Passage)
    L2 = dd_engine.layout()
    new_kind = next((t["Kind"] for t in L2.thresholds
                     if {t["RoomA"], t["RoomB"]} == {leaf, nb} and t["Plane"] == "Vertical"), None)
    check("3.door kind preserved across detach/reattach", new_kind == orig_kind, f"was {orig_kind}, now {new_kind}")

    n_ok = sum(1 for c in results if c)
    print(f"\n==== ROOM-MOVE SELF-TEST: {n_ok}/{len(results)} checks pass ====")
    return n_ok == len(results)


if __name__ == "__main__":
    raise SystemExit(0 if main() else 1)
