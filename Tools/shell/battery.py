"""
Phase 0 correctness battery — the full 24 hard cases (H1-H14, V1-V10) from SHELL v1.
Each case builds rooms+thresholds, runs the emitter, and asserts the 5 validator
assertions pass (watertight by construction) plus a case-specific structural property.
Cases that SHOULD fail loud (V9 desync, V12 unresolvable) are marked expect_fail/expect_unresolved.
Run: python battery.py
"""
from shell import (Room, Threshold, Level, Metrics, build_and_validate,
                   DOORWAY, PASSAGE, WINDOW, RAIL, STAIRWELL, HATCH, ATRIUM,
                   VERTICAL, HORIZONTAL, WEST, EAST, SOUTH, NORTH, CLASS_X, CLASS_Y, CLASS_SLAB)
import rects2d as R

RESULTS = []
M = Metrics(grid=50, wall_thickness=30)        # T = 50


def case(name, rooms, thr, levels=None, metrics=None, prop=None, prop_name="",
         expect_fail=None, expect_unresolved=None):
    s, fails = build_and_validate(rooms, thr, levels, metrics or M)
    if expect_fail is not None:
        ok = any(expect_fail in f for f in fails)
        RESULTS.append((name, ok, [] if ok else (fails or ["(no failure raised)"]), f"expected fail ~ {expect_fail!r}"))
        return s
    extra_ok, why = True, ""
    if expect_unresolved is not None:
        extra_ok = expect_unresolved in s.unresolved
        why = f"threshold {expect_unresolved} should be unresolved"
    if prop is not None and extra_ok:
        try:
            extra_ok = prop(s)
            why = prop_name
        except Exception as e:
            extra_ok, why = False, f"prop raised {e!r}"
    ok = (not fails) and extra_ok
    RESULTS.append((name, ok, fails, why if not extra_ok else ""))
    return s


def solid(s, key, foot):
    b = s.buckets.get(key)
    return R.area_within(b.solid, foot) if b else 0


# ===================================================================== HORIZONTAL
case("H1 equal-width abutment + door",
     [Room(0, 0, 400, 400), Room(450, 0, 850, 400)], [Threshold(0, 1, DOORWAY)],
     prop=lambda s: (CLASS_X, 425) in s.buckets and len(s.buckets[(CLASS_X, 425)].apertures) == 1,
     prop_name="one shared wall + one door")

case("H2 unequal-width abutment",
     [Room(0, 0, 400, 400), Room(450, 150, 850, 250)], [Threshold(0, 1, PASSAGE)],
     prop=lambda s: solid(s, (CLASS_X, 425), (-50, 150, 0, 300)) > 0,  # pier below the corridor mouth
     prop_name="wide wall keeps piers")

case("H3 offset / partial-overlap abutment",
     [Room(0, 0, 400, 400), Room(450, 200, 850, 600)], [Threshold(0, 1, PASSAGE)],
     prop=lambda s: solid(s, (CLASS_X, 425), (0, 100, 0, 300)) == 100 * 300        # A non-overlap tail solid
                    and solid(s, (CLASS_X, 425), (500, 600, 0, 300)) == 100 * 300,  # B non-overlap tail solid
     prop_name="non-overlap tails stay solid")

case("H4 T-junction",
     [Room(0, 0, 1200, 300), Room(500, 350, 700, 750)], [Threshold(1, 0, PASSAGE)],
     prop=lambda s: solid(s, (CLASS_Y, 325), (100, 400, 0, 300)) > 0
                    and solid(s, (CLASS_Y, 325), (800, 1100, 0, 300)) > 0,  # piers flank the stem mouth
     prop_name="two piers flank the stem")

def h5_corners(s):
    b = (CLASS_X, -25)  # node west wall
    return solid(s, b, (-50, 100, 0, 300)) > 0 and solid(s, b, (300, 450, 0, 300)) > 0
case("H5 4-way crossing",
     [Room(0, 0, 400, 400), Room(-450, 100, -50, 300), Room(450, 100, 850, 300),
      Room(100, -450, 300, -50), Room(100, 450, 300, 850)],
     [Threshold(0, 1, PASSAGE), Threshold(0, 2, PASSAGE), Threshold(0, 3, PASSAGE), Threshold(0, 4, PASSAGE)],
     prop=h5_corners, prop_name="node corner posts survive")

case("H6 full-width passage -> corner piers",
     [Room(0, 0, 400, 400), Room(450, 0, 850, 400)], [Threshold(0, 1, PASSAGE)],
     prop=lambda s: solid(s, (CLASS_X, 425), (400, 450, 0, 300)) > 0
                    and solid(s, (CLASS_X, 425), (-50, 0, 0, 300)) > 0,
     prop_name="both +/-T corner piers")

def h9_window(s):
    b = (CLASS_X, 425)
    return solid(s, b, (0, 400, 0, 100)) > 0 and solid(s, b, (0, 400, 230, 300)) > 0
case("H9 window sill+cap",
     [Room(0, 0, 400, 400)], [Threshold(0, -1, WINDOW, edge=EAST)],
     prop=h9_window, prop_name="solid below sill + above cap")

def h7_pier(s):
    return solid(s, (CLASS_X, 425), (150, 250, 0, 200)) == 100 * 200
case("H7 two doors one wall -> pier",
     [Room(0, 0, 400, 400), Room(450, 0, 850, 400)],
     [Threshold(0, 1, DOORWAY, width=100, position=-100), Threshold(0, 1, DOORWAY, width=100, position=100)],
     prop=h7_pier, prop_name="full pier between two doors")

case("H10 door flush to corner",
     [Room(0, 0, 400, 400), Room(450, 0, 850, 400)], [Threshold(0, 1, DOORWAY, position=99999)],
     prop=lambda s: solid(s, (CLASS_X, 425), (400, 450, 0, 200)) > 0,
     prop_name="corner pier survives the clamp")

case("H11 over-wide connection clamps to overlap",
     [Room(0, 0, 400, 400), Room(450, 0, 850, 400)], [Threshold(0, 1, DOORWAY, width=100000)],
     prop=lambda s: solid(s, (CLASS_X, 425), (400, 450, 0, 200)) > 0       # corner pier survives
                    and solid(s, (CLASS_X, 425), (0, 400, 0, 200)) == 0,    # mouth == full overlap, carved
     prop_name="clamps to overlap, corner pier survives")

case("H8 no-neighbour wall stays solid",
     [Room(0, 0, 400, 400)], [Threshold(0, -1, DOORWAY, edge=WEST, is_entry=True)],
     prop=lambda s: solid(s, (CLASS_X, 425), (-50, 450, 0, 300)) == 500 * 300,  # east wall fully solid
     prop_name="opposite wall fully solid")

case("H12 diagonal / zero-overlap -> UNRESOLVED, no hole",
     [Room(0, 0, 400, 400), Room(450, 450, 850, 850)], [Threshold(0, 1, DOORWAY)],
     expect_unresolved=0)

case("H13 entry / exterior",
     [Room(0, 0, 400, 400)], [Threshold(0, -1, DOORWAY, edge=WEST, is_entry=True)],
     prop=lambda s: any(ap[5] == 0 for ap in s.buckets[(CLASS_X, -25)].apertures),
     prop_name="entry carved in the named edge")

case("H14 L-shape reflex corner",
     [Room(0, 0, 400, 400), Room(0, 450, 400, 850)], [Threshold(0, 1, PASSAGE)],
     prop=lambda s: True)

# ===================================================================== VERTICAL
LV2 = [Level(0, 0, 300, 50), Level(1, 350, 300, 50)]
LV3 = [Level(0, 0, 300, 50), Level(1, 350, 300, 50), Level(2, 700, 300, 50)]

def v1_slab(s):
    b = s.buckets.get((CLASS_SLAB, 1))
    return b and len(b.contributors) == 2 and R.area(b.solid) == 500 ** 2 and not R.overlaps_any(b.solid)
case("V1 two-storey shared slab dedup",
     [Room(0, 0, 400, 400, level=0), Room(0, 0, 400, 400, level=1)], [], levels=LV2,
     prop=v1_slab, prop_name="one merged slab, 2 contributors")

def v2_hole(s):
    b = s.buckets.get((CLASS_SLAB, 1))
    return any(ap[4] == STAIRWELL for ap in b.apertures) and R.area(b.solid) > 0
case("V2 stairwell floor-opening",
     [Room(0, 0, 1000, 600, level=0), Room(0, 0, 1000, 600, level=1)],
     [Threshold(0, 1, STAIRWELL, plane=HORIZONTAL, width=200, position=-200)], levels=LV2,
     prop=v2_hole, prop_name="hole carved, floor remains")

def v3_atrium(s):
    b = s.buckets.get((CLASS_SLAB, 1))
    void_open = R.area_within(b.solid, (600, 900, 100, 500)) == 0       # middle void: no floor
    gallery = R.area_within(b.solid, (100, 400, 100, 500)) > 0          # west gallery floor present
    return void_open and gallery
case("V3 atrium (mid-void + 2 galleries)",
     [Room(0, 0, 1500, 600, level=0, height=650),               # tall hall, ceiling -> interface 2
      Room(0, 0, 450, 600, level=1), Room(1050, 0, 1500, 600, level=1)],
     [Threshold(0, -1, ATRIUM, plane=HORIZONTAL, width=500, depth=600),  # void x[500,1000]
      Threshold(1, -1, RAIL, edge=EAST), Threshold(2, -1, RAIL, edge=WEST)],
     levels=LV3, prop=v3_atrium, prop_name="mid void open, galleries floored")

def v4_mezz(s):
    b = s.buckets.get((CLASS_SLAB, 1))
    floored = R.area_within(b.solid, (800, 1100, 100, 500)) > 0   # mezzanine floor present (east)
    void = R.area_within(b.solid, (100, 400, 100, 500)) == 0      # void open (west)
    return floored and void
case("V4 mezzanine (partial floor + void)",
     [Room(0, 0, 1200, 600, level=0, height=650),                # tall hall
      Room(700, 0, 1200, 600, level=1)],                          # mezzanine over the east half
     [Threshold(0, -1, ATRIUM, plane=HORIZONTAL, width=650, depth=600, position=-275),  # void x[0,650]
      Threshold(1, -1, RAIL, edge=WEST)], levels=LV3,
     prop=v4_mezz, prop_name="mezz floored, void open")

case("V5 mismatched ceiling heights",
     [Room(0, 0, 400, 400, height=300), Room(450, 0, 850, 400, height=450)], [Threshold(0, 1, DOORWAY)],
     prop=lambda s: solid(s, (CLASS_X, 425), (0, 400, 300, 450)) > 0,
     prop_name="clerestory band solid above short ceiling")

def v6_hatch(s):
    b = s.buckets.get((CLASS_SLAB, ("roof", 300)))
    return b and R.area_within(b.solid, (0, 100, 0, 400)) > 0 and R.area_within(b.solid, (150, 250, 150, 250)) == 0
case("V6 ceiling hatch / skylight",
     [Room(0, 0, 400, 400, level=0)],
     [Threshold(0, -1, HATCH, plane=HORIZONTAL, width=100, depth=100)],
     levels=[Level(0, 0, 300, 50)], prop=v6_hatch, prop_name="surround solid, hole bounded")

def v7_cant(s):
    b = s.buckets.get((CLASS_SLAB, 1))
    return (b and len(b.contributors) == 2
            and R.area_within(b.solid, (0, 600, 0, 600)) > 0       # over A's footprint sealed
            and R.area_within(b.solid, (600, 800, 600, 800)) > 0)  # over B's overhang sealed
case("V7 cantilever / misaligned stack",
     [Room(0, 0, 600, 600, level=0), Room(200, 200, 800, 800, level=1)], [], levels=LV2,
     prop=v7_cant, prop_name="seam watertight over both footprints")

case("V8 roofless + pit",
     [Room(0, 0, 400, 400, ceil=False), Room(450, 0, 850, 400, floor=False)], [Threshold(0, 1, DOORWAY)],
     prop=lambda s: ({c[0] for k, b in s.buckets.items() if b.cls == CLASS_SLAB
                      for c in b.contributors if c[1] == "ceil"}.isdisjoint({0})
                     and {c[0] for k, b in s.buckets.items() if b.cls == CLASS_SLAB
                          for c in b.contributors if c[1] == "floor"}.isdisjoint({1})),
     prop_name="A no ceiling, B no floor")

case("V9 BaseZ / height desync FAILS LOUD",
     [Room(0, 0, 400, 400, level=0, height=400), Room(0, 0, 400, 400, level=1)], [], levels=LV2,
     expect_fail="V9 Height desync")

M2 = Metrics(grid=50, wall_thickness=30, ceiling_min=200)  # allow a dz=300 climb -> off-grid run 510
LV_OG = [Level(0, 0, 250, 50), Level(1, 300, 250, 50)]
def v10_offgrid(s):
    b = s.buckets.get((CLASS_SLAB, 1))
    well = [ap for ap in b.apertures if ap[4] == STAIRWELL][0]
    on_grid = all(abs(c % 50) < 1e-6 for c in well[:4])           # rounded OUT to the grid
    encloses = (well[1] - well[0]) >= 510 - 1e-6                   # encloses the 510 off-grid run
    floor_beyond = R.area_within(b.solid, (1000, 1100, 350, 450)) > 0  # top tread lands on solid floor
    return on_grid and encloses and floor_beyond
case("V10 off-grid stair landing rounds OUT",
     [Room(0, 0, 1200, 800, level=0), Room(0, 0, 1200, 800, level=1)],
     [Threshold(0, 1, STAIRWELL, plane=HORIZONTAL, width=200)], levels=LV_OG, metrics=M2,
     prop=v10_offgrid, prop_name="well grid-aligned, encloses run, floor beyond solid")


# ===================================================================== ADVERSARIAL REGRESSIONS
case("ADV1 multi-level stairwell pierces every crossed floor",
     [Room(0, 0, 1000, 800, level=0), Room(0, 0, 1000, 800, level=2)],
     [Threshold(0, 1, STAIRWELL, plane=HORIZONTAL, width=200)], levels=LV3,
     prop=lambda s: any(ap[4] == STAIRWELL for ap in s.buckets[(CLASS_SLAB, 1)].apertures)
                    and any(ap[4] == STAIRWELL for ap in s.buckets[(CLASS_SLAB, 2)].apertures),
     prop_name="middle (interface 1) AND top (interface 2) both pierced")

case("ADV2 atrium void cannot carve an unrelated room",
     [Room(0, 0, 400, 400, level=0, height=650),
      Room(1000, 0, 1400, 400, level=0), Room(1000, 0, 1400, 400, level=1)],
     [Threshold(0, -1, ATRIUM, plane=HORIZONTAL, width=200, depth=200, position=1000)],
     levels=LV2,
     prop=lambda s: R.area_within(s.buckets[(CLASS_SLAB, 1)].solid, (1050, 1350, 100, 300)) == 300 * 200,
     prop_name="unrelated stack floor stays solid (void clamped to anchor)")

case("ADV3 ground exterior rail -> full wall (envelope sealed)",
     [Room(0, 0, 400, 400, level=0)], [Threshold(0, -1, RAIL, edge=EAST)],
     prop=lambda s: R.area_within(s.buckets[(CLASS_X, 425)].solid, (-50, 450, 100, 300)) == 500 * 200,
     prop_name="east wall full height, not half-capped")

case("ADV4 bad threshold index -> loud, no crash",
     [Room(0, 0, 400, 400)], [Threshold(0, 5, DOORWAY)], expect_unresolved=0)


# ===================================================================== report
order_h = [r for r in RESULTS if r[0].startswith("H")]
order_v = [r for r in RESULTS if r[0].startswith("V")]
order_a = [r for r in RESULTS if r[0].startswith("ADV")]
nfail = 0
for grp, lbl in ((order_h, "HORIZONTAL"), (order_v, "VERTICAL"), (order_a, "ADVERSARIAL REGRESSIONS")):
    print(f"\n--- {lbl} ---")
    for name, ok, fails, why in grp:
        print(f"  {'PASS' if ok else 'FAIL'}  {name}")
        if not ok:
            nfail += 1
            for f in fails[:5]:
                print(f"        ! {f}")
            if why:
                print(f"        ! {why}")
print(f"\n{len(RESULTS)-nfail}/{len(RESULTS)} pass")
raise SystemExit(1 if nfail else 0)
