# draftDesk

A metrics-driven greybox blockout tool for Unreal Engine 5.8.

Define your game's **player-movement metrics** (door size, step rise/run, ceiling, vault, jump, grapple, …) in the Details panel, and generate **test space to spec** — live. Change a value, the blockout responds. The point is to design by *configuration*, not place-test-rebuild-repeat.

> LLM/MCP-enabled: built and extended through Claude driving the in-editor MCP server, but the runtime tool is plain UE C++ — no AI required to use it.

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

## Layout
- `Source/DraftDesk/` — C++ module
  - `DraftDeskMetrics.h` — `FDraftDeskMetrics` (the schema)
  - `DraftDeskGenerator.h/.cpp` — `ADraftDeskGenerator` (build-on-edit actor)
- `Content/` — plugin content (grid material, presets)
- `Docs/` — construction rules + metric defaults

## Roadmap
- Expose the room/link graph as authored arrays (hand-built layouts, not just presets)
- Mezzanine / balcony preset with `RailEdgeMask` guard rails; ramp preset
- Door frames and stair flanking walls (cosmetic polish on openings)
- `ULDMetricsDataAsset` presets (save/load a game's numbers)
- Details-panel customization (grouped, with derived read-outs)
- Auto-place a `PlayerStart` per space at its entry threshold (R2)
