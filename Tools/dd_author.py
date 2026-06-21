"""
draftDesk SHELL authoring layer — compose a dictated layout from pieces, emit the Custom-preset
payload, and write a UE-sandbox apply script that pushes it onto the generator (run via ddrun.py).

Pieces map to the engine's authored arrays (SHELL v1 model):
  level                                   -> AuthoredLevels (FDdLevel)
  room / corridor                         -> AuthoredRooms (FDdRoom)
  door/passage/window/rail/stairwell/...  -> AuthoredThresholds (FDdThreshold)  [the ONE opening primitive]
  cover/crate/dais/pillar/ledge           -> AuthoredBoxes (FDraftDeskBlock)

There is no OpenEdgeMask / RailEdgeMask: openness is RELATIONAL. A wall is solid by construction and
opens ONLY where a threshold proves a connection. Coordinates are actor-local cm, +X into the space.
Abutting rooms are placed with a WallThickness gap so their wall planes coincide and the engine dedups
to ONE shared wall. Levels are the discrete vertical truth (BaseZ[n+1] = BaseZ[n] + Height[n] + SlabT);
a room references one by index.

(The marker-seeding mirror — threshold_points/_face/_normalize_shift — is intentionally gone; the
engine now owns projection via ProjectAnchorToPlane, added in the sync phase.)
"""
import json
import math
from dd_config import GEN

WEST, EAST, SOUTH, NORTH = 0, 1, 2, 3
EDGE_NAME = {WEST: "West", EAST: "East", SOUTH: "South", NORTH: "North"}

GRID = 50.0   # default authoring grid (cm); matches FDraftDeskMetrics.GridSnap. Precision, not art.


class Layout:
    def __init__(self, wall=None, corridor=200.0, snap=GRID, ceiling_min=300.0):
        self.snap = float(snap)
        if wall is None:
            wall = self.snap if self.snap > 0 else 30.0
        self.wall = self._snap_up(wall)
        self.cw = corridor
        self.ceiling_min = ceiling_min
        self.levels = []
        self.rooms = []
        self.thresholds = []
        self.flights = []
        self.boxes = []

    # --- grid helpers ---
    def _snap(self, v):
        return round(float(v) / self.snap) * self.snap if self.snap > 0 else float(v)

    def _snap_up(self, v):
        return max(self.snap, math.ceil(float(v) / self.snap) * self.snap) if self.snap > 0 else float(v)

    # --- levels (the discrete vertical truth) ---
    def level(self, base_z, height=None, slab_t=0.0):
        """Add a storey. height defaults to ceiling_min; slab_t 0 => engine BuiltWallT.
        INVARIANT: BaseZ[n+1] = BaseZ[n] + Height[n] + SlabT (the engine warns + snaps if violated)."""
        h = self.ceiling_min if height is None else height
        self.levels.append(dict(Index=len(self.levels), BaseZ=float(self._snap(base_z)),
                                Height=float(self._snap(h)), SlabT=float(slab_t)))
        return len(self.levels) - 1

    def stacked_levels(self, count, storey=None, slab_t=None):
        """Build `count` contiguous stacked levels; returns the level count."""
        storey = self.ceiling_min if storey is None else storey
        slab = self.wall if slab_t is None else slab_t
        z = 0.0
        for _ in range(count):
            self.level(z, storey, slab)
            z += storey + slab
        return count

    # --- rooms ---
    def room(self, x0, y0, x1, y1, level=0, height=0.0, floor=True, ceil=True, columns=False, floor_z=-1.0):
        if self.levels and not (0 <= level < len(self.levels)):
            raise ValueError(f"room level {level} out of range (have {len(self.levels)} levels)")
        x0, y0, x1, y1 = self._snap(x0), self._snap(y0), self._snap(x1), self._snap(y1)
        h = self._snap(height) if height else float(height)
        self.rooms.append(dict(Min=(float(x0), float(y0)), Max=(float(x1), float(y1)),
                               Level=int(level), FloorZ=float(floor_z), Height=float(h),
                               bFloor=bool(floor), bCeil=bool(ceil), bColumns=bool(columns)))
        return len(self.rooms) - 1

    def corridor(self, x0, length, width=None, level=0, axis="X", y_center=0.0, **kw):
        w = self.cw if width is None else width
        if axis == "X":
            return self.room(x0, y_center - w / 2, x0 + length, y_center + w / 2, level=level, **kw)
        return self.room(y_center - w / 2, x0, y_center + w / 2, x0 + length, level=level, **kw)

    def east_of(self, idx, depth, width=None, level=None, align="center", **kw):
        r = self.rooms[idx]
        w = (r["Max"][1] - r["Min"][1]) if width is None else width
        cy = (r["Min"][1] + r["Max"][1]) / 2
        x0 = r["Max"][0] + self.wall
        lvl = r["Level"] if level is None else level
        return self.room(x0, cy - w / 2, x0 + depth, cy + w / 2, level=lvl, **kw)

    def north_of(self, idx, depth, width=None, level=None, **kw):
        r = self.rooms[idx]
        w = (r["Max"][0] - r["Min"][0]) if width is None else width
        cx = (r["Min"][0] + r["Max"][0]) / 2
        y0 = r["Max"][1] + self.wall
        lvl = r["Level"] if level is None else level
        return self.room(cx - w / 2, y0, cx + w / 2, y0 + depth, level=lvl, **kw)

    # --- thresholds (the ONE opening primitive) ---
    def _thr(self, a, b, kind, plane="Vertical", position=0.0, position2=0.0, width=0.0,
             depth=0.0, height=0.0, sill=0.0, entry=False, ramp=False, edge=WEST):
        self.thresholds.append(dict(RoomA=int(a), RoomB=int(b), Kind=kind, Plane=plane,
                                    Position=float(position), Position2=float(position2),
                                    Width=float(width), Depth=float(depth), Height=float(height),
                                    Sill=float(sill), bIsEntry=bool(entry), bRamp=bool(ramp),
                                    ExteriorEdge=EDGE_NAME[edge]))

    def door(self, a, b, position=0.0, width=0.0, height=0.0):
        self._thr(a, b, "Doorway", position=position, width=width, height=height)

    def passage(self, a, b):
        self._thr(a, b, "Passage")

    def window(self, a, b=-1, edge=EAST, position=0.0, width=0.0, height=0.0, sill=0.0):
        self._thr(a, b, "Window", position=position, width=width, height=height, sill=sill, edge=edge)

    def rail(self, a, b=-1, edge=WEST):
        self._thr(a, b, "Rail", edge=edge)

    def entry(self, a, edge=WEST, position=0.0, width=0.0, height=0.0):
        self._thr(a, -1, "Doorway", position=position, width=width, height=height, entry=True, edge=edge)

    def exterior(self, a, edge, kind="Doorway", position=0.0, width=0.0, height=0.0, sill=0.0):
        self._thr(a, -1, kind, position=position, width=width, height=height, sill=sill, edge=edge)

    def stairwell(self, a, b, width=0.0, position=0.0, position2=0.0, ramp=False):
        self._thr(a, b, "Ramp" if ramp else "Stairwell", plane="Horizontal",
                  position=position, position2=position2, width=width, ramp=ramp)

    def atrium(self, a, width, depth, position=0.0, position2=0.0):
        self._thr(a, -1, "Atrium", plane="Horizontal", width=width, depth=depth,
                  position=position, position2=position2)

    def hatch(self, a, width=0.0, depth=0.0, position=0.0, position2=0.0, skylight=False):
        self._thr(a, -1, "Skylight" if skylight else "Hatch", plane="Horizontal",
                  width=width, depth=depth, position=position, position2=position2)

    # --- explicit flights (grand staircases that land at an edge; fill, not a slab pierce) ---
    def flight(self, along_x, start_u, cross_v, from_z, to_z, width=0.0, direction=1, ramp=False):
        self.flights.append(dict(bAlongX=bool(along_x), StartU=float(start_u), Dir=int(direction),
                                 CrossV=float(cross_v), FromZ=float(from_z), ToZ=float(to_z),
                                 Width=float(width), bRamp=bool(ramp)))

    # --- solids ---
    def box(self, cx, cy, cz, sx, sy, sz):
        self.boxes.append(dict(Center=(float(cx), float(cy), float(cz)),
                               Size=(float(sx), float(sy), float(sz))))

    def cover(self, cx, cy, full=False, w=140, d=80):
        h = 200 if full else 100
        self.box(cx, cy, h / 2, w, d, h)

    def pillar(self, cx, cy, height=300, dia=90):
        self.box(cx, cy, height / 2, dia, dia, height)

    def dais(self, cx, cy, w=300, d=300, h=40):
        self.box(cx, cy, h / 2, w, d, h)

    # --- serialization to the UE Custom payload ---
    def payload(self):
        def v2(p):
            return {"x": p[0], "y": p[1]}

        def v3(p):
            return {"x": p[0], "y": p[1], "z": p[2]}

        levels = list(self.levels) if self.levels else [dict(Index=0, BaseZ=0.0, Height=self.ceiling_min, SlabT=0.0)]
        rooms = [dict(r, Min=v2(r["Min"]), Max=v2(r["Max"])) for r in self.rooms]
        boxes = [dict(Center=v3(b["Center"]), Size=v3(b["Size"])) for b in self.boxes]
        return {"Preset": "Custom", "AuthoredLevels": levels, "AuthoredRooms": rooms,
                "AuthoredThresholds": self.thresholds, "AuthoredFlights": self.flights,
                "AuthoredBoxes": boxes}

    def write_apply(self, path="_apply.py", gen=GEN):
        if self.snap > 0:
            g = f"{self.snap:g}"
            print(f"draftDesk: authoring on a {g}x{g}x{g} cm grid (wall = one cell); engine snaps to Spec.GridSnap.")
        else:
            print("draftDesk: layout grid snap OFF (snap=0) — the engine still snaps to Spec.GridSnap.")
        pj = json.dumps(self.payload())
        # Apply order is load-bearing: every array is cleared to [] before being set (a non-empty->
        # non-empty change confuses the property system's array diff), and LEVELS are set before ROOMS
        # before THRESHOLDS, so no transient rebuild ever sees a room referencing a missing level or a
        # threshold referencing a missing room.
        script = (
            "import json\n"
            'SET = "editor_toolset.toolsets.object.ObjectTools.set_properties"\n'
            'FIND = "editor_toolset.toolsets.scene.SceneTools.find_actors"\n'
            'BOUNDS = "editor_toolset.toolsets.actor.ActorTools.get_actor_bounds"\n'
            'SETXF = "editor_toolset.toolsets.actor.ActorTools.set_actor_transform"\n'
            'NAV = "/Script/NavigationSystem.NavMeshBoundsVolume"\n'
            f'GEN = "{gen}"\n'
            f"PAYLOAD = json.loads(r'''{pj}''')\n"
            "def run():\n"
            '    inst = {"refPath": GEN}\n'
            "    def setp(d): execute_tool(SET, json.dumps({\"instance\": inst, \"values\": json.dumps(d)}))\n"
            '    setp({"AuthoredThresholds": [], "AuthoredFlights": [], "AuthoredBoxes": []})\n'
            '    setp({"Preset": "Custom", "AuthoredRooms": []})\n'
            '    setp({"AuthoredLevels": []})\n'
            '    setp({"AuthoredLevels": PAYLOAD["AuthoredLevels"]})\n'
            '    setp({"AuthoredRooms": PAYLOAD["AuthoredRooms"]})\n'
            '    setp({"AuthoredBoxes": PAYLOAD["AuthoredBoxes"], "AuthoredFlights": PAYLOAD["AuthoredFlights"]})\n'
            '    setp({"AuthoredThresholds": PAYLOAD["AuthoredThresholds"]})\n'
            '    out = {"ok": True, "levels": len(PAYLOAD["AuthoredLevels"]), "rooms": len(PAYLOAD["AuthoredRooms"]),\n'
            '           "thresholds": len(PAYLOAD["AuthoredThresholds"]), "flights": len(PAYLOAD["AuthoredFlights"]),\n'
            '           "boxes": len(PAYLOAD["AuthoredBoxes"])}\n'
            "    # nav-sync: any footprint change re-fits the NavMeshBoundsVolume to the new geometry\n"
            '    b = execute_tool(BOUNDS, json.dumps({"actor": {"refPath": GEN}}))["returnValue"]\n'
            '    mn = b["min"]; mx = b["max"]; MARGIN = 500.0; BASE = 200.0\n'
            '    nav = execute_tool(FIND, json.dumps({"name": "", "tag": "", "collision_channels": [],\n'
            '        "actor_type": {"refPath": NAV}}))["returnValue"]\n'
            "    if nav:\n"
            '        execute_tool(SETXF, json.dumps({"actor": {"refPath": nav[0]["refPath"]}, "xform": {\n'
            '            "location": {"x": (mn["x"]+mx["x"])/2.0, "y": (mn["y"]+mx["y"])/2.0, "z": (mn["z"]+mx["z"])/2.0},\n'
            '            "rotation": {"pitch": 0, "yaw": 0, "roll": 0},\n'
            '            "scale": {"x": (mx["x"]-mn["x"]+2*MARGIN)/BASE, "y": (mx["y"]-mn["y"]+2*MARGIN)/BASE,\n'
            '                      "z": (mx["z"]-mn["z"]+2*MARGIN)/BASE}}}))\n'
            '        out["nav_refit"] = True\n'
            "    return out\n")
        open(path, "w", encoding="utf-8").write(script)
        return path


if __name__ == "__main__":
    # minimal smoke test: entry room -> doorway -> raised room with a window, a dais
    L = Layout()
    L.level(0)
    a = L.room(0, -300, 800, 300)
    b = L.east_of(a, depth=1000)
    L.entry(a, WEST)
    L.door(a, b)
    L.window(b, edge=EAST)
    L.dais(L.rooms[b]["Max"][0] - 250, 0)
    L.write_apply()
    print(json.dumps(L.payload(), indent=1)[:1200])
