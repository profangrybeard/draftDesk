"""draftDesk harness configuration — THE ONE FILE YOU EDIT PER PROJECT/PC.

Every script here reads these values (system-python scripts import this module; UE-sandbox
scripts get the values substituted in by ddrun.py via {{GEN}} / {{SPEC}} / {{THRESH}} placeholders),
so the whole authoring loop is portable: clone the plugin, set the two paths below, and run.

How to find the two project-specific paths
------------------------------------------
GEN  - the object path of YOUR ADraftDeskGenerator actor in YOUR level. Format:
         /Game/<path>/<Level>.<Level>:PersistentLevel.<ActorName>
       Find it: select the generator in the editor, or run
         python ddrun.py sandbox/find_generator.py
SPEC - the object path of YOUR UDraftDeskSpec data asset. Format:
         /Game/<path>/<Asset>.<Asset>
       Find it: right-click the asset > Copy Reference, then strip any class prefix.

THRESH and MCP_URL are stable across projects and rarely need changing.
"""

# --- per-project (EDIT THESE) ------------------------------------------------
GEN  = "/Game/_project/levels/draftDesk_v0_1.draftDesk_v0_1:PersistentLevel.DraftDeskGenerator_2"
SPEC = "/Game/_project/DA_DraftDeskSpec.DA_DraftDeskSpec"

# --- stable (rarely change) --------------------------------------------------
THRESH  = "/Script/DraftDesk.DraftDeskThreshold"   # the movable marker actor class
MCP_URL = "http://127.0.0.1:8000/mcp"              # in-editor MCP server (live only while the editor is open)
