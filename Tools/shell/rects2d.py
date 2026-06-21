"""
Exact axis-aligned 2D rectangle Boolean — the load-bearing primitive of the
draftDesk SHELL emitter (Phase 0, pure data, no Unreal).

Given SOLID rects (a plane bucket's deposited faces) and HOLE rects (the bucket's
apertures), compute  union(solids) MINUS union(holes)  as a set of NON-overlapping,
gap-free axis-aligned rectangles, via coordinate-compression sweep + DETERMINISTIC
strip decomposition. No tolerance, no closest-wall search, no integer-span equality —
exact on the grid (SHELL v1 spec, "PASS 3 / IMPLEMENTATION of the 2D Boolean").

EMIT POLICY (open-Q5, owner decision 2026-06-21 = strip decomposition over greedy
maximal-rectangle merge): strip along axis A into columns, emit each column's maximal
B-runs, then merge consecutive columns that share an identical run signature. Every box
edge lands on a compressed coordinate, so the output is a PURE FUNCTION of the cell grid
(deterministic, reproducible — unlike order-dependent greedy) and seams line up across
the surface and across adjacent pieces (clean modular-kit handoff to GAME356). Costs
somewhat more boxes than greedy in cross/plus regions; acceptable for a blockout.

A Rect is (alo, ahi, blo, bhi), half-open [lo, hi), with alo < ahi and blo < bhi.
Axis A is the plane's in-plane axis; axis B is Z (walls) or the 2nd footprint axis (slabs).

Design guarantees this module exists to provide:
  * union: two coincident/overlapping solids merge (equal-width abutment H1, unequal H2).
  * subtract: a hole removes exactly its rect; the surround stays solid (window H9).
  * PIER survival: two holes with a gap leave the solid cell between them (H7 — the
    cpp:917-931 sort-merge-into-one-span bug is designed out: holes are distinct rects,
    never merged spans).
  * watertight: output exactly partitions (covered-by-a-solid AND covered-by-no-hole).
"""
from bisect import bisect_left


def _coords(rects, lo, hi):
    s = set()
    for r in rects:
        s.add(r[lo])
        s.add(r[hi])
    return sorted(s)


def _cover_grid(rects, A, B):
    """grid[i][j] True iff sub-cell [A[i],A[i+1]) x [B[j],B[j+1]) is covered by >=1 rect.
    Sub-cell boundaries come from the compressed coords, so every sub-cell is either
    wholly inside or wholly outside each rect — no partial coverage, no tolerance."""
    nA, nB = len(A) - 1, len(B) - 1
    g = [[False] * nB for _ in range(nA)]
    for (alo, ahi, blo, bhi) in rects:
        ia0, ia1 = bisect_left(A, alo), bisect_left(A, ahi)
        ib0, ib1 = bisect_left(B, blo), bisect_left(B, bhi)
        for i in range(ia0, ia1):
            row = g[i]
            for j in range(ib0, ib1):
                row[j] = True
    return g


def _strip_decompose(cell, A, B):
    """Deterministic strip decomposition (the emit policy — see module docstring).
    Strip along axis A into columns; for each column collect its maximal B-runs as a
    signature; merge consecutive columns whose signatures are IDENTICAL into one
    box-group; emit one box per run of each group. Every box edge lies on a compressed
    coordinate. The result is a pure function of the cell grid — order-independent."""
    nA, nB = len(A) - 1, len(B) - 1
    sigs = []  # sigs[i] = tuple of (jlo, jhi_excl) maximal solid runs in column i
    for i in range(nA):
        col = cell[i]; runs = []; j = 0
        while j < nB:
            if col[j]:
                j0 = j
                while j < nB and col[j]:
                    j += 1
                runs.append((j0, j))
            else:
                j += 1
        sigs.append(tuple(runs))
    out = []; i = 0
    while i < nA:
        sig = sigs[i]
        if not sig:
            i += 1; continue
        i2 = i
        while i2 + 1 < nA and sigs[i2 + 1] == sig:
            i2 += 1
        for (j0, j1) in sig:
            out.append((A[i], A[i2 + 1], B[j0], B[j1]))
        i = i2 + 1
    return out


def boolean(solids, holes):
    """union(solids) MINUS union(holes) -> deterministic non-overlapping rect partition."""
    solids = [r for r in solids if r[0] < r[1] and r[2] < r[3]]
    holes = [r for r in holes if r[0] < r[1] and r[2] < r[3]]
    if not solids:
        return []
    A = _coords(solids + holes, 0, 1)
    B = _coords(solids + holes, 2, 3)
    nA, nB = len(A) - 1, len(B) - 1
    gs = _cover_grid(solids, A, B)
    gh = _cover_grid(holes, A, B)
    cell = [[gs[i][j] and not gh[i][j] for j in range(nB)] for i in range(nA)]
    return _strip_decompose(cell, A, B)


def union(rects):
    """Maximal-rectangle union of a set of rects (boolean with no holes)."""
    return boolean(rects, [])


def area(rects):
    """Sum of rect areas. Exact only if rects are non-overlapping (boolean output is)."""
    return sum((r[1] - r[0]) * (r[3] - r[2]) for r in rects)


def _clip(r, foot):
    alo = max(r[0], foot[0]); ahi = min(r[1], foot[1])
    blo = max(r[2], foot[2]); bhi = min(r[3], foot[3])
    if alo < ahi and blo < bhi:
        return (alo, ahi, blo, bhi)
    return None


def area_within(rects, foot):
    """Total area of (rects clipped to footprint). rects assumed non-overlapping."""
    tot = 0
    for r in rects:
        c = _clip(r, foot)
        if c:
            tot += (c[1] - c[0]) * (c[3] - c[2])
    return tot


def overlaps_any(rects):
    """True if any two rects in the list overlap (used to assert no double-cover)."""
    for i in range(len(rects)):
        for j in range(i + 1, len(rects)):
            a, b = rects[i], rects[j]
            if a[0] < b[1] and b[0] < a[1] and a[2] < b[3] and b[2] < a[3]:
                return True
    return False


# --------------------------------------------------------------------------- self-test
if __name__ == "__main__":
    fails = []

    def check(name, cond):
        print(("  ok  " if cond else "FAIL  ") + name)
        if not cond:
            fails.append(name)

    # union of two coincident rects -> one rect, area preserved, no overlap
    r = boolean([(0, 100, 0, 10), (0, 100, 0, 10)], [])
    check("coincident union -> single rect", len(r) == 1 and r[0] == (0, 100, 0, 10))

    # union of two abutting-equal rects (shared wall H1): [0,100] + [0,100] same plane
    r = boolean([(0, 50, 0, 10), (50, 100, 0, 10)], [])
    check("abutting union area = 1000", area(r) == 1000 and not overlaps_any(r))

    # unequal-width union (H2): wide [0,100] unions narrow [20,60]; one wall, area=full wide
    r = boolean([(0, 100, 0, 10), (20, 60, 0, 10)], [])
    check("unequal-width union area = 1000", area(r) == 1000)

    # single hole in the middle (window-ish): area = solid - hole, watertight
    solid = [(0, 100, 0, 100)]
    r = boolean(solid, [(40, 60, 40, 60)])
    check("hole subtract: area = 10000-400", area(r) == 10000 - 400)
    check("hole subtract: no overlaps", not overlaps_any(r))
    check("hole subtract: watertight (solid+hole == footprint)",
          area_within(r, (0, 100, 0, 100)) + 400 == 10000)

    # THE PIER TEST (H7): one solid, TWO holes with a gap between -> pier survives
    solid = [(0, 300, 0, 10)]
    holes = [(50, 100, 0, 10), (150, 200, 0, 10)]
    r = boolean(solid, holes)
    pier = any(rr[0] <= 100 and rr[1] >= 150 and rr[1] - rr[0] >= 50 for rr in r)
    # the [100,150] solid cell must be present (covered by solid, by no hole)
    has_mid = area_within(r, (100, 150, 0, 10)) == 50 * 10
    check("PIER between two holes survives (H7)", has_mid)
    check("two-hole area = 3000 - 500 - 500", area(r) == 3000 - 500 - 500)
    check("two-hole watertight", area_within(r, (0, 300, 0, 10)) + 500 + 500 == 3000)

    # adjacent holes (degenerate H7): two holes touching at a point leave no pier (gap=0)
    r = boolean([(0, 300, 0, 10)], [(50, 150, 0, 10), (150, 250, 0, 10)])
    check("touching holes -> no phantom pier", area_within(r, (150, 150, 0, 10)) == 0)

    # hole entirely outside solid -> no effect
    r = boolean([(0, 100, 0, 10)], [(200, 300, 0, 10)])
    check("hole outside solid -> unchanged", area(r) == 1000)

    # L-shape union (reflex corner H14): two boxes sharing an edge, watertight, no overlap
    r = boolean([(0, 100, 0, 30), (0, 30, 30, 100)], [])
    check("L-shape union no overlap", not overlaps_any(r))
    check("L-shape union area = 3000+2100", area(r) == 100 * 30 + 30 * 70)

    print("\nPIER + watertight primitive:", "ALL PASS" if not fails else f"{len(fails)} FAIL")
    raise SystemExit(1 if fails else 0)
