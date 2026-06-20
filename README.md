# draftDesk

A metrics-driven greybox blockout tool for Unreal Engine 5.8.

Define your game's **player-movement metrics** (door size, step rise/run, ceiling, vault, jump, grapple, …) in the Details panel, and generate **test space to spec** — live. Change a value, the blockout responds. The point is to design by *configuration*, not place-test-rebuild-repeat.

> LLM/MCP-enabled: built and extended through Claude driving the in-editor MCP server, but the runtime tool is plain UE C++ — no AI required to use it.

## Documentation
Everything needed to rebuild and operate the kit on a fresh machine lives in this repo:
- **[Docs/SETUP.md](Docs/SETUP.md)** — fresh-machine setup from zero (UE, host project, MCP server, assets).
- **[Docs/GOALS.md](Docs/GOALS.md)** — the vision, threshold-first philosophy, and roadmap.
- **[Docs/RULES.md](Docs/RULES.md)** — the invariants (LD rules R1–R4, engine/data-model, MCP/authoring).
- **[Docs/RESISTANCE.md](Docs/RESISTANCE.md)** — the gotchas/failure-modes as symptom → cause → fix.
- **[Docs/WORKFLOW.md](Docs/WORKFLOW.md)** — operate it end to end (build cycle + authoring loop).
- **[Tools/](Tools/README.md)** — the Python authoring harness (drives the generator over MCP).
- `Docs/LD_RULES.md`, `Docs/LD_Metrics.json` — the construction rules + metric schema.

## Install
1. Clone into your project's `Plugins/` folder (or the engine's `Plugins/` to share across all projects):
   ```
   git clone <repo-url> YourProject/Plugins/DraftDesk
   ```
2. Enable **draftDesk** in the project's Plugins (or it's enabled by the `.uproject`).
3. Rebuild the editor target when prompted.

## Use
1. **Author the spec.** Create a `UDraftDeskSpec` Data Asset and set your metrics — this is the single source of truth.
2. **Blockout.** Drop an `ADraftDeskGenerator` into the level and point its `Spec` at that asset. Pick a **Preset** from the `draftDesk|Layout` dropdown — the greybox (world-aligned grid by default) builds instantly and **rebuilds live** when you edit the spec or the layout:
   - **2D layouts:** Single Room, Corridor, Room-Hall-Room, L-Bend, T-Junction, Cross (4-way), 2×2 Room Grid.
   - **Verticality:** Split Level and Tower — metric-correct stair flights (built from `StepRise`/`StepRun` within `MaxStepTraversalAngle`) bridge rooms at different floor heights. `CellSize`, `HallLength`, and `FloorDelta` resize everything live.

   The actor origin is the entry threshold (R1), so a `PlayerStart` at the generator's location spawns you in the doorway of any preset.
3. **Game feel.** Make a `GameMode` that consumes the same spec — either subclass `ADraftDeskGameMode` or, in your own GameMode, hold a `Spec` and call `UDraftDeskStatics::ApplyLocomotion(Pawn, Spec)` on spawn. It pushes `RunSpeed → MaxWalkSpeed` etc. onto the pawn's `CharacterMovementComponent`.

> **Important — match your project's playable setup.** `ADraftDeskGameMode` only applies the spec; it does **not** know your character, controller, or HUD. A playable pawn needs its matching `PlayerControllerClass` too — that's usually where input (Enhanced Input mapping contexts) is wired. A character with the wrong controller spawns and just stands there.
>
> Two clean options:
> - **Subclass `ADraftDeskGameMode`** and set `DefaultPawnClass`, `PlayerControllerClass`, and `HUDClass` to match your project's default GameMode (copy them from it).
> - **Or keep your existing GameMode** (which already has all that wired) and just hold a `Spec` + call `UDraftDeskStatics::ApplyLocomotion(NewPlayer->GetPawn(), Spec)` on `RestartPlayer`. Often the lower-friction path.

## Author by threshold (the graph + markers)
Beyond presets, draftDesk is a **rooms-and-links graph** you author directly, the
[threshold-first way](Docs/GOALS.md): plan the connections, fill the spaces.
- **`Custom` preset** builds from four authored arrays (rooms / links / stairs / boxes). The
  [`Tools/`](Tools/README.md) harness composes these from a dictated layout.
- **Movable threshold markers** (`ADraftDeskThreshold`) are the authoring surface: seed one per
  connection, **drag** them to where the player should pass, then **sync** — the geometry rebuilds
  around them (slide / resize / merge-on-delete). A declared connection can never resolve to a solid
  wall, and an opening can never slide off its wall (engine-clamped, any source).

## Layout
- `Source/DraftDesk/` — C++ module
  - `DraftDeskMetrics.h` — `FDraftDeskMetrics` (the schema)
  - `DraftDeskGenerator.h/.cpp` — `ADraftDeskGenerator` (build-on-edit actor; the whole pipeline)
  - `DraftDeskThreshold.h/.cpp` — `ADraftDeskThreshold` (movable marker actor)
  - `DraftDeskStatics.*` — `ApplyLocomotion` (push metrics onto the pawn)
- `Content/` — plugin content (grid material, presets)
- `Tools/` — Python authoring harness (compose → push → seed → drag → sync → screenshot)
- `Docs/` — goals, rules, resistance, workflow, construction rules + metric defaults

## Roadmap
**Done:** authored rooms/links graph (`Custom`); ramp + mezzanine presets; connection guarantee and
durable opening clamp (engine); movable threshold markers (engine); marker→geometry sync Stage A and
nav auto-sync (harness, in `Tools/`).
**Next:**
- **Stage B reshape** — a marker dragged perpendicular off its wall moves/grows the wall + room to follow it.
- Robustness backlog: stairs markers → cross-flight offset; window Z-drag → sill; 3D-aware abutment;
  column-in-doorway check; min-pier between two doors on one wall; idempotent delete→re-add.
- Door frames / stair flanking walls (opening polish); save/load metric presets; details-panel grouping.
- Auto-place a `PlayerStart` per space at its entry threshold (R2).
- Consume the **GAME356 kit** — emit real `AInteractableDoor` actors at thresholds.
