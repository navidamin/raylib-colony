# Site Selection — Design Documents

**Status: SETTLED** — simplified 2026-08-25; step 1 of 4 implemented.

How a player gets from "somewhere on the Moon" to "the base goes *here*".

## The whole design in two sentences

> **Resources belong to the region. Terrain belongs to the spot.**
>
> You pick a region for what it has. You pick a spot for whether you can
> build on it.

Two decisions at two scales — a 100 km region (one playfield) chosen for
what it holds, and a 1.5 km footprint chosen for whether it can carry a
base. Everything between the two is camera movement, not decision.

The mechanic teaches itself in one movement: **the terrain panel follows
the cursor and the region panel does not.** That is the whole of it —
there is no band to read, no ring to interpret, no instrument to learn.

## Documents

| Document | Purpose | Status |
|----------|---------|--------|
| [site-selection-master-design.md](site-selection-master-design.md) | The two decisions, cursor behaviour, where resource information lives, implementation plan | SETTLED |
| ↳ Appendix A | The five-level instrument-floor model this replaced, kept as reasoning, not as work | ARCHIVED |

## Progress

| Step | State |
|------|-------|
| 1 — Cursor infrastructure | **done** — `survey_cursor.{h,cpp}`, self-test, `lunar_map --ladder` |
| 2 — Region resource panel | not started |
| 3 — Site terrain panel | not started |
| 4 — Placement and commit | not started |

## Cross-references

**Source files**

| File | Relevance |
|------|-----------|
| `src/TerrainGen/lola_dem.{h,cpp}` | `EvaluateSite` / `TerrainBuildability` — the real terrain gate |
| `src/TerrainGen/survey_cursor.{h,cpp}` | Cursor geometry — screen ↔ km ↔ lat/lon, snapping, stack (step 1) |
| `src/TerrainGen/terrain_synthesis.h` | `TerrainGridCellToLatLon`, `OrbitalPickToLatLon`, `SetTerrainAnchor`, `TERRAIN_CELL_KM` |
| `src/ResourceManager/resource_manager.{h,cpp}` | `OrbitalSurveyData`, `GetSiteArchetype` — the region's holdings |
| `src/Engine/gamemanager.cpp` | Existing `View::SITE_SELECTION` flow, Ctrl+click placement |
| `src/Engine/rendermanager.cpp` | `DrawSiteSelectionView`, instrument panels |
| `tools/surveycursor/survey_cursor_test.cpp` | Headless self-test for the cursor geometry |
| `tools/lunarmap/lunarmap_main.cpp` | `--place` prototypes the site panel; `--layer` / `--ladder` prototyped the archived model |

**Related design docs**

- [`docs/design/prospecting/README.md`](../prospecting/README.md) — the only way to learn local resource truth, and therefore the payoff for the regional-resource rule
- [`docs/design/sect-view/README.md`](../sect-view/README.md) — the view the player lands in once built

**Roadmap**

- `ROADMAP_IMMINENT.md` — site selection is part of the colony founding loop

## Open questions

`[?]` **Region size.** 100 km = one playfield needs no new machinery and
is the default. If regions feel too coarse to choose between, several
per playfield is the fallback — but that weakens "one region, one
playfield", so try the simple version first.

`[?]` **Do the regions differ enough?** A requirement on
`ResourceManager`'s generation, not on the UI. If every region reads
alike, decision 1 is not a decision. No panel work substitutes for it.

`[?]` **Eight resource bars or three groups?** Decide by playtest (§4.6).

**Settled:**

- The region panel has **two frozen groups** — *resources* (the natural
  extractables `H2, O2, C, Fe, Si, Ti, Al, Ca`; everything else in
  `ResourceType` is produced, not found) and *terrain character* (mean
  slope + **rock abundance**, §4.5).
- The site panel is **live terrain only**.
- **No rings and no footprint indicators anywhere** — the frozen panel
  already says what they were for.
