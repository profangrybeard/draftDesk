// Standalone proof that DdShellCore.h (the C++ geometry core) matches the Python oracle.
// Mirrors battery.py (28 cases) + castle_shell.py. Build/run: cmd //c ".\_build.bat ShellBatteryTest.cpp"
// Pass "digest" as argv[1] to print the canonical castle bucket digest for parity diffing.
#include "../../../Source/DraftDesk/Private/Shell/DdShellCore.h"
#include <functional>
#include <cstdio>
#include <iostream>
#include <string>

using namespace dd;

static int g_total = 0, g_fail = 0;
static std::string g_group;

static void report(const std::string& name, bool ok, const std::vector<std::string>& fails, const std::string& why) {
    ++g_total;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", name.c_str());
    if (!ok) {
        ++g_fail;
        for (size_t i = 0; i < fails.size() && i < 5; ++i) std::printf("        ! %s\n", fails[i].c_str());
        if (!why.empty()) std::printf("        ! %s\n", why.c_str());
    }
}

using Prop = std::function<bool(Shell&)>;
static Metrics M() { Metrics m; m.grid = 50; m.wall_thickness = 30; return m; }  // T=50

static void case_prop(const std::string& name, std::vector<Room> rooms, std::vector<Threshold> thr,
                      std::vector<Level> levels, Metrics m, Prop prop, const std::string& why = "prop",
                      std::vector<Flight> flights = {}) {
    Shell s(std::move(rooms), std::move(thr), std::move(levels), m, std::move(flights)); s.build();
    auto fails = s.validate();
    bool extra = prop ? prop(s) : true;
    report(name, fails.empty() && extra, fails, extra ? "" : why);
}
static void case_fail(const std::string& name, std::vector<Room> rooms, std::vector<Threshold> thr,
                      std::vector<Level> levels, Metrics m, const std::string& expect) {
    Shell s(std::move(rooms), std::move(thr), std::move(levels), m); s.build();
    auto fails = s.validate();
    bool ok = false; for (auto& f : fails) if (f.find(expect) != std::string::npos) ok = true;
    report(name, ok, ok ? std::vector<std::string>{} : (fails.empty() ? std::vector<std::string>{"(no failure raised)"} : fails),
           "expected fail ~ " + expect);
}
static void case_unresolved(const std::string& name, std::vector<Room> rooms, std::vector<Threshold> thr,
                            std::vector<Level> levels, Metrics m, int idx) {
    Shell s(std::move(rooms), std::move(thr), std::move(levels), m); s.build();
    auto fails = s.validate();
    bool un = std::find(s.unresolved.begin(), s.unresolved.end(), idx) != s.unresolved.end();
    report(name, fails.empty() && un, fails, un ? "" : ("threshold " + std::to_string(idx) + " should be unresolved"));
}

static double sld(Shell& s, Key k, Rect foot) { auto b = s.bucket(k); return b ? area_within(b->solid, foot) : 0; }
static Key WX(double p) { return wall_key(CLASS_X, p); }
static Key WY(double p) { return wall_key(CLASS_Y, p); }
static Key SL(int i) { return slab_key_level(i); }
static Key SR(double z) { return slab_key_roof(z); }
static bool has_ap_kind(const Bucket* b, Kind k) { if (!b) return false; for (auto& a : b->apertures) if (a.kind == k) return true; return false; }

static std::vector<Level> none() { return {}; }
static std::vector<Level> LV2() { return {Level(0,0,300,50), Level(1,350,300,50)}; }
static std::vector<Level> LV3() { return {Level(0,0,300,50), Level(1,350,300,50), Level(2,700,300,50)}; }

// ---- canonical digest (must match castle_shell.py 'digest' output byte for byte) ----
static std::string fmt(double v) {
    double r = std::nearbyint(v);
    if (std::fabs(v - r) < 1e-6) return std::to_string((long long)r);
    char buf[32]; std::snprintf(buf, sizeof(buf), "%.4f", v); return buf;
}
static std::string digest(Shell& s) {
    std::string out;
    for (auto& kv : s.buckets) {  // std::map already sorted by (cls,tag,a)
        const Key& k = kv.first; Bucket& b = kv.second;
        std::vector<Rect> rs = b.solid;
        std::sort(rs.begin(), rs.end(), [](const Rect& a, const Rect& c) {
            if (a.alo != c.alo) return a.alo < c.alo;
            if (a.ahi != c.ahi) return a.ahi < c.ahi;
            if (a.blo != c.blo) return a.blo < c.blo;
            return a.bhi < c.bhi;
        });
        out += std::to_string(k.cls) + " " + std::to_string(k.a) + " " + std::to_string(k.tag) + " :";
        for (auto& r : rs)
            out += " (" + fmt(r.alo) + "," + fmt(r.ahi) + "," + fmt(r.blo) + "," + fmt(r.bhi) + ")";
        out += "\n";
    }
    return out;
}

// ============================================================================ presets
// Mirror DraftDeskGenerator BuildPreset_* exactly (same coords) and prove each is watertight in the
// core BEFORE the UE build. Watertightness is translation/grid invariant, so NormalizeToEntry +
// SnapLayoutToGrid are omitted (preset coords are on-grid; snap is identity).
struct Layout { std::vector<Room> rooms; std::vector<Threshold> thr; std::vector<Level> lv; };

static const double PC = 1200, PCW = 200, PT = 50, PCM = 300, PFD = 350, PHL = 1000;
static std::vector<Level> stacked(int n) {
    double storey = std::max(PCM, PFD - PT); std::vector<Level> v;
    for (int k = 0; k < n; ++k) v.push_back(Level(k, k * (storey + PT), storey, PT));
    return v;
}
static std::vector<Level> flat1() { return {Level(0, 0, PCM, PT)}; }

static Layout preset_RoomHallRoom() {
    double ED = PC*0.5, MD = PC, Cx0 = ED+PT+PHL+PT; Layout L; L.lv = flat1();
    L.rooms = {Room(0,-ED*0.5,ED,ED*0.5), Room(ED+PT,-PCW*0.5,ED+PT+PHL,PCW*0.5), Room(Cx0,-MD*0.5,Cx0+MD,MD*0.5)};
    L.thr = {Threshold(0,-1,Doorway).eg(WEST).entry(), Threshold(0,1,Doorway), Threshold(1,2,Doorway)};
    return L;
}
static Layout preset_SingleRoom() { Layout L; L.lv = flat1();
    L.rooms = {Room(0,-PC*0.5,PC,PC*0.5)}; L.thr = {Threshold(0,-1,Doorway).eg(WEST).entry()}; return L; }
static Layout preset_Corridor() { Layout L; L.lv = flat1();
    L.rooms = {Room(0,-PCW*0.5,3*PC,PCW*0.5)};
    L.thr = {Threshold(0,-1,Doorway).eg(WEST).entry(), Threshold(0,-1,Doorway).eg(EAST)}; return L; }
static Layout preset_LBend() { Layout L; L.lv = flat1();
    L.rooms = {Room(0,-PCW*0.5,2*PC,PCW*0.5), Room(2*PC+PT,-PCW*0.5,2*PC+PT+PCW,PCW*0.5),
               Room(2*PC+PT,PCW*0.5+PT,2*PC+PT+PCW,PCW*0.5+PT+2*PC)};
    L.thr = {Threshold(0,1,Passage), Threshold(1,2,Passage), Threshold(0,-1,Doorway).eg(WEST).entry(),
             Threshold(2,-1,Doorway).eg(NORTH)}; return L; }
static Layout preset_TJunction() { Layout L; L.lv = flat1();
    L.rooms = {Room(0,-PCW*0.5,3*PC,PCW*0.5), Room(1.5*PC-PCW*0.5,PCW*0.5+PT,1.5*PC+PCW*0.5,PCW*0.5+PT+2*PC)};
    L.thr = {Threshold(0,-1,Doorway).eg(WEST).entry(), Threshold(0,-1,Doorway).eg(EAST),
             Threshold(1,-1,Doorway).eg(NORTH), Threshold(0,1,Passage)}; return L; }
static Layout preset_Cross() { Layout L; L.lv = flat1(); double Hh = PCW*0.5, Nx0 = 2*PC+PT, Ex0 = Nx0+PCW+PT;
    L.rooms = {Room(0,-Hh,2*PC,Hh), Room(Nx0,-Hh,Nx0+PCW,Hh), Room(Ex0,-Hh,Ex0+2*PC,Hh),
               Room(Nx0,-Hh-PT-2*PC,Nx0+PCW,-Hh-PT), Room(Nx0,Hh+PT,Nx0+PCW,Hh+PT+2*PC)};
    L.thr = {Threshold(0,1,Passage), Threshold(1,2,Passage), Threshold(3,1,Passage), Threshold(1,4,Passage),
             Threshold(0,-1,Doorway).eg(WEST).entry(), Threshold(2,-1,Doorway).eg(EAST),
             Threshold(3,-1,Doorway).eg(SOUTH), Threshold(4,-1,Doorway).eg(NORTH)}; return L; }
static Layout preset_Grid2x2() { Layout L; L.lv = flat1();
    L.rooms = {Room(0,-PC,PC,0), Room(PC+PT,-PC,2*PC+PT,0), Room(0,PT,PC,PC+PT), Room(PC+PT,PT,2*PC+PT,PC+PT)};
    L.thr = {Threshold(0,-1,Doorway).eg(WEST).entry(), Threshold(0,1,Doorway), Threshold(2,3,Doorway),
             Threshold(0,2,Doorway), Threshold(1,3,Doorway)}; return L; }
static Layout preset_SplitLevel() { Layout L; L.lv = stacked(2);
    L.rooms = {Room(0,-PC*0.5,PC,PC*0.5).lvl(0), Room(0,-PC*0.5,PC,PC*0.5).lvl(1)};
    L.thr = {Threshold(0,-1,Doorway).eg(WEST).entry(), Threshold(0,1,Stairwell).pln(HORIZONTAL).w(PCW)}; return L; }
static Layout preset_Tower() { Layout L; L.lv = stacked(3);
    L.rooms = {Room(0,-PC*0.5,PC,PC*0.5).lvl(0), Room(0,-PC*0.5,PC,PC*0.5).lvl(1), Room(0,-PC*0.5,PC,PC*0.5).lvl(2)};
    L.thr = {Threshold(0,-1,Doorway).eg(WEST).entry(),
             Threshold(0,1,Stairwell).pln(HORIZONTAL).w(PCW).pos2(PC*0.25),
             Threshold(1,2,Stairwell).pln(HORIZONTAL).w(PCW).pos2(-PC*0.25)}; return L; }
static Layout preset_Ramp() { Layout L; L.lv = stacked(2);
    L.rooms = {Room(0,-PC*0.5,PC,PC*0.5).lvl(0), Room(0,-PC*0.5,PC,PC*0.5).lvl(1)};
    L.thr = {Threshold(0,-1,Doorway).eg(WEST).entry(), Threshold(0,1,Ramp).pln(HORIZONTAL).w(PCW).ramp()}; return L; }
static Layout preset_Mezzanine() { Layout L; L.lv = stacked(2);
    double storey = std::max(PCM, PFD - PT), TallH = 2*storey + PT, MezX0 = 1.4*PC;
    double VoidW = MezX0, VoidPos = -(2*PC - VoidW)*0.5;
    L.rooms = {Room(0,-PC,2*PC,PC).lvl(0).h(TallH), Room(MezX0,-PC,2*PC,PC).lvl(1)};
    L.rooms[1].ceil = false;
    L.thr = {Threshold(0,-1,Atrium).pln(HORIZONTAL).w(VoidW).d(2*PC).pos(VoidPos),
             Threshold(1,-1,Rail).eg(WEST), Threshold(0,-1,Doorway).eg(WEST).entry(),
             Threshold(0,1,Stairwell).pln(HORIZONTAL).w(PCW)}; return L; }

static void validate_preset(const std::string& name, Layout L) {
    Shell s(std::move(L.rooms), std::move(L.thr), std::move(L.lv), M()); s.build();
    auto fails = s.validate();
    bool ok = fails.empty() && s.warnings.empty();
    report(name, ok, fails, s.warnings.empty() ? "" : (std::to_string(s.warnings.size()) + " warnings: " + (s.warnings.empty() ? "" : s.warnings[0])));
}

// ============================================================================ castle
static Shell build_castle() {
    const double T = 50, CW = 400;
    std::vector<Room> rooms; std::vector<Threshold> thr;
    auto Rm = [&](Room r) -> int { rooms.push_back(r); return (int)rooms.size() - 1; };
    auto door = [&](int a, int b) { thr.push_back(Threshold(a, b, Doorway)); };
    auto doorWH = [&](int a, int b, double w, double h) { thr.push_back(Threshold(a, b, Doorway).w(w).h(h)); };
    auto passage = [&](int a, int b) { thr.push_back(Threshold(a, b, Passage)); };

    int hall = Rm(Room(0,-1000,3000,1000).lvl(0).h(800).nm("hall"));
    int app  = Rm(Room(-1050,-CW/2,-50,CW/2).lvl(0).h(400).nm("approach"));
    int guard= Rm(Room(-850,-750,-350,-250).lvl(0).h(300).nm("guard"));
    int weap = Rm(Room(-850,-1300,-350,-800).lvl(0).h(300).nm("weapons"));
    thr.push_back(Threshold(app, -1, Doorway).eg(WEST).entry().w(350).h(350).nm("entry"));
    doorWH(hall, app, 350, 350);
    door(app, guard);
    door(guard, weap);

    int bal = Rm(Room(2600,-1000,3000,1000).lvl(1).noceil().nm("balcony"));
    thr.push_back(Threshold(bal, -1, Rail).eg(WEST).nm("balcony rail"));

    auto wing = [&](double door_y, double sign, const std::string& tag) {
        int arm = Rm(Room(3050, door_y-CW/2, 3450, door_y+CW/2).lvl(1).nm(tag+"arm"));
        door(bal, arm);
        int node = Rm(Room(3500, door_y-CW/2, 3900, door_y+CW/2).lvl(1).nm(tag+"node"));
        passage(arm, node);
        double near = door_y + sign*(CW/2 + T);
        double far = near + sign*1800.0;
        int corr = Rm(Room(3500, std::min(near,far), 3900, std::max(near,far)).lvl(1).nm(tag+"corr"));
        passage(node, corr);
        double rx0 = 3900 + T;
        for (int k = 0; k < 3; ++k) {
            double ry = near + sign*(400.0 + k*500.0);
            int r = Rm(Room(rx0, ry-200, rx0+500, ry+200).lvl(1).nm(tag+"room"+std::to_string(k)));
            door(corr, r);
        }
        double tc = far + sign*(T + 300.0);
        int shaft = Rm(Room(3500, tc-300, 3900, tc+300).lvl(1).nm(tag+"shaft"));
        passage(corr, shaft);
    };
    wing(-600, -1, "S-");
    wing(600, 1, "N-");

    int ch = Rm(Room(3050,-CW/2,4050,CW/2).lvl(1).nm("chamber")); door(bal, ch);
    int ante = Rm(Room(4100,-400,4700,400).lvl(1).nm("antechamber")); door(ch, ante);
    int bed = Rm(Room(4750,-700,5750,700).lvl(1).h(350).nm("bedroom")); door(ante, bed);
    int bath = Rm(Room(4800,750,5300,1250).lvl(1).nm("bath")); door(bed, bath);
    int closet = Rm(Room(4800,-1250,5300,-750).lvl(1).nm("closet")); door(bed, closet);
    (void)closet;

    std::vector<Level> levels = {Level(0,0,250,50), Level(1,300,300,50)};
    Metrics m; m.grid = 50; m.wall_thickness = 30;
    Shell s(std::move(rooms), std::move(thr), std::move(levels), m);
    s.build();
    return s;
}

// ============================================================================ main
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "digest") {
        Shell s = build_castle();
        std::cout << digest(s);
        return 0;
    }

    std::printf("--- HORIZONTAL ---\n");
    case_prop("H1 equal-width abutment + door",
        {Room(0,0,400,400), Room(450,0,850,400)}, {Threshold(0,1,Doorway)}, none(), M(),
        [](Shell& s){ auto b = s.bucket(WX(425)); return b && b->apertures.size() == 1; }, "one shared wall + one door");
    case_prop("H2 unequal-width abutment",
        {Room(0,0,400,400), Room(450,150,850,250)}, {Threshold(0,1,Passage)}, none(), M(),
        [](Shell& s){ return sld(s, WX(425), Rect(-50,150,0,300)) > 0; }, "wide wall keeps piers");
    case_prop("H3 offset / partial-overlap abutment",
        {Room(0,0,400,400), Room(450,200,850,600)}, {Threshold(0,1,Passage)}, none(), M(),
        [](Shell& s){ return sld(s, WX(425), Rect(0,100,0,300)) == 100*300 && sld(s, WX(425), Rect(500,600,0,300)) == 100*300; },
        "non-overlap tails stay solid");
    case_prop("H4 T-junction",
        {Room(0,0,1200,300), Room(500,350,700,750)}, {Threshold(1,0,Passage)}, none(), M(),
        [](Shell& s){ return sld(s, WY(325), Rect(100,400,0,300)) > 0 && sld(s, WY(325), Rect(800,1100,0,300)) > 0; },
        "two piers flank the stem");
    case_prop("H5 4-way crossing",
        {Room(0,0,400,400), Room(-450,100,-50,300), Room(450,100,850,300), Room(100,-450,300,-50), Room(100,450,300,850)},
        {Threshold(0,1,Passage), Threshold(0,2,Passage), Threshold(0,3,Passage), Threshold(0,4,Passage)}, none(), M(),
        [](Shell& s){ return sld(s, WX(-25), Rect(-50,100,0,300)) > 0 && sld(s, WX(-25), Rect(300,450,0,300)) > 0; },
        "node corner posts survive");
    case_prop("H6 full-width passage -> corner piers",
        {Room(0,0,400,400), Room(450,0,850,400)}, {Threshold(0,1,Passage)}, none(), M(),
        [](Shell& s){ return sld(s, WX(425), Rect(400,450,0,300)) > 0 && sld(s, WX(425), Rect(-50,0,0,300)) > 0; },
        "both +/-T corner piers");
    case_prop("H7 two doors one wall -> pier",
        {Room(0,0,400,400), Room(450,0,850,400)},
        {Threshold(0,1,Doorway).w(100).pos(-100), Threshold(0,1,Doorway).w(100).pos(100)}, none(), M(),
        [](Shell& s){ return sld(s, WX(425), Rect(150,250,0,200)) == 100*200; }, "full pier between two doors");
    case_prop("H8 no-neighbour wall stays solid",
        {Room(0,0,400,400)}, {Threshold(0,-1,Doorway).eg(WEST).entry()}, none(), M(),
        [](Shell& s){ return sld(s, WX(425), Rect(-50,450,0,300)) == 500*300; }, "opposite wall fully solid");
    case_prop("H9 window sill+cap",
        {Room(0,0,400,400)}, {Threshold(0,-1,Window).eg(EAST)}, none(), M(),
        [](Shell& s){ return sld(s, WX(425), Rect(0,400,0,100)) > 0 && sld(s, WX(425), Rect(0,400,230,300)) > 0; },
        "solid below sill + above cap");
    case_prop("H10 door flush to corner",
        {Room(0,0,400,400), Room(450,0,850,400)}, {Threshold(0,1,Doorway).pos(99999)}, none(), M(),
        [](Shell& s){ return sld(s, WX(425), Rect(400,450,0,200)) > 0; }, "corner pier survives the clamp");
    case_prop("H11 over-wide connection clamps to overlap",
        {Room(0,0,400,400), Room(450,0,850,400)}, {Threshold(0,1,Doorway).w(100000)}, none(), M(),
        [](Shell& s){ return sld(s, WX(425), Rect(400,450,0,200)) > 0 && sld(s, WX(425), Rect(0,400,0,200)) == 0; },
        "clamps to overlap, corner pier survives");
    case_unresolved("H12 diagonal / zero-overlap -> UNRESOLVED, no hole",
        {Room(0,0,400,400), Room(450,450,850,850)}, {Threshold(0,1,Doorway)}, none(), M(), 0);
    case_prop("H13 entry / exterior",
        {Room(0,0,400,400)}, {Threshold(0,-1,Doorway).eg(WEST).entry()}, none(), M(),
        [](Shell& s){ auto b = s.bucket(WX(-25)); if (!b) return false; for (auto& a : b->apertures) if (a.thr == 0) return true; return false; },
        "entry carved in the named edge");
    case_prop("H14 L-shape reflex corner",
        {Room(0,0,400,400), Room(0,450,400,850)}, {Threshold(0,1,Passage)}, none(), M(),
        [](Shell&){ return true; });

    std::printf("\n--- VERTICAL ---\n");
    case_prop("V1 two-storey shared slab dedup",
        {Room(0,0,400,400).lvl(0), Room(0,0,400,400).lvl(1)}, {}, LV2(), M(),
        [](Shell& s){ auto b = s.bucket(SL(1)); return b && b->contributors.size() == 2 && area(b->solid) == 500.0*500 && !overlaps_any(b->solid); },
        "one merged slab, 2 contributors");
    case_prop("V2 stairwell floor-opening",
        {Room(0,0,1000,600).lvl(0), Room(0,0,1000,600).lvl(1)},
        {Threshold(0,1,Stairwell).pln(HORIZONTAL).w(200).pos(-200)}, LV2(), M(),
        [](Shell& s){ auto b = s.bucket(SL(1));
            return has_ap_kind(b, Stairwell) && area(b->solid) > 0
                && s.flights.size() == 1 && s.flights[0].z0 == 0 && s.flights[0].z1 == 350; },
        "hole carved, floor remains, one flight 0->350");
    case_prop("V3 atrium (mid-void + 2 galleries)",
        {Room(0,0,1500,600).lvl(0).h(650), Room(0,0,450,600).lvl(1), Room(1050,0,1500,600).lvl(1)},
        {Threshold(0,-1,Atrium).pln(HORIZONTAL).w(500).d(600), Threshold(1,-1,Rail).eg(EAST), Threshold(2,-1,Rail).eg(WEST)},
        LV3(), M(),
        [](Shell& s){ auto b = s.bucket(SL(1)); return area_within(b->solid, Rect(600,900,100,500)) == 0 && area_within(b->solid, Rect(100,400,100,500)) > 0; },
        "mid void open, galleries floored");
    case_prop("V4 mezzanine (partial floor + void)",
        {Room(0,0,1200,600).lvl(0).h(650), Room(700,0,1200,600).lvl(1)},
        {Threshold(0,-1,Atrium).pln(HORIZONTAL).w(650).d(600).pos(-275), Threshold(1,-1,Rail).eg(WEST)}, LV3(), M(),
        [](Shell& s){ auto b = s.bucket(SL(1)); return area_within(b->solid, Rect(800,1100,100,500)) > 0 && area_within(b->solid, Rect(100,400,100,500)) == 0; },
        "mezz floored, void open");
    case_prop("V5 mismatched ceiling heights",
        {Room(0,0,400,400).h(300), Room(450,0,850,400).h(450)}, {Threshold(0,1,Doorway)}, none(), M(),
        [](Shell& s){ return sld(s, WX(425), Rect(0,400,300,450)) > 0; }, "clerestory band solid above short ceiling");
    case_prop("V6 ceiling hatch / skylight",
        {Room(0,0,400,400).lvl(0)}, {Threshold(0,-1,Hatch).pln(HORIZONTAL).w(100).d(100)}, {Level(0,0,300,50)}, M(),
        [](Shell& s){ auto b = s.bucket(SR(300)); return b && area_within(b->solid, Rect(0,100,0,400)) > 0 && area_within(b->solid, Rect(150,250,150,250)) == 0; },
        "surround solid, hole bounded");
    case_prop("V7 cantilever / misaligned stack",
        {Room(0,0,600,600).lvl(0), Room(200,200,800,800).lvl(1)}, {}, LV2(), M(),
        [](Shell& s){ auto b = s.bucket(SL(1)); return b && b->contributors.size() == 2 && area_within(b->solid, Rect(0,600,0,600)) > 0 && area_within(b->solid, Rect(600,800,600,800)) > 0; },
        "seam watertight over both footprints");
    case_prop("V8 roofless + pit",
        {Room(0,0,400,400).noceil(), Room(450,0,850,400).nofloor()}, {Threshold(0,1,Doorway)}, none(), M(),
        [](Shell& s){
            std::set<int> ceils, floors;
            for (auto& kv : s.buckets) if (kv.second.cls == CLASS_SLAB)
                for (auto& c : kv.second.contributors) { if (c.kind == 1) ceils.insert(c.room); else floors.insert(c.room); }
            return ceils.count(0) == 0 && floors.count(1) == 0;
        }, "A no ceiling, B no floor");
    case_fail("V9 BaseZ / height desync FAILS LOUD",
        {Room(0,0,400,400).lvl(0).h(400), Room(0,0,400,400).lvl(1)}, {}, LV2(), M(), "V9 Height desync");
    {
        Metrics m2; m2.grid = 50; m2.wall_thickness = 30; m2.ceiling_min = 200;
        std::vector<Level> lv_og = {Level(0,0,250,50), Level(1,300,250,50)};
        case_prop("V10 off-grid stair landing rounds OUT",
            {Room(0,0,1200,800).lvl(0), Room(0,0,1200,800).lvl(1)},
            {Threshold(0,1,Stairwell).pln(HORIZONTAL).w(200)}, lv_og, m2,
            [](Shell& s){
                auto b = s.bucket(SL(1)); if (!b) return false;
                const Aperture* well = nullptr;
                for (auto& a : b->apertures) if (a.kind == Stairwell) { well = &a; break; }
                if (!well) return false;
                bool on_grid = std::fabs(std::fmod(well->alo,50)) < 1e-6 && std::fabs(std::fmod(well->ahi,50)) < 1e-6
                            && std::fabs(std::fmod(well->blo,50)) < 1e-6 && std::fabs(std::fmod(well->bhi,50)) < 1e-6;
                bool encloses = (well->ahi - well->alo) >= 510 - 1e-6;
                bool floor_beyond = area_within(b->solid, Rect(1000,1100,350,450)) > 0;
                return on_grid && encloses && floor_beyond;
            }, "well grid-aligned, encloses run, floor beyond solid");
    }

    case_prop("V11 rail gap derives from flight",
        {Room(0,0,400,400).lvl(1)}, {Threshold(0,-1,Rail).eg(WEST)},
        {Level(0,0,300,50), Level(1,350,300,50)}, M(),
        [](Shell& s){ auto b = s.bucket(WX(-25)); if (!b) return false;
            return area_within(b->solid, Rect(100,300,350,450)) == 0      // rail notched at the landing
                && area_within(b->solid, Rect(-50,100,350,450)) > 0; },    // rail survives beside the gap
        "WEST rail notched at the flight landing",
        { Flight{true, -600, 200, 200, 0, 350, 1, false, -1} });           // along_x,start_u,cross_v,w,z0,z1,dir,ramp,thr

    std::printf("\n--- ADVERSARIAL REGRESSIONS ---\n");
    case_prop("ADV1 multi-level stairwell pierces every crossed floor",
        {Room(0,0,1000,800).lvl(0), Room(0,0,1000,800).lvl(2)},
        {Threshold(0,1,Stairwell).pln(HORIZONTAL).w(200)}, LV3(), M(),
        [](Shell& s){ return has_ap_kind(s.bucket(SL(1)), Stairwell) && has_ap_kind(s.bucket(SL(2)), Stairwell); },
        "middle (interface 1) AND top (interface 2) both pierced");
    case_prop("ADV2 atrium void cannot carve an unrelated room",
        {Room(0,0,400,400).lvl(0).h(650), Room(1000,0,1400,400).lvl(0), Room(1000,0,1400,400).lvl(1)},
        {Threshold(0,-1,Atrium).pln(HORIZONTAL).w(200).d(200).pos(1000)}, LV2(), M(),
        [](Shell& s){ return area_within(s.bucket(SL(1))->solid, Rect(1050,1350,100,300)) == 300.0*200; },
        "unrelated stack floor stays solid (void clamped to anchor)");
    case_prop("ADV3 ground exterior rail -> full wall (envelope sealed)",
        {Room(0,0,400,400).lvl(0)}, {Threshold(0,-1,Rail).eg(EAST)}, none(), M(),
        [](Shell& s){ return area_within(s.bucket(WX(425))->solid, Rect(-50,450,100,300)) == 500.0*200; },
        "east wall full height, not half-capped");
    case_unresolved("ADV4 bad threshold index -> loud, no crash",
        {Room(0,0,400,400)}, {Threshold(0,5,Doorway)}, none(), M(), 0);

    // ---- presets (mirror the generator; prove each watertight before the UE build) ----
    std::printf("\n--- PRESETS ---\n");
    validate_preset("RoomHallRoom", preset_RoomHallRoom());
    validate_preset("SingleRoom", preset_SingleRoom());
    validate_preset("Corridor", preset_Corridor());
    validate_preset("LBend", preset_LBend());
    validate_preset("TJunction", preset_TJunction());
    validate_preset("Cross", preset_Cross());
    validate_preset("Grid2x2", preset_Grid2x2());
    validate_preset("SplitLevel", preset_SplitLevel());
    validate_preset("Tower", preset_Tower());
    validate_preset("Ramp", preset_Ramp());
    validate_preset("Mezzanine", preset_Mezzanine());

    // ---- castle ----
    std::printf("\n--- CASTLE ---\n");
    {
        Shell s = build_castle();
        auto fails = s.validate();
        report("Castle Chorrol watertight (0 warn, 0 fail)", fails.empty() && s.warnings.empty(), fails,
               s.warnings.empty() ? "" : (std::to_string(s.warnings.size()) + " warnings"));
        auto boxes = s.emit_boxes();
        bool boxes_ok = !boxes.empty();
        for (auto& b : boxes) if (!(b.sx > 0 && b.sy > 0 && b.sz > 0)) boxes_ok = false;
        report("emit_boxes: nonempty, all positive size (" + std::to_string(boxes.size()) + " boxes)", boxes_ok, {}, "degenerate box");
    }

    std::printf("\n%d/%d pass\n", g_total - g_fail, g_total);
    return g_fail ? 1 : 0;
}
