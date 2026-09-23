# Site Selection — Design Documents

**Status: SETTLED, and built as three levels in `lunar_map`.** Not yet
the game's founding flow — see "Where it runs" below.

## The one level ladder

> **Globe → District (200 km) → Site (25 km).**
>
> | Level | Window | Cursor | Question |
> |-------|--------|--------|----------|
> | 1 ORBITAL | the globe | 200 km, snapped | Which economy? |
> | 2 DISTRICT | 200 km | 25 km, snapped | Which mix? |
> | 3 SITE | 25 km | 1.5 km base footprint, free | Which ground? |
>
> This is the only level ladder in the repository.
> `SURVEY_LEVEL_COUNT = 3` in `src/TerrainGen/survey_cursor.h` is the
> authority; `survey_cursor_test` fails if it changes.

**Not levels**, though each has been mistaken for one:

- the game's **Planet (100 km) / Colony (25 km) / Sect (5 km) views** —
  where a colony is managed once it exists;
- the terrain chain's **internal steps** in `terrain_synthesis.cpp`
  (100 → 25 → 5 km crops it walks to build one picture);
- the **crater bench**'s free zoom at `/regolith/` (a design prototype).

Four other ladders lived in this branch until 2026-09-23 and were
removed because sessions kept following them — `docs/graveyard.md`
entry 10.

## The whole design in two sentences

> **Resources belong to the region. Terrain belongs to the spot.**
>
> You pick a region for what it has. You pick a spot for whether you can
> build on it.

Chemistry decides once, at level 1, and never refines; every level
below decides *position* — geometry that sharpens honestly because the
player moved, not because an instrument improved. One commitment:
clicking the footprint at level 3 founds the base. Everything above it
is freely revisable.

The mechanic teaches itself in one movement: **the terrain panel follows
the cursor and the region panel does not.** That is the whole of it —
there is no band to read, no ring to interpret, no instrument to learn.

## Where it runs

| | |
|---|---|
| `lunar_map` (bare, or `--site`) | the ladder, interactive, on the real Moon |
| `/lunarmap/` on Pages | the same, in a browser — the playtest |
| `lunar_map --siteshot` / `--flyshot` | the ladder walked headlessly, one PNG per step |
| the game (`src/Engine/`) | **not yet.** The game in this branch still sites colonies through its old grid picker (`View::SITE_SELECTION`). Putting the ladder into the game is the `lunarmap-wiring-site-selection` branch's work. |

## Documents

| Document | Purpose | Status |
|----------|---------|--------|
| [site-selection-master-design.md](site-selection-master-design.md) | The three levels, cursor behaviour, where resource information lives, the coherency contract | SETTLED |
| ↳ Appendix A | The five-level instrument-floor model this replaced, kept as reasoning, not as work | ARCHIVED |
| [site-ground-texture.md](site-ground-texture.md) | Why the site level looked like grey noise over 99.8 % of the Moon, and laying the terrain synthesizer over it — platform tiers by measured cost, web memory | BUILT — `--chain`, on by default in the web build |

## Progress

| Step | State |
|------|-------|
| 1 — Cursor infrastructure | **done** — `survey_cursor.{h,cpp}`, self-test |
| 2 — Region identity + panel | **done in `lunar_map`** — named regions from `zones.json`, terranes, the frozen region card, hover hints |
| 3 — Site terrain panel | **done in `lunar_map`** — live `EvaluateSite` under the footprint, the verdict |
| 4 — Placement and commit | **done in `lunar_map`**; not in the game (see "Where it runs") |

## Cross-references

**Source files**

| File | Relevance |
|------|-----------|
| `src/TerrainGen/lola_dem.{h,cpp}` | `EvaluateSite` / `TerrainBuildability` — the real terrain gate |
| `src/TerrainGen/survey_cursor.{h,cpp}` | **The ladder itself** — the level table, cursor geometry, screen ↔ km ↔ lat/lon, snapping, stack |
| `src/TerrainGen/terrain_synthesis.h` | `TerrainGridCellToLatLon`, `OrbitalPickToLatLon`, `SetTerrainAnchor`, `TERRAIN_CELL_KM` |
| `src/ResourceManager/resource_manager.{h,cpp}` | `OrbitalSurveyData`, `GetSiteArchetype` — the region's holdings |
| `src/Engine/gamemanager.cpp` | The game's OLD grid picker, `View::SITE_SELECTION` — to be replaced by the ladder |
| `src/Engine/rendermanager.cpp` | `DrawSiteSelectionView` — the old picker's panels |
| `tools/surveycursor/survey_cursor_test.cpp` | Headless self-test for the cursor geometry |
| `tools/lunarmap/lunarmap_main.cpp` | The ladder, interactive and headless (`--site`, `--siteshot`, `--flyshot`); `--layer` prototyped the archived model |

**Related design docs**

- [`docs/design/prospecting/README.md`](../prospecting/README.md) — the only way to learn local resource truth, and therefore the payoff for the regional-resource rule
- [`docs/design/sect-view/README.md`](../sect-view/README.md) — the view the player lands in once built

**Roadmap**

- `ROADMAP_IMMINENT.md` — site selection is part of the colony founding loop

## Removed on purpose

Earlier versions of this ladder — the flat near-side level 1, the
scroll-that-changes-level, the site level's zoom-driven cursor
refinement, the 500 km and LOCALITY rungs — and the other ladders that
lived beside it are described in [`../../graveyard.md`](../../graveyard.md)
(entries 1, 5, 6 and 10) with the constants they used. Read it before
reinstating any of them.

## Open questions

`[?]` **Region size.** A region is what the globe names under the
cursor, and one 200 km district sits inside one. If regions feel too
coarse to choose between, several per district is the fallback — but
that weakens "one region per colony", so try the simple version first.

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

- Regions are **named and real** (§4.5): terrane on the globe (Jolliff
  2000, five entries), named feature under the cursor straight from
  `src/assets/planet/zones.json` — 73 real lunar features. Colour fill on the disc only;
  boundaries over unmodified imagery below it.
- The numbers **never refine as you descend** (§4.6). The information
  actually runs the other way — Fe/Ti/Al/Ca come from 20–200 m
  multispectral imaging, while one gamma-ray pixel is 45–200 km — up to
  a whole district.
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
