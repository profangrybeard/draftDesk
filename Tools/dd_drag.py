"""dd_drag.py — close the bidirectional authoring loop. The author drags ADraftDeskThreshold markers in
the editor; this folds each move into the layout, rebuilds, and re-reconciles (engine-side, one undo step):
  - a slide ALONG a wall -> the threshold's Position moves with it (relative to its last reconciled home);
  - a marker dragged PERPENDICULAR off its wall -> deferred to Stage-B reshape (3b), snapped back;
  - a DELETED interior marker -> its wall dissolves to a full-clear Passage (entry/rail/exterior refused, R1);
  - the ENTRY is never folded (it is the normalize origin).
Then every marker is snapped back onto its opening (strict bijection) and the layout is saved.

Replaces the slide/merge path of dd_sync. Run after dragging:
    python dd_drag.py
"""
import ddrun

_SCRIPT = '''import json
TOOL = "DraftDeskEditor.DdNavToolset.SyncDrags"
GEN = "{{GEN}}"
def run():
    r = execute_tool(TOOL, json.dumps({"GeneratorPath": GEN}))
    rv = r["returnValue"] if (isinstance(r, dict) and "returnValue" in r) else r
    return {"report": rv}
'''


def sync():
    """Fold dragged markers into the model + rebuild + reconcile. Returns the engine report."""
    res = ddrun.run_text(_SCRIPT)
    return res.get("report") if isinstance(res, dict) else res


if __name__ == "__main__":
    import dd_save
    print("syncdrags:", sync())
    print("saved:", dd_save.save())
