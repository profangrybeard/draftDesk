"""
draftDesk authoring layer — compose a dictated layout from pieces, emit the Custom-preset
payload, and write a UE-sandbox apply script that pushes it onto the generator (run via ddrun.py).

Pieces map to the engine's authored arrays:
  room/corridor/junction -> AuthoredRooms (FDraftDeskRoom)
  door/arch/window/stairs/ramp link -> AuthoredLinks (FDraftDeskLink)
  manual flight -> AuthoredStairs (FDraftDeskStair)
  cover/crate/dais/pillar/ledge -> AuthoredBoxes (FDraftDeskBlock)

Coordinates are actor-local cm, +X into the space. Abutting rooms are placed with a WallThickness
gap so their wall planes coincide and the engine dedups to ONE shared wall (the abutment rule).
"""
import json
import math
from dd_config import GEN   # one source of the generator path (see dd_config.py)

WEST, EAST, SOUTH, NORTH = 0, 1, 2, 3
EDGE_NAME = {WEST: "West", EAST: "East", SOUTH: "South", NORTH: "North"}

GRID = 50.0   # default authoring grid (cm); matches FDraftDeskMetrics.GridSnap. Precision, not art.


def bit(*edges):
    m = 0
    for e in edges:
        m |= (1 << e)
    return m


class Layout:
    def __init__(self, wall=None, corridor=200.0, snap=GRID):
        # The grid every footprint snaps to (cm). Generally one locked value; 0 disables snapping.
        self.snap = float(snap)
        # Wall thickness rides as a whole number of grid cells so abutting faces stay one cell apart and
        # the engine dedups them to one shared wall (matches the engine's BuiltWallT). Default: one cell.
        if wall is None:
            wall = self.snap if self.snap > 0 else 30.0
        self.wall = self._snap_up(wall)
        self.cw = corridor
        self.rooms = []
        self.links = []
        self.stairs = []
        self.boxes = []

    # --- grid helpers ---
    def _snap(self, v):
        """Round a coordinate onto the grid (no-op when snap is 0)."""
        return round(float(v) / self.snap) * self.snap if self.snap > 0 else float(v)

    def _snap_up(self, v):
        """Round a thickness UP to a whole cell, min one cell (no-op when snap is 0)."""
        return max(self.snap, math.ceil(float(v) / self.snap) * self.snap) if self.snap > 0 else float(v)

    # --- rooms ---
    def room(self, x0, y0, x1, y1, floor=0.0, height=0.0, ceiling=False,
             columns=False, open_edges=0, rail_edges=0, no_floor=False):
        # snap the footprint onto the grid so the harness mirrors what the engine will build
        x0, y0, x1, y1 = self._snap(x0), self._snap(y0), self._snap(x1), self._snap(y1)
        floor = self._snap(floor)
        height = self._snap(height) if height else height
        self.rooms.append(dict(Min=(float(x0), float(y0)), Max=(float(x1), float(y1)),
                               FloorZ=float(floor), Height=float(height), bCeiling=bool(ceiling),
                               bColumns=bool(columns), OpenEdgeMask=int(open_edges),
                               RailEdgeMask=int(rail_edges), bNoFloor=bool(no_floor)))
        return len(self.rooms) - 1

    def corridor(self, x0, length, width=None, floor=0.0, axis="X", y_center=0.0, **kw):
        w = self.cw if width is None else width
        if axis == "X":
            return self.room(x0, y_center - w / 2, x0 + length, y_center + w / 2, floor=floor, **kw)
        return self.room(y_center - w / 2, x0, y_center + w / 2, x0 + length, floor=floor, **kw)

    # place a new room abutting an existing one (leaves the T gap so walls dedup)
    def east_of(self, idx, depth, width=None, floor=None, align="center", **kw):
        r = self.rooms[idx]
        w = (r["Max"][1] - r["Min"][1]) if width is None else width
        cy = (r["Min"][1] + r["Max"][1]) / 2
        x0 = r["Max"][0] + self.wall
        fz = r["FloorZ"] if floor is None else floor
        return self.room(x0, cy - w / 2, x0 + depth, cy + w / 2, floor=fz, **kw)

    def north_of(self, idx, depth, width=None, floor=None, **kw):
        r = self.rooms[idx]
        w = (r["Max"][0] - r["Min"][0]) if width is None else width
        cx = (r["Min"][0] + r["Max"][0]) / 2
        y0 = r["Max"][1] + self.wall
        fz = r["FloorZ"] if floor is None else floor
        return self.room(cx - w / 2, y0, cx + w / 2, y0 + depth, floor=fz, **kw)

    # --- links ---
    def link(self, a, b, kind="Doorway", position=0.0, width=0.0, height=0.0, sill=0.0):
        self.links.append(dict(RoomA=a, RoomB=b, Kind=kind, Position=float(position),
                               Width=float(width), Height=float(height), Sill=float(sill),
                               bIsEntry=False, ExteriorEdge="West"))

    def entry(self, a, edge=WEST, kind="Doorway", position=0.0, width=0.0, height=0.0):
        self.links.append(dict(RoomA=a, RoomB=-1, Kind=kind, Position=float(position),
                               Width=float(width), Height=float(height), Sill=0.0, bIsEntry=True, ExteriorEdge=EDGE_NAME[edge]))

    def exterior(self, a, edge, kind="Doorway", position=0.0, width=0.0, height=0.0, sill=0.0):
        self.links.append(dict(RoomA=a, RoomB=-1, Kind=kind, Position=float(position),
                               Width=float(width), Height=float(height), Sill=float(sill),
                               bIsEntry=False, ExteriorEdge=EDGE_NAME[edge]))

    # --- manual flight / solids ---
    def stair(self, along_x, start_u, cross_v, from_z, to_z, width=0.0, ramp=False, direction=1):
        self.stairs.append(dict(bAlongX=bool(along_x), StartU=float(start_u), Dir=int(direction),
                                CrossV=float(cross_v), FromZ=float(from_z), ToZ=float(to_z),
                                Width=float(width), bRamp=bool(ramp)))

    def box(self, cx, cy, cz, sx, sy, sz):
        self.boxes.append(dict(Center=(float(cx), float(cy), float(cz)),
                               Size=(float(sx), float(sy), float(sz))))

    # sugar
    def cover(self, cx, cy, full=False, w=140, d=80):
        h = 200 if full else 100
        self.box(cx, cy, h / 2, w, d, h)

    def pillar(self, cx, cy, height=300, dia=90):
        self.box(cx, cy, height / 2, dia, dia, height)

    def dais(self, cx, cy, w=300, d=300, h=40):
        self.box(cx, cy, h / 2, w, d, h)

    # --- threshold points (for seeding movable marker actors), in normalized world coords ---
    def _normalize_shift(self):
        """Mirror NormalizeToEntry: world = local + shift, where shift moves the entry to the origin."""
        T = self.wall
        dx = dy = 0.0
        for L in self.links:
            if not L["bIsEntry"]:
                continue
            a = self.rooms[L["RoomA"]]
            cx = (a["Min"][0] + a["Max"][0]) / 2 + L["Position"]
            cy = (a["Min"][1] + a["Max"][1]) / 2 + L["Position"]
            e = L["ExteriorEdge"]
            if e == "West":    dx, dy = a["Min"][0] - T / 2, cy
            elif e == "East":  dx, dy = a["Max"][0] + T / 2, cy
            elif e == "South": dx, dy = cx, a["Min"][1] - T / 2
            else:              dx, dy = cx, a["Max"][1] + T / 2
            break
        minz = min((r["FloorZ"] for r in self.rooms), default=0.0)
        return -dx, -dy, -minz

    def _face(self, a, b):
        """Mirror FaceConnection. Returns {axis, x, y, overlap}: axis 0 = constant-X wall (door
        slides in Y), axis 1 = constant-Y wall (slides in X); overlap = shared edge length."""
        T = self.wall
        cands = []  # (axis, plane, center, gap, lo, hi)
        oyLo, oyHi = max(a["Min"][1], b["Min"][1]), min(a["Max"][1], b["Max"][1])
        if oyHi > oyLo:
            if a["Max"][0] <= b["Min"][0] + 1:
                cands.append((0, ((a["Max"][0] + T / 2) + (b["Min"][0] - T / 2)) / 2, (oyLo + oyHi) / 2, abs(b["Min"][0] - a["Max"][0]), oyLo, oyHi))
            if b["Max"][0] <= a["Min"][0] + 1:
                cands.append((0, ((a["Min"][0] - T / 2) + (b["Max"][0] + T / 2)) / 2, (oyLo + oyHi) / 2, abs(a["Min"][0] - b["Max"][0]), oyLo, oyHi))
        oxLo, oxHi = max(a["Min"][0], b["Min"][0]), min(a["Max"][0], b["Max"][0])
        if oxHi > oxLo:
            if a["Max"][1] <= b["Min"][1] + 1:
                cands.append((1, ((a["Max"][1] + T / 2) + (b["Min"][1] - T / 2)) / 2, (oxLo + oxHi) / 2, abs(b["Min"][1] - a["Max"][1]), oxLo, oxHi))
            if b["Max"][1] <= a["Min"][1] + 1:
                cands.append((1, ((a["Min"][1] - T / 2) + (b["Max"][1] + T / 2)) / 2, (oxLo + oxHi) / 2, abs(a["Min"][1] - b["Max"][1]), oxLo, oxHi))
        if not cands:
            return None
        cands.sort(key=lambda c: c[3])
        axis, plane, center, _, lo, hi = cands[0]
        x, y = (plane, center) if axis == 0 else (center, plane)
        return {"axis": axis, "x": x, "y": y, "overlap": hi - lo}

    def threshold_points(self):
        sx, sy, sz = self._normalize_shift()
        T = self.wall
        out = []
        for i, L in enumerate(self.links):
            a = self.rooms[L["RoomA"]]
            axis, overlap = 0, 0.0
            if L["RoomB"] == -1:  # exterior threshold on a named edge of RoomA
                cx = (a["Min"][0] + a["Max"][0]) / 2 + L["Position"]
                cy = (a["Min"][1] + a["Max"][1]) / 2 + L["Position"]
                e = L["ExteriorEdge"]
                if e == "West":    lx, ly, axis, overlap = a["Min"][0] - T / 2, cy, 0, a["Max"][1] - a["Min"][1]
                elif e == "East":  lx, ly, axis, overlap = a["Max"][0] + T / 2, cy, 0, a["Max"][1] - a["Min"][1]
                elif e == "South": lx, ly, axis, overlap = cx, a["Min"][1] - T / 2, 1, a["Max"][0] - a["Min"][0]
                else:              lx, ly, axis, overlap = cx, a["Max"][1] + T / 2, 1, a["Max"][0] - a["Min"][0]
                lz = a["FloorZ"]
                label = "entry" if L["bIsEntry"] else f"ext{i}"
            else:
                b = self.rooms[L["RoomB"]]
                lz = min(a["FloorZ"], b["FloorZ"])
                label = f"{L['RoomA']}-{L['RoomB']}"
                if L["Kind"] in ("Stairs", "Ramp"):
                    lx = (a["Min"][0] + a["Max"][0] + b["Min"][0] + b["Max"][0]) / 4
                    ly = (a["Min"][1] + a["Max"][1] + b["Min"][1] + b["Max"][1]) / 4
                else:
                    f = self._face(a, b)
                    if f is None:
                        continue
                    lx, ly, axis, overlap = f["x"], f["y"], f["axis"], f["overlap"]
            out.append({"i": i, "label": label, "x": lx + sx, "y": ly + sy, "z": lz + sz,
                        "kind": L["Kind"], "width": L["Width"], "height": L["Height"],
                        "axis": axis, "overlap": overlap})
        return out

    # --- serialization to the UE Custom payload ---
    def payload(self):
        def v2(p):
            return {"x": p[0], "y": p[1]}

        def v3(p):
            return {"x": p[0], "y": p[1], "z": p[2]}

        rooms = [dict(r, Min=v2(r["Min"]), Max=v2(r["Max"])) for r in self.rooms]
        boxes = [dict(Center=v3(b["Center"]), Size=v3(b["Size"])) for b in self.boxes]
        return {"Preset": "Custom", "AuthoredRooms": rooms, "AuthoredLinks": self.links,
                "AuthoredStairs": self.stairs, "AuthoredBoxes": boxes}

    def write_apply(self, path="_apply.py", gen=GEN):
        # Echo the grid on every apply so it's a conscious, ever-present decision — a designer never
        # builds without seeing which grid they committed to. The engine enforces Spec.GridSnap.
        if self.snap > 0:
            g = f"{self.snap:g}"
            print(f"draftDesk: authoring on a {g}x{g}x{g} cm grid (wall = one cell); engine snaps to Spec.GridSnap.")
        else:
            print("draftDesk: layout grid snap OFF (snap=0) — the engine still snaps to Spec.GridSnap.")
        pj = json.dumps(self.payload())
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
            "    # Order matters: clear CONNECTIONS before rooms, and set rooms before connections, so a\n"
            "    # link is never present while its rooms are absent (which logs spurious broken-threshold noise).\n"
            '    execute_tool(SET, json.dumps({"instance": inst, "values": json.dumps(\n'
            '        {"AuthoredLinks": [], "AuthoredStairs": [], "AuthoredBoxes": []})}))\n'
            '    execute_tool(SET, json.dumps({"instance": inst, "values": json.dumps(\n'
            '        {"Preset": "Custom", "AuthoredRooms": []})}))\n'
            '    execute_tool(SET, json.dumps({"instance": inst, "values": json.dumps({"AuthoredRooms": PAYLOAD["AuthoredRooms"]})}))\n'
            '    execute_tool(SET, json.dumps({"instance": inst, "values": json.dumps(\n'
            '        {"AuthoredStairs": PAYLOAD["AuthoredStairs"], "AuthoredBoxes": PAYLOAD["AuthoredBoxes"]})}))\n'
            '    execute_tool(SET, json.dumps({"instance": inst, "values": json.dumps({"AuthoredLinks": PAYLOAD["AuthoredLinks"]})}))\n'
            '    out = {"ok": True, "rooms": len(PAYLOAD["AuthoredRooms"]), "links": len(PAYLOAD["AuthoredLinks"]),\n'
            '           "stairs": len(PAYLOAD["AuthoredStairs"]), "boxes": len(PAYLOAD["AuthoredBoxes"])}\n'
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
    # minimal format smoke test: entry room -> doorway -> raised room with a window, a dais
    L = Layout()
    a = L.room(0, -300, 800, 300)                       # entry room
    b = L.east_of(a, depth=1000)                        # same-width room east (dedups to one wall)
    L.entry(a, WEST)
    L.link(a, b, "Doorway")
    L.exterior(b, EAST, "Window")                       # window on the far wall
    L.dais(L.rooms[b]["Max"][0] - 250, 0)               # dais in room b
    L.write_apply()
    print(json.dumps(L.payload(), indent=1)[:1200])
