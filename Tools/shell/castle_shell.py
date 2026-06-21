"""
End-to-end validation: Castle Chorrol (the real 50cm dd_castle layout) translated into the
new SHELL model and run through the emitter + 5 validator assertions. Proves the redesign
produces a watertight castle on a real, non-trivial two-level building. Run: python castle_shell.py

Translation rules (old model -> shell):
  - OpenEdgeMask pass-through  -> Passage threshold (full-overlap opening)
  - link / door                -> Doorway threshold
  - rail_edges                 -> Rail threshold
  - upper floor (Z=300)        -> Level 1; ground -> Level 0 (BaseZ invariant: 0 +250+50 = 300)
  - tall Great Hall            -> a level-0 room with a height override (800)
  - balcony                    -> a roofless level-1 mezzanine inside the hall (rail over the drop)
  - stairs                     -> mezzanine EDGE access (rail gaps) — a circulation detail, not a
                                  floor shaft, so not a watertightness element here; flights are fill.
  - dais / throne / cases / throne-wall box -> decorative solids, omitted from the shell test.
"""
from shell import (Room, Threshold, Level, Metrics, build_and_validate,
                   DOORWAY, PASSAGE, RAIL, WEST, EAST, SOUTH, NORTH)

T, Z, CW = 50.0, 300.0, 400.0
rooms, thr = [], []


def Rm(*a, **k):
    rooms.append(Room(*a, **k)); return len(rooms) - 1

def door(a, b, **k): thr.append(Threshold(a, b, DOORWAY, **k))
def passage(a, b):   thr.append(Threshold(a, b, PASSAGE))

# ---------------------------------------------------------------- ground floor (Level 0)
hall = Rm(0, -1000, 3000, 1000, level=0, height=800, name="hall")
app  = Rm(-1050, -CW / 2, -50, CW / 2, level=0, height=400, name="approach")
guard = Rm(-850, -750, -350, -250, level=0, height=300, name="guard")
weap = Rm(-850, -1300, -350, -800, level=0, height=300, name="weapons")
thr.append(Threshold(app, -1, DOORWAY, edge=WEST, is_entry=True, width=350, height=350, name="entry"))
door(hall, app, width=350, height=350)
door(app, guard)
door(guard, weap)

# ---------------------------------------------------------------- balcony: roofless mezzanine in the hall
bal = Rm(2600, -1000, 3000, 1000, level=1, ceil=False, name="balcony")   # open to the hall above
thr.append(Threshold(bal, -1, RAIL, edge=WEST, name="balcony rail"))     # guard rail over the hall drop

# ---------------------------------------------------------------- two mirrored wings off the balcony
def wing(door_y, sign, tag):
    arm = Rm(3050, door_y - CW / 2, 3450, door_y + CW / 2, level=1, name=f"{tag}arm")
    door(bal, arm)
    node = Rm(3500, door_y - CW / 2, 3900, door_y + CW / 2, level=1, name=f"{tag}node")
    passage(arm, node)
    near = door_y + sign * (CW / 2 + T)
    far = near + sign * 1800.0
    corr = Rm(3500, min(near, far), 3900, max(near, far), level=1, name=f"{tag}corr")
    passage(node, corr)
    rx0 = 3900 + T
    for k in (0, 1, 2):
        ry = near + sign * (400.0 + k * 500.0)
        r = Rm(rx0, ry - 200, rx0 + 500, ry + 200, level=1, name=f"{tag}room{k}")
        door(corr, r)
    tc = far + sign * (T + 300.0)
    shaft = Rm(3500, tc - 300, 3900, tc + 300, level=1, name=f"{tag}shaft")
    passage(corr, shaft)

wing(-600, -1, "S-")
wing(600, 1, "N-")

# ---------------------------------------------------------------- central royal suite (Level 1)
ch = Rm(3050, -CW / 2, 4050, CW / 2, level=1, name="chamber");   door(bal, ch)
ante = Rm(4100, -400, 4700, 400, level=1, name="antechamber");   door(ch, ante)
bed = Rm(4750, -700, 5750, 700, level=1, height=350, name="bedroom"); door(ante, bed)
bath = Rm(4800, 750, 5300, 1250, level=1, name="bath");          door(bed, bath)
closet = Rm(4800, -1250, 5300, -750, level=1, name="closet");    door(bed, closet)

# ---------------------------------------------------------------- build + validate
levels = [Level(0, 0, 250, 50), Level(1, 300, 300, 50)]
s, fails = build_and_validate(rooms, thr, levels, Metrics(grid=50, wall_thickness=30))


def _keytuple(key):
    """(cls, tag, a) — tag=1 for a roof slab bucket; matches the C++ Key ordering."""
    cls, k1 = key[0], key[1]
    return (cls, 1, k1[1]) if isinstance(k1, tuple) else (cls, 0, k1)


def _fmt(v):
    r = round(v)
    return str(int(r)) if abs(v - r) < 1e-6 else f"{v:.4f}"


def digest(shell):
    """Canonical bucket digest, byte-identical to ShellBatteryTest.cpp `digest` (parity proof)."""
    out = []
    for key in sorted(shell.buckets, key=_keytuple):
        cls, tag, a = _keytuple(key)
        line = f"{cls} {a} {tag} :"
        for r in sorted(shell.buckets[key].solid):
            line += " (" + ",".join(_fmt(x) for x in r) + ")"
        out.append(line)
    return "\n".join(out) + "\n"


import sys
if len(sys.argv) > 1 and sys.argv[1] == "digest":
    sys.stdout.write(digest(s))
    raise SystemExit(0)

print(f"Castle Chorrol -> SHELL:  {len(rooms)} rooms, {len(thr)} thresholds, 2 levels")
print(f"loud notices (warnings): {len(s.warnings)}")
for w in s.warnings:
    print("   -", w)
print(f"validator failures: {len(fails)}")
for f in fails:
    print("   !", f)
print("\n" + ("==> CASTLE IS WATERTIGHT <==" if not fails else "==> NOT WATERTIGHT <=="))
raise SystemExit(0 if not fails else 1)
