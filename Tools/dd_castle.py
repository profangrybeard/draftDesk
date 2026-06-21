"""
Example layout — Castle Chorrol (Oblivion), the reference dictation that exercises the whole kit:
- entrance approach with a guard station + attached weapons room (ground floor),
- two mirrored side wings off the balcony (turn, a corridor with 3 rooms, a round-room stair shaft),
- a central royal suite off the balcony: antechamber -> large bedroom + bath + closet,
- two grand staircases bridging ground (Z=0) to the balcony/upper floor (Z=300).

Importing this module just BUILDS the layout (use `dd_castle.L`); running it directly
(`python dd_castle.py`) also pushes it onto the live generator via ddrun.
Copy this file as a template for your own dictated layouts.

Authored fully ON the 50cm grid: every footprint, height, and abutment gap is a whole cell (gap = T =
one cell), so the engine's snap is a no-op and shared walls dedup cleanly. Keep new layouts on-grid.
NOTE: 'round room' is a square placeholder — the kit is axis-aligned (no curved geometry yet).
"""
import math
from dd_author import Layout, WEST, EAST, SOUTH, NORTH, bit

Z = 300.0      # balcony / upper-floor level (6 cells)
CW = 400.0     # corridor width (hallways, 8 cells)
T = 50.0       # abutment gap = one grid cell (matches Layout.wall / the engine's BuiltWallT)


def total_run(dz, rise=18.0, run=30.0, max_angle=40.0):
    if dz <= 1:
        return 0.0
    nr = math.ceil(dz / rise)
    tanmax = math.tan(math.radians(min(max_angle, 89.0)))
    na = math.ceil(dz / (run * tanmax)) if tanmax > 1e-4 else nr
    return max(1, nr, na) * run


def side_wing(L, bal, door_y, sign):
    """A wing off a balcony door: short arm -> corner -> corridor with 3 rooms -> round-room shaft.
    sign = +1 fans North, -1 fans South. All gaps are one cell (T)."""
    arm = L.room(3050, door_y - CW / 2, 3450, door_y + CW / 2, floor=Z, height=300, open_edges=bit(WEST, EAST))
    L.link(bal, arm, "Doorway", position=0)                       # door in the balcony wall at door_y

    nx0, nx1 = 3500.0, 3500.0 + CW                                # corner: one cell east of the arm
    node = L.room(nx0, door_y - CW / 2, nx1, door_y + CW / 2, floor=Z, height=300)
    L.rooms[node]["OpenEdgeMask"] = bit(WEST) | (bit(NORTH) if sign > 0 else bit(SOUTH))

    near = door_y + sign * (CW / 2 + T)                           # one cell off the corner
    far = near + sign * 1800.0
    corr = L.room(nx0, min(near, far), nx1, max(near, far), floor=Z, height=300)
    L.rooms[corr]["OpenEdgeMask"] = bit(NORTH) | bit(SOUTH)        # open at both ends (corner + shaft)

    rx0 = nx1 + T                                                 # three standard rooms off the East side
    for k in (0, 1, 2):
        ry = near + sign * (400.0 + k * 550.0)                    # 500-deep rooms, one-cell gaps (550 pitch)
        r = L.room(rx0, ry - 250, rx0 + 550, ry + 250, floor=Z, height=300, open_edges=bit(WEST))
        L.link(corr, r, "Doorway", position=0)

    tc = far + sign * (T + 300.0)                                 # round-room (square placeholder) stair shaft
    L.room(nx0 - 50, tc - 300, nx1 + 50, tc + 300, floor=Z, height=300,
           open_edges=(bit(SOUTH) if sign > 0 else bit(NORTH)))


L = Layout()
RUN = total_run(Z)

# ---------------------------------------------------------------- Great Hall + entrance
hall = L.room(0, -1000, 3000, 1000, floor=0, height=800, open_edges=bit(EAST))
app = L.room(-1050, -CW / 2, -50, CW / 2, floor=0, height=400, open_edges=bit(EAST))
L.entry(app, WEST, "Doorway", width=350, height=300)
L.link(hall, app, "Doorway", position=0, width=350, height=300)   # grand castle doors

# guard station off the approach, with an attached weapons room
guard = L.room(-850, -750, -350, -250, floor=0, height=300, open_edges=bit(NORTH))
L.link(app, guard, "Doorway", position=0)
weapons = L.room(-850, -1300, -350, -800, floor=0, height=300, open_edges=bit(NORTH))
L.link(guard, weapons, "Doorway", position=0)

# ---------------------------------------------------------------- throne end
L.box(3000, 0, Z / 2, T, 2000 + 2 * T, Z)                         # ground wall behind the throne
bal = L.room(2600, -1000, 3000, 1000, floor=Z, height=300,
             open_edges=bit(NORTH, SOUTH), rail_edges=bit(WEST))
L.exterior(bal, WEST, "Open", position=-750, width=300)           # rail gaps for the two staircases
L.exterior(bal, WEST, "Open", position=750, width=300)
L.stair(along_x=True, start_u=2600 - RUN, cross_v=-750, from_z=0, to_z=Z, width=300, direction=1)
L.stair(along_x=True, start_u=2600 - RUN, cross_v=+750, from_z=0, to_z=Z, width=300, direction=1)
L.box(2800, 0, 50, 300, 750, 100)       # dais platform (grid-aligned: center on a cell)
L.box(2850, 0, 200, 150, 200, 200)      # throne (sits on the dais)
L.box(2950, -250, 100, 50, 200, 200)    # display cases
L.box(2950,  250, 100, 50, 200, 200)

# ---------------------------------------------------------------- the wings + royal suite
side_wing(L, bal, door_y=-600, sign=-1)   # south wing
side_wing(L, bal, door_y=+600, sign=+1)   # north wing (mirror)

# central royal suite: hall extends out -> antechamber -> bedroom + bath + closet (one-cell gaps)
ch = L.room(3050, -CW / 2, 4050, CW / 2, floor=Z, height=300, open_edges=bit(WEST))
L.link(bal, ch, "Doorway", position=0)
ante = L.room(4100, -400, 4700, 400, floor=Z, height=350, open_edges=bit(WEST))
L.link(ch, ante, "Doorway", position=0)
bed = L.room(4750, -700, 5750, 700, floor=Z, height=400, open_edges=bit(WEST))
L.link(ante, bed, "Doorway", position=0)
bath = L.room(4800, 750, 5300, 1250, floor=Z, height=300, open_edges=bit(SOUTH))
L.link(bed, bath, "Doorway", position=0)
closet = L.room(4800, -1250, 5300, -750, floor=Z, height=300, open_edges=bit(NORTH))
L.link(bed, closet, "Doorway", position=0)

if __name__ == "__main__":
    import ddrun
    print("rooms", len(L.rooms), "links", len(L.links), "stairs", len(L.stairs), "boxes", len(L.boxes))
    L.write_apply("_apply.py")
    print(ddrun.run("_apply.py"))
