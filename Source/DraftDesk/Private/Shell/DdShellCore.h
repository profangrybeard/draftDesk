// draftDesk SHELL v1 — portable geometry core (UE-agnostic).
//
// A 1:1 port of Tools/shell/rects2d.py + shell.py. NO Unreal types: this header compiles
// standalone (cl.exe, for the battery test) AND is included by DraftDeskGenerator.cpp, which
// converts its reflected USTRUCTs into these plain structs at the boundary. The Python module
// stays the executable oracle; this mirrors it exactly so the watertight contract is one piece
// of logic proven in two places.
//
// EMIT POLICY (open-Q5, owner decision 2026-06-21): deterministic STRIP DECOMPOSITION, not
// greedy maximal-rectangle merge — seam-aligned, reproducible boxes for the GAME356 kit handoff.
#pragma once

#include <vector>
#include <string>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace dd {

// ============================================================================ rects2d
// Half-open rect [alo,ahi) x [blo,bhi). Axis A is the plane's in-plane axis; axis B is Z
// (walls) or the 2nd footprint axis (slabs).
struct Rect {
    double alo = 0, ahi = 0, blo = 0, bhi = 0;
    Rect() = default;
    Rect(double a0, double a1, double b0, double b1) : alo(a0), ahi(a1), blo(b0), bhi(b1) {}
};

inline long long ddround(double v) {
    // round-half-to-even, matching Python's round() (FE_TONEAREST is the default mode).
    return static_cast<long long>(std::nearbyint(v));
}

inline std::vector<double> coords(const std::vector<Rect>& rects, bool aAxis) {
    std::set<double> s;
    for (const Rect& r : rects) {
        if (aAxis) { s.insert(r.alo); s.insert(r.ahi); }
        else       { s.insert(r.blo); s.insert(r.bhi); }
    }
    return std::vector<double>(s.begin(), s.end());
}

inline int lower_idx(const std::vector<double>& xs, double v) {
    return static_cast<int>(std::lower_bound(xs.begin(), xs.end(), v) - xs.begin());
}

// grid[i][j] True iff sub-cell [A[i],A[i+1]) x [B[j],B[j+1]) is covered by >=1 rect.
inline std::vector<std::vector<char>> cover_grid(
    const std::vector<Rect>& rects, const std::vector<double>& A, const std::vector<double>& B) {
    int nA = (int)A.size() - 1, nB = (int)B.size() - 1;
    std::vector<std::vector<char>> g(std::max(nA, 0), std::vector<char>(std::max(nB, 0), 0));
    for (const Rect& r : rects) {
        int ia0 = lower_idx(A, r.alo), ia1 = lower_idx(A, r.ahi);
        int ib0 = lower_idx(B, r.blo), ib1 = lower_idx(B, r.bhi);
        for (int i = ia0; i < ia1; ++i)
            for (int j = ib0; j < ib1; ++j)
                g[i][j] = 1;
    }
    return g;
}

// Deterministic strip decomposition: strip along axis A into columns, collect each column's
// maximal B-runs as a signature, merge consecutive columns with identical signatures, emit one
// box per run. Every box edge lands on a compressed coordinate; output is a pure function of the
// cell grid (order-independent — unlike greedy).
inline std::vector<Rect> strip_decompose(
    const std::vector<std::vector<char>>& cell, const std::vector<double>& A, const std::vector<double>& B) {
    int nA = (int)A.size() - 1, nB = (int)B.size() - 1;
    std::vector<std::vector<std::pair<int,int>>> sigs(nA);
    for (int i = 0; i < nA; ++i) {
        const std::vector<char>& col = cell[i];
        int j = 0;
        while (j < nB) {
            if (col[j]) {
                int j0 = j;
                while (j < nB && col[j]) ++j;
                sigs[i].push_back({j0, j});
            } else ++j;
        }
    }
    std::vector<Rect> out;
    int i = 0;
    while (i < nA) {
        if (sigs[i].empty()) { ++i; continue; }
        int i2 = i;
        while (i2 + 1 < nA && sigs[i2 + 1] == sigs[i]) ++i2;
        for (const auto& run : sigs[i])
            out.emplace_back(A[i], A[i2 + 1], B[run.first], B[run.second]);
        i = i2 + 1;
    }
    return out;
}

// union(solids) MINUS union(holes) -> deterministic non-overlapping rect partition.
inline std::vector<Rect> rboolean(std::vector<Rect> solids, std::vector<Rect> holes) {
    auto bad = [](const Rect& r) { return !(r.alo < r.ahi && r.blo < r.bhi); };
    solids.erase(std::remove_if(solids.begin(), solids.end(), bad), solids.end());
    holes.erase(std::remove_if(holes.begin(), holes.end(), bad), holes.end());
    if (solids.empty()) return {};
    std::vector<Rect> all = solids;
    all.insert(all.end(), holes.begin(), holes.end());
    std::vector<double> A = coords(all, true), B = coords(all, false);
    int nA = (int)A.size() - 1, nB = (int)B.size() - 1;
    auto gs = cover_grid(solids, A, B);
    auto gh = cover_grid(holes, A, B);
    std::vector<std::vector<char>> cell(nA, std::vector<char>(nB, 0));
    for (int i = 0; i < nA; ++i)
        for (int j = 0; j < nB; ++j)
            cell[i][j] = gs[i][j] && !gh[i][j];
    return strip_decompose(cell, A, B);
}

inline std::vector<Rect> runion(const std::vector<Rect>& rects) { return rboolean(rects, {}); }

inline double area(const std::vector<Rect>& rects) {
    double t = 0;
    for (const Rect& r : rects) t += (r.ahi - r.alo) * (r.bhi - r.blo);
    return t;
}

inline bool clip(const Rect& r, const Rect& f, Rect& out) {
    double alo = std::max(r.alo, f.alo), ahi = std::min(r.ahi, f.ahi);
    double blo = std::max(r.blo, f.blo), bhi = std::min(r.bhi, f.bhi);
    if (alo < ahi && blo < bhi) { out = Rect(alo, ahi, blo, bhi); return true; }
    return false;
}

inline double area_within(const std::vector<Rect>& rects, const Rect& foot) {
    double t = 0; Rect c;
    for (const Rect& r : rects)
        if (clip(r, foot, c)) t += (c.ahi - c.alo) * (c.bhi - c.blo);
    return t;
}

inline bool overlaps_any(const std::vector<Rect>& rects) {
    for (size_t i = 0; i < rects.size(); ++i)
        for (size_t j = i + 1; j < rects.size(); ++j) {
            const Rect& a = rects[i]; const Rect& b = rects[j];
            if (a.alo < b.ahi && b.alo < a.ahi && a.blo < b.bhi && b.blo < a.bhi) return true;
        }
    return false;
}

// ============================================================================ shell
enum { WEST = 0, EAST = 1, SOUTH = 2, NORTH = 3 };
enum { CLASS_X = 0, CLASS_Y = 1, CLASS_SLAB = 2 };
enum Plane { VERTICAL = 0, HORIZONTAL = 1 };
enum Kind { Doorway = 0, Passage, Window, Rail, Stairwell, Ramp, Hatch, Skylight, Atrium };

struct Metrics {
    double grid = 50.0, wall_thickness = 30.0, door_width = 240.0, door_height = 200.0;
    double corridor_width = 200.0, ceiling_min = 300.0, half_wall = 100.0;
    double window_clear = 130.0, window_sill = 100.0;
    double step_rise = 18.0, step_run = 30.0, max_step_angle = 40.0;
    double T() const {
        double g = grid;
        return g > 0 ? std::max(g, std::ceil(wall_thickness / g) * g) : wall_thickness;
    }
    double slab_t() const { return T(); }
};

struct Level {
    int index = 0; double base_z = 0, height = 300, slab_t = 50;
    Level() = default;
    Level(int i, double bz, double h, double st) : index(i), base_z(bz), height(h), slab_t(st) {}
};

struct Room {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int level = 0;
    bool has_floor_z = false; double floor_z = 0;
    double height = 0.0;
    bool floor = true, ceil = true;
    std::string name;
    Room() = default;
    Room(double a, double b, double c, double d) : x0(a), y0(b), x1(c), y1(d) {}
    double W() const { return x1 - x0; }
    double D() const { return y1 - y0; }
    // fluent setters (parity with battery.py kwargs)
    Room& lvl(int v) { level = v; return *this; }
    Room& fz(double v) { has_floor_z = true; floor_z = v; return *this; }
    Room& h(double v) { height = v; return *this; }
    Room& noceil() { ceil = false; return *this; }
    Room& nofloor() { floor = false; return *this; }
    Room& nm(const std::string& s) { name = s; return *this; }
};

struct Threshold {
    int room_a = 0, room_b = -1;
    Kind kind = Doorway;
    Plane plane = VERTICAL;
    double position = 0, position2 = 0, width = 0, depth = 0, height = 0, sill = 0;
    bool is_entry = false;
    bool bRamp = false;   // Stairwell/Atrium with bRamp => pitched slab instead of treads
    int edge = WEST;
    std::string name;
    Threshold() = default;
    Threshold(int a, int b = -1, Kind k = Doorway) : room_a(a), room_b(b), kind(k) {}
    Threshold& pln(Plane p) { plane = p; return *this; }
    Threshold& pos(double v) { position = v; return *this; }
    Threshold& pos2(double v) { position2 = v; return *this; }
    Threshold& w(double v) { width = v; return *this; }
    Threshold& d(double v) { depth = v; return *this; }
    Threshold& h(double v) { height = v; return *this; }
    Threshold& sl(double v) { sill = v; return *this; }
    Threshold& entry() { is_entry = true; return *this; }
    Threshold& ramp() { bRamp = true; return *this; }
    Threshold& eg(int e) { edge = e; return *this; }
    Threshold& nm(const std::string& s) { name = s; return *this; }
};

// A stair / ramp flight derived alongside its slab well (the generator renders the treads;
// the core owns the math so it stays testable). One per resolved Stairwell/Ramp threshold.
struct Flight {
    bool along_x; double start_u, cross_v, w, z0, z1; int dir; bool ramp; int thr;
};

// A ready-to-place 3D box (cm). emit_boxes() returns one per solid output rect across all buckets.
// kind: 0 = wall, 1 = floor-bearing slab (someone stands on it — always emit), 2 = ceiling/roof-only
// slab (hideable for a top-down editor view; the watertight VALIDATION always sees the full shell).
struct Box { double cx, cy, cz, sx, sy, sz; int kind; };

// Bucket key. Walls: cls in {0,1}, a = round(plane), tag=0. Slabs: cls=2, tag=0 + a=level index
// (a real interface) OR tag=1 + a=round(ceiling_z) (a roof bucket with no level above).
struct Key {
    int cls = 0; long long a = 0; int tag = 0;
    bool operator<(const Key& o) const {
        if (cls != o.cls) return cls < o.cls;
        if (tag != o.tag) return tag < o.tag;
        return a < o.a;
    }
    bool operator==(const Key& o) const { return cls == o.cls && a == o.a && tag == o.tag; }
};
inline Key wall_key(int cls, double plane) { return Key{cls, ddround(plane), 0}; }
inline Key slab_key_level(int iface) { return Key{CLASS_SLAB, (long long)iface, 0}; }
inline Key slab_key_roof(double z) { return Key{CLASS_SLAB, ddround(z), 1}; }

// The interface a room's ceiling belongs to: a real level (is_level) or a roof bucket.
struct Iface { bool is_level = false; int level = 0; double roofz = 0; };

struct Aperture { double alo, ahi, blo, bhi; Kind kind; int thr; };
struct FaceRect { double alo, ahi, blo, bhi; int room; };
struct Contributor { int room; int kind; double facez; };  // kind: 0 floor, 1 ceil

struct Bucket {
    int cls = 0; Key key;
    std::vector<FaceRect> faces;
    std::vector<Aperture> apertures;
    std::vector<Contributor> contributors;
    std::vector<Rect> voids;
    std::vector<Rect> solid;
    // emission metadata (set at deposit; does NOT affect the watertight Boolean / digest).
    // walls: `plane` = perpendicular coord, `thick` = wall thickness, box Z = the rect's B span.
    // slabs: box Z = [slab_zlo, slab_zhi]; the rect's (A,B) is the XY footprint.
    double plane = 0, thick = 0, slab_zlo = 0, slab_zhi = 0;
    std::vector<Rect> face_rects() const {
        std::vector<Rect> v; v.reserve(faces.size());
        for (const auto& f : faces) v.emplace_back(f.alo, f.ahi, f.blo, f.bhi);
        return v;
    }
    std::vector<Rect> ap_rects() const {
        std::vector<Rect> v; v.reserve(apertures.size());
        for (const auto& a : apertures) v.emplace_back(a.alo, a.ahi, a.blo, a.bhi);
        return v;
    }
};

inline double step_total_run(double dz, const Metrics& m) {
    if (dz <= 1) return 0.0;
    double nr = std::ceil(dz / m.step_rise);
    double tanmax = std::tan((std::min(m.max_step_angle, 89.0)) * 3.14159265358979323846 / 180.0);
    double na = tanmax > 1e-4 ? std::ceil(dz / (m.step_run * tanmax)) : nr;
    return std::max(1.0, std::max(nr, na)) * m.step_run;
}

class Shell {
public:
    std::vector<Room> rooms;
    std::vector<Threshold> thresholds;
    std::vector<Level> levels;
    Metrics metrics;
    std::map<Key, Bucket> buckets;
    std::vector<std::string> errors, warnings;
    std::vector<int> unresolved;
    std::vector<Flight> flights;       // OUTPUT: flights derived from Stairwell/Ramp thresholds
    std::vector<Flight> in_flights;    // INPUT: explicit flights, used to derive rail gaps

    Shell(std::vector<Room> r, std::vector<Threshold> t, std::vector<Level> lv, Metrics m,
          std::vector<Flight> fin = {})
        : rooms(std::move(r)), thresholds(std::move(t)), levels(std::move(lv)), metrics(m),
          in_flights(std::move(fin)) {
        if (levels.empty()) infer_levels();
    }

    Shell& build() { pass0(); pass1(); pass2(); rail_gaps_from_flights(); emit(); return *this; }

    // RailGap-from-flight: where an explicit flight lands at a RAILED edge, notch the rail at the
    // flight's CrossV (width = flight width). The gap derives from the flight, never drifting from it.
    void rail_gaps_from_flights() {
        const Metrics& m = metrics; double T = m.T();
        for (const Flight& f : in_flights) {
            double run = step_total_run(std::fabs(f.z1 - f.z0), m);
            double u_top = f.start_u + f.dir * run, cross = f.cross_v;
            for (size_t idx = 0; idx < rooms.size(); ++idx) {
                const Room& r = rooms[idx];
                if (r.W() <= 1 || r.D() <= 1) continue;
                double fz = eff_[idx].first, H = eff_[idx].second;
                if (std::fabs(fz - f.z1) > 1) continue;
                int cls, edge; double plane;
                if (f.along_x) {
                    if (f.dir > 0 && std::fabs(r.x0 - u_top) <= T + 1 && r.y0 <= cross && cross <= r.y1) { cls = CLASS_X; plane = r.x0 - T/2; edge = WEST; }
                    else if (f.dir < 0 && std::fabs(r.x1 - u_top) <= T + 1 && r.y0 <= cross && cross <= r.y1) { cls = CLASS_X; plane = r.x1 + T/2; edge = EAST; }
                    else continue;
                } else {
                    if (f.dir > 0 && std::fabs(r.y0 - u_top) <= T + 1 && r.x0 <= cross && cross <= r.x1) { cls = CLASS_Y; plane = r.y0 - T/2; edge = SOUTH; }
                    else if (f.dir < 0 && std::fabs(r.y1 - u_top) <= T + 1 && r.x0 <= cross && cross <= r.x1) { cls = CLASS_Y; plane = r.y1 + T/2; edge = NORTH; }
                    else continue;
                }
                if (!rail_edges((int)idx).count(edge)) continue;
                double w = f.w > 0 ? f.w : m.corridor_width;
                wall_bucket(cls, plane).apertures.push_back({cross - w/2, cross + w/2, fz, fz + H, Passage, -1});
                break;
            }
        }
    }

    const Bucket* bucket(const Key& k) const {
        auto it = buckets.find(k);
        return it == buckets.end() ? nullptr : &it->second;
    }
    std::pair<double,double> eff(int idx) const { return eff_[idx]; }

    // Every solid output rect as a placed 3D box (cm). Walls extrude `thick` on the perpendicular
    // axis with B as the Z span; slabs extrude [slab_zlo, slab_zhi] with (A,B) the XY footprint.
    std::vector<Box> emit_boxes() const {
        std::vector<Box> out;
        for (auto& kv : buckets) {
            const Bucket& b = kv.second;
            if (b.cls == CLASS_SLAB) {
                double cz = (b.slab_zlo + b.slab_zhi) / 2, sz = b.slab_zhi - b.slab_zlo;
                bool has_floor = false;
                for (const Contributor& c : b.contributors) if (c.kind == 0) { has_floor = true; break; }
                int kind = has_floor ? 1 : 2;
                for (const Rect& r : b.solid)
                    out.push_back({(r.alo+r.ahi)/2, (r.blo+r.bhi)/2, cz, r.ahi-r.alo, r.bhi-r.blo, sz, kind});
            } else {
                for (const Rect& r : b.solid) {
                    double ca = (r.alo+r.ahi)/2, cz = (r.blo+r.bhi)/2, la = r.ahi-r.alo, hz = r.bhi-r.blo;
                    if (b.cls == CLASS_X) out.push_back({b.plane, ca, cz, b.thick, la, hz, 0});  // const-X, A=Y
                    else                  out.push_back({ca, b.plane, cz, la, b.thick, hz, 0});  // const-Y, A=X
                }
            }
        }
        return out;
    }

private:
    std::vector<std::pair<double,double>> eff_;
    bool inferred = false;
    std::map<double,int> zmap;

    bool xy_overlap(const Room& a, const Room& b) const {
        return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
    }
    bool intersect(const Rect& r, const Rect& b, Rect& out) const {
        double alo = std::max(r.alo, b.alo), ahi = std::min(r.ahi, b.ahi);
        double blo = std::max(r.blo, b.blo), bhi = std::min(r.bhi, b.bhi);
        if (alo < ahi && blo < bhi) { out = Rect(alo, ahi, blo, bhi); return true; }
        return false;
    }

    void infer_levels() {
        inferred = true;
        std::set<double> zs;
        for (const Room& r : rooms) zs.insert(r.has_floor_z ? r.floor_z : 0.0);
        int i = 0;
        for (double z : zs) { zmap[z] = i; levels.emplace_back(i, z, metrics.ceiling_min, metrics.slab_t()); ++i; }
    }
    int level_of(const Room& r) const {
        if (inferred) return zmap.at(r.has_floor_z ? r.floor_z : 0.0);
        return r.level;
    }

    Iface ceiling_interface(double ceiling_z) const {
        for (size_t i = 0; i < levels.size(); ++i)
            if (std::fabs(levels[i].base_z - (ceiling_z + metrics.slab_t())) < 1e-6)
                return Iface{true, (int)i, 0};
        Iface f; f.is_level = false; f.roofz = ceiling_z; return f;
    }
    Key slab_key(const Iface& f) const {
        return f.is_level ? slab_key_level(f.level) : slab_key_roof(f.roofz);
    }

    Bucket& wall_bucket(int cls, double plane) {
        Key k = wall_key(cls, plane);
        Bucket& b = buckets[k]; b.cls = cls; b.key = k; return b;
    }
    Bucket& slab_bucket(const Iface& f) {
        Key k = slab_key(f);
        Bucket& b = buckets[k]; b.cls = CLASS_SLAB; b.key = k; return b;
    }

    // (axis, pa, pb, lo, hi); returns false if rooms don't face.
    bool face_connection(int a, int b, int& axis, double& pa, double& pb, double& lo, double& hi) const {
        const Room& A = rooms[a]; const Room& B = rooms[b]; double T = metrics.T();
        struct C { double gap; int axis; double pa, pb, lo, hi; };
        std::vector<C> cands;
        double oyl = std::max(A.y0, B.y0), oyh = std::min(A.y1, B.y1);
        if (oyh > oyl) {
            if (A.x1 <= B.x0 + 1) cands.push_back({std::fabs(B.x0 - A.x1), CLASS_X, A.x1 + T/2, B.x0 - T/2, oyl, oyh});
            if (B.x1 <= A.x0 + 1) cands.push_back({std::fabs(A.x0 - B.x1), CLASS_X, A.x0 - T/2, B.x1 + T/2, oyl, oyh});
        }
        double oxl = std::max(A.x0, B.x0), oxh = std::min(A.x1, B.x1);
        if (oxh > oxl) {
            if (A.y1 <= B.y0 + 1) cands.push_back({std::fabs(B.y0 - A.y1), CLASS_Y, A.y1 + T/2, B.y0 - T/2, oxl, oxh});
            if (B.y1 <= A.y0 + 1) cands.push_back({std::fabs(A.y0 - B.y1), CLASS_Y, A.y0 - T/2, B.y1 + T/2, oxl, oxh});
        }
        if (cands.empty()) return false;
        std::stable_sort(cands.begin(), cands.end(), [](const C& x, const C& y){ return x.gap < y.gap; });
        axis = cands[0].axis; pa = cands[0].pa; pb = cands[0].pb; lo = cands[0].lo; hi = cands[0].hi;
        return true;
    }

    // -1 if no facing.
    int internal_edge(int a, int b) const {
        int axis; double pa, pb, lo, hi;
        if (!face_connection(a, b, axis, pa, pb, lo, hi)) return -1;
        const Room& A = rooms[a]; const Room& B = rooms[b];
        if (axis == CLASS_X) return A.x1 <= B.x0 + 1 ? EAST : WEST;
        return A.y1 <= B.y0 + 1 ? NORTH : SOUTH;
    }

    // returns {edge -> threshold idx}
    std::map<int,int> rail_edges(int room_idx) const {
        std::map<int,int> out;
        bool elevated = rooms[room_idx].level > 0;
        for (size_t ti = 0; ti < thresholds.size(); ++ti) {
            const Threshold& t = thresholds[ti];
            if (t.kind == Rail && t.room_a == room_idx) {
                if (t.room_b == -1 && !elevated) continue;
                int e = (t.room_b == -1) ? t.edge : internal_edge(t.room_a, t.room_b);
                if (e >= 0) out[e] = (int)ti;
            }
        }
        return out;
    }

    void resolve_exterior(const Threshold& t, int& cls, double& pa, double& lo, double& hi) const {
        const Room& A = rooms[t.room_a]; double T = metrics.T();
        if (t.edge == WEST)      { cls = CLASS_X; pa = A.x0 - T/2; lo = A.y0; hi = A.y1; }
        else if (t.edge == EAST) { cls = CLASS_X; pa = A.x1 + T/2; lo = A.y0; hi = A.y1; }
        else if (t.edge == SOUTH){ cls = CLASS_Y; pa = A.y0 - T/2; lo = A.x0; hi = A.x1; }
        else                     { cls = CLASS_Y; pa = A.y1 + T/2; lo = A.x0; hi = A.x1; }
    }

    void aperture_zband(const Threshold& t, double fz, double H, double& zlo, double& zhi) const {
        const Metrics& m = metrics; double top = fz + H;
        if (t.kind == Doorway) {
            double h = t.height > 0 ? t.height : m.door_height;
            zlo = fz; zhi = fz + std::min(h, H); return;
        }
        if (t.kind == Window) {
            double sill = t.sill > 0 ? t.sill : m.window_sill;
            double clear = t.height > 0 ? t.height : m.window_clear;
            zlo = fz + sill; zhi = std::min(fz + sill + clear, top); return;
        }
        if (t.kind == Rail) { zlo = fz; zhi = fz + m.half_wall; return; }
        zlo = fz; zhi = top;
    }

    // ---------------------------------------------------------------- passes
    void pass0() {
        const Metrics& m = metrics; auto& L = levels;
        for (size_t n = 0; n + 1 < L.size(); ++n) {
            double want = L[n].base_z + L[n].height + L[n].slab_t;
            if (std::fabs(L[n+1].base_z - want) > 1e-6)
                errors.push_back("BaseZ invariant: Level " + std::to_string(n+1) + " base_z mismatch");
        }
        eff_.assign(rooms.size(), {0,0});
        for (size_t idx = 0; idx < rooms.size(); ++idx) {
            Room& r = rooms[idx];
            int lvl = level_of(r); r.level = lvl;
            double base = L[lvl].base_z;
            double fz = r.has_floor_z ? r.floor_z : base;
            if (r.has_floor_z && std::fabs(r.floor_z - base) > 1e-6) {
                warnings.push_back("room " + std::to_string(idx) + " FloorZ != level base; snapping");
                fz = base;
            }
            double e = r.height > 0 ? r.height : L[lvl].height;
            e = std::max(e, m.ceiling_min);
            eff_[idx] = {fz, e};
        }
        // V9 desync guard
        for (size_t idx = 0; idx < rooms.size(); ++idx) {
            Room& r = rooms[idx];
            if (!(r.height > 0)) continue;
            double fz = eff_[idx].first, e = eff_[idx].second, zc = fz + e;
            if (ceiling_interface(zc).is_level) continue;
            bool stacked_on = false;
            for (size_t j = 0; j < rooms.size(); ++j)
                if ((int)j != (int)idx && std::fabs(eff_[j].first - zc) <= m.slab_t() + 1e-6 && xy_overlap(r, rooms[j]))
                    { stacked_on = true; break; }
            if (stacked_on)
                errors.push_back("V9 Height desync: room " + std::to_string(idx) + " ceiling carries a stacked room but lands on no level interface");
        }
    }

    void pass1() {
        const Metrics& m = metrics; double T = m.T();
        for (size_t idx = 0; idx < rooms.size(); ++idx) {
            Room& r = rooms[idx];
            if (r.W() <= 1 || r.D() <= 1) continue;
            double fz = eff_[idx].first, H = eff_[idx].second, top = fz + H;
            double st = m.slab_t();
            std::map<int,int> rails = rail_edges((int)idx);
            auto zt = [&](int edge) { return fz + (rails.count(edge) ? m.half_wall : H); };
            auto wall = [&](int cls, double plane, double a0, double a1, double z1) {
                Bucket& b = wall_bucket(cls, plane);
                b.plane = plane; b.thick = T;
                b.faces.push_back({a0, a1, fz, z1, (int)idx});
            };
            wall(CLASS_X, r.x0 - T/2, r.y0 - T, r.y1 + T, zt(WEST));
            wall(CLASS_X, r.x1 + T/2, r.y0 - T, r.y1 + T, zt(EAST));
            wall(CLASS_Y, r.y0 - T/2, r.x0 - T, r.x1 + T, zt(SOUTH));
            wall(CLASS_Y, r.y1 + T/2, r.x0 - T, r.x1 + T, zt(NORTH));
            Rect foot(r.x0 - T, r.x1 + T, r.y0 - T, r.y1 + T);
            if (r.floor) {
                Iface f{true, r.level, 0};
                Bucket& b = slab_bucket(f);
                b.faces.push_back({foot.alo, foot.ahi, foot.blo, foot.bhi, (int)idx});
                b.contributors.push_back({(int)idx, 0, fz});
                b.thick = st; b.slab_zlo = fz - st; b.slab_zhi = fz;   // floor slab sits below the surface
            }
            if (r.ceil) {
                Iface f = ceiling_interface(top);
                Bucket& b = slab_bucket(f);
                b.faces.push_back({foot.alo, foot.ahi, foot.blo, foot.bhi, (int)idx});
                b.contributors.push_back({(int)idx, 1, top});
                b.thick = st; b.slab_zlo = top; b.slab_zhi = top + st; // ceiling slab sits above the room
            }
        }
    }

    void pass2() {
        int n = (int)rooms.size();
        for (size_t ti = 0; ti < thresholds.size(); ++ti) {
            const Threshold& t = thresholds[ti];
            bool bad_a = !(0 <= t.room_a && t.room_a < n);
            bool bad_b = (t.room_b != -1) && !(0 <= t.room_b && t.room_b < n);
            if (bad_a || bad_b) {
                warnings.push_back("threshold " + std::to_string(ti) + " (" + t.name + ") bad room index; skipped");
                unresolved.push_back((int)ti); continue;
            }
            if (t.plane == HORIZONTAL || t.kind == Stairwell || t.kind == Ramp ||
                t.kind == Hatch || t.kind == Skylight || t.kind == Atrium)
                aperture_horizontal((int)ti, t);
            else if (t.kind == Rail)
                rail_gaps((int)ti, t);
            else
                aperture_vertical((int)ti, t);
        }
    }

    void aperture_vertical(int ti, const Threshold& t) {
        const Metrics& m = metrics;
        std::vector<std::pair<int,double>> planes;
        double lo = 0, hi = 0;
        if (t.room_b == -1) {
            int cls; double pa;
            resolve_exterior(t, cls, pa, lo, hi);
            planes.push_back({cls, pa});
        } else {
            int axis; double pa, pb;
            if (!face_connection(t.room_a, t.room_b, axis, pa, pb, lo, hi)) {
                warnings.push_back("threshold " + std::to_string(ti) + " (" + t.name + ") UNRESOLVED: no overlap");
                unresolved.push_back(ti); return;
            }
            planes.push_back({axis, pa}); planes.push_back({axis, pb});
        }
        double span = hi - lo;
        if (span <= 0) {
            warnings.push_back("threshold " + std::to_string(ti) + " (" + t.name + ") UNRESOLVED: zero span");
            unresolved.push_back(ti); return;
        }
        double default_w;
        if (t.kind == Passage) default_w = span;
        else if (t.kind == Doorway || t.kind == Window) default_w = m.door_width;
        else default_w = m.corridor_width;
        double weff = std::min(t.width > 0 ? t.width : default_w, span);
        double mid = (lo + hi) / 2.0;
        double center = std::min(std::max(mid + t.position, lo + weff/2), hi - weff/2);
        double olo = center - weff/2, ohi = center + weff/2;
        double fz = eff_[t.room_a].first, H = eff_[t.room_a].second;
        double zlo, zhi; aperture_zband(t, fz, H, zlo, zhi);
        std::set<std::pair<int,long long>> seen;
        for (auto& pl : planes) {
            std::pair<int,long long> k{pl.first, ddround(pl.second)};
            if (seen.count(k)) continue;
            seen.insert(k);
            wall_bucket(pl.first, pl.second).apertures.push_back({olo, ohi, zlo, zhi, t.kind, ti});
        }
    }

    void rail_gaps(int ti, const Threshold& t) {
        if (t.room_b == -1 && 0 <= t.room_a && t.room_a < (int)rooms.size() && rooms[t.room_a].level == 0)
            warnings.push_back("rail " + std::to_string(ti) + " (" + t.name + ") on a ground-level exterior wall has no drop; emitted as a full wall");
    }

    Rect round_out(const Rect& r, double g) const {
        if (g <= 0) return r;
        return Rect(std::floor(r.alo/g)*g, std::ceil(r.ahi/g)*g, std::floor(r.blo/g)*g, std::ceil(r.bhi/g)*g);
    }
    Rect clamp_rect(const Rect& r, const Rect& bound, double g) const {
        double alo = std::max(r.alo, bound.alo), ahi = std::min(r.ahi, bound.ahi);
        double blo = std::max(r.blo, bound.blo), bhi = std::min(r.bhi, bound.bhi);
        if (ahi - alo < g) { double mid = (r.alo + r.ahi)/2; alo = std::max(bound.alo, mid - g/2); ahi = std::min(bound.ahi, mid + g/2); }
        if (bhi - blo < g) { double mid = (r.blo + r.bhi)/2; blo = std::max(bound.blo, mid - g/2); bhi = std::min(bound.bhi, mid + g/2); }
        return Rect(alo, ahi, blo, bhi);
    }

    void aperture_horizontal(int ti, const Threshold& t) {
        const Metrics& m = metrics;
        int a = t.room_a; const Room& A = rooms[a];
        double fzA = eff_[a].first, HA = eff_[a].second;
        if (t.kind == Atrium) {
            Iface f{true, A.level + 1, 0};
            Rect anchor(A.x0, A.x1, A.y0, A.y1), voidr;
            if (t.width > 0 && t.depth > 0) {
                double cx = (A.x0 + A.x1)/2 + t.position, cy = (A.y0 + A.y1)/2 + t.position2;
                voidr = Rect(cx - t.width/2, cx + t.width/2, cy - t.depth/2, cy + t.depth/2);
            } else voidr = anchor;
            Rect vi;
            if (!intersect(voidr, anchor, vi)) {
                warnings.push_back("atrium " + std::to_string(ti) + " (" + t.name + ") void outside its anchor room; skipped");
                unresolved.push_back(ti); return;
            }
            Bucket& b = slab_bucket(f);
            b.apertures.push_back({vi.alo, vi.ahi, vi.blo, vi.bhi, Atrium, ti});
            b.voids.push_back(vi);
            return;
        }
        if (t.kind == Hatch || t.kind == Skylight || t.room_b == -1) {
            Iface f = ceiling_interface(fzA + HA);
            double w = t.width > 0 ? t.width : m.corridor_width;
            double d = t.depth > 0 ? t.depth : m.corridor_width;
            double cx = (A.x0 + A.x1)/2 + t.position, cy = (A.y0 + A.y1)/2 + t.position2;
            Rect hole, hi;
            hole = Rect(cx - w/2, cx + w/2, cy - d/2, cy + d/2);
            if (!intersect(hole, Rect(A.x0, A.x1, A.y0, A.y1), hi)) {
                warnings.push_back("hatch " + std::to_string(ti) + " (" + t.name + ") outside its room ceiling; skipped");
                unresolved.push_back(ti); return;
            }
            slab_bucket(f).apertures.push_back({hi.alo, hi.ahi, hi.blo, hi.bhi, t.kind, ti});
            return;
        }
        // STAIRWELL / RAMP
        int b = t.room_b;
        double fzB = eff_[b].first;
        int lo_i = (fzA < fzB) ? a : b, hi_i = (fzA < fzB) ? b : a;
        const Room& Lo = rooms[lo_i]; const Room& Hi = rooms[hi_i];
        if (rooms[hi_i].level <= rooms[lo_i].level) {
            warnings.push_back("stairwell " + std::to_string(ti) + " (" + t.name + ") connects same-level rooms; nothing to pierce");
            unresolved.push_back(ti); return;
        }
        double sxl = std::max(Lo.x0, Hi.x0), sxh = std::min(Lo.x1, Hi.x1);
        double syl = std::max(Lo.y0, Hi.y0), syh = std::min(Lo.y1, Hi.y1);
        double dz = std::fabs(fzB - fzA), run = step_total_run(dz, m);
        bool along_x = (sxh > sxl && syh > syl) ? ((sxh - sxl) >= (syh - syl)) : (Lo.W() >= Lo.D());
        Rect well; double start_u, cv, width;
        if (along_x) {
            cv = ((syh > syl) ? (syl + syh)/2 : (Hi.y0 + Hi.y1)/2) + t.position2;
            width = t.width > 0 ? t.width : std::max(m.corridor_width, (syh > syl) ? (syh - syl) : m.corridor_width);
            start_u = ((sxh > sxl) ? (sxl + sxh)/2 : (Hi.x0 + Hi.x1)/2) - run/2 + t.position;
            well = Rect(start_u, start_u + run, cv - width/2, cv + width/2);
        } else {
            cv = ((sxh > sxl) ? (sxl + sxh)/2 : (Hi.x0 + Hi.x1)/2) + t.position2;
            width = t.width > 0 ? t.width : std::max(m.corridor_width, (sxh > sxl) ? (sxh - sxl) : m.corridor_width);
            start_u = ((syh > syl) ? (syl + syh)/2 : (Hi.y0 + Hi.y1)/2) - run/2 + t.position;
            well = Rect(cv - width/2, cv + width/2, start_u, start_u + run);
        }
        // the flight fills the well from the lower floor to the upper floor (raw extent, grid-EXEMPT — R4)
        flights.push_back({along_x, start_u, cv, width, eff_[lo_i].first, eff_[hi_i].first, 1,
                           (t.kind == Ramp || t.bRamp), ti});
        well = round_out(well, m.grid);
        if (sxh > sxl && syh > syl) {
            Rect clamped = clamp_rect(well, Rect(sxl, sxh, syl, syh), m.grid);
            double wl = along_x ? (clamped.ahi - clamped.alo) : (clamped.bhi - clamped.blo);
            if (wl + 1e-6 < run)
                warnings.push_back("stairwell " + std::to_string(ti) + " (" + t.name + ") cramped: well < flight run (shared overlap too small)");
            well = clamped;
        }
        for (int iface = rooms[lo_i].level + 1; iface <= rooms[hi_i].level; ++iface)
            slab_bucket(Iface{true, iface, 0}).apertures.push_back({well.alo, well.ahi, well.blo, well.bhi, t.kind, ti});
    }

    void emit() {
        for (auto& kv : buckets) {
            Bucket& b = kv.second;
            b.solid = rboolean(b.face_rects(), b.ap_rects());
        }
    }

public:
    // ------------------------------------------------------------ validation (5 assertions)
    std::vector<std::string> validate() {
        std::vector<std::string> fails = errors;  // hard failures first
        const Metrics& m = metrics; double T = m.T();

        for (auto& kv : buckets)
            if (overlaps_any(kv.second.solid))
                fails.push_back("A2 double-cover in a bucket");

        for (size_t idx = 0; idx < rooms.size(); ++idx) {
            Room& r = rooms[idx];
            if (r.W() <= 1 || r.D() <= 1) continue;
            double fz = eff_[idx].first, H = eff_[idx].second;
            std::map<int,int> rails = rail_edges((int)idx);
            struct WF { int cls; double plane; double alo, ahi; int edge; };
            WF wf[4] = {
                {CLASS_X, r.x0 - T/2, r.y0 - T, r.y1 + T, WEST},
                {CLASS_X, r.x1 + T/2, r.y0 - T, r.y1 + T, EAST},
                {CLASS_Y, r.y0 - T/2, r.x0 - T, r.x1 + T, SOUTH},
                {CLASS_Y, r.y1 + T/2, r.x0 - T, r.x1 + T, NORTH},
            };
            for (const WF& f : wf) {
                double ztop = fz + (rails.count(f.edge) ? m.half_wall : H);
                assert_face(fails, wall_key(f.cls, f.plane), Rect(f.alo, f.ahi, fz, ztop), "wall");
            }
            Rect sf(r.x0 - T, r.x1 + T, r.y0 - T, r.y1 + T);
            if (r.floor) assert_face(fails, slab_key_level(r.level), sf, "floor");
            if (r.ceil)  assert_face(fails, slab_key(ceiling_interface(fz + H)), sf, "ceil");
        }

        // A3 interface coherence
        for (auto& kv : buckets) {
            const Bucket& b = kv.second;
            if (b.cls != CLASS_SLAB || kv.first.tag != 0) continue;
            int iface = (int)kv.first.a;
            bool has_floor_above = false;
            for (const Contributor& c : b.contributors) if (c.kind == 0) { has_floor_above = true; break; }
            if (!has_floor_above || iface < 0 || iface >= (int)levels.size()) continue;
            double want = levels[iface].base_z;
            for (const Contributor& c : b.contributors)
                if (c.kind == 1 && std::fabs(c.facez + m.slab_t() - want) > 1e-6)
                    fails.push_back("A3 desync: room " + std::to_string(c.room) + " ceiling != interface base (V9)");
        }

        // A4 connection guarantee
        std::set<int> deposited;
        for (auto& kv : buckets) for (const Aperture& ap : kv.second.apertures) deposited.insert(ap.thr);
        std::set<int> unres(unresolved.begin(), unresolved.end());
        for (size_t ti = 0; ti < thresholds.size(); ++ti) {
            if (unres.count((int)ti) || thresholds[ti].kind == Rail) continue;
            if (!deposited.count((int)ti))
                fails.push_back("A4 connection: threshold " + std::to_string(ti) + " (" + thresholds[ti].name + ") resolved but deposited NO aperture");
        }

        // A5 cantilever / void guard
        for (auto& kv : buckets)
            for (const Rect& v : kv.second.voids)
                if (area_within(kv.second.solid, v) > 1e-6)
                    fails.push_back("A5 cantilever: floor covers a void");
        return fails;
    }

private:
    void assert_face(std::vector<std::string>& fails, const Key& key, const Rect& foot, const std::string& label) {
        double foot_area = (foot.ahi - foot.alo) * (foot.bhi - foot.blo);
        const Bucket* b = bucket(key);
        double solid_a = b ? area_within(b->solid, foot) : 0;
        double ap_a = 0;
        if (b && !b->apertures.empty()) ap_a = area_within(runion(b->ap_rects()), foot);
        if (std::fabs(solid_a + ap_a - foot_area) > 1e-6)
            fails.push_back("A1 watertight: " + label + " face not closed");
    }
};

}  // namespace dd
