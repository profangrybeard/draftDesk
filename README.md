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
2. **Blockout.** Drop an `ADraftDeskGenerator` into the level and point its `Spec` at that asset. It builds a room→hall→room greybox (world-aligned grid by default) and **rebuilds live** when you edit the spec.
3. **Game feel.** Make a `GameMode` that consumes the same spec — either subclass `ADraftDeskGameMode` or, in your own GameMode, hold a `Spec` and call `UDraftDeskStatics::ApplyLocomotion(Pawn, Spec)` on spawn. It pushes `RunSpeed → MaxWalkSpeed` etc. onto the pawn's `CharacterMovementComponent`.

> **Important — set a real pawn.** `ADraftDeskGameMode` applies the spec but does **not** pick your character (the plugin can't know it). In your GameMode, set **`DefaultPawnClass` to your playable character** (one with input + a camera). A bare `ACharacter` has a movement component but no input, so it spawns and just stands there.

## Layout
- `Source/DraftDesk/` — C++ module
  - `DraftDeskMetrics.h` — `FDraftDeskMetrics` (the schema)
  - `DraftDeskGenerator.h/.cpp` — `ADraftDeskGenerator` (build-on-edit actor)
- `Content/` — plugin content (grid material, presets)
- `Docs/` — construction rules + metric defaults

## Roadmap
- Full room→hall→room layouts and columns/door-frames from metrics
- `ULDMetricsDataAsset` presets (save/load a game's numbers)
- Details-panel customization (grouped, with derived read-outs)
- PlayerStart placed at the generator origin (R1)
