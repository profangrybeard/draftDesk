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


def step_total_run(dz, rise=18.0, run=30.0, max_angle=40.0):
    """Horizontal run of a flight climbing dz — mirrors the engine's StepCount*StepRun (metric
    defaults). Used only to anchor a flight's top point + a detour baseline, both of which have
    slack, so an exact match to the Spec metrics isn't required."""
    if dz <= 1:
        return 0.0
    nr = math.ceil(dz / rise)
    tanmax = math.tan(math.radians(min(max_angle, 89.0)))
    na = math.ceil(dz / (run * tanmax)) if tanmax > 1e-4 else nr
    return max(1, nr, na) * run


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
    """THE first test: every declared connection must be nav-traversable A<->B — every threshold
    AND every flight. Catches a single blocked/sealed door that room-from-entrance reachability would
    mask, a same-level door forced into a long detour (the opening is blocked but the rooms connect
    elsewhere), and a broken stair in a dual staircase (one side dead, the other still walkable)."""
    dx, dy, minz = _shift(L)

    def base_z(level):
        return L.levels[level]["BaseZ"] if L.levels and 0 <= level < len(L.levels) else 0.0

    def floor_z(r):
        return r["FloorZ"] if r["FloorZ"] >= 0 else base_z(r["Level"])

    def center(i):
        r = L.rooms[i]
        return [(r["Min"][0] + r["Max"][0]) / 2 - dx, (r["Min"][1] + r["Max"][1]) / 2 - dy,
                base_z(r["Level"]) - minz + 30.0]

    def world(along, cross, z, along_x):     # a flight point -> normalized world (mirrors NormalizeToEntry)
        return ([along - dx, cross - dy, z - minz] if along_x
                else [cross - dx, along - dy, z - minz])

    conns = []  # (label, kind, A, B, chk)   chk => apply the detour test (a long path means blocked)
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
        conns.append((f"{a_i}-{b_i}", kind, center(a_i), center(b_i), same and kind in ("Doorway", "Passage")))

    # A flight is a connection too, but it is NOT a threshold, so it never appeared above. Test each
    # flight from the BASE of that stair to the TOP of that stair (the stair's own endpoints) — this
    # ISOLATES one flight: a dual staircase with one broken side still passes global reachability AND
    # hall->balcony centre-to-centre (you take the other stair); only a base->top query forces THIS
    # flight, where a break shows up as UNREACHABLE or a large detour around via the working stair.
    for fi, f in enumerate(L.flights):
        ax = bool(f["bAlongX"]); d = 1 if f["Dir"] >= 0 else -1
        u0, cv, z0, z1 = f["StartU"], f["CrossV"], f["FromZ"], f["ToZ"]
        u_top = u0 + d * step_total_run(abs(z1 - z0))

        def span(r):
            return (r["Min"][1], r["Max"][1]) if ax else (r["Min"][0], r["Max"][0])

        def near_edge(r):                  # the room edge facing the climb origin
            return (r["Min"][0] if d > 0 else r["Max"][0]) if ax else (r["Min"][1] if d > 0 else r["Max"][1])

        land, best = None, None            # landing room: at z1, cross in span, nearest edge up-climb
        for i, r in enumerate(L.rooms):
            if abs(floor_z(r) - z1) > 1:
                continue
            lo, hi = span(r)
            if not (lo <= cv <= hi):
                continue
            adv = d * (near_edge(r) - u0)
            if adv > 0 and (best is None or adv < best):
                best, land, u_top = adv, i, near_edge(r)   # real edge beats the metric estimate
        bot = None                         # bottom room: at z0, footprint over the flight base
        for i, r in enumerate(L.rooms):
            if abs(floor_z(r) - z0) > 1:
                continue
            inside = (r["Min"][0] - 1 <= u0 <= r["Max"][0] + 1 and r["Min"][1] <= cv <= r["Max"][1]) if ax \
                else (r["Min"][1] - 1 <= u0 <= r["Max"][1] + 1 and r["Min"][0] <= cv <= r["Max"][0])
            if inside:
                bot = i; break

        A = world(u0 - d * 100.0, cv, z0 + 30.0, ax)    # 1 m back into the bottom room
        B = world(u_top + d * 100.0, cv, z1 + 30.0, ax)  # 1 m onto the landing floor
        kind = "Ramp" if f["bRamp"] else "Stairs"
        label = f"flight{fi}" + (f" {bot}->{land}" if (bot is not None and land is not None) else "")
        conns.append((label, kind, A, B, True))

    pairs = [[c[2][0], c[2][1], c[2][2], c[3][0], c[3][1], c[3][2]] for c in conns]

    def expected_of(A, B):
        # 3D baseline: a flight's climb counts TOWARD the expected length, not against it
        return math.hypot(math.hypot(B[0] - A[0], B[1] - A[1]), B[2] - A[2])

    def broken_of(rows):
        bad = []
        for (label, kind, A, B, chk), r in zip(conns, rows):
            reach = bool(r["reachable"]) if "reachable" in r else False
            length = r["length"] if "length" in r else -1
            detour = reach and chk and length > 2.0 * expected_of(A, B) + 600
            if (not reach) or detour:
                bad.append(label)
        return bad

    rows = _query_pairs(pairs)
    for _ in range(retries):                # ride out the async navmesh rebuild
        if not broken_of(rows):
            break
        time.sleep(wait)
        rows = _query_pairs(pairs)

    print("NAV per-connection traversal (every threshold + every flight is a declared connection):")
    broken = []
    for (label, kind, A, B, chk), r in zip(conns, rows):
        reach = bool(r["reachable"]) if "reachable" in r else False
        length = r["length"] if "length" in r else -1
        expected = expected_of(A, B)
        ratio = (length / expected) if (reach and expected > 1) else 0.0
        detour = reach and chk and length > 2.0 * expected + 600
        tag = "OK" if (reach and not detour) else ("DETOUR(blocked?)" if detour else "BLOCKED")
        print(f"  {label:<14} {kind:<9} {tag:<16} len={length:>7.0f}  expect={expected:>6.0f}  x{ratio:.1f}")
        if (not reach) or detour:
            broken.append(label)
    n = len(conns)
    print(f"\n{n - len(broken)}/{n} declared connections traversable (thresholds + flights).")
    if broken:
        print(f"BLOCKED connections: {broken}  <-- a threshold or flight nav can't cross")
    else:
        print("==> EVERY DECLARED CONNECTION IS WALKABLE (threshold by threshold, flight by flight) <==")
    return broken


if __name__ == "__main__":
    import dd_castle
    check_connections(dd_castle.L)   # the primary, threshold-by-threshold gate
    print()
    check(dd_castle.L)               # complementary global reachability from the entrance
