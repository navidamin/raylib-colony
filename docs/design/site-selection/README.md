# Site Selection — Design Documents

**Status: SETTLED** — simplified 2026-08-25; step 1 of 4 implemented.

How a player gets from "somewhere on the Moon" to "the base goes *here*":
**five levels, five different questions, two commitments.** Chemistry
decides once at level 1 and never refines; every level below decides
*position* — geometry that sharpens honestly because the player moved,
not because an instrument improved. See §2 and the coherency contract in
§5.0 (which game systems must exist for each decision to be real).

## The whole design in two sentences

> **Resources belong to the region. Terrain belongs to the spot.**
>
> You pick a region for what it has. You pick a spot for whether you can
> build on it.

Two decisions at two scales — a 200 km district (widened from the 100 km
playfield on 2026-09-02, so the descent zooms 15x / 8x / 5x) chosen for
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
| 2 — Region identity + panel | not started |
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

`[?]` **Region size.** 100 km = one playfield needed no new machinery and
was the default until 2026-09-02; the district is 200 km now (see the
master design's "as built" note). If regions feel too coarse to choose between, several
per playfield is the fallback — but that weakens "one region, one
playfield", so try the simple version first.

`[?]` **Do the regions differ enough?** A requirement on
`ResourceManager`'s generation, not on the UI. If every region reads
alike, decision 1 is not a decision. No panel work substitutes for it.

`[?]` **Eight resource bars or three groups?** Decide by playtest (§4.8).
Note Fe/Al/Ca are one number, not three, and Si should never be a gauge
(§4.6).

`[?]` **Do regions trade off, or does one dominate?** On published
figures the PKT leads on iron, thorium and mare coverage at once (§4.6).

`[?]` **Composition currently has no gameplay effect at all** —
`Colony::GetArchetypeBonus` is never called. Invert the generator rather
than adding multipliers (§4.6).

**Settled:**

- Regions are **named and real** (§4.5): terrane on the orbital disc
  (Jolliff 2000, five entries), named feature at 100 km straight from
  `src/assets/planet/zones.json` — 73 real lunar features that already
  ship and that no C++ file reads. Colour fill on the disc only;
  boundaries over unmodified imagery below it.
- The numbers **never refine as you descend** (§4.6). The information
  actually runs the other way — Fe/Ti/Al/Ca come from 20–200 m
  multispectral imaging, while one gamma-ray pixel is 45–200 km, wider
  than the whole playfield.
- The region panel has **two frozen groups** — *resources* (the natural
  extractables `H2, O2, C, Fe, Si, Ti, Al, Ca`; everything else in
  `ResourceType` is produced, not found) and *terrain character* (mean
  slope + **rock abundance**, §4.5).
- The site panel is **live terrain only**.
- **No rings and no footprint indicators anywhere** — the frozen panel
  already says what they were for.
- **Hover hints** (§4.9): every panel row explains itself on hover —
  what high/low titanium means, what the rock gives, what PSR proximity
  buys. Data-driven (`src/survey_hints.h`), thresholds defined once,
  never permanently visible.
