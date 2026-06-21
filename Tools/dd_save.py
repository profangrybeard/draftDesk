"""dd_save — persist the authored state so it survives an editor restart.

Saves the LEVEL asset, which writes the generator's synced Custom payload (the geometry) AND every
ADraftDeskThreshold marker (the dragged transforms = the durable threshold-anchor input) into the
.umap. Without this, a sync only updates the LIVE editor in-memory and is lost on close/rebuild.
(save_actor only works for World-Partition external actors; this is a classic level, so we save the
level asset via AssetTools.save_assets.) Called at the end of dd_seedmarkers + dd_sync.
"""
import ddrun
import dd_config as C

# GEN = "/Game/<path>/<Level>.<Level>:PersistentLevel.<Actor>"  ->  level asset = the part before the first '.'
LEVEL = C.GEN.split(".")[0]

SAVE_SCRIPT = '''import json
SAVE = "editor_toolset.toolsets.asset.AssetTools.save_assets"
def run():
    r = execute_tool(SAVE, json.dumps({"asset_paths": ["%s"]}))
    ok = r["returnValue"] if isinstance(r, dict) and "returnValue" in r else r
    return {"level": "%s", "saved": ok}
''' % (LEVEL, LEVEL)


def save():
    return ddrun.run_text(SAVE_SCRIPT, substitute=False)


if __name__ == "__main__":
    print(save())
