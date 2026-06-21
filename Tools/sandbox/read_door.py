"""Read the live door/corridor metric defaults from the generator's Spec asset.
Run via: python ddrun.py sandbox/read_door.py   (ddrun substitutes {{GEN}})
Nested struct fields come back first-char-lowercase (doorWidth, not DoorWidth)."""
import json
GET = "editor_toolset.toolsets.object.ObjectTools.get_properties"
GEN = "{{GEN}}"
def run():
    g = json.loads(execute_tool(GET, json.dumps({"instance": {"refPath": GEN}, "properties": ["Spec"]}))["returnValue"])
    spec = g["Spec"]
    specref = spec["refPath"] if isinstance(spec, dict) and "refPath" in spec else spec
    m = json.loads(execute_tool(GET, json.dumps({"instance": {"refPath": specref}, "properties": ["Metrics"]}))["returnValue"])
    metrics = m["Metrics"]
    if isinstance(metrics, str):
        metrics = json.loads(metrics)
    dw = metrics["doorWidth"] if "doorWidth" in metrics else None
    dh = metrics["doorHeight"] if "doorHeight" in metrics else None
    cw = metrics["corridorWidth"] if "corridorWidth" in metrics else None
    gs = metrics["gridSnap"] if "gridSnap" in metrics else None   # FVector {x,y,z}: the live blocking grid
    return {"spec": specref, "doorWidth": dw, "doorHeight": dh, "corridorWidth": cw, "gridSnap": gs}
