"""dd_reconcile.py — make the threshold MARKERS exactly match the engine's OPENINGS (strict bijection),
by calling the engine-side reconciler DraftDeskEditor.DdNavToolset.ReconcileMarkers. The engine owns the
markers now: it spawns one for every opening that lacks a marker, moves any marker that drifted off its
opening onto it (this erases the ~25cm pre-snap seed drift), and deletes orphans — leaving exactly one
marker per opening, at the opening. The entry + rail markers are never deleted on a transient unresolve.

This is the steady-state placer; dd_seedmarkers is now bootstrap-only. Run after an apply:
    python dd_castle.py        # apply the layout (generator refills Openings)
    python dd_reconcile.py     # markers <- openings, then save
"""
import ddrun

_SCRIPT = '''import json
TOOL = "DraftDeskEditor.DdNavToolset.ReconcileMarkers"
GEN = "{{GEN}}"
def run():
    r = execute_tool(TOOL, json.dumps({"GeneratorPath": GEN}))
    rv = r["returnValue"] if (isinstance(r, dict) and "returnValue" in r) else r
    return {"report": rv}
'''


def reconcile():
    """Call the engine reconciler; returns its {spawned,moved,deleted,kept,total,duplicates} report."""
    res = ddrun.run_text(_SCRIPT)
    return res.get("report") if isinstance(res, dict) else res


if __name__ == "__main__":
    import dd_save
    print("reconcile:", reconcile())
    print("saved:", dd_save.save())
