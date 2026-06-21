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


if __name__ == "__main__":
    import dd_castle
    check(dd_castle.L)
