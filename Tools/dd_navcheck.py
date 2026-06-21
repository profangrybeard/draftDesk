"""
dd_navcheck.py — assert a dictated layout is WALKABLE by querying the live navmesh, not screenshots.

Computes the entrance + every room centre in normalized world space (mirroring the engine's
NormalizeToEntry shift), then calls the engine-side MCP tool DraftDeskEditor.DdNavToolset.CheckReachability
(UNavigationSystemV1::FindPathToLocationSynchronously on the editor world) and reports, per room,
whether a COMPLETE nav path exists from the entrance and its length. An unreachable room (a stair that
doesn't connect, an island) fails here even when the geometry is perfectly watertight.

Usage:  python dd_navcheck.py            # checks dd_castle.L
        import dd_navcheck; dd_navcheck.check(my_layout)
The editor must be open with the layout applied (run dd_castle.py first).
"""
import json
import math
import time
import ddrun

TOOL = "DraftDeskEditor.DdNavToolset.CheckReachability"


def _shift(L):
    """Replicate the engine's NormalizeToEntry: world = local - (dx, dy), Z unshifted (min level base = 0)."""
    T = L.wall
    dx = dy = 0.0
    for t in L.thresholds:
        if not t["bIsEntry"]:
            continue
        a = L.rooms[t["RoomA"]]
        cx = (a["Min"][0] + a["Max"][0]) / 2 + t["Position"]
        cy = (a["Min"][1] + a["Max"][1]) / 2 + t["Position"]
        e = t["ExteriorEdge"]
        if e == "West":    dx, dy = a["Min"][0] - T / 2, cy
        elif e == "East":  dx, dy = a["Max"][0] + T / 2, cy
        elif e == "South": dx, dy = cx, a["Min"][1] - T / 2
        else:              dx, dy = cx, a["Max"][1] + T / 2
        break
    minz = min((lv["BaseZ"] for lv in L.levels), default=0.0)
    return dx, dy, minz


def world_points(L):
    dx, dy, minz = _shift(L)

    def base_z(level):
        return L.levels[level]["BaseZ"] if L.levels and 0 <= level < len(L.levels) else 0.0

    targets = []
    for i, r in enumerate(L.rooms):
        cx = (r["Min"][0] + r["Max"][0]) / 2 - dx
        cy = (r["Min"][1] + r["Max"][1]) / 2 - dy
        cz = base_z(r["Level"]) - minz + 30.0   # a hair above the floor; nav projects to the mesh
        targets.append((i, r["Level"], cx, cy, cz))
    entry_room = next(t["RoomA"] for t in L.thresholds if t["bIsEntry"])
    ar = L.rooms[entry_room]
    start = ((ar["Min"][0] + ar["Max"][0]) / 2 - dx, (ar["Min"][1] + ar["Max"][1]) / 2 - dy,
             base_z(ar["Level"]) - minz + 30.0)
    return start, targets


def _query(start, targets):
    payload = {"Start": {"x": start[0], "y": start[1], "z": start[2]},
               "Targets": [{"x": t[2], "y": t[3], "z": t[4]} for t in targets]}
    script = (
        "import json\n"
        f'TOOL = "{TOOL}"\n'
        f"ARGS = json.loads(r'''{json.dumps(payload)}''')\n"
        "def run():\n"
        "    r = execute_tool(TOOL, json.dumps(ARGS))\n"
        "    return {'results': r['returnValue'] if isinstance(r, dict) and 'returnValue' in r else r}\n"
    )
    res = ddrun.run_text(script, substitute=False)
    return res["results"] if isinstance(res, dict) and "results" in res else res


def check(L, retries=4, wait=4.0):
    start, targets = world_points(L)

    def reachable_count(rows):
        n = 0
        for (i, level, cx, cy, cz), r in zip(targets, rows):
            is_start = abs(cx - start[0]) < 1.0 and abs(cy - start[1]) < 1.0 and abs(cz - start[2]) < 1.0
            if is_start or bool(r.get("bReachable", r.get("breachable"))):
                n += 1
        return n

    # The navmesh rebuilds ASYNC after an apply; retry until the reachable count stops climbing.
    rows = _query(start, targets)
    for _ in range(retries):
        if reachable_count(rows) == len(targets):
            break
        time.sleep(wait)
        rows = _query(start, targets)

    def g(d, *keys):
        for k in keys:
            if k in d:
                return d[k]
        return None

    print(f"NAV reachability from the entrance ({start[0]:.0f},{start[1]:.0f},{start[2]:.0f}):")
    unreachable = []
    for (i, level, cx, cy, cz), r in zip(targets, rows):
        reachable = bool(g(r, "bReachable", "breachable"))
        length = g(r, "length", "Length")
        partial = bool(g(r, "bPartial", "bpartial"))
        is_start = abs(cx - start[0]) < 1.0 and abs(cy - start[1]) < 1.0 and abs(cz - start[2]) < 1.0
        if is_start:        # querying the start against itself returns no path; you are already there
            reachable, length, tag = True, 0.0, "OK (start)"
        else:
            tag = "OK " if reachable else ("PARTIAL" if partial else "UNREACHABLE")
        print(f"  room {i:>2} L{level}  ({cx:>6.0f},{cy:>6.0f},{cz:>4.0f})  {tag:<11} len={length}")
        if not reachable:
            unreachable.append(i)
    n = len(targets)
    print(f"\n{n - len(unreachable)}/{n} rooms reachable from the entrance.")
    if unreachable:
        print(f"UNREACHABLE rooms: {unreachable}  <-- broken paths")
    else:
        print("==> EVERY ROOM IS WALKABLE FROM THE ENTRANCE <==")
    return unreachable


def _query_pairs(pairs):
    """Run one nav query per (start,end) pair via a sandbox loop over the existing tool."""
    script = (
        "import json\n"
        f'TOOL = "{TOOL}"\n'
        f"PAIRS = json.loads(r'''{json.dumps(pairs)}''')\n"
        "def run():\n"
        "    out = []\n"
        "    for p in PAIRS:\n"
        "        r = execute_tool(TOOL, json.dumps({\"Start\": {\"x\": p[0], \"y\": p[1], \"z\": p[2]},\n"
        "                                           \"Targets\": [{\"x\": p[3], \"y\": p[4], \"z\": p[5]}]}))\n"
        "        rv = r[\"returnValue\"] if \"returnValue\" in r else r\n"
        "        if rv:\n"
        "            res = rv[0]\n"
        "            out.append({\"reachable\": res[\"bReachable\"], \"length\": res[\"length\"]})\n"
        "        else:\n"
        "            out.append({\"reachable\": False, \"length\": -1})\n"
        "    return {\"results\": out}\n"
    )
    res = ddrun.run_text(script, substitute=False)
    return res["results"] if isinstance(res, dict) and "results" in res else res


def check_connections(L, retries=4, wait=4.0):
    """THE first test: every declared threshold (a connection) must be nav-traversable A<->B.
    Catches a single blocked/sealed door that room-from-entrance reachability would mask, and a
    same-level door forced into a long detour (the opening is blocked but the rooms connect elsewhere)."""
    dx, dy, minz = _shift(L)

    def base_z(level):
        return L.levels[level]["BaseZ"] if L.levels and 0 <= level < len(L.levels) else 0.0

    def center(i):
        r = L.rooms[i]
        return [(r["Min"][0] + r["Max"][0]) / 2 - dx, (r["Min"][1] + r["Max"][1]) / 2 - dy,
                base_z(r["Level"]) - minz + 30.0]

    conns = []  # (label, kind, A, B, same_level)
    for t in L.thresholds:
        a_i, b_i, kind = t["RoomA"], t["RoomB"], t["Kind"]
        if b_i == -1:
            if t["bIsEntry"]:
                a = [0.0, 0.0, base_z(L.rooms[a_i]["Level"]) - minz + 30.0]
                conns.append((f"entry->{a_i}", kind, a, center(a_i), True))
            continue                       # rails / windows / other exterior openings aren't walk-throughs
        if kind in ("Rail", "Window"):
            continue
        same = L.rooms[a_i]["Level"] == L.rooms[b_i]["Level"]
        conns.append((f"{a_i}-{b_i}", kind, center(a_i), center(b_i), same))

    pairs = [[c[2][0], c[2][1], c[2][2], c[3][0], c[3][1], c[3][2]] for c in conns]

    def broken_of(rows):
        bad = []
        for (label, kind, A, B, same), r in zip(conns, rows):
            reach = bool(r["reachable"]) if "reachable" in r else False
            length = r["length"] if "length" in r else -1
            straight = math.hypot(B[0] - A[0], B[1] - A[1])
            # a same-level door forced way off the straight line means THIS opening is blocked
            detour = reach and same and kind in ("Doorway", "Passage") and length > 2.0 * straight + 600
            if (not reach) or detour:
                bad.append(label)
        return bad

    rows = _query_pairs(pairs)
    for _ in range(retries):                # ride out the async navmesh rebuild
        if not broken_of(rows):
            break
        time.sleep(wait)
        rows = _query_pairs(pairs)

    print("NAV per-connection traversal (each threshold is a declared connection):")
    broken = []
    for (label, kind, A, B, same), r in zip(conns, rows):
        reach = bool(r["reachable"]) if "reachable" in r else False
        length = r["length"] if "length" in r else -1
        straight = math.hypot(B[0] - A[0], B[1] - A[1])
        ratio = (length / straight) if (reach and straight > 1) else 0.0
        detour = reach and same and kind in ("Doorway", "Passage") and length > 2.0 * straight + 600
        tag = "OK" if (reach and not detour) else ("DETOUR(blocked?)" if detour else "BLOCKED")
        print(f"  {label:<11} {kind:<9} {tag:<16} len={length:>7.0f}  straight={straight:>6.0f}  x{ratio:.1f}")
        if (not reach) or detour:
            broken.append(label)
    n = len(conns)
    print(f"\n{n - len(broken)}/{n} declared connections traversable.")
    if broken:
        print(f"BLOCKED connections: {broken}  <-- a threshold that nav can't cross")
    else:
        print("==> EVERY DECLARED CONNECTION IS WALKABLE (threshold by threshold) <==")
    return broken


if __name__ == "__main__":
    import dd_castle
    check_connections(dd_castle.L)   # the primary, threshold-by-threshold gate
    print()
    check(dd_castle.L)               # complementary global reachability from the entrance
