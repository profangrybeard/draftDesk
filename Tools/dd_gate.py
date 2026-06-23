"""dd_gate.py — THE acceptance gate for iterative threshold authoring. Everything is QUERIED from the
live engine (markers, navmesh) or the proven-byte-identical shell core (geometry). No screenshots, no
eyes. Three checks, each PASS/FAIL:

  1. BIJECTION  — every model element (each threshold + each flight) has exactly ONE marker, and every
                  marker maps to exactly one element. This is the test that catches "a marker without a
                  threshold" and "a threshold without a marker". (queries the LIVE marker actors)
  2. WATERTIGHT — the shell validator reports 0 failures. (shell core == the engine's DdShellCore,
                  proven byte-identical by the digest test; engine-side exposure lands in step 2.)
  3. NAV WHOLE  — every threshold AND every flight traversable, every room reachable from the entrance.
                  (queries the LIVE navmesh via the DdNavToolset.)

Run on the CURRENT castle it reads RED on the real divergence (no markers for 7-8 + the 2 staircases);
that is the point — the gate measures the thing we actually care about, before we fix anything.

  python dd_gate.py [layout]          # default: dd_castle
"""
import contextlib
import importlib
import io
import math
import sys

sys.path.insert(0, "shell")
import shell as S
import ddrun
import dd_anchor
import dd_navcheck

_KIND = {"Doorway": S.DOORWAY, "Passage": S.PASSAGE, "Window": S.WINDOW, "Rail": S.RAIL,
         "Stairwell": S.STAIRWELL, "Ramp": S.RAMP, "Hatch": S.HATCH, "Skylight": S.SKYLIGHT,
         "Atrium": S.ATRIUM}
_PLANE = {"Vertical": S.VERTICAL, "Horizontal": S.HORIZONTAL}
_EDGE = {"West": 0, "East": 1, "South": 2, "North": 3}


def _full_shell(L):
    rooms = [S.Room(r["Min"][0], r["Min"][1], r["Max"][0], r["Max"][1], level=r["Level"],
                    floor_z=(None if r["FloorZ"] < 0 else r["FloorZ"]), height=r["Height"],
                    floor=r["bFloor"], ceil=r["bCeil"]) for r in L.rooms]
    thr = [S.Threshold(t["RoomA"], t["RoomB"], _KIND[t["Kind"]], plane=_PLANE.get(t["Plane"], S.VERTICAL),
                       position=t["Position"], position2=t["Position2"], width=t["Width"], depth=t["Depth"],
                       height=t["Height"], sill=t["Sill"], is_entry=t["bIsEntry"], edge=_EDGE[t["ExteriorEdge"]])
           for t in L.thresholds]
    flights = [S.Flight(along_x=f["bAlongX"], start_u=f["StartU"], cross_v=f["CrossV"], z0=f["FromZ"],
                        z1=f["ToZ"], w=f["Width"], direction=f["Dir"]) for f in L.flights]
    levels = [S.Level(lv["Index"], lv["BaseZ"], lv["Height"], lv["SlabT"]) for lv in L.levels]
    return S.Shell(rooms, thr, levels, S.Metrics(grid=L.snap or 50.0, wall_thickness=30), flights)


def _read_markers():
    """Live threshold markers -> [{label, invalid}]. Reads bInvalid so the bijection can tell a red-X (a
    known, mid-iteration broken connection) from a true orphan (a marker whose threshold is gone)."""
    READ = '''import json
FIND="editor_toolset.toolsets.scene.SceneTools.find_actors"
GET="editor_toolset.toolsets.object.ObjectTools.get_properties"
CLS="{{THRESH}}"
def run():
    acts=execute_tool(FIND,json.dumps({"name":"","tag":"","collision_channels":[],"actor_type":{"refPath":CLS}}))["returnValue"]
    out=[]
    for a in acts:
        pr=execute_tool(GET,json.dumps({"instance":{"refPath":a["refPath"]},"properties":["Label","bInvalid"]}))["returnValue"]
        pj=json.loads(pr) if isinstance(pr,str) else pr
        out.append({"label":str(pj.get("Label","")),"invalid":bool(pj.get("bInvalid",False))})
    return {"markers":out}
'''
    return ddrun.run_text(READ)["markers"]


def _read_handles():
    """Live ADraftDeskRoomHandle actors -> their RoomIndex (the room each handle owns)."""
    import dd_config
    READ = ('import json\n'
            'FIND="editor_toolset.toolsets.scene.SceneTools.find_actors"\n'
            'GET="editor_toolset.toolsets.object.ObjectTools.get_properties"\n'
            f'CLS="{dd_config.ROOMHANDLE}"\n'
            'def run():\n'
            '    acts=execute_tool(FIND,json.dumps({"name":"","tag":"","collision_channels":[],"actor_type":{"refPath":CLS}}))["returnValue"]\n'
            '    out=[]\n'
            '    for a in acts:\n'
            '        pr=execute_tool(GET,json.dumps({"instance":{"refPath":a["refPath"]},"properties":["RoomIndex"]}))["returnValue"]\n'
            '        pj=json.loads(pr) if isinstance(pr,str) else pr\n'
            '        out.append(int(pj.get("roomIndex", pj.get("RoomIndex", -1))) if isinstance(pj,dict) else -1)\n'
            '    return {"rooms": out}\n')
    return ddrun.run_text(READ, substitute=False)["rooms"]


def _of(o, *keys):
    for k in keys:
        if isinstance(o, dict) and k in o:
            return o[k]
    return None


def _engine_openings():
    """The ENGINE's emitted openings (one per resolved threshold + per flight), read straight off the
    generator's reflected Openings array. The engine's own truth — no Python re-derivation."""
    READ = '''import json
GET="editor_toolset.toolsets.object.ObjectTools.get_properties"
GEN="{{GEN}}"
def run():
    pr=execute_tool(GET,json.dumps({"instance":{"refPath":GEN},"properties":["Openings"]}))["returnValue"]
    pj=json.loads(pr) if isinstance(pr,str) else pr
    return {"openings": (pj.get("Openings") or pj.get("openings") or []) if isinstance(pj,dict) else []}
'''
    return ddrun.run_text(READ)["openings"]


def bijection(L):
    # MULTISET, not set: count openings per label and markers per label. A set-compare would silently
    # collapse two openings that share a label (a door + a window in one wall -> both "A-B") and certify
    # a FALSE green while one opening has no marker. Counting catches it.
    ops = _engine_openings()
    exp = {}
    for o in ops:
        lab = _of(o, "Label", "label")
        if lab is not None:
            exp[lab] = exp.get(lab, 0) + 1
    markers = _read_markers()
    invalid = sorted(m["label"] for m in markers if m["invalid"])               # red-X: broken-but-visible connections
    liv = {}
    for m in markers:
        if not m["invalid"]:                                                   # multiset on VALID markers only --
            liv[m["label"]] = liv.get(m["label"], 0) + 1                       # an invalid marker has no opening + must not read as an orphan
    labels = set(exp) | set(liv)
    missing = sorted(l for l in labels if liv.get(l, 0) < exp.get(l, 0))       # opening(s) with too few markers
    orphan = sorted(l for l in labels if l not in exp)                          # marker with no opening (true orphan)
    over = sorted(l for l in labels if l in exp and liv.get(l, 0) > exp.get(l, 0))  # too many markers
    collide = sorted(l for l, n in exp.items() if n > 1)                        # two openings, one label (build bug)
    ok = not (missing or orphan or over or collide or invalid)                  # a live red-X blocks GREEN: finish or remove it
    print(f"  1. BIJECTION   {'PASS' if ok else 'FAIL'}   (engine openings {sum(exp.values())}, live markers {sum(liv.values())} valid + {len(invalid)} invalid)")
    if missing:  print(f"        opening with NO marker:          {missing}")
    if orphan:   print(f"        marker with NO engine opening:   {orphan}")
    if over:     print(f"        too many markers for opening:    {over}")
    if collide:  print(f"        COLLIDING opening labels (two thresholds share a wall — needs SourceId re-key): {collide}")
    if invalid:  print(f"        INVALID (red-X) — re-place onto a wall or delete: {invalid}")
    return ok


def oracle_drift_note(L):
    """Informational: how far the OLD Python seed-model (dd_anchor.seeds) sits from the ENGINE's real
    openings. The engine is AUTHORITATIVE — its openings are built from the same normalize+grid-snapped
    geometry the walls are. Any delta here is the seed-model's error (it never replicated the snap), which
    is exactly why dd_seedmarkers left markers floating off their openings. Slice 2 reconciles markers to
    the engine and the delta vanishes by construction."""
    ops = {}
    for o in _engine_openings():
        pos = _of(o, "Position", "position") or {}
        ops[_of(o, "Label", "label")] = (pos.get("x", 0.0), pos.get("y", 0.0))
    deltas = [math.hypot(ops[s["label"]][0] - s["x"], ops[s["label"]][1] - s["y"])
              for s in dd_anchor.seeds(L) if s["label"] in ops]
    if deltas:
        n_off = sum(1 for d in deltas if d > 1.0)
        print(f"  NOTE: the old Python seed-model is off the ENGINE's openings by up to {max(deltas):.0f} cm "
              f"({n_off}/{len(deltas)} thresholds).")
        print("        Engine is authoritative; markers seeded from the old model inherit that float — "
              "slice 2 reconciles markers TO the engine and it vanishes.")


def watertight(L):
    s = _full_shell(L); s.build(); fails = s.validate()
    ok = not fails
    print(f"  2. WATERTIGHT  {'PASS' if ok else 'FAIL'}   (shell validator: {len(fails)} failures)")
    for f in fails[:5]: print(f"        ! {f}")
    return ok


def room_bijection(L):
    """One ADraftDeskRoomHandle per room, keyed by RoomIndex (0..N-1), no orphans/dups. The room<->handle
    half of the gate — proves 'a room without a handle' and 'a handle without a room' can't ship, the way
    the threshold bijection does for openings. (queries the LIVE handle actors)"""
    n = len(L.rooms)
    live = _read_handles()
    cnt = {}
    for ri in live:
        cnt[ri] = cnt.get(ri, 0) + 1
    missing = sorted(i for i in range(n) if cnt.get(i, 0) < 1)
    orphan = sorted(ri for ri in cnt if ri < 0 or ri >= n)
    over = sorted(ri for ri, c in cnt.items() if 0 <= ri < n and c > 1)
    ok = not (missing or orphan or over)
    print(f"  4. ROOM HANDLES {'PASS' if ok else 'FAIL'}   (rooms {n}, handles {len(live)})")
    if missing: print(f"        room with NO handle:                           {missing}")
    if orphan:  print(f"        handle with NO room (RoomIndex out of range):  {orphan}")
    if over:    print(f"        too many handles for room:                     {over}")
    return ok


def nav_whole(L):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        broken = dd_navcheck.check_connections(L)
        unreach = dd_navcheck.check(L)
    ncon = buf.getvalue().count("declared connections traversable")  # for the count line
    ok = not broken and not unreach
    print(f"  3. NAV WHOLE   {'PASS' if ok else 'FAIL'}   (blocked connections {len(broken)}, unreachable rooms {len(unreach)})")
    if broken:  print(f"        BLOCKED: {broken}")
    if unreach: print(f"        UNREACHABLE rooms: {unreach}")
    return ok


def gate(L):
    print("=== ITERATIVE AUTHORING GATE (queried from the live engine, no screenshots) ===")
    b = bijection(L)
    w = watertight(L)
    n = nav_whole(L)
    h = room_bijection(L)
    allok = b and w and n and h
    print("  " + "-" * 60)
    print(f"  GATE: {'PASS — green by query' if allok else 'FAIL'}")
    return allok


def gate_engine():
    """Gate against ENGINE TRUTH — the live generator's authored arrays, NOT a static dictation. This is
    the iteration-time gate: after a drag/move mutates the engine, dd_castle.L is stale, so all three
    checks (bijection, watertight, nav) must read the geometry the generator actually holds."""
    import dd_engine
    return gate(dd_engine.layout())


if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "engine"
    if arg == "engine":
        import dd_engine
        L = dd_engine.layout()
        print("=== GATE source: ENGINE TRUTH (live generator's authored arrays) ===")
    else:
        L = importlib.import_module(arg).L
        print(f"=== GATE source: static layout '{arg}' ===")
    ok = gate(L)
    print()
    if arg != "engine":
        oracle_drift_note(L)    # informational: the old seed-model's offset from the engine's real openings
    raise SystemExit(0 if ok else 1)
