"""List every ADraftDeskGenerator in the open level + the current level path, so you can fill in
GEN in dd_config.py on a fresh project. Run via: python ddrun.py sandbox/find_generator.py"""
import json
FIND = "editor_toolset.toolsets.scene.SceneTools.find_actors"
LEVEL = "editor_toolset.toolsets.scene.SceneTools.get_current_level"
CLS = "/Script/DraftDesk.DraftDeskGenerator"
def run():
    lvl = execute_tool(LEVEL, json.dumps({}))["returnValue"]
    gens = execute_tool(FIND, json.dumps({"name": "", "tag": "", "collision_channels": [],
        "actor_type": {"refPath": CLS}}))["returnValue"]
    return {"level": lvl, "generators": [g["refPath"] for g in gens]}
