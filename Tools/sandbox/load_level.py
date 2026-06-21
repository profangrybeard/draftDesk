"""Load the draftDesk authoring level (the relaunched editor opened the FirstPerson template map
instead), then report the current level + any generators. Run: python ddrun.py sandbox/load_level.py"""
import json
LOAD = "editor_toolset.toolsets.scene.SceneTools.load_level"
LEVEL = "editor_toolset.toolsets.scene.SceneTools.get_current_level"
FIND = "editor_toolset.toolsets.scene.SceneTools.find_actors"
CLS = "/Script/DraftDesk.DraftDeskGenerator"
TARGET = "/Game/_project/levels/draftDesk_v0_1"
def run():
    execute_tool(LOAD, json.dumps({"level_path": TARGET}))
    lvl = execute_tool(LEVEL, json.dumps({}))["returnValue"]
    gens = execute_tool(FIND, json.dumps({"name": "", "tag": "", "collision_channels": [],
        "actor_type": {"refPath": CLS}}))["returnValue"]
    return {"level": lvl, "generators": [g["refPath"] for g in gens]}
