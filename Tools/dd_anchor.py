"""
dd_anchor — project threshold markers onto their resolved plane (the "ProjectAnchorToPlane" step).

The single source of truth for a threshold's world opening position + which way it slides. Computed
with shell.face_connection — the SAME geometry the engine core (DdShellCore) uses, proven byte-identical
by the digest test — so the Python projection and the engine carve agree by construction (not a
drift-prone mirror). Used by dd_seedmarkers (where to spawn each marker) and dd_sync (project a dragged
marker back to a Position / detect a perpendicular Stage-B reshape).

World space mirrors the engine: world = local - (dx, dy), Z from the level base, where (dx,dy) is the
NormalizeToEntry shift (entry threshold -> origin).
"""
import sys

sys.path.insert(0, "shell")
import shell as S
from dd_navcheck import _shift   # reuse the NormalizeToEntry shift

EDGE = {"West": 0, "East": 1, "South": 2, "North": 3}


def _shell(L):
    rooms = [S.Room(r["Min"][0], r["Min"][1], r["Max"][0], r["Max"][1],
                    level=r["Level"], floor_z=(None if r["FloorZ"] < 0 else r["FloorZ"]),
                    height=r["Height"], floor=r["bFloor"], ceil=r["bCeil"]) for r in L.rooms]
    levels = [S.Level(lv["Index"], lv["BaseZ"], lv["Height"], lv["SlabT"]) for lv in L.levels]
    s = S.Shell(rooms, [], levels, S.Metrics(grid=L.snap or 50.0, wall_thickness=30))
    return s, rooms


def label_of(L, i):
    t = L.thresholds[i]
    if t["RoomB"] != -1:
        return f"{t['RoomA']}-{t['RoomB']}"
    return "entry" if t["bIsEntry"] else f"ext{i}"


def seeds(L):
    """Per-threshold resolved opening: world (x,y,z) + the face frame (axis/plane/overlap, LOCAL).
    axis 0 = constant-X wall (slides in Y); axis 1 = constant-Y wall (slides in X)."""
    s, rooms = _shell(L)
    dx, dy, minz = _shift(L)
    T = s.metrics.T   # @property, not a method
    out = []
    for i, t in enumerate(L.thresholds):
        a, b, kind, pos = t["RoomA"], t["RoomB"], t["Kind"], t["Position"]
        lvl = rooms[a].level if 0 <= a < len(rooms) else 0
        lz = s.levels[lvl].base_z if 0 <= lvl < len(s.levels) else 0.0
        if b == -1:                                  # exterior: opening on a named edge
            r = rooms[a]
            e = t["ExteriorEdge"]
            if e == "West":    axis, plane, lo, hi = 0, r.x0 - T / 2, r.y0, r.y1
            elif e == "East":  axis, plane, lo, hi = 0, r.x1 + T / 2, r.y0, r.y1
            elif e == "South": axis, plane, lo, hi = 1, r.y0 - T / 2, r.x0, r.x1
            else:              axis, plane, lo, hi = 1, r.y1 + T / 2, r.x0, r.x1
            pa = pb = plane
        else:
            fc = s.face_connection(a, b)             # (axis, pa, pb, lo, hi) in LOCAL coords, or None
            if fc is None:
                continue                             # unresolved (no facing) — no marker
            axis, pa, pb, lo, hi = fc
            plane = (pa + pb) / 2.0                   # the wall centreline between the two rooms
        along = (lo + hi) / 2.0 + pos
        lx, ly = (plane, along) if axis == 0 else (along, plane)
        out.append({"i": i, "label": label_of(L, i), "x": lx - dx, "y": ly - dy, "z": lz - minz + 50.0,
                    "axis": axis, "plane": plane, "pa": pa, "pb": pb, "lo": lo, "hi": hi,
                    "kind": kind, "width": t["Width"], "height": t["Height"], "roomb": b,
                    "is_entry": t["bIsEntry"]})
    return out


def project(seed, world_x, world_y):
    """Project a dragged marker's world (x,y) back onto its seed's face.
    Returns (position, perp): position = signed along-wall offset from the overlap centre (the new
    Threshold.Position); perp = perpendicular distance off the wall plane (the Stage-B reshape signal)."""
    dx, dy = world_x, world_y                        # caller passes ALREADY-local coords (see dd_sync)
    along = dy if seed["axis"] == 0 else dx           # axis 0: along = Y; axis 1: along = X
    perp = dx if seed["axis"] == 0 else dy
    center = (seed["lo"] + seed["hi"]) / 2.0
    return along - center, perp - seed["plane"]
