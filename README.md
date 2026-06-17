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
- Drop an **ADraftDeskGenerator** into a level.
- Assign a world-aligned **Grid Material** (so 1 grid square = 1 m at any scale).
- Edit the **Metrics** in the Details panel — the greybox rebuilds in place.

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
