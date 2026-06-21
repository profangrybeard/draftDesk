"""
Example layout — Castle Chorrol (Oblivion), the reference dictation, re-authored on the SHELL model.
This mirrors the proven `Tools/shell/castle_shell.py` (24 rooms, 24 thresholds, 2 levels, watertight)
and adds the throne-room character (dais / throne / cases / throne wall) plus a stairwell up to the
balcony for walkability.

- ground floor (Level 0): a tall Great Hall + an entrance approach with a guard station + weapons room.
- upper floor (Level 1): a balcony (roofless mezzanine open to the tall hall, rail over the drop),
  two mirrored side wings (arm -> corner -> corridor with 3 rooms -> shaft), and a royal suite
  (chamber -> antechamber -> bedroom + bath + closet).

Importing this module BUILDS the layout (use `dd_castle.L`); running it (`python dd_castle.py`) also
pushes it onto the live generator via ddrun. Copy as a template for your own dictated layouts.

Authored fully ON the 50cm grid (gap = T = one cell), so the engine's snap is a no-op and shared walls
dedup cleanly. 'Round room' is a square placeholder — the kit is axis-aligned (no curves yet).
There is NO OpenEdgeMask: a corridor mouth is a Passage threshold; a balcony edge is a Rail threshold.
"""
import math
from dd_author import Layout, WEST, EAST, SOUTH, NORTH

CW = 400.0     # corridor / hall width (8 cells)
Z = 300.0      # balcony / upper-floor base Z (6 cells)
T = 50.0       # abutment gap = one grid cell


def total_run(dz, rise=18.0, run=30.0, max_angle=40.0):
    """Horizontal run of a flight climbing dz (mirrors the engine's StepCount * StepRun)."""
    if dz <= 1:
        return 0.0
    nr = math.ceil(dz / rise)
    tanmax = math.tan(math.radians(min(max_angle, 89.0)))
    na = math.ceil(dz / (run * tanmax)) if tanmax > 1e-4 else nr
    return max(1, nr, na) * run


def side_wing(L, bal, door_y, sign):
    """A wing off a balcony door: short arm -> corner -> corridor with 3 rooms -> shaft.
    sign = +1 fans North, -1 fans South. Every gap is one cell (T); openness is relational."""
    arm = L.room(3050, door_y - CW / 2, 3450, door_y + CW / 2, level=1)
    L.door(bal, arm)                                  # door in the balcony wall at door_y

    node = L.room(3500, door_y - CW / 2, 3900, door_y + CW / 2, level=1)
    L.passage(arm, node)                              # open corner

    near = door_y + sign * (CW / 2 + T)
    far = near + sign * 1800.0
    corr = L.room(3500, min(near, far), 3900, max(near, far), level=1)
    L.passage(node, corr)

    rx0 = 3900 + T                                    # three rooms off the East side
    for k in (0, 1, 2):
        ry = near + sign * (400.0 + k * 500.0)
        r = L.room(rx0, ry - 200, rx0 + 500, ry + 200, level=1)
        L.door(corr, r)

    tc = far + sign * (T + 300.0)                     # shaft (square placeholder for the round room)
    shaft = L.room(3500, tc - 300, 3900, tc + 300, level=1)
    L.passage(corr, shaft)


L = Layout()
L.level(0, 250, 50)     # ground:  0 + 250 + 50 == 300 (the BaseZ invariant)
L.level(Z, 300, 50)     # balcony / upper floor

# ---------------------------------------------------------------- Great Hall + entrance (Level 0)
hall = L.room(0, -1000, 3000, 1000, level=0, height=800)       # tall hall; balcony is a mezzanine in it
app = L.room(-1050, -CW / 2, -50, CW / 2, level=0, height=400)
L.entry(app, WEST, width=350, height=350)                      # grand castle doors
L.door(hall, app, width=350, height=350)

guard = L.room(-850, -750, -350, -250, level=0, height=300)
L.door(app, guard)
weapons = L.room(-850, -1300, -350, -800, level=0, height=300)
L.door(guard, weapons)

# ---------------------------------------------------------------- balcony + grand double staircase (Level 1)
bal = L.room(2600, -1000, 3000, 1000, level=1, ceil=False)     # roofless mezzanine, open to the hall above
L.rail(bal, edge=WEST)                                         # guard rail over the hall drop
# Two grand staircases climb the back of the hall up to the balcony's front (west) edge, each landing
# through a 300-wide gap in the rail at y = -/+750. The flights are FILL (solid stepped boxes), not a
# slab pierce — you walk UP them from the hall floor onto the balcony.
RUN = total_run(Z)
for cv in (-750.0, +750.0):
    L.flight(along_x=True, start_u=2600 - RUN, cross_v=cv, from_z=0.0, to_z=Z, width=300)
    L.exterior(bal, WEST, "Passage", position=cv, width=300)   # rail gap at the landing

# ---------------------------------------------------------------- the wings + royal suite (Level 1)
side_wing(L, bal, door_y=-600, sign=-1)   # south wing
side_wing(L, bal, door_y=+600, sign=+1)   # north wing (mirror)

ch = L.room(3050, -CW / 2, 4050, CW / 2, level=1);   L.door(bal, ch)
ante = L.room(4100, -400, 4700, 400, level=1);        L.door(ch, ante)
bed = L.room(4750, -700, 5750, 700, level=1, height=350); L.door(ante, bed)
bath = L.room(4800, 750, 5300, 1250, level=1);        L.door(bed, bath)
closet = L.room(4800, -1250, 5300, -750, level=1);    L.door(bed, closet)

# ---------------------------------------------------------------- throne-room character (decorative)
L.box(3000, 0, Z / 2, T, 2000 + 2 * T, Z)   # wall behind the throne (under the balcony)
L.box(2800, 0, 50, 300, 750, 100)           # dais platform
L.box(2850, 0, 200, 150, 200, 200)          # throne
L.box(2950, -250, 100, 50, 200, 200)        # display cases
L.box(2950,  250, 100, 50, 200, 200)

if __name__ == "__main__":
    import ddrun
    print("levels", len(L.levels), "rooms", len(L.rooms), "thresholds", len(L.thresholds), "boxes", len(L.boxes))
    L.write_apply("_apply.py")
    print(ddrun.run("_apply.py"))
