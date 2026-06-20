# Level Design Construction Rules

Boilerplate launched with every MCP blockout. **Tested here first** — keep what speeds the workflow, cut what's slow, expensive, or creatively harmful. Each rule notes its status.

## Origin & entry
- **R1 — PlayerStart at the space's origin.** The origin is the *door/threshold the player first experiences* in a space. The PlayerStart sits at that threshold, facing into the space. _(status: testing)_
- **R2 — Every space needs an origin point.** Even a short corridor/threshold before the first room — "entering is half the work." No spawns floating in the middle of a room. _(status: testing)_

## Materials & scale
- **R3 — Blocking meshes use the world-aligned grid material.** The plugin ships
  `/DraftDesk/Materials/M_DraftDeskGrid` (`Content/Materials/M_DraftDeskGrid.uasset` +
  `Content/Textures/T_DraftDeskGrid.uasset`); the generator applies it as `GridMaterial` (loaded in
  the actor ctor). The grid **must** use world-aligned (triplanar) projection, **never** mesh UVs — a
  mesh-UV grid stretches with mesh scale and lies about scale. Set `TextureSize` so the cell reads
  1 m on every surface regardless of scale (verified: 6 cells/6 m room, 4/4 m wall, 2/2 m corridor).
  _(status: **kept** — proven. The old host-project path `/Game/_project/materials/T_Grid_2K_Mat` is
  historical and not in this repo.)_

## Metrics
- **R4 — Blocking conforms to LD metrics.** Doors, steps, ceilings, vault/window, jump, grapple, etc. are built from `LD_Metrics.json` (mirrored to the in-engine `LD_Metrics` AgentSkill) — not eyeballed. _(status: testing)_

---
### Candidate rules (not yet earned a slot)
_Add here as we validate them; promote to a numbered rule once proven to speed the workflow._
