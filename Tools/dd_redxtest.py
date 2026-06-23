"""dd_redxtest.py — RED-X (whole-tool slice 1) acceptance by query. A connection that goes DORMANT (its
rooms moved apart) must keep its marker on screen as an INVALID red-X -- not vanish. The reconciler reports
it, the gate isn't green while it dangles; drag the room back and the red-X clears and the gate goes green.
Run from a reconciled-green baseline (castle loaded, navmesh Dynamic):

    python dd_redxtest.py
"""
import dd_drag
import dd_gate
import dd_engine
from dd_selftest import move
from dd_roomtest import read_handles


def _invalid():
    ms = dd_gate._read_markers()
    return [m for m in ms if m["invalid"]], ms


def main():
    results = []

    def check(s, c, d=""):
        results.append(c); print(f"  [{'PASS' if c else 'FAIL'}] {s}{(' - ' + d) if d else ''}")

    print("=== RED-X SELF-TEST (invalid markers stay visible, block green) ===")
    L = dd_engine.layout()
    entry = next((t["RoomA"] for t in L.thresholds if t["bIsEntry"]), -1)
    deg, nbr = {}, {}
    for t in L.thresholds:
        a, b = t["RoomA"], t["RoomB"]
        if b is None or b < 0 or t["Kind"] == "Rail":
            continue
        deg[a] = deg.get(a, 0) + 1; deg[b] = deg.get(b, 0) + 1
        nbr.setdefault(a, []).append(b); nbr.setdefault(b, []).append(a)
    leaf = next((r for r in range(len(L.rooms)) if r != entry and deg.get(r, 0) == 1), None)
    if leaf is None:
        print("  no leaf room"); return False
    nb = nbr[leaf][0]
    lc = ((L.rooms[leaf]["Min"][0] + L.rooms[leaf]["Max"][0]) / 2, (L.rooms[leaf]["Min"][1] + L.rooms[leaf]["Max"][1]) / 2)
    nc = ((L.rooms[nb]["Min"][0] + L.rooms[nb]["Max"][0]) / 2, (L.rooms[nb]["Min"][1] + L.rooms[nb]["Max"][1]) / 2)
    ddx, ddy = lc[0] - nc[0], lc[1] - nc[1]
    mv = ((1000.0 if ddx >= 0 else -1000.0), 0.0) if abs(ddx) >= abs(ddy) else (0.0, (1000.0 if ddy >= 0 else -1000.0))
    print(f"leaf room {leaf} -> door to room {nb}; detach move {mv}")

    inv0, ms0 = _invalid()
    check("0.baseline: no invalid markers", len(inv0) == 0, f"{len(inv0)} invalid")

    h = next((x for x in read_handles() if x["ri"] == leaf), None)
    if not h:
        print(f"  no handle for room {leaf}"); return False
    home = (h["x"], h["y"], h["z"])

    # 1) DETACH -> the door goes dormant; its marker must STAY as red-X (not be deleted)
    print(f"\n[1] DETACH room {leaf}")
    move(h["ref"], home[0] + mv[0], home[1] + mv[1], home[2])
    rep = dd_drag.sync()
    inv1, ms1 = _invalid()
    check("1.report Invalid >= 1", (rep or {}).get("invalid", 0) >= 1, f"invalid={(rep or {}).get('invalid')}")
    check("1.a marker is now red-X invalid", len(inv1) >= 1, f"{len(inv1)} invalid")
    check("1.NOTHING vanished (marker count held)", len(ms1) == len(ms0), f"{len(ms0)} -> {len(ms1)} markers")
    check("1.gate NOT green (red-X blocks it)", not dd_gate.gate_engine())

    # 2) REATTACH -> the door re-resolves; the red-X clears; green again
    print(f"\n[2] REATTACH room {leaf}")
    h2 = next((x for x in read_handles() if x["ri"] == leaf), None)
    move(h2["ref"], home[0], home[1], home[2])
    dd_drag.sync()
    inv2, _ = _invalid()
    check("2.red-X cleared", len(inv2) == 0, f"{len(inv2)} invalid")
    check("2.gate GREEN", dd_gate.gate_engine())

    n = sum(1 for c in results if c)
    print(f"\n==== RED-X: {n}/{len(results)} checks pass ====")
    return n == len(results)


if __name__ == "__main__":
    raise SystemExit(0 if main() else 1)
