"""dd_engine.py — reconstruct a layout from the LIVE generator's reflected authored arrays, so the
acceptance gate measures ENGINE TRUTH (the real, possibly-moved geometry + any auto-created connection)
instead of a static Python dictation (dd_castle). This is the gate's eyes on the engine: after a drag
mutates AuthoredRooms/AuthoredThresholds in the editor, dd_castle.L is unchanged and stale — only what
the generator actually holds is the truth, and that is what this reads.

The returned object is shape-compatible with dd_author.Layout (.rooms/.thresholds/.flights/.levels as
lists of PascalCase-keyed dicts, .snap/.wall floats), so dd_gate._full_shell and dd_navcheck consume it
unchanged. Reflection gives lowerCamel field names (min, floorZ, roomA, startU, baseZ) with the b-prefix
kept on bools (bFloor, bIsEntry, bAlongX) and enums as clean strings ("Doorway", "Vertical", "West");
this maps them back to the keys the consumers expect.
"""
import ddrun

# GridSnap (50cm) and the built wall thickness (one cell == 50cm for this kit) are fixed project metrics
# — the same 50 the gate's _full_shell and dd_author already default to. The engine snaps to Spec.GridSnap;
# if a Spec ever changes the grid, source these from the DA asset instead of the constants.
GRID = 50.0
WALL = 50.0


class _EngineLayout:
    """Mirror of dd_author.Layout's READ surface, populated from the engine's reflected arrays."""

    def __init__(self, rooms, thresholds, flights, levels, snap=GRID, wall=WALL):
        self.rooms = rooms
        self.thresholds = thresholds
        self.flights = flights
        self.levels = levels
        self.snap = snap
        self.wall = wall
        self.boxes = []


def _read_authored():
    """The generator's authored arrays, straight off its reflected properties (engine truth)."""
    READ = '''import json
GET="editor_toolset.toolsets.object.ObjectTools.get_properties"
GEN="{{GEN}}"
def run():
    pr=execute_tool(GET,json.dumps({"instance":{"refPath":GEN},
        "properties":["AuthoredLevels","AuthoredRooms","AuthoredThresholds","AuthoredFlights"]}))["returnValue"]
    pj=json.loads(pr) if isinstance(pr,str) else pr
    g=lambda k:(pj.get(k) or pj.get(k[0].lower()+k[1:]) or []) if isinstance(pj,dict) else []
    return {"levels":g("AuthoredLevels"),"rooms":g("AuthoredRooms"),
            "thresholds":g("AuthoredThresholds"),"flights":g("AuthoredFlights")}
'''
    return ddrun.run_text(READ)


def _xy(v):
    """{x,y} -> (x,y); pass an existing [x,y]/(x,y) through."""
    if isinstance(v, dict):
        return (float(v.get("x", 0.0)), float(v.get("y", 0.0)))
    return (float(v[0]), float(v[1]))


def _enum(v, default):
    """An enum string, tolerant of a 'EType::Value' prefix or a stray int."""
    return str(v).split("::")[-1] if isinstance(v, str) else default


def _room(r):
    return dict(Min=_xy(r["min"]), Max=_xy(r["max"]),
                Level=int(r.get("level", 0)), FloorZ=float(r.get("floorZ", -1.0)),
                Height=float(r.get("height", 0.0)),
                bFloor=bool(r.get("bFloor", True)), bCeil=bool(r.get("bCeil", True)),
                bColumns=bool(r.get("bColumns", False)))


def _thr(t):
    return dict(RoomA=int(t["roomA"]), RoomB=int(t["roomB"]),
                Kind=_enum(t.get("kind"), "Doorway"), Plane=_enum(t.get("plane"), "Vertical"),
                Position=float(t.get("position", 0.0)), Position2=float(t.get("position2", 0.0)),
                Width=float(t.get("width", 0.0)), Depth=float(t.get("depth", 0.0)),
                Height=float(t.get("height", 0.0)), Sill=float(t.get("sill", 0.0)),
                bIsEntry=bool(t.get("bIsEntry", False)), bRamp=bool(t.get("bRamp", False)),
                ExteriorEdge=_enum(t.get("exteriorEdge"), "West"))


def _flight(f):
    return dict(bAlongX=bool(f["bAlongX"]), StartU=float(f["startU"]), Dir=int(f.get("dir", 1)),
                CrossV=float(f["crossV"]), FromZ=float(f["fromZ"]), ToZ=float(f["toZ"]),
                Width=float(f.get("width", 0.0)), bRamp=bool(f.get("bRamp", False)))


def _level(lv):
    return dict(Index=int(lv["index"]), BaseZ=float(lv["baseZ"]),
                Height=float(lv.get("height", 0.0)), SlabT=float(lv.get("slabT", 0.0)))


def layout(snap=GRID, wall=WALL):
    """A Layout-shaped object built from the live generator's authored arrays (ENGINE TRUTH)."""
    raw = _read_authored()
    return _EngineLayout(
        rooms=[_room(r) for r in raw["rooms"]],
        thresholds=[_thr(t) for t in raw["thresholds"]],
        flights=[_flight(f) for f in raw["flights"]],
        levels=[_level(lv) for lv in raw["levels"]],
        snap=snap, wall=wall)


if __name__ == "__main__":
    L = layout()
    print("ENGINE layout:", len(L.levels), "levels,", len(L.rooms), "rooms,",
          len(L.thresholds), "thresholds,", len(L.flights), "flights")
    if L.rooms:
        print("  room0:", L.rooms[0])
    if L.thresholds:
        print("  thr0: ", L.thresholds[0])
    if L.flights:
        print("  flight0:", L.flights[0])
