"""dd_genrepair — backfill any threshold marker whose Width/Height is 0 with the layout's value,
preserving each marker's author-moved position. Use after seeding if some markers read back 0
(e.g. seeded by an older tool that wrote PascalCase keys, which set_properties silently ignores).

  python dd_genrepair.py [layout]
"""
import importlib
import json
import sys

import ddrun

layout_mod = sys.argv[1] if len(sys.argv) > 1 else "dd_castle"
dims = {p["label"]: {"width": p["width"], "height": p["height"], "kind": p["kind"]}
        for p in importlib.import_module(layout_mod).L.threshold_points()}

REPAIR = '''import json
FIND = "editor_toolset.toolsets.scene.SceneTools.find_actors"
GET = "editor_toolset.toolsets.object.ObjectTools.get_properties"
SET = "editor_toolset.toolsets.object.ObjectTools.set_properties"
CLS = "{{THRESH}}"
DIMS = json.loads(r"""__DIMS__""")
def run():
    m = execute_tool(FIND, json.dumps({"name": "", "tag": "", "collision_channels": [], "actor_type": {"refPath": CLS}}))["returnValue"]
    fixed = []
    for a in m:
        ref = a["refPath"]
        p = json.loads(execute_tool(GET, json.dumps({"instance": {"refPath": ref}, "properties": ["Label", "Width", "Height"]}))["returnValue"])
        lab = p["Label"] if "Label" in p else "?"
        if lab not in DIMS:
            continue
        d = DIMS[lab]
        vals = {}
        if not (p["Width"] if "Width" in p else 0) > 0:
            vals["width"] = d["width"]
        if not (p["Height"] if "Height" in p else 0) > 0:
            vals["height"] = d["height"]
        if vals:
            execute_tool(SET, json.dumps({"instance": {"refPath": ref}, "values": json.dumps(vals)}))
            fixed.append(lab)
    return {"repaired": fixed, "count": len(fixed)}
'''

print(ddrun.run_text(REPAIR.replace("__DIMS__", json.dumps(dims))))
