"""dd_seedmarkers — spawn one movable ADraftDeskThreshold marker at every connection of a layout,
positioned at its resolved world opening (via dd_anchor), so the author can drag them then run dd_sync.
Clears existing markers first. Usage:

  python dd_seedmarkers.py            # seed markers for the example layout (dd_castle)
  python dd_seedmarkers.py my_layout
"""
import importlib
import json
import sys

import ddrun
import dd_anchor

layout_mod = sys.argv[1] if len(sys.argv) > 1 else "dd_castle"
L = importlib.import_module(layout_mod).L
seeds = dd_anchor.seeds(L)

META = []
for sd in seeds:
    t = L.thresholds[sd["i"]]
    META.append({"label": sd["label"], "x": sd["x"], "y": sd["y"], "z": sd["z"],
                 "kind": t["Kind"], "plane": t["Plane"], "roomA": t["RoomA"], "roomB": t["RoomB"],
                 "width": t["Width"], "height": t["Height"], "isEntry": t["bIsEntry"]})

SPAWN = '''import json
ADD = "editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_class"
SET = "editor_toolset.toolsets.object.ObjectTools.set_properties"
FIND = "editor_toolset.toolsets.scene.SceneTools.find_actors"
RM = "editor_toolset.toolsets.scene.SceneTools.remove_from_scene"
CLS = "{{THRESH}}"
PTS = json.loads(r"""__PTS__""")
def run():
    old = execute_tool(FIND, json.dumps({"name": "", "tag": "", "collision_channels": [],
        "actor_type": {"refPath": CLS}}))["returnValue"]
    for o in old:
        try:
            execute_tool(RM, json.dumps({"actor": {"refPath": o["refPath"]}}))
        except Exception:
            pass
    n = 0
    for p in PTS:
        r = execute_tool(ADD, json.dumps({"actor_type": {"refPath": CLS}, "name": "DD_TH_" + p["label"],
            "xform": {"location": {"x": p["x"], "y": p["y"], "z": p["z"]},
                      "rotation": {"pitch": 0, "yaw": 0, "roll": 0}, "scale": {"x": 1, "y": 1, "z": 1}}}))["returnValue"]
        try:
            execute_tool(SET, json.dumps({"instance": {"refPath": r["refPath"]}, "values": json.dumps(
                {"Kind": p["kind"], "Plane": p["plane"], "RoomA": p["roomA"], "RoomB": p["roomB"],
                 "Width": p["width"], "Height": p["height"], "bIsEntry": p["isEntry"], "Label": p["label"]})}))
        except Exception:
            pass
        n += 1
    return {"spawned": n, "cleared": len(old)}
'''

script = SPAWN.replace("__PTS__", json.dumps(META))
print("seed points:", len(META))
print(ddrun.run_text(script))

import dd_save                          # persist the markers so they survive an editor restart
print("saved:", dd_save.save())
