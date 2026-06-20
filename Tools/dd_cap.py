"""dd_cap — top-down screenshot of the blockout, saved to a PNG (base64 never hits your terminal).

  python dd_cap.py out.png                 # frame the whole generator
  python dd_cap.py out.png 6085 -715 700   # frame a region: center x y, radius (cm)

Useful for eyeballing a sync/stress result without opening the editor viewport.
"""
import base64
import json
import sys

import ddrun

out = sys.argv[1] if len(sys.argv) > 1 else "shot.png"
region = sys.argv[2:5] if len(sys.argv) >= 5 else None   # [cx, cy, radius]

CAP = '''import json
BOUNDS = "editor_toolset.toolsets.actor.ActorTools.get_actor_bounds"
CAP = "EditorToolset.EditorAppToolset.CaptureViewport"
GEN = "{{GEN}}"
REGION = __REGION__
def run():
    if REGION:
        cx, cy, rad = float(REGION[0]), float(REGION[1]), float(REGION[2])
        cz = 320.0; camz = cz + rad * 1.6 + 400.0
    else:
        b = execute_tool(BOUNDS, json.dumps({"actor": {"refPath": GEN}}))["returnValue"]
        mn = b["min"]; mx = b["max"]
        cx = (mn["x"]+mx["x"])/2.0; cy = (mn["y"]+mx["y"])/2.0; cz = (mn["z"]+mx["z"])/2.0
        camz = cz + max(mx["x"]-mn["x"], mx["y"]-mn["y"]) * 1.25 + 400.0
    xform = {"location": {"x": cx, "y": cy, "z": camz},
             "rotation": {"pitch": -89.0, "yaw": 0.0, "roll": 0.0},
             "scale": {"x": 1.0, "y": 1.0, "z": 1.0}}
    ann = {"gridSpacing": 0.0, "gridExtent": 0.0, "gridHeight": 0.0,
           "maxLabelDistance": 0.0, "classFilter": None, "maxLabels": 0}
    cap = execute_tool(CAP, json.dumps({"captureTransform": xform, "annotations": ann, "bShowUI": False}))["returnValue"]
    img = cap["image"]
    return {"mimeType": img["mimeType"], "data": img["data"]}
'''

script = CAP.replace("__REGION__", repr(region))   # Python literal (None / [..]), NOT JSON null
res = ddrun.run_text(script)
data = res.pop("data", None)
if data:
    with open(out, "wb") as f:
        f.write(base64.b64decode(data))
    print("saved:", out, res)
else:
    print("no image:", res)
