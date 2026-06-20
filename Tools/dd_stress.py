"""dd_stress — prove the DURABLE engine clamp by driving EXTREME values straight into a link
(no marker round-trip). Confirms CarveOpenings keeps the opening on the shared wall for ANY source.

  python dd_stress.py <scenario> [label] [layout]
    slidefar  - Position = +100000  (door clamps flush to the far wall end, stays on wall)
    slideneg  - Position = -100000  (clamps flush to the near end)
    toowide   - Width = 100000      (Weff caps to overlap; door fills shared wall, no overflow)
    mergeuneq - Kind=Open, Width=big (full SHARED-overlap open; non-overlap wall stays solid)
    reset     - canonical baseline (no mutation)
  label defaults to 21-23 (a side-wing doorway, clamp is visible); layout defaults to dd_castle.
"""
import importlib
import sys

import ddrun
import dd_config as C

scen = sys.argv[1] if len(sys.argv) > 1 else "reset"
label = sys.argv[2] if len(sys.argv) > 2 else "21-23"
layout_mod = sys.argv[3] if len(sys.argv) > 3 else "dd_castle"
L = importlib.import_module(layout_mod).L

def find(lbl):
    a, b = lbl.split("-")
    for lk in L.links:
        if str(lk.get("RoomA")) == a and str(lk.get("RoomB")) == b:
            return lk
    return None

target = find(label) or L.links[1]
if scen == "slidefar":
    target["Position"] = 100000.0
elif scen == "slideneg":
    target["Position"] = -100000.0
elif scen == "toowide":
    target["Width"] = 100000.0
elif scen == "mergeuneq":
    target["Kind"] = "Open"; target["Width"] = 100000.0

L.write_apply("_apply.py", gen=C.GEN)
print("scenario:", scen, "| target", target.get("RoomA"), target.get("RoomB"),
      "| Pos", target.get("Position"), "| W", target.get("Width"), "| Kind", target.get("Kind"))
print("apply:", ddrun.run("_apply.py"))
