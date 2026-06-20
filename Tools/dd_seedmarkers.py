"""dd_seedmarkers — spawn one movable ADraftDeskThreshold marker at every connection of a layout,
so the author can drag them and then run dd_sync. Clears any existing markers first.

  python dd_seedmarkers.py            # seed markers for the example layout (dd_castle)
  python dd_seedmarkers.py my_layout

set_properties is case-insensitive (kind/Kind both apply); the spawn script uses lowercase by
convention. If a marker reads back 0 later it's the write-persistence flakiness, not casing —
run dd_genrepair.py (see Docs/RESISTANCE.md).
"""
import importlib
import json
import sys

import ddrun

layout_mod = sys.argv[1] if len(sys.argv) > 1 else "dd_castle"
pts = importlib.import_module(layout_mod).L.threshold_points()

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
                {"kind": p["kind"], "width": p["width"], "height": p["height"], "label": p["label"]})}))
        except Exception:
            pass
        n += 1
    return {"spawned": n, "cleared": len(old)}
'''

script = SPAWN.replace("__PTS__", json.dumps(pts))
print("seed points:", len(pts))
print(ddrun.run_text(script))
