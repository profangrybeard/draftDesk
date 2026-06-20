"""
Example layout — Castle Chorrol (Oblivion), the reference dictation that exercises the whole kit:
- entrance approach with a guard station + attached weapons room (ground floor),
- two mirrored side wings off the balcony (turn, a corridor with 3 rooms, a round-room stair shaft),
- a central royal suite off the balcony: antechamber -> large bedroom + bath + closet,
- two grand staircases bridging ground (Z=0) to the balcony/upper floor (Z=320).

Importing this module just BUILDS the layout (use `dd_castle.L`); running it directly
(`python dd_castle.py`) also pushes it onto the live generator via ddrun.
Copy this file as a template for your own dictated layouts.
NOTE: 'round room' is a square placeholder — the kit is axis-aligned (no curved geometry yet).
"""
import math
from dd_author import Layout, WEST, EAST, SOUTH, NORTH, bit

Z = 320.0      # balcony / upper-floor level
CW = 400.0     # corridor width (hallways)
T = 30.0


def total_run(dz, rise=18.0, run=30.0, max_angle=40.0):
    if dz <= 1:
        return 0.0
    nr = math.ceil(dz / rise)
    tanmax = math.tan(math.radians(min(max_angle, 89.0)))
    na = math.ceil(dz / (run * tanmax)) if tanmax > 1e-4 else nr
    return max(1, nr, na) * run


def side_wing(L, bal, door_y, sign):
    """A wing off a balcony door: short arm -> corner -> corridor with 3 rooms -> round-room shaft.
    sign = +1 fans North, -1 fans South."""
    arm = L.room(3030, door_y - CW / 2, 3430, door_y + CW / 2, floor=Z, height=300, open_edges=bit(WEST, EAST))
    L.link(bal, arm, "Doorway", position=0)                       # door in the balcony wall at door_y

    nx0, nx1 = 3460.0, 3460.0 + CW                                # corner + corridor share the hallway width
    node = L.room(nx0, door_y - CW / 2, nx1, door_y + CW / 2, floor=Z, height=300)
    L.rooms[node]["OpenEdgeMask"] = bit(WEST) | (bit(NORTH) if sign > 0 else bit(SOUTH))

    near = door_y + sign * (CW / 2 + T)
    far = near + sign * 1800.0
    corr = L.room(nx0, min(near, far), nx1, max(near, far), floor=Z, height=300)
    L.rooms[corr]["OpenEdgeMask"] = bit(NORTH) | bit(SOUTH)        # open at both ends (corner + shaft)

    rx0 = nx1 + T                                                 # three standard rooms off the East side
    for k in (0, 1, 2):
        ry = near + sign * (400.0 + k * 520.0)
        r = L.room(rx0, ry - 230, rx0 + 560, ry + 230, floor=Z, height=300, open_edges=bit(WEST))
        L.link(corr, r, "Doorway", position=0)

    tc = far + sign * (T + 280.0)                                 # round-room (square placeholder) stair shaft
    L.room(nx0 - 60, tc - 280, nx1 + 60, tc + 280, floor=Z, height=300,
           open_edges=(bit(SOUTH) if sign > 0 else bit(NORTH)))


L = Layout()
RUN = total_run(Z)

# ---------------------------------------------------------------- Great Hall + entrance
hall = L.room(0, -1000, 3000, 1000, floor=0, height=820, open_edges=bit(EAST))
app = L.room(-1030, -CW / 2, -30, CW / 2, floor=0, height=420, open_edges=bit(EAST))
L.entry(app, WEST, "Doorway", width=360, height=320)
L.link(hall, app, "Doorway", position=0, width=360, height=320)   # grand castle doors

# guard station off the approach, with an attached weapons room
guard = L.room(-830, -710, -330, -210, floor=0, height=300, open_edges=bit(NORTH))
L.link(app, guard, "Doorway", position=0)
weapons = L.room(-830, -1240, -330, -740, floor=0, height=300, open_edges=bit(NORTH))
L.link(guard, weapons, "Doorway", position=0)

# ---------------------------------------------------------------- throne end
L.box(3000, 0, Z / 2, T, 2000 + 2 * T, Z)                          # ground wall behind the throne
bal = L.room(2600, -1000, 3000, 1000, floor=Z, height=300,
             open_edges=bit(NORTH, SOUTH), rail_edges=bit(WEST))
L.exterior(bal, WEST, "Open", position=-750, width=320)            # rail gaps for the two staircases
L.exterior(bal, WEST, "Open", position=750, width=320)
L.stair(along_x=True, start_u=2600 - RUN, cross_v=-750, from_z=0, to_z=Z, width=300, direction=1)
L.stair(along_x=True, start_u=2600 - RUN, cross_v=+750, from_z=0, to_z=Z, width=300, direction=1)
L.box(2800, 0, 36, 320, 760, 72)        # dais platform
L.box(2575, 0, 18, 160, 760, 36)        # front step
L.box(2870, 0, 146, 150, 220, 148)      # throne
L.box(2955, -260, 60, 50, 200, 120)     # display cases
L.box(2955,  260, 60, 50, 200, 120)

# ---------------------------------------------------------------- the wings + royal suite
side_wing(L, bal, door_y=-600, sign=-1)   # south wing
side_wing(L, bal, door_y=+600, sign=+1)   # north wing (mirror)

# central royal suite: hall extends out -> antechamber -> bedroom + bath + closet
ch = L.room(3030, -CW / 2, 4030, CW / 2, floor=Z, height=300, open_edges=bit(WEST))
L.link(bal, ch, "Doorway", position=0)
ante = L.room(4060, -400, 4660, 400, floor=Z, height=320, open_edges=bit(WEST))
L.link(ch, ante, "Doorway", position=0)
bed = L.room(4690, -700, 5690, 700, floor=Z, height=360, open_edges=bit(WEST))
L.link(ante, bed, "Doorway", position=0)
bath = L.room(4790, 730, 5290, 1230, floor=Z, height=300, open_edges=bit(SOUTH))
L.link(bed, bath, "Doorway", position=0)
closet = L.room(4790, -1230, 5290, -730, floor=Z, height=300, open_edges=bit(NORTH))
L.link(bed, closet, "Doorway", position=0)

if __name__ == "__main__":
    import ddrun
    print("rooms", len(L.rooms), "links", len(L.links), "stairs", len(L.stairs), "boxes", len(L.boxes))
    L.write_apply("_apply.py")
    print(ddrun.run("_apply.py"))
