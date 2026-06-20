# draftDesk — Setup (fresh machine, from zero)

What a new PC needs before the [authoring loop](WORKFLOW.md) works. The repo is a **plugin only**
(no host `.uproject`); you supply the Unreal project that hosts it.

## 1. Unreal Engine 5.8 + C++ toolchain

Install UE 5.8 (Epic Games Launcher → default path `C:/Program Files/Epic Games/UE_5.8`). The build
command below assumes that path; substitute yours.

draftDesk is a **C++ plugin**, so you also need a compiler: **Visual Studio 2022** with the
**"Game development with C++"** workload (MSVC + Windows 10/11 SDK). UE's installer does not bundle
it — a fresh PC can't compile the plugin without it.

## 2. A host UE 5.8 project

You need a C++-capable UE 5.8 project to host the plugin (the plugin can't run standalone). Either
use an existing project or create a blank C++ project. Then place this plugin under its `Plugins/`:

- **Simple:** clone into `YourProject/Plugins/DraftDesk`.
- **As its own repo (the author's setup):** keep draftDesk as a standalone git repo elsewhere and
  **junction** it into the project so the plugin stays independently versioned:
  ```
  mklink /J "C:\YourProject\Plugins\DraftDesk" "C:\_Projects\draftDesk"
  ```

Enable **draftDesk** in the project's Plugins, then build the editor target (step 4).

## 3. The in-editor MCP server (required for the harness)

The `Tools/` harness drives the editor over Unreal's **built-in experimental MCP server** — it is
*not* part of draftDesk and not shipped here; it's a UE feature you enable on the host project:

- In the editor's **Plugins** browser, enable **Settings → Show Experimental Plugins** (these are
  hidden by default), then enable the experimental **`ModelContextProtocol`** plugin and the
  **`AllToolsets`** plugin (they expose the ~50 `editor_toolset.*` / `EditorToolset.*` toolsets the
  harness calls). **Restart the editor** for the plugins to take effect.
- With those enabled, the editor hosts an HTTP-streamable MCP server at **`http://127.0.0.1:8000/mcp`**,
  **alive only while the editor is open** (it runs inside `UnrealEditor.exe`).
- The harness pins protocol version `2025-06-18` and uses `toolset_name=
  editor_toolset.toolsets.programmatic.ProgrammaticToolset`, `tool_name=execute_tool_script`
  (see `Tools/ddrun.py`). If a future UE changes the protocol/toolset names, update `ddrun.py`.

Confirm it's up once the editor is running: `cd Tools && python ddrun.py sandbox/find_generator.py`.

## 4. Build the editor target (editor CLOSED)

Run this as a **single line** (cmd doesn't treat `\` as a line continuation):
```
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" <YourProject>Editor Win64 Development -Project="C:/path/to/YourProject.uproject" -WaitMutex
```

The plugin DLL is locked while the editor is open — **close it first**. After a crash, `taskkill`
the lingering `CrashReportClientEditor.exe`/`UnrealEditor.exe` holding the DLL, then rebuild. (You
can also build from your IDE, or right-click the `.uproject` → *Generate Visual Studio project files*
first if needed.)

## 5. Create the scene objects

In the host level:

1. **Spec asset** — Content Browser → right-click → *Miscellaneous → Data Asset* → pick
   `DraftDeskSpec`. Set your metrics (door size, step rise/run, ceiling, …) — the single source of
   truth.
2. **Generator** — drop an `ADraftDeskGenerator` into the level; set its `Spec` to the asset above.
   Pick a `Preset` (or leave it for the harness to set `Custom`). It builds live.
3. **NavMeshBoundsVolume** — add one (UE auto-spawns a `RecastNavMesh` when the first bounds volume
   is placed; if nav never builds on a bare template, check **Project Settings → Navigation System**
   is enabled). The harness re-fits this volume to the geometry on every apply.
4. **PlayerStart** — place one at the generator's location; every layout's entry threshold
   normalizes to the origin, so one PlayerStart spawns you in the doorway (R1).

## 6. Configure + run the harness

```
cd Tools                      # all scripts import siblings; run from THIS directory
```
Edit `dd_config.py`: set `GEN` (your generator's object path — get it from
`python ddrun.py sandbox/find_generator.py`) and `SPEC` (your spec asset — right-click → *Copy
Reference*). The committed `GEN`/`SPEC` are the author's example paths and **must be replaced**.

Requirements: **Python 3.8+** (system, not the UE-sandbox interpreter), **no pip packages** (the
harness is stdlib-only), and **real `curl`** on PATH (Windows 11 ships `curl.exe`; git-bash has it).
⚠️ In PowerShell, `curl` is an alias for `Invoke-WebRequest` — run the harness from `cmd`, git-bash,
or ensure `curl.exe` resolves, or `ddrun` will fail.

Then follow [WORKFLOW.md](WORKFLOW.md).
