"""Read every ADraftDeskThreshold marker actor: its Label/Kind/Width/Height + world location.
Run via ddrun (substitutes {{THRESH}}). Returns {"markers": [...]}.
Top-level UPROPERTY names come back PascalCase (Label/Kind/Width/Height)."""
import json
FIND = "editor_toolset.toolsets.scene.SceneTools.find_actors"
XF = "editor_toolset.toolsets.actor.ActorTools.get_actor_transform"
GET = "editor_toolset.toolsets.object.ObjectTools.get_properties"
CLS = "{{THRESH}}"
def run():
    m = execute_tool(FIND, json.dumps({"name": "", "tag": "", "collision_channels": [], "actor_type": {"refPath": CLS}}))["returnValue"]
    out = []
    for a in m:
        ref = a["refPath"]
        loc = execute_tool(XF, json.dumps({"actor": {"refPath": ref}}))["returnValue"]["location"]
        p = json.loads(execute_tool(GET, json.dumps({"instance": {"refPath": ref}, "properties": ["Label", "Kind", "Width", "Height"]}))["returnValue"])
        out.append({"label": p["Label"] if "Label" in p else "?", "kind": p["Kind"] if "Kind" in p else "Doorway",
                    "width": p["Width"] if "Width" in p else 0.0, "height": p["Height"] if "Height" in p else 0.0,
                    "x": loc["x"], "y": loc["y"], "z": loc["z"], "ref": ref})
    return {"markers": out}
