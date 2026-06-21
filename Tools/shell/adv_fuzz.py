"""
Adversarial oracle + fuzzer for the SHELL watertightness validator.

INDEPENDENT oracle: for each room and each of its 6 intended faces, sample points on a
fine sub-grid across the face footprint. A sample passes iff it is inside some emitted
SOLID rect of that face's bucket OR inside a declared APERTURE rect of that bucket.
A sample that is neither => REAL LEAK. A point in TWO solids => double-cover.

We do NOT reuse the validator's area accounting. We sample geometry directly.
"""
import math
import random
import itertools
from shell import (Room, Threshold, Level, Metrics, build_and_validate,
                   DOORWAY, PASSAGE, WINDOW, RAIL, STAIRWELL, HATCH, ATRIUM,
                   VERTICAL, HORIZONTAL, WEST, EAST, SOUTH, NORTH,
                   CLASS_X, CLASS_Y, CLASS_SLAB)

EDGE_NAME = {WEST: "W", EAST: "E", SOUTH: "S", NORTH: "N"}


def _round(v):
    return int(round(v))


def pt_in_rect(a, b, rect, closed_hi=False):
    """Is point (a,b) inside rect (alo,ahi,blo,bhi)? Half-open [lo,hi) by default."""
    alo, ahi, blo, bhi = rect[0], rect[1], rect[2], rect[3]
    if closed_hi:
        return alo <= a <= ahi and blo <= b <= bhi
    return alo <= a < ahi and blo <= b < bhi


def count_solid_cover(a, b, solids):
    """Number of solid rects covering point (a,b). Half-open."""
    n = 0
    for r in solids:
        if r[0] <= a < r[1] and r[2] <= b < r[3]:
            n += 1
    return n


def in_any_aperture(a, b, apertures):
    """Point in any aperture rect (closed, since aperture is a designed opening). apertures: (alo,ahi,blo,bhi,...)"""
    for r in apertures:
        if r[0] <= a <= r[1] and r[2] <= b <= r[3]:
            return True
    return False


def sample_face(shell, key, foot, label, nsamp=13):
    """Sample the face footprint `foot`=(alo,ahi,blo,bhi) in bucket `key`.
    Returns list of (kind, detail) findings: 'leak' or 'double'."""
    findings = []
    b = shell.buckets.get(key)
    solids = b.solid if b else []
    apertures = b.apertures if b else []
    alo, ahi, blo, bhi = foot
    da = ahi - alo
    db = bhi - blo
    if da <= 0 or db <= 0:
        return findings
    # Sample interior points using an irrational offset (golden ratio frac) so samples
    # essentially never land exactly on a rect boundary => half-open/closed ambiguity at
    # internal seams cannot create phantom leaks/doubles. Real gaps span many cells.
    leaks = 0
    doubles = 0
    leak_pt = None
    dbl_pt = None
    na = nsamp
    nb = nsamp
    OFF = 0.3819660112501051  # 1 - 1/phi, irrational-ish
    for i in range(na):
        a = alo + da * (i + OFF) / na
        for j in range(nb):
            bb = blo + db * (j + OFF) / nb
            cov = count_solid_cover(a, bb, solids)
            apc = in_any_aperture(a, bb, apertures)
            if cov == 0 and not apc:
                leaks += 1
                if leak_pt is None:
                    leak_pt = (a, bb)
            if cov >= 2:
                doubles += 1
                if dbl_pt is None:
                    dbl_pt = (a, bb)
    if leaks:
        findings.append(("leak", label, key, leak_pt, leaks, na * nb))
    if doubles:
        findings.append(("double", label, key, dbl_pt, doubles, na * nb))
    return findings


def oracle_faces(shell):
    """Enumerate every room's 6 intended faces with footprint & bucket key, mirroring pass1.
    Returns list of (key, foot, label)."""
    m = shell.metrics
    T = m.T
    out = []
    for idx, r in enumerate(shell.rooms):
        if r.W() <= 1 or r.D() <= 1:
            continue
        fz, H = shell.eff(idx)
        rails = shell._rail_edges(idx)
        def zt(edge):
            return fz + (m.half_wall if edge in rails else H)
        # walls
        out.append(((CLASS_X, _round(r.x0 - T / 2)), (r.y0 - T, r.y1 + T, fz, zt(WEST)), f"room{idx} W"))
        out.append(((CLASS_X, _round(r.x1 + T / 2)), (r.y0 - T, r.y1 + T, fz, zt(EAST)), f"room{idx} E"))
        out.append(((CLASS_Y, _round(r.y0 - T / 2)), (r.x0 - T, r.x1 + T, fz, zt(SOUTH)), f"room{idx} S"))
        out.append(((CLASS_Y, _round(r.y1 + T / 2)), (r.x0 - T, r.x1 + T, fz, zt(NORTH)), f"room{idx} N"))
        foot = (r.x0 - T, r.x1 + T, r.y0 - T, r.y1 + T)
        if r.floor:
            out.append(((CLASS_SLAB, r.level), foot, f"room{idx} floor"))
        if r.ceil:
            out.append(((CLASS_SLAB, shell._ceiling_interface(fz + H)), foot, f"room{idx} ceil"))
    return out


def oracle_check(shell):
    """Run the full oracle. Returns (leaks, doubles) lists of findings."""
    leaks = []
    doubles = []
    for key, foot, label in oracle_faces(shell):
        finds = sample_face(shell, key, foot, label)
        for f in finds:
            if f[0] == "leak":
                leaks.append(f)
            else:
                doubles.append(f)
    # Also global double-cover scan per bucket (independent of faces)
    return leaks, doubles


def global_double_cover(shell):
    """Per-bucket: sample bucket bounding box, find any point covered by >=2 solid rects."""
    out = []
    for key, b in shell.buckets.items():
        if not b.solid:
            continue
        alo = min(r[0] for r in b.solid); ahi = max(r[1] for r in b.solid)
        blo = min(r[2] for r in b.solid); bhi = max(r[3] for r in b.solid)
        da = ahi - alo; db = bhi - blo
        if da <= 0 or db <= 0:
            continue
        n = 17
        for i in range(n):
            a = alo + da * (i + 0.5) / n
            for j in range(n):
                bb = blo + db * (j + 0.5) / n
                if count_solid_cover(a, bb, b.solid) >= 2:
                    out.append((key, (a, bb)))
                    break
            else:
                continue
            break
    return out


# ============================================================ fuzzer
def fmt_room(r):
    parts = [f"{r.x0:g}", f"{r.y0:g}", f"{r.x1:g}", f"{r.y1:g}"]
    extra = []
    if r.level: extra.append(f"level={r.level}")
    if r.floor_z is not None: extra.append(f"floor_z={r.floor_z:g}")
    if r.height: extra.append(f"height={r.height:g}")
    if not r.floor: extra.append("floor=False")
    if not r.ceil: extra.append("ceil=False")
    return "Room(" + ", ".join(parts + extra) + ")"


def fmt_thr(t):
    kindmap = {DOORWAY:"DOORWAY", PASSAGE:"PASSAGE", WINDOW:"WINDOW", RAIL:"RAIL",
               STAIRWELL:"STAIRWELL", HATCH:"HATCH", ATRIUM:"ATRIUM"}
    parts = [str(t.room_a), str(t.room_b), kindmap.get(t.kind, repr(t.kind))]
    extra = []
    if t.plane != VERTICAL: extra.append("plane=HORIZONTAL")
    if t.position: extra.append(f"position={t.position:g}")
    if t.position2: extra.append(f"position2={t.position2:g}")
    if t.width: extra.append(f"width={t.width:g}")
    if t.depth: extra.append(f"depth={t.depth:g}")
    if t.height: extra.append(f"height={t.height:g}")
    if t.sill: extra.append(f"sill={t.sill:g}")
    if t.is_entry: extra.append("is_entry=True")
    if t.edge != WEST: extra.append(f"edge={EDGE_NAME[t.edge]}")
    return "Threshold(" + ", ".join(parts + extra) + ")"


def fmt_case(rooms, thrs, levels=None, metrics=None):
    lines = []
    lines.append("rooms = [" + ", ".join(fmt_room(r) for r in rooms) + "]")
    lines.append("thrs = [" + ", ".join(fmt_thr(t) for t in thrs) + "]")
    if levels:
        ls = ", ".join(f"Level({l.index}, {l.base_z:g}, {l.height:g}, {l.slab_t:g})" for l in levels)
        lines.append("levels = [" + ls + "]")
    else:
        lines.append("levels = None")
    if metrics:
        lines.append(f"metrics = Metrics(grid={metrics.grid:g}, wall_thickness={metrics.wall_thickness:g}, ceiling_min={metrics.ceiling_min:g})")
    else:
        lines.append("metrics = None")
    return "\n".join(lines)


def rand_coord(rng, grid_bias=0.6):
    if rng.random() < grid_bias:
        return rng.randint(-6, 18) * 50
    return rng.randint(-300, 900)


def rand_room(rng, level=0, floor_z=None):
    x0 = rand_coord(rng)
    y0 = rand_coord(rng)
    w = rng.choice([50, 100, 150, 200, 300, 400, 50, 100, 200, 1, 0])  # include degenerate
    d = rng.choice([50, 100, 150, 200, 300, 400, 50, 100, 200, 1, 0])
    x1 = x0 + w
    y1 = y0 + d
    floor = rng.random() > 0.15
    ceil = rng.random() > 0.15
    height = 0.0
    if rng.random() < 0.25:
        height = rng.choice([200, 300, 400, 450, 650])
    return Room(x0, y0, x1, y1, level=level, floor_z=floor_z, height=height, floor=floor, ceil=ceil)


def rand_threshold(rng, nrooms):
    a = rng.randrange(nrooms)
    exterior = rng.random() < 0.3
    if exterior:
        b = -1
        edge = rng.choice([WEST, EAST, SOUTH, NORTH])
    else:
        b = rng.randrange(nrooms)
        edge = WEST
    kind = rng.choice([DOORWAY, PASSAGE, WINDOW, DOORWAY, PASSAGE])
    t = Threshold(a, b, kind=kind, edge=edge)
    if rng.random() < 0.5:
        t.position = rng.choice([-200, -100, -50, 50, 100, 200, 99999])
    if rng.random() < 0.4:
        t.width = rng.choice([50, 100, 200, 240, 100000])
    if kind == WINDOW and rng.random() < 0.5:
        t.sill = rng.choice([50, 100, 150])
        t.height = rng.choice([100, 130, 200])
    return t


def gen_flat_case(rng):
    n = rng.randint(1, 5)
    rooms = [rand_room(rng) for _ in range(n)]
    thrs = []
    for _ in range(rng.randint(0, n + 1)):
        thrs.append(rand_threshold(rng, n))
    return rooms, thrs, None, None


def gen_stack_case(rng):
    """Multi-level stack with random heights/slabs (well-formed BaseZ chain)."""
    nlev = rng.randint(2, 3)
    grid = 50
    base = 0.0
    levels = []
    h = rng.choice([200, 250, 300])
    slab = rng.choice([50])
    for i in range(nlev):
        levels.append(Level(i, base, h, slab))
        base = base + h + slab
    rooms = []
    for i in range(nlev):
        nr = rng.randint(1, 2)
        for _ in range(nr):
            r = rand_room(rng, level=i)
            r.floor_z = levels[i].base_z if rng.random() < 0.8 else None
            r.height = 0.0  # use level height mostly
            if rng.random() < 0.2:
                r.height = h
            rooms.append(r)
    thrs = []
    # add some horizontal thresholds between stacked rooms
    for _ in range(rng.randint(0, 2)):
        if len(rooms) >= 2:
            a = rng.randrange(len(rooms)); b = rng.randrange(len(rooms))
            if a != b:
                kind = rng.choice([STAIRWELL, ATRIUM, HATCH])
                t = Threshold(a, b if kind == STAIRWELL else -1, kind=kind, plane=HORIZONTAL)
                if rng.random() < 0.5:
                    t.width = rng.choice([100, 200, 300])
                    t.depth = rng.choice([100, 200, 300])
                thrs.append(t)
    for _ in range(rng.randint(0, 2)):
        thrs.append(rand_threshold(rng, len(rooms)))
    return rooms, thrs, levels, None


def run_fuzz(seed=0, ncases=400):
    rng = random.Random(seed)
    crashes = []
    false_neg = []   # validator says [] but oracle finds leak/double
    false_pos = []   # validator flags but oracle says watertight
    npass = 0
    nbuilt = 0
    for c in range(ncases):
        flat = rng.random() < 0.7
        try:
            if flat:
                rooms, thrs, levels, metrics = gen_flat_case(rng)
            else:
                rooms, thrs, levels, metrics = gen_stack_case(rng)
        except Exception as e:
            continue
        try:
            shell, fails = build_and_validate(rooms, thrs, levels, metrics)
        except Exception as e:
            import traceback
            crashes.append((fmt_case(rooms, thrs, levels, metrics), repr(e), traceback.format_exc()))
            continue
        nbuilt += 1
        try:
            leaks, doubles = oracle_check(shell)
            gdc = global_double_cover(shell)
        except Exception as e:
            import traceback
            crashes.append((fmt_case(rooms, thrs, levels, metrics), "ORACLE " + repr(e), traceback.format_exc()))
            continue
        oracle_bad = bool(leaks) or bool(doubles) or bool(gdc)
        validator_bad = bool(fails)
        if not validator_bad and oracle_bad:
            false_neg.append((fmt_case(rooms, thrs, levels, metrics), leaks, doubles, gdc, fails))
        elif validator_bad and not oracle_bad:
            false_pos.append((fmt_case(rooms, thrs, levels, metrics), fails))
        else:
            npass += 1
    return dict(crashes=crashes, false_neg=false_neg, false_pos=false_pos,
                npass=npass, nbuilt=nbuilt, ncases=ncases)


if __name__ == "__main__":
    import sys
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    ncases = int(sys.argv[2]) if len(sys.argv) > 2 else 600
    res = run_fuzz(seed, ncases)
    print(f"built {res['nbuilt']}/{res['ncases']}  agree-watertight {res['npass']}")
    print(f"CRASHES={len(res['crashes'])}  FALSE_NEG={len(res['false_neg'])}  FALSE_POS={len(res['false_pos'])}")
    for label, lst in (("FALSE_NEG", res['false_neg']), ("FALSE_POS", res['false_pos'])):
        for item in lst[:5]:
            print("\n==== " + label + " ====")
            print(item[0])
            if label == "FALSE_NEG":
                print("leaks:", item[1][:2])
                print("doubles:", item[2][:2])
                print("gdc:", item[3][:2])
            else:
                print("validator fails:", item[1][:3])
    for item in res['crashes'][:5]:
        print("\n==== CRASH ====")
        print(item[0])
        print(item[1])
