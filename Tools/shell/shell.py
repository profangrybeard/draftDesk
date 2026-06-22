"""
draftDesk SHELL v1 — Phase 0 reference emitter (pure data, no Unreal).

Implements the unified plane-sweep from the spec: every room deposits a solid rect on
each of its 6 faces (4 walls ALWAYS, floor/ceiling per SurfaceFlags) into a per-plane
ledger bucket; every threshold deposits an aperture rect into the same bucket(s); each
bucket emits  union(faces) MINUS union(apertures)  via the exact 2D Boolean (rects2d).

Walls, floors, ceilings, doors, passages, windows, rails, stairwells, hatches and atria
are ONE mechanism. A face is solid by construction and opens ONLY where a threshold
proves a connection. There is no OpenEdgeMask. The validator (5 assertions) proves
watertightness before any UE wiring. SLABS KEY ON INTERFACE INDEX (the COS graft): a
lower room's ceiling and the upper room's floor share ONE bucket by construction, never
a near-equal-Z heuristic.
"""
import math
from dataclasses import dataclass
import rects2d as R

WEST, EAST, SOUTH, NORTH = 0, 1, 2, 3
EDGE_NAME = {WEST: "W", EAST: "E", SOUTH: "S", NORTH: "N"}
VERTICAL, HORIZONTAL = "V", "H"
DOORWAY, PASSAGE, WINDOW, RAIL, STAIRWELL, RAMP, HATCH, SKYLIGHT, ATRIUM = (
    "Doorway", "Passage", "Window", "Rail", "Stairwell", "Ramp", "Hatch", "Skylight", "Atrium")
CLASS_X, CLASS_Y, CLASS_SLAB = 0, 1, 2


@dataclass
class Metrics:
    grid: float = 50.0
    wall_thickness: float = 30.0
    door_width: float = 240.0
    door_height: float = 200.0
    corridor_width: float = 200.0
    ceiling_min: float = 300.0
    half_wall: float = 100.0
    window_clear: float = 130.0
    window_sill: float = 100.0
    step_rise: float = 18.0
    step_run: float = 30.0
    max_step_angle: float = 40.0

    @property
    def T(self):
        g = self.grid
        return max(g, math.ceil(self.wall_thickness / g) * g) if g > 0 else self.wall_thickness

    @property
    def slab_t(self):
        return self.T


@dataclass
class Level:
    index: int
    base_z: float
    height: float
    slab_t: float


@dataclass
class Room:
    x0: float; y0: float; x1: float; y1: float
    level: int = 0
    floor_z: float = None
    height: float = 0.0
    floor: bool = True
    ceil: bool = True
    name: str = ""
    def W(self): return self.x1 - self.x0
    def D(self): return self.y1 - self.y0


@dataclass
class Threshold:
    room_a: int
    room_b: int = -1
    kind: str = DOORWAY
    plane: str = VERTICAL
    position: float = 0.0
    position2: float = 0.0
    width: float = 0.0
    depth: float = 0.0
    height: float = 0.0
    sill: float = 0.0
    is_entry: bool = False
    edge: int = WEST
    name: str = ""


@dataclass
class Flight:
    """An explicit stair/ramp flight (solid stepped FILL that lands at an edge). The core does NOT
    emit treads (pure data); it uses the flight only to DERIVE a rail gap where it lands (RailGap-
    from-flight): the gap tracks the flight's CrossV/width, so it can never drift from the stairs."""
    along_x: bool = True
    start_u: float = 0.0
    cross_v: float = 0.0
    z0: float = 0.0
    z1: float = 0.0
    w: float = 0.0
    direction: int = 1


def step_total_run(dz, m):
    if dz <= 1:
        return 0.0
    nr = math.ceil(dz / m.step_rise)
    tanmax = math.tan(math.radians(min(m.max_step_angle, 89.0)))
    na = math.ceil(dz / (m.step_run * tanmax)) if tanmax > 1e-4 else nr
    return max(1, nr, na) * m.step_run


def _round(v):
    return int(round(v))


class Bucket:
    __slots__ = ("cls", "key", "faces", "apertures", "contributors", "voids", "solid")
    def __init__(self, cls, key):
        self.cls = cls; self.key = key
        self.faces = []          # (alo,ahi,blo,bhi, room_idx)
        self.apertures = []      # (alo,ahi,blo,bhi, kind, thr_idx)
        self.contributors = []   # (room_idx, 'floor'|'ceil', face_z) for slabs
        self.voids = []
        self.solid = []


class Shell:
    def __init__(self, rooms, thresholds, levels, metrics, flights=None):
        self.rooms = rooms
        self.thresholds = thresholds
        self.flights = flights or []
        self.metrics = metrics
        self.levels = levels or self._infer_levels()
        self.buckets = {}
        self.errors = []      # hard failures (BaseZ / height desync) — always fail the build
        self.warnings = []    # loud notices (FloorZ snap, unresolved threshold) — a test decides
        self.unresolved = []
        self._eff = []

    def _xy_overlap(self, a, b):
        return a.x0 < b.x1 and b.x0 < a.x1 and a.y0 < b.y1 and b.y0 < a.y1

    def _intersect(self, r, b):
        alo = max(r[0], b[0]); ahi = min(r[1], b[1]); blo = max(r[2], b[2]); bhi = min(r[3], b[3])
        return (alo, ahi, blo, bhi) if alo < ahi and blo < bhi else None

    def _infer_levels(self):
        m = self.metrics
        zs = sorted({(r.floor_z if r.floor_z is not None else 0.0) for r in self.rooms})
        self._zmap = {z: i for i, z in enumerate(zs)}
        return [Level(i, z, m.ceiling_min, m.slab_t) for i, z in enumerate(zs)]

    def _level_of(self, r):
        if hasattr(self, "_zmap"):
            return self._zmap[r.floor_z if r.floor_z is not None else 0.0]
        return r.level

    def pass0(self):
        m = self.metrics; L = self.levels
        for n in range(len(L) - 1):
            want = L[n].base_z + L[n].height + L[n].slab_t
            if abs(L[n + 1].base_z - want) > 1e-6:
                self.errors.append(f"BaseZ invariant: Level {n+1} base_z={L[n+1].base_z} != {want}")
        for idx, r in enumerate(self.rooms):
            lvl = self._level_of(r); r.level = lvl
            base = L[lvl].base_z
            fz = r.floor_z if r.floor_z is not None else base
            if r.floor_z is not None and abs(r.floor_z - base) > 1e-6:
                self.warnings.append(f"room {idx} FloorZ {r.floor_z} != level base {base}; snapping")
                fz = base
            eff = r.height if r.height > 0 else L[lvl].height
            eff = max(eff, m.ceiling_min)
            self._eff.append((fz, eff))
        # V9 desync guard: a room's ceiling must land on a level interface. A clean multi-height
        # room (ceiling lands on a HIGHER level) is fine; a ceiling that floats BETWEEN levels with
        # a room sitting above & overlapping it is a desync (the slab can't dedup coherently).
        for idx, r in enumerate(self.rooms):
            if not (r.height and r.height > 0):
                continue
            fz, eff = self._eff[idx]
            zc = fz + eff
            if isinstance(self._ceiling_interface(zc), int):
                continue   # ceiling lands on a real level interface (single OR clean multi-height) — fine
            # Desync only if another room's FLOOR sits ON this ceiling (a stacked slab) yet the ceiling
            # is off-grid. A mezzanine/atrium room nested HIGH inside a tall room (its floor far below
            # this ceiling) is NOT a desync — it shares no slab here.
            stacked_on = any(j != idx and abs(self._eff[j][0] - zc) <= m.slab_t + 1e-6
                             and self._xy_overlap(r, self.rooms[j])
                             for j in range(len(self.rooms)))
            if stacked_on:
                self.errors.append(
                    f"V9 Height desync: room {idx} ceiling z={zc} carries a stacked room but lands on no level interface")

    def eff(self, idx):
        return self._eff[idx]

    def wall_bucket(self, cls, plane):
        key = (cls, _round(plane))
        b = self.buckets.get(key)
        if b is None:
            b = Bucket(cls, key); self.buckets[key] = b
        return b

    def slab_bucket(self, interface):
        key = (CLASS_SLAB, interface)
        b = self.buckets.get(key)
        if b is None:
            b = Bucket(CLASS_SLAB, key); self.buckets[key] = b
        return b

    def _ceiling_interface(self, ceiling_z):
        """The interface a room's ceiling belongs to: the level whose floor sits on this
        ceiling's slab (base_z == ceiling_z + slab_t). Generalizes 'level+1' to double-height
        rooms. No matching level => a roof bucket keyed by its z."""
        for i, lv in enumerate(self.levels):
            if abs(lv.base_z - (ceiling_z + self.metrics.slab_t)) < 1e-6:
                return i
        return ("roof", _round(ceiling_z))

    def _rail_edges(self, room_idx):
        # A rail caps a face to half-height only at a genuine FALL edge. An exterior rail on the
        # LOWEST level has no drop behind it, so capping it would open the envelope: emit a full
        # wall instead (warned in _rail_gaps). Elevated rooms / rails facing another room cap.
        out = {}
        elevated = self.rooms[room_idx].level > 0
        for ti, t in enumerate(self.thresholds):
            if t.kind == RAIL and t.room_a == room_idx:
                if t.room_b == -1 and not elevated:
                    continue
                e = t.edge if t.room_b == -1 else self._internal_edge(t.room_a, t.room_b)
                if e is not None:
                    out[e] = ti
        return out

    def _internal_edge(self, a, b):
        fc = self.face_connection(a, b)
        if fc is None:
            return None
        axis = fc[0]; A = self.rooms[a]; B = self.rooms[b]
        if axis == CLASS_X:
            return EAST if A.x1 <= B.x0 + 1 else WEST
        return NORTH if A.y1 <= B.y0 + 1 else SOUTH

    def pass1(self):
        m = self.metrics; T = m.T
        for idx, r in enumerate(self.rooms):
            if r.W() <= 1 or r.D() <= 1:
                continue
            fz, H = self.eff(idx); top = fz + H
            rails = self._rail_edges(idx)
            def zt(edge):
                return fz + (m.half_wall if edge in rails else H)
            self.wall_bucket(CLASS_X, r.x0 - T / 2).faces.append((r.y0 - T, r.y1 + T, fz, zt(WEST), idx))
            self.wall_bucket(CLASS_X, r.x1 + T / 2).faces.append((r.y0 - T, r.y1 + T, fz, zt(EAST), idx))
            self.wall_bucket(CLASS_Y, r.y0 - T / 2).faces.append((r.x0 - T, r.x1 + T, fz, zt(SOUTH), idx))
            self.wall_bucket(CLASS_Y, r.y1 + T / 2).faces.append((r.x0 - T, r.x1 + T, fz, zt(NORTH), idx))
            foot = (r.x0 - T, r.x1 + T, r.y0 - T, r.y1 + T)
            if r.floor:
                b = self.slab_bucket(r.level)
                b.faces.append((*foot, idx)); b.contributors.append((idx, "floor", fz))
            if r.ceil:
                b = self.slab_bucket(self._ceiling_interface(top))
                b.faces.append((*foot, idx)); b.contributors.append((idx, "ceil", top))

    def face_connection(self, a, b):
        A, B = self.rooms[a], self.rooms[b]; T = self.metrics.T
        # A wall abutment is exactly one cell apart on the snapped grid (gap == T); +1 absorbs float dust.
        # Beyond gmax the rooms do NOT face (no single shared wall) -> a room dragged away DISCONNECTS.
        gmax = T + 1.0
        cands = []
        oyl, oyh = max(A.y0, B.y0), min(A.y1, B.y1)
        if oyh > oyl:
            if A.x1 <= B.x0 + 1 and abs(B.x0 - A.x1) <= gmax:
                cands.append((abs(B.x0 - A.x1), CLASS_X, A.x1 + T / 2, B.x0 - T / 2, oyl, oyh))
            if B.x1 <= A.x0 + 1 and abs(A.x0 - B.x1) <= gmax:
                cands.append((abs(A.x0 - B.x1), CLASS_X, A.x0 - T / 2, B.x1 + T / 2, oyl, oyh))
        oxl, oxh = max(A.x0, B.x0), min(A.x1, B.x1)
        if oxh > oxl:
            if A.y1 <= B.y0 + 1 and abs(B.y0 - A.y1) <= gmax:
                cands.append((abs(B.y0 - A.y1), CLASS_Y, A.y1 + T / 2, B.y0 - T / 2, oxl, oxh))
            if B.y1 <= A.y0 + 1 and abs(A.y0 - B.y1) <= gmax:
                cands.append((abs(A.y0 - B.y1), CLASS_Y, A.y0 - T / 2, B.y1 + T / 2, oxl, oxh))
        if not cands:
            return None
        cands.sort(key=lambda c: c[0])
        _, axis, pa, pb, lo, hi = cands[0]
        return (axis, pa, pb, lo, hi)

    def resolve_exterior(self, t):
        A = self.rooms[t.room_a]; T = self.metrics.T
        if t.edge == WEST:  return (CLASS_X, A.x0 - T / 2, A.y0, A.y1)
        if t.edge == EAST:  return (CLASS_X, A.x1 + T / 2, A.y0, A.y1)
        if t.edge == SOUTH: return (CLASS_Y, A.y0 - T / 2, A.x0, A.x1)
        return (CLASS_Y, A.y1 + T / 2, A.x0, A.x1)

    def _aperture_zband(self, t, fz, H):
        m = self.metrics; top = fz + H
        if t.kind == DOORWAY:
            h = t.height if t.height > 0 else m.door_height
            return (fz, fz + min(h, H))
        if t.kind == WINDOW:
            sill = t.sill if t.sill > 0 else m.window_sill
            clear = t.height if t.height > 0 else m.window_clear
            return (fz + sill, min(fz + sill + clear, top))
        if t.kind == RAIL:
            return (fz, fz + m.half_wall)
        return (fz, top)

    def pass2(self):
        n = len(self.rooms)
        for ti, t in enumerate(self.thresholds):
            if not (0 <= t.room_a < n) or (t.room_b != -1 and not (0 <= t.room_b < n)):
                self.warnings.append(f"threshold {ti} ({t.name}) bad room index a={t.room_a} b={t.room_b}; skipped")
                self.unresolved.append(ti); continue
            if t.plane == HORIZONTAL or t.kind in (STAIRWELL, RAMP, HATCH, SKYLIGHT, ATRIUM):
                self._aperture_horizontal(ti, t)
            elif t.kind == RAIL:
                self._rail_gaps(ti, t)
            else:
                self._aperture_vertical(ti, t)

    def _aperture_vertical(self, ti, t):
        m = self.metrics
        if t.room_b == -1:
            cls, pa, lo, hi = self.resolve_exterior(t)
            planes = [(cls, pa)]
        else:
            fc = self.face_connection(t.room_a, t.room_b)
            if fc is None:
                self.warnings.append(f"threshold {ti} ({t.name}) UNRESOLVED: no overlap")
                self.unresolved.append(ti); return
            axis, pa, pb, lo, hi = fc
            cls = axis; planes = [(cls, pa), (cls, pb)]
        span = hi - lo
        if span <= 0:
            self.warnings.append(f"threshold {ti} ({t.name}) UNRESOLVED: zero span")
            self.unresolved.append(ti); return
        if t.kind == PASSAGE:
            default_w = span
        elif t.kind in (DOORWAY, WINDOW):
            default_w = m.door_width
        else:
            default_w = m.corridor_width
        weff = min(t.width if t.width > 0 else default_w, span)
        mid = (lo + hi) / 2.0
        center = min(max(mid + t.position, lo + weff / 2), hi - weff / 2)
        olo, ohi = center - weff / 2, center + weff / 2
        fz, H = self.eff(t.room_a)
        zlo, zhi = self._aperture_zband(t, fz, H)
        seen = set()
        for (cls, plane) in planes:
            k = (cls, _round(plane))   # gap==T => both facing planes are ONE bucket: deposit once
            if k in seen:
                continue
            seen.add(k)
            self.wall_bucket(cls, plane).apertures.append((olo, ohi, zlo, zhi, t.kind, ti))

    def _rail_gaps(self, ti, t):
        # a Rail's face cap is applied in pass1; landing gaps from a paired flight derive in the C++ port.
        if t.room_b == -1 and 0 <= t.room_a < len(self.rooms) and self.rooms[t.room_a].level == 0:
            self.warnings.append(
                f"rail {ti} ({t.name}) on a ground-level exterior wall has no drop; emitted as a full wall")

    def _aperture_horizontal(self, ti, t):
        m = self.metrics
        a = t.room_a; A = self.rooms[a]
        fzA, HA = self.eff(a)
        # ATRIUM: suppress the slab at the level ABOVE A over the void footprint (interface a.level+1)
        if t.kind == ATRIUM:
            interface = A.level + 1
            if t.width > 0 and t.depth > 0:
                cx = (A.x0 + A.x1) / 2 + t.position; cy = (A.y0 + A.y1) / 2 + t.position2
                void = (cx - t.width / 2, cx + t.width / 2, cy - t.depth / 2, cy + t.depth / 2)
            else:
                void = (A.x0, A.x1, A.y0, A.y1)
            void = self._intersect(void, (A.x0, A.x1, A.y0, A.y1))  # void can ONLY open over its anchor
            if void is None:
                self.warnings.append(f"atrium {ti} ({t.name}) void outside its anchor room; skipped")
                self.unresolved.append(ti); return
            b_ = self.slab_bucket(interface)
            b_.apertures.append((*void, ATRIUM, ti)); b_.voids.append(void)
            return
        # HATCH / SKYLIGHT / exterior horizontal: a bounded hole in THIS room's ceiling slab
        if t.kind in (HATCH, SKYLIGHT) or t.room_b == -1:
            interface = self._ceiling_interface(fzA + HA)
            w = t.width if t.width > 0 else m.corridor_width
            d = t.depth if t.depth > 0 else m.corridor_width
            cx = (A.x0 + A.x1) / 2 + t.position; cy = (A.y0 + A.y1) / 2 + t.position2
            hole = self._intersect((cx - w / 2, cx + w / 2, cy - d / 2, cy + d / 2),
                                   (A.x0, A.x1, A.y0, A.y1))  # a hatch can ONLY open this room's own ceiling
            if hole is None:
                self.warnings.append(f"hatch {ti} ({t.name}) outside its room ceiling; skipped")
                self.unresolved.append(ti); return
            self.slab_bucket(interface).apertures.append((*hole, t.kind, ti))
            return
        # STAIRWELL / RAMP: the designed stair-hole resolver
        b = t.room_b
        fzB, _ = self.eff(b)
        lo_i, hi_i = (a, b) if fzA < fzB else (b, a)
        Lo, Hi = self.rooms[lo_i], self.rooms[hi_i]
        if self.rooms[hi_i].level <= self.rooms[lo_i].level:   # no level crossed -> misauthored
            self.warnings.append(f"stairwell {ti} ({t.name}) connects same-level rooms; nothing to pierce")
            self.unresolved.append(ti); return
        interface = self.rooms[hi_i].level
        sxl, sxh = max(Lo.x0, Hi.x0), min(Lo.x1, Hi.x1)
        syl, syh = max(Lo.y0, Hi.y0), min(Lo.y1, Hi.y1)
        dz = abs(fzB - fzA); run = step_total_run(dz, m)
        if sxh > sxl and syh > syl:
            along_x = (sxh - sxl) >= (syh - syl)
        else:
            along_x = Lo.W() >= Lo.D()
        if along_x:
            cv = ((syl + syh) / 2 if syh > syl else (Hi.y0 + Hi.y1) / 2) + t.position2
            width = t.width if t.width > 0 else max(m.corridor_width, (syh - syl) if syh > syl else m.corridor_width)
            u0 = ((sxl + sxh) / 2 if sxh > sxl else (Hi.x0 + Hi.x1) / 2) - run / 2 + t.position
            well = (u0, u0 + run, cv - width / 2, cv + width / 2)
        else:
            cv = ((sxl + sxh) / 2 if sxh > sxl else (Hi.x0 + Hi.x1) / 2) + t.position2
            width = t.width if t.width > 0 else max(m.corridor_width, (sxh - sxl) if sxh > sxl else m.corridor_width)
            v0 = ((syl + syh) / 2 if syh > syl else (Hi.y0 + Hi.y1) / 2) - run / 2 + t.position
            well = (cv - width / 2, cv + width / 2, v0, v0 + run)
        well = self._round_out(well, m.grid)
        if sxh > sxl and syh > syl:
            clamped = self._clamp_rect(well, (sxl, sxh, syl, syh), m.grid)
            wl = (clamped[1] - clamped[0]) if along_x else (clamped[3] - clamped[2])
            if wl + 1e-6 < run:   # never SILENT: a too-small shared overlap can't fit the flight
                self.warnings.append(
                    f"stairwell {ti} ({t.name}) cramped: well {wl:.0f} < flight run {run:.0f} (shared overlap too small)")
            well = clamped
        # pierce EVERY interface the flight crosses (a 2-storey climb must hole the middle floor too)
        for iface in range(self.rooms[lo_i].level + 1, self.rooms[hi_i].level + 1):
            self.slab_bucket(iface).apertures.append((*well, t.kind, ti))

    def _round_out(self, rect, g):
        if g <= 0:
            return rect
        return (math.floor(rect[0] / g) * g, math.ceil(rect[1] / g) * g,
                math.floor(rect[2] / g) * g, math.ceil(rect[3] / g) * g)

    def _clamp_rect(self, rect, bound, g):
        alo = max(rect[0], bound[0]); ahi = min(rect[1], bound[1])
        blo = max(rect[2], bound[2]); bhi = min(rect[3], bound[3])
        if ahi - alo < g:
            mid = (rect[0] + rect[1]) / 2
            alo, ahi = max(bound[0], mid - g / 2), min(bound[1], mid + g / 2)
        if bhi - blo < g:
            mid = (rect[2] + rect[3]) / 2
            blo, bhi = max(bound[2], mid - g / 2), min(bound[3], mid + g / 2)
        return (alo, ahi, blo, bhi)

    def emit(self):
        self.output = {}
        for key, b in self.buckets.items():
            faces = [(f[0], f[1], f[2], f[3]) for f in b.faces]
            holes = [(a[0], a[1], a[2], a[3]) for a in b.apertures]
            b.solid = R.boolean(faces, holes)
            self.output[key] = b.solid

    def build(self):
        self.pass0(); self.pass1(); self.pass2(); self.rail_gaps_from_flights()
        self.enforce_min_pier(); self.emit()
        return self

    def rail_gaps_from_flights(self):
        """RailGap-from-flight: where an explicit flight lands at a RAILED edge, carve a gap in that
        rail at the flight's CrossV (width = flight width). The gap derives from the flight, so it can
        never drift from where the stairs actually arrive (no hand-authored, decouple-able gaps)."""
        m = self.metrics; T = m.T
        for f in self.flights:
            run = step_total_run(abs(f.z1 - f.z0), m)
            u_top = f.start_u + f.direction * run          # where the top tread lands
            cross = f.cross_v
            for idx, r in enumerate(self.rooms):
                if r.W() <= 1 or r.D() <= 1:
                    continue
                fz, H = self.eff(idx)
                if abs(fz - f.z1) > 1:                      # the room the flight lands ON
                    continue
                if f.along_x:
                    if f.direction > 0 and abs(r.x0 - u_top) <= T + 1 and r.y0 <= cross <= r.y1:
                        cls, plane, edge = CLASS_X, r.x0 - T / 2, WEST
                    elif f.direction < 0 and abs(r.x1 - u_top) <= T + 1 and r.y0 <= cross <= r.y1:
                        cls, plane, edge = CLASS_X, r.x1 + T / 2, EAST
                    else:
                        continue
                else:
                    if f.direction > 0 and abs(r.y0 - u_top) <= T + 1 and r.x0 <= cross <= r.x1:
                        cls, plane, edge = CLASS_Y, r.y0 - T / 2, SOUTH
                    elif f.direction < 0 and abs(r.y1 - u_top) <= T + 1 and r.x0 <= cross <= r.x1:
                        cls, plane, edge = CLASS_Y, r.y1 + T / 2, NORTH
                    else:
                        continue
                if edge not in self._rail_edges(idx):       # only notch an actual rail
                    continue
                w = f.w if f.w > 0 else m.corridor_width
                self.wall_bucket(cls, plane).apertures.append((cross - w / 2, cross + w / 2, fz, fz + H, PASSAGE, -1))
                break

    def enforce_min_pier(self):
        """Guarantee a solid pier of >= T between any two openings that share a wall plane and overlap
        in Z. Two openings authored closer than T would leave a sub-T sliver (or merge into one hole);
        each is trimmed symmetrically on its facing side to reopen a T gap. If a trim would shrink an
        opening below one grid cell, the pair is left as-authored and a warning is logged (the wall
        genuinely can't seat both openings plus a pier). Openings whose Z-bands don't overlap (e.g. a
        door and a clerestory window stacked above it) never contend for a pier."""
        m = self.metrics; min_pier = m.T; min_open = max(m.grid, 1.0)
        for key, b in self.buckets.items():
            if b.cls == CLASS_SLAB or len(b.apertures) < 2:
                continue
            aps = [list(a) for a in b.apertures]
            order = sorted(range(len(aps)), key=lambda i: aps[i][0] + aps[i][1])   # by along-axis centre
            changed = False
            for x in range(len(order)):
                for y in range(x + 1, len(order)):
                    a = aps[order[x]]; c = aps[order[y]]            # a's centre <= c's centre
                    if not (a[2] < c[3] and c[2] < a[3]):
                        continue                                    # Z-bands disjoint -> no pier needed
                    gap = c[0] - a[1]
                    if gap >= min_pier - 1e-6:
                        continue
                    d = (min_pier - gap) / 2.0
                    if (a[1] - d - a[0]) < min_open or (c[1] - (c[0] + d)) < min_open:
                        self.warnings.append(
                            f"min-pier: openings {a[5]} & {c[5]} on plane {key} only {gap:.0f} apart "
                            f"(< T={min_pier:.0f}) and can't be trimmed to a pier without collapsing one")
                        continue
                    a[1] -= d; c[0] += d; changed = True
            if changed:
                b.apertures = [tuple(v) for v in aps]

    # ----------------------------------------------------------------- validation
    def validate(self):
        fails = list(self.errors)   # hard failures only; unresolved/snap notices live in self.warnings
        m = self.metrics; T = m.T

        for key, b in self.buckets.items():
            if R.overlaps_any(b.solid):
                fails.append(f"A2 double-cover in bucket {key}")

        for idx, r in enumerate(self.rooms):
            if r.W() <= 1 or r.D() <= 1:
                continue
            fz, H = self.eff(idx); rails = self._rail_edges(idx)
            wf = [(CLASS_X, r.x0 - T / 2, (r.y0 - T, r.y1 + T), WEST),
                  (CLASS_X, r.x1 + T / 2, (r.y0 - T, r.y1 + T), EAST),
                  (CLASS_Y, r.y0 - T / 2, (r.x0 - T, r.x1 + T), SOUTH),
                  (CLASS_Y, r.y1 + T / 2, (r.x0 - T, r.x1 + T), NORTH)]
            for cls, plane, aint, edge in wf:
                ztop = fz + (m.half_wall if edge in rails else H)
                foot = (aint[0], aint[1], fz, ztop)
                self._assert_face(fails, (cls, _round(plane)), foot, f"room {idx} {EDGE_NAME[edge]}")
            slabfoot = (r.x0 - T, r.x1 + T, r.y0 - T, r.y1 + T)
            zspan = (slabfoot[2], slabfoot[3])
            if r.floor:
                self._assert_face(fails, (CLASS_SLAB, r.level),
                                  (slabfoot[0], slabfoot[1], zspan[0], zspan[1]), f"room {idx} floor")
            if r.ceil:
                self._assert_face(fails, (CLASS_SLAB, self._ceiling_interface(fz + H)),
                                  (slabfoot[0], slabfoot[1], zspan[0], zspan[1]), f"room {idx} ceil")

        # A3 interface coherence: a ceil contributor whose Height breaks BaseZ under a stack
        L = self.levels
        for key, b in self.buckets.items():
            if b.cls != CLASS_SLAB:
                continue
            iface = key[1]
            has_floor_above = any(c[1] == "floor" for c in b.contributors)
            if not has_floor_above or not isinstance(iface, int) or iface >= len(L):
                continue
            want = L[iface].base_z
            for (idx, kind, fzc) in b.contributors:
                if kind == "ceil" and abs(fzc + m.slab_t - want) > 1e-6:
                    fails.append(f"A3 desync: room {idx} ceiling z={fzc}+slab != interface {iface} base {want} (V9)")

        deposited = {ap[5] for b in self.buckets.values() for ap in b.apertures}
        for ti, t in enumerate(self.thresholds):
            if ti in self.unresolved or t.kind == RAIL:
                continue
            if ti not in deposited:
                fails.append(f"A4 connection: threshold {ti} ({t.name}) resolved but deposited NO aperture")

        for key, b in self.buckets.items():
            for v in b.voids:
                if R.area_within(b.solid, v) > 1e-6:
                    fails.append(f"A5 cantilever: floor covers void {v} in bucket {key}")
        return fails

    def _assert_face(self, fails, key, foot, label):
        foot_area = (foot[1] - foot[0]) * (foot[3] - foot[2])
        b = self.buckets.get(key)
        solid_a = R.area_within(b.solid, foot) if b else 0
        ap = [(a[0], a[1], a[2], a[3]) for a in b.apertures] if b else []
        ap_a = R.area_within(R.union(ap), foot) if ap else 0  # union: apertures may overlap/dup
        if abs(solid_a + ap_a - foot_area) > 1e-6:
            fails.append(f"A1 watertight: {label}: solid {solid_a:.0f}+ap {ap_a:.0f} != foot {foot_area:.0f}")


def a_level(shell, idx):
    return shell.rooms[idx].level


def build_and_validate(rooms, thresholds, levels=None, metrics=None, flights=None):
    s = Shell(rooms, thresholds, levels, metrics or Metrics(), flights)
    s.build()
    return s, s.validate()
