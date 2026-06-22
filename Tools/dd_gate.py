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
    READ = '''import json
FIND="editor_toolset.toolsets.scene.SceneTools.find_actors"
GET="editor_toolset.toolsets.object.ObjectTools.get_properties"
CLS="{{THRESH}}"
def run():
    acts=execute_tool(FIND,json.dumps({"name":"","tag":"","collision_channels":[],"actor_type":{"refPath":CLS}}))["returnValue"]
    out=[]
    for a in acts:
        pr=execute_tool(GET,json.dumps({"instance":{"refPath":a["refPath"]},"properties":["Label","Kind"]}))["returnValue"]
        pj=json.loads(pr) if isinstance(pr,str) else pr
        out.append(pj.get("Label",""))
    return {"labels":out}
'''
    return ddrun.run_text(READ)["labels"]


def _model_elements(L):
    """The set of things that MUST each have exactly one marker: every resolvable threshold + every
    flight. seeds() already drops unresolved thresholds (no facing), matching what the engine carves."""
    labels = [s["label"] for s in dd_anchor.seeds(L)]
    labels += [f"flight{i}" for i in range(len(L.flights))]
    return labels


def bijection(L):
    expected = _model_elements(L)
    live = _read_markers()
    exp_set, live_counts = set(expected), {}
    for lab in live:
        live_counts[lab] = live_counts.get(lab, 0) + 1
    missing = sorted(exp_set - set(live_counts))
    orphan = sorted(set(live_counts) - exp_set)
    dup = sorted(l for l, n in live_counts.items() if n > 1)
    ok = not (missing or orphan or dup)
    print(f"  1. BIJECTION   {'PASS' if ok else 'FAIL'}   (model elements {len(expected)}, live markers {len(live)})")
    if missing: print(f"        threshold/flight with NO marker: {missing}")
    if orphan:  print(f"        marker with NO model element:    {orphan}")
    if dup:     print(f"        duplicate markers:               {dup}")
    return ok


def watertight(L):
    s = _full_shell(L); s.build(); fails = s.validate()
    ok = not fails
    print(f"  2. WATERTIGHT  {'PASS' if ok else 'FAIL'}   (shell validator: {len(fails)} failures)")
    for f in fails[:5]: print(f"        ! {f}")
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
    allok = b and w and n
    print("  " + "-" * 60)
    print(f"  GATE: {'PASS — green by query' if allok else 'FAIL'}")
    return allok


if __name__ == "__main__":
    mod = sys.argv[1] if len(sys.argv) > 1 else "dd_castle"
    L = importlib.import_module(mod).L
    raise SystemExit(0 if gate(L) else 1)
