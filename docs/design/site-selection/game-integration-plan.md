# Site Selection — Game Integration Plan

**Status: IMPLEMENTED** — written 2026-09-18 against `161ad96` on
`claude/lunarmap-wiring-site-selection-b9lwaw`, revised the same day
after review, **built 2026-09-21** (B0, A1, A2, B1, and the parts of
B2–B4 listed in §8 "As built" at the end). Line numbers below are for
`161ad96` and describe the code as it was before the work.

**Scope, in two parts, in this order:**

- **Part A — retire the 20x20 playfield.** The globe *is* the planet.
  Every colony, sect and unit lives at a real latitude and longitude, and
  the game can hold colonies anywhere on the Moon at once. The fixed
  100 km grid, its single anchor, and the world-unit coordinate system
  built on it are removed.
- **Part B — the descent is how a colony is founded.** The three-rung
  survey ladder that lives in `lunar_map --site` becomes the game's
  founding flow, Globe → District → Site, freely walkable up and down at
  any time, and the grid picker behind `View::SITE_SELECTION` is retired.

Part A comes first because the first draft of this plan tried to fit the
descent onto the grid, and every awkward decision it produced — deriving
an anchor from the founded site, forbidding a second descent, refusing
polar sites because a lat/lon grid smears — was a workaround for the grid
rather than a property of the design. With the grid gone those decisions
do not exist.

**Read first:** [`README.md`](README.md) (the design in two sentences),
[`site-selection-master-design.md`](site-selection-master-design.md)
§2–§5, and
[`../../guides/feature-completeness.md`](../../guides/feature-completeness.md).

Line numbers are for `161ad96`.

---

# Part A — Retire the playfield: the Moon is the world

## A.0 What the grid is today

Twenty by twenty cells of 5 km, 100 units each, one unit = 50 m, so the
world is a 2000 x 2000 square of "world units" pinned to one real
lat/lon by `SetTerrainAnchor` (`terrain_synthesis.cpp:1723`, clamped to
±78°). Everything positional is expressed in it:

| Where | What depends on the grid | Refs |
|---|---|---|
| `game_constants.h:10–13` | `PLANET_SIZE`, `PLANET_WIDTH`, `PLANET_HEIGHT`; `SECT_CORE_RADIUS` doubles as the cell size | 4 |
| `ResourceManager` | three 20x20 arrays — `resourceGrid`, `surveyGrid`, `layeredGrid` — filled once at startup by random clusters that are not geographic; `GetResourcesAtGrid`, `GetResourcesAtGridLayer`, `UpdateResourceDepletion`, `GetOrbitalSurveyAt`, `GetSiteArchetype`, `WorldToGrid` / `GridToWorld` all take cell indices | 31 |
| `Planet` | owns the `ResourceManager`, a `map` nobody reads, `GetRandomValidPosition`, `GetWorldPosition`, the "active area" the Planet camera clamps to | 26 |
| `Sect` | `SectPosition` in world units | 1 (+ every caller) |
| `Unit` | `parentSectPosition` in world units; `WorldToGrid` of it picks the cell whose resources extraction reads and depletes (`unit.cpp:1470–1593`); excavator `gridPos` | 19 |
| `ProspectingGrid` | keyed by `(parentGridX, parentGridY)`; reads the parent cell's layered resources; hashes its sub-grid from the cell indices (`prospecting_grid.cpp:118, 169, 231`) | 1 (+ constructor) |
| `Colony` | centroid and jurisdiction radius in world units; `SECT_CORE_RADIUS * 4` | 3 |
| `Road` | length in world units, travel time from it | — |
| `RenderManager` | `DrawPlanetView` (100 km, level 0 of the chain over the grid), `DrawColonyView` (cell-registered level 1), `DrawSiteSelectionView`, `DrawCellInfo`, `DrawPlanetMapLayer` + `PlanetMapWorldRect` (the whole-moon map placed around the playfield), the terrain cache keyed by `(gx, gy, anchorVersion)`, `EnsureTerrainForCell`, `RequestNeighbourTerrain`, `DrawSectTerrainBackground`, the moon-tile fallback | 67 |
| `ViewManager` | Planet camera zoom limits and pan clamp, Colony camera clamp to `PLANET_WIDTH`, `ResetCameraForCurrentView` | 19 |
| `GameManager`, `Engine` | the grid picker, F6 debug cell lookup, initial camera on the playfield centre | 15 |
| `terrain_synthesis` | `TerrainGridCellToLatLon` (`:1871`), the anchor API and its version token | 3 |
| Tools | `colony_preview --cell`, `colony_viewtest` cell hopping, `sectwalk` / `playtest` / `c1_test` / `colony_inspect` construct `ResourceManager(PLANET_SIZE, …)` and place sects by cell | 26 |
| Tests | `MakeTestResourceManager()` builds a 20-cell grid with seed 42; `test_prospecting_grid` reads parent cells (8, 8) | 2 (+ ~45 helper calls) |
| `Engine_copy.{h,cpp}` | a stale copy, not in any CMake target, 29 grid refs | delete |

Two things are **not** the grid and stay: the sect's 5 km footprint
(`SECT_CORE_RADIUS`, radius 2.5 km) and the prospecting sub-grid inside a
sect, which is a lattice *within* one sect, not a lattice of the world.

## A.1 The replacement model

**The world is the Moon. A position is a `LunarPoint { latDeg, lonDeg }`.**

- `Sect`, `Colony`, excavators, roads and every marker carry a
  `LunarPoint`. Nothing carries a world-unit position.
- **Drawing happens in a local frame.** A view has a centre `LunarPoint`
  and a span in km; a point's place on screen is its km offset east and
  north of that centre. `survey_cursor.*` already implements exactly this
  (`SurveyLatLonToOffsetKm`, `SurveyCursorLatLon`, `SurveyViewport`), so
  the Colony view, the descent rungs and the globe markers all use one
  mapping. Inside a local frame **1 unit = 50 m is kept** as the drawing
  scale (`LOCAL_UNITS_PER_KM = 20`), so every existing radius, road
  speed and dome size keeps its meaning unchanged — only the origin
  moves from the corner of a global square to the centre of the view.
- **Ground truth is a function of location, not a cell in an array.**
  `ResourceManager` stops owning grids and answers questions about
  points: `GroundAt(LunarPoint)` returns the layered quantities for the
  5 km footprint at that point, generated deterministically from the
  point (hash of the quantised coordinates, the same idea the terrain
  chain and `ProspectingGrid` already use) scaled by the region's
  composition (`IdentifyRegion`: real named feature, terrane, mare or
  highland), plus smooth low-frequency variation so neighbouring sects
  differ and nearby ones correlate. The founding sect's floor
  (`EnsureBasicResources`) applies to that sect's ground. Depletion is
  recorded per sect. This is the "invert the generator so the region
  decides abundance" that the master design asked for (§4.6); removing
  the grid forces it now.
- **The survey is regional plus measured.** `GetOrbitalSurveyAt(gx, gy)`
  becomes `SurveyAt(LunarPoint)`: composition from the region identity
  (uniform inside a region, per §4.6), slope / illumination / Earth
  visibility from `EvaluateSite` at the point. The Colony-view Ctrl+hover
  "ORBITAL SURVEY" tooltip (`rendermanager.cpp:620–640`) and the F6 debug
  dump read this.
- **Terrain is keyed by location.** The chain is already
  `GenerateTerrainChain(lat, lon, …)`; `TerrainGridCellToLatLon` was only
  the lookup layer. The cache key becomes a quantised `LunarPoint` plus
  span; neighbour prefetch requests the eight points one footprint away.
  The anchor API and its clamp are deleted.
- **Views.** `View::Planet` (the 100 km grid) and `View::SITE_SELECTION`
  (the grid picker) are retired. What sits between the globe and a colony
  is the District rung (Part B). `View::Colony` is the 25 km window
  centred on the colony — the same window the descent's Site rung uses —
  drawn in its local frame with the colony's sects in it. `View::Sect`
  and `View::Unit` are unchanged; the Sect view is already screen-space.
- **Any number of colonies, anywhere.** Each colony has its own centre,
  its own ground, its own 25 km window. The globe and the District show
  them as markers (`OrbitalLatLonToScreen` and the local frame).

## A.2 Decisions

**A-D1 — Sect placement inside a colony.** *Recommendation:* free
placement, as today, with a minimum spacing of one footprint (5 km)
between sect centres so footprints never overlap, and the existing
jurisdiction rule. No local lattice: each sect generates its own ground
at its own point, so nothing needs cells to line up.

**A-D2 — What `Planet` becomes.** *Recommendation:* the owner of the
`ResourceManager` and the colony list, nothing else. `map`,
`GetRandomValidPosition`, `GetWorldPosition`, `DrawPlanetGrid` and the
active area go; the Planet camera that used the active area goes with
the Planet view.

**A-D3 — How quantities are generated without clusters.**
*Recommendation:* per point, base quantity per element from the region's
composition (iron and titanium from `fePct` / `tiPct`, aluminium and
calcium inversely, per §4.6), multiplied by a deterministic value noise
in lat/lon at a 10–30 km wavelength, then the existing depth-bias table
(`resource_manager.cpp:202–211`) for the four layers. Tuned once against
`colony_inspect` and held by the c1 economics test, which must still
pass on a mare point and a highland point. This is the one piece of new
game logic in Part A and the one with balance risk.

**A-D4 — Keep 1 unit = 50 m as the local drawing scale.**
*Recommendation:* yes. Changing the unit would touch every radius,
speed and layout constant for no gain; changing the origin touches one
conversion.

## A.3 Phases

### A1 — Data lives at locations (L)

- [x] `LunarPoint` in `game_structs.h`; `LocalFrame { centre, spanKm }`
      with `ToLocal(point) → Vector2 units` and `FromLocal`, built on
      `survey_cursor`'s km mapping.
- [x] `Sect` stores a `LunarPoint`; `GetPosition()` becomes
      `GetLocalPosition(const LocalFrame&)`. `Colony` centroid and
      radius computed in km and converted per frame. `Road` length from
      km offsets × 20. Minimum spacing rule (A-D1) in `BuildNewSect`.
- [x] `ResourceManager`: delete the three grids and `WorldToGrid` /
      `GridToWorld`; add `GroundAt`, `GetResourcesAtLayer(point, layer)`,
      `Deplete(point, type, amount)`, `SurveyAt(point)`,
      `ArchetypeAt(point)` (the last two on top of `IdentifyRegion` and
      `EvaluateSite`, which is why Part B's Phase 0 extraction of
      `region_identity` is a prerequisite and is scheduled first).
- [x] `Unit::ProcessExtraction` reads and depletes by the parent sect's
      point; excavator `gridPos` becomes a local offset or is dropped
      (it is only ever set to the parent cell).
- [x] `ProspectingGrid` / `ProspectingSystem` keyed by `LunarPoint`;
      `HashSeed` from quantised coordinates so the same spot yields the
      same sub-grid.
- [x] Tests: `MakeTestResourceManager()` → `MakeTestGround()` at a fixed
      point (Mare Imbrium centre) with a fixed seed; `test_prospecting_grid`
      reads that point. New tests: two points 5 km apart give different
      but correlated ground; the same point twice gives identical ground;
      a mare point out-irons a highland point.
- [x] Tools: `colony_inspect --pick LAT,LON`; `sectwalk`, `playtest`,
      `c1_test`, `preview` panel mode construct a sect at a point.

**Accept when:** `grep -rn "PLANET_SIZE\|GetResourcesAtGrid\|WorldToGrid"
src tools tests` hits only rendering and camera code; `ctest`, `c1_test`
and `preview.sh --all` are clean; `colony_inspect` at Imbrium and at
Tycho prints ground in the direction the region cards say.

### A2 — Views and terrain keyed by location; the grid views go (L)

- [x] `RenderManager`: `EnsureTerrainAt(point)` replaces
      `EnsureTerrainForCell`; cache key `(quantised point, span)`;
      neighbour prefetch by footprint offsets; the CPU pool job carries a
      point. `DrawColonyView` draws level 1 centred on the colony's point
      in its local frame. `DrawSectTerrainBackground` by point. Delete
      `DrawPlanetView`, `DrawPlanetMapLayer`, `PlanetMapWorldRect`,
      `DrawSiteSelectionView`, `DrawCellInfo`'s cell maths (it reads
      `GroundAt` for the hovered point), the moon-tile fallback if no
      caller remains.
- [x] `ViewManager`: Colony camera clamps to the 25 km window plus a
      margin; delete the Planet camera and the SITE_SELECTION case.
- [x] `terrain_synthesis`: delete `SetTerrainAnchor`, `GetTerrainAnchor`,
      `GetTerrainAnchorVersion`, `TerrainGridCellToLatLon`, the
      `TERRAIN_ANCHOR_*` constants and the ±78° clamp. `TERRAIN_CELL_KM`
      stays as the footprint.
- [x] `game_enums.h`: remove `View::Planet` and `View::SITE_SELECTION`
      (Part B adds `View::District`). `game_constants.h`: remove
      `PLANET_SIZE / WIDTH / HEIGHT`, add `LOCAL_UNITS_PER_KM`.
- [x] **Founding stub so the game stays playable:** in the Orbital view a
      click that is not a drag founds a colony at the picked point
      (no cards, no verdict yet) and opens its Colony view; existing
      colonies are drawn as markers on the globe and clicking a marker
      opens that colony; Esc from Colony returns to the globe. Ctrl+click
      in Colony view keeps adding sects. Part B replaces this stub with
      the descent.
- [x] `GameManager`: `FoundColony(point, identity)`; delete the grid
      picker methods and state; `UpdatePlanetActiveArea` goes.
- [x] Delete `Engine_copy.{h,cpp}`.
- [x] Tools: `colony_preview --view colony|sect --pick LAT,LON`;
      `colony_viewtest` walks Orbital → Colony → Sect with `--pick` and
      `--shots`, its Planet level removed; `terrain_probe` unchanged.
- [x] `docs/graveyard.md` entry 10: the 100 km playfield — the 20x20
      constants, the anchor and its clamp, `TerrainGridCellToLatLon`, the
      whole-moon map layer around the playfield, the grid picker's tint
      formula (`rendermanager.cpp:1742–1750`) and its bonus text, and why
      it all went.

**Accept when:** `grep -rn "PLANET_SIZE\|PLANET_WIDTH\|PLANET_HEIGHT\|
TerrainAnchor\|TerrainGridCellToLatLon\|SITE_SELECTION\|View::Planet"
src tools tests` is empty; two colonies founded on opposite sides of the
Moon each show their own real ground in Colony and Sect view and Esc
walks back to the globe from either; `viewtest --shots` renders globe,
colony and sect for a `--pick` on the far side; every target builds and
`ctest` is green.

---

# Part B — The descent is how a colony is founded

## B.0 Where the two flows stand

| | The game after Part A | `lunar_map --site` |
|---|---|---|
| **Level 1** | the globe; a click founds directly (the A2 stub) | the globe; hover names the region under the pointer, click claims it and flies down (`UpdateSiteSelect`, `lunarmap_main.cpp:3807–4401`) |
| **Level 2** | — | 200 km DISTRICT window, 25 km snapping cursor |
| **Level 3** | the Colony view is the 25 km window, but nothing is judged before founding | 25 km SITE window, free 1.5 km cursor = the base's footprint, live `EvaluateSite` + `JudgeSite` verdict with the blocker named |
| **Region data** | `SurveyAt(point)` built on `IdentifyRegion` (Part A) | `IdentifyRegion` (`:2860`), frozen at the claim |
| **Commit** | `FoundColony` | `app.founded = true`, a flag |
| **Input, feel** | click | drag-vs-click gesture, release-to-commit, tap-to-aim then tap-to-commit, BACK button, bounded zoom per rung, flights, hints, labels toggle |
| **Harness** | `viewtest --shots` | `--siteshot`, `--flyshot`, `--ladder`, `--demo` |
| **Web** | needs the DEM for the verdict — see D5 | ships with the DEM at `/lunarmap/` |

What is shared today: `survey_cursor.*` (compiled into every game target,
called by none of them). What is `static` inside the 5,535-line tool
file: everything else.

## B.1 The target

```
Menu ─ENTER─▶ ORBITAL     the globe. hover: region chip + region card
                 │         click (release, no drag): claim → flight
                 │         click a colony marker: open that colony
                 ▼
              DISTRICT    200 km window, 25 km snapping cursor
                 │         "which mix?" — colonies inside shown as markers
                 │         click: descend → flight
                 ▼
              COLONY      25 km window. With no colony here it is the SITE
                 │         rung: free 1.5 km cursor, live verdict,
                 │         click on green: FOUND → the colony appears in place.
                 │         With a colony here: sects, roads, Ctrl+click adds a sect.
                 ▼
              SECT ─▶ UNIT   as today

Esc / right-click / BACK go up one rung from anywhere, at any time,
with each rung's cursor where it was left. Nothing binds before FOUND.
Found as many colonies as you like, anywhere; founding inside another
colony's jurisdiction is refused with the reason shown.
```

"Unified" means one controller, one region identity, one verdict, one
set of cards, one hint layer, one geometry, used by `colony_game`,
`colony_viewtest`, `colony_preview` and `lunar_map`. It does **not** mean
one terrain renderer (D3).

## B.2 Decisions

*Retired by Part A:* the first draft's D1 (derive an anchor from the
site), D2 (forbid a second descent; found later colonies from a grid
view) and D7's clamp refusal. None has a referent once positions are
lunar.

### D3 — What draws the ground at the DISTRICT and SITE rungs

`lunar_map` draws a LOLA elevation mesh through its own shader
(`BuildScene`, `:1396–1615`). The game draws every geographic view as a
chain texture in a local frame.

**Recommendation: the game draws both rungs with the terrain chain and
uses the DEM only for numbers** (verdict, level card, ground stats).
The SITE window is chain level 1, the Colony view's own ground, so
founding does not change the picture under the cursor at all. At 200 km
the WAC out-resolves the DEM; at 25 km neither resolves anything and the
chain's regolith is the picture ([`site-ground-texture.md`](site-ground-texture.md)
§2). `TerrainChainSpansForWindow(spanKm)` (`terrain_synthesis.cpp:2156`)
already builds arbitrary spans on both paths. The DEM relief look and the
tilt view stay in the instrument; lighting the game's ground with its
own sun is a separate track (site-ground-texture.md Phase 2).

### D4 — Where the shared code lives

`src/SiteSelection/`, in the module shape of
[`../../guides/module-architecture.md`](../../guides/module-architecture.md):

| File | Holds | From `lunarmap_main.cpp` |
|---|---|---|
| `site_selection_constants.h` | verdict thresholds (8° mean, 25° peak, 40 m rough, 400 m relief), horizon km, flight rates, drag threshold, jump distance, card layout constants shared by draw and hit-test | `JudgeSite` literals, `SITE_TRANS_*`, `SITE_DRAG_THRESHOLD_PX`, the `24.0f` jump |
| `region_identity.{h,cpp}` | `RegionIdentity`, `IdentifyRegion(dem, point)`, PKT polygon, `SiteArchetype` descriptor table (name, tint, gives / costs from master design §2.1) | `:2703–2963` |
| `site_verdict.{h,cpp}` | `PlacementVerdict`, `JudgeSite`, `GroundStats`, `CursorGroundStats` | `:1859–1876`, `:2152–2192` |
| `survey_input.h` | `SurveyInput { pointer, click, escape, wheel, dt, pointerJumped }` — the seam the game, `viewtest` and `--siteshot` feed | `FakePointer`, `PressGesture`, `SiteClick`, `SiteEscape` (`:3254–3350`) |
| `site_selection_controller.{h,cpp}` | rungs, `SurveyDescent` stack, claim, zoom within rung, flights as pure interpolation, touch mode, hover hint key, pending verdict, colony markers. **No GL, no drawing.** | the state half of `UpdateSiteSelect`, `BeginGlobeDescent`, `BeginDescentZoom`, `RunDescentZoom`'s interpolation, `RegionCardHintAt` |
| `TerrainGen/lunar_dem_shared.{h,cpp}` | `const LolaDem* GetLunarDem()`: one load, one path constant, `--dem` override | `resource_manager.cpp:12–26`, `lunarmap_main.cpp:62`, `:5085` |

`BUILTIN_FEATURES` (`:2713–2733`) folds into `lunar_regions.cpp` so a
`LunarRegion` always carries Fe/Ti/Th. Drawing goes to `RenderManager`
in a new translation unit `src/Engine/rendermanager_survey.cpp`: region
card, level card, hint tooltip, hover chip, globe feature outlines, globe
cursor box, feature arcs in window, ladder cursor, cursor callout, prompt
strip, BACK button, colony markers — restyled to the game's fonts and the
dark-kit tokens ([`../../guides/ui-panels.md`](../../guides/ui-panels.md)).
The controller exports `Founded()`, `FoundPoint()`, `FoundRegion()`,
`FoundVerdict()`, and `EnterColony(index)`; that is the contract.

### D5 — The DEM in the game build

Move `ldem_16_uint.tif` (32 MB) to `src/assets/planet/lola/` so every
build that copies assets carries it; retarget `fetch-dem.yml`; add it to
the game's `--preload-file`. The two SLDEM overlays (51 MB) stay optional
and desktop-only. The web game currently decodes the WAC **three** times
(`EnsureWacLoaded`, `LoadAlbedo`, `LoadPlanetMap`); Part A deletes the
third, and Phase B5 shares the other two. Measure heap with the shell
badge before and after.

### D6 — Archetype, bonus, composition

`IdentifyRegion` returns the `SiteArchetype` enum; `Colony::SetArchetype`
keeps receiving it. `GetArchetypeBonus` (`colony.cpp:557`, no caller) and
the old panel's bonus text are deleted and recorded in the graveyard
entry. Composition coherency is Part A's A-D3, not a later phase.

### D7 — The poles, honestly

With no grid there is no clamp, so a polar site can be founded. Two
things still need measuring there, not assuming: the local frame's
longitude widening uses a cosine floor (`0.05` in `survey_cursor.cpp`,
`0.2` in the chain's macro crop), so a 25 km window very near the pole
draws its ground stretched; and PSR distance on the region card is the
instrument's placeholder (`psrKm = (|lat| > 80) ? 4 : 999`). Run
`terrain_probe` and `viewtest --pick -89.7,110` at Shackleton in B1 and
decide from the pictures whether a tangent-plane frame is needed before
the polar strategy is advertised. The refusal reason, if any, must name
the picture, never the ground.

**Measured 2026-09-21** (`viewtest --shots --pick -89.6,0 --aim 8,6`,
pictures in `figures/b1pole_*.png`): at 89.6° S the 200 km district and
the 25 km site window are smeared into east-west streaks — the mosaic
crop's cos(lat) floor of 0.2 is fully engaged, so a "square" window is
5x wider in km than it draws — and the founded sect does not appear in
its Colony view at all, because the survey cursor's 0.05 floor and the
local frame's 0.2 floor put "8 km east" at different places. A
tangent-plane frame is needed. Until it exists, a claim inside
`SITE_POLAR_FRAME_LAT_DEG` (80°) is refused at the globe and the strip
says the window there is not drawn truthfully yet — the picture, not the
ground.

### D8 — Zoom within a rung

Keep `SurveyZoomMax` (one notch in) and `SurveyZoomMin` (2x out).
Zoom-in draws the rung's texture larger; zoom-out requests a 2x-span
chain from the same cache. The instrument's wide-window swap and
speculation thread (`:3387–3510`) are not ported.

## B.3 Architecture

```
Engine::HandleInput
  ├─ InputManager::Update            press gesture, pointer-jumped, wheel
  ├─ SurveyInput in = inputManager.Survey()
  └─ if view ∈ {Orbital, District, Colony-without-colony}:
        controller.Update(in, w, h, dt)      pure state
        if controller.WantsGround(point, spanKm):
            renderManager.RequestGround(point, spanKm)     prefetch
        if controller.Founded():
            gameManager.FoundColony(controller)             Colony + Sect
        if controller.EnteredColony(i):
            gameManager.SelectColony(i); view = Colony
Engine::Draw
  └─ renderManager.DrawSurveyView(controller, planet, dem)
        rung 0: globe + outlines + cursor box + chip + region card + markers
        rung 1/2: ground texture + feature arcs + ladder cursor + callout
                  + region card + level card + markers
        flight: previous rung's texture under the interpolated camera
        hint tooltip last; prompt strip and BACK on top
```

Ground for the rungs: the location-keyed cache Part A built, asked for
`(point, 200 km)` and `(point, 25 km)`; requested when a flight starts so
the 1–3 s flight hides the CPU path's ~0.5 s build; `TerrainWarmMosaic()`
in `Engine::InitGame` so the first descent never pays the JPEG decode.

## B.4 Phases

Each leaves every target building and the game playable.

### B0 — Extraction, no behaviour change (M) — *runs before Part A's A1*

- [ ] Create `src/SiteSelection/` per D4; move `RegionIdentity`,
      `IdentifyRegion`, PKT polygon, `JudgeSite`, `GroundStats`,
      `CursorGroundStats` out of `lunarmap_main.cpp`. `IdentifyRegion`
      returns `SiteArchetype`; add the descriptor table.
- [ ] Fold `BUILTIN_FEATURES` into `lunar_regions.cpp`.
- [ ] `lunar_dem_shared`: one `LolaDem`; `ResourceManager` and
      `lunar_map` use it.
- [ ] `SurveyInput` + controller lifted from `UpdateSiteSelect`;
      `lunar_map` drives it, keeps its own drawing.
- [ ] `tests/test_site_selection.cpp` (Catch2; `WORKING_DIRECTORY
      ${CMAKE_SOURCE_DIR}` on `catch_discover_tests` so assets resolve):
      descent stack; `IdentifyRegion` at Imbrium → MARE_INDUSTRIAL, Tycho
      → HIGHLAND_CONSTRUCTION, Shackleton → POLAR_VOLATILE; `JudgeSite` at
      each threshold.

**Accept when:** all targets build, `ctest` green, `lunar_map --siteshot`
unchanged beyond text antialiasing (`compare.py`), no `static` copy of
anything in `src/SiteSelection/` remains in the tool.

*Then Part A, A1 and A2.*

### B1 — Founding through the descent (L)

- [x] `InputManager`: press gesture, jumped pointer, `Survey()`.
- [x] `View::District` added; the Orbital view runs the controller's
      rung 0 (ENTER claims at the sub-point); District runs rung 1; the
      Colony view with no colony under it runs rung 2. The A2 founding
      stub is removed.
- [x] `rendermanager_survey.cpp`: cards, chip, outlines, arcs, cursor,
      callout, strip, BACK, hint tooltip, colony markers. No flights yet.
- [x] `GameManager::FoundColony(point, identity, verdict)`: Colony
      (archetype) + first Sect at the point; the Colony view is already
      looking at that window, so the base appears under the cursor.
- [x] `colony_preview --view survey --rung orbital|district|site --pick
      LAT,LON [--aim DX,DY]`.
- [x] `colony_viewtest --shots`: hover → claim → district → site → found
      → colony → sect, scripted `SurveyInput`; then Esc back to the
      globe, claim a second region on the far side, found again.
- [x] Tests: FOUND refused on a red verdict; refused inside another
      colony's jurisdiction; the founded sect's `GroundAt` equals what
      `SurveyAt` showed on the card.
- [x] D7 check at Shackleton, pictures kept in `figures/`.

**Accept when:** a new player can turn the globe, read a region, claim
it, descend twice, see a green or red 1.5 km cursor with the blocker
named, found, and be in a Colony view whose ground did not change at the
click; then walk back up and found a second colony elsewhere. **Look at
the PNGs.**

### B2 — The feel (M)

Flights (globe turn+zoom on claim; log-space dive with the
straight-approach law between rungs, `:3675–3758`), ground requested at
flight start; tap-to-aim then tap-to-commit; BACK; narrow layout kept;
hints on every card row; labels toggle; the ghosted previous-region card
(§4.5); zoom per D8.

**Accept when:** `--shots` has three flight frames at 25/50/80 % showing
the target falling straight to centre; a scripted jumped tap does not
claim on the first tap and does on the second; every row renders a hint.

### B3 — Web and device (M)

DEM under `src/assets/planet/lola/`, preload extended, `fetch-dem.yml`
retargeted; one WAC decode shared by `terrain_synthesis` and
`lunar_globe`; this branch in `deploy-web.yml` and the Pages environment
while playtesting; device playtest at `/`.

**Accept when:** the shell badge reports a game heap no larger than
`lunar_map`'s 271 MB on the same iPad and a colony is founded on device.

### B4 — Documents (S)

This file → IMPLEMENTED with "as built" notes; `README.md` progress
table; master design §2 "as built" (the ladder is now the game's view
stack: Globe / District / Colony / Sect / Unit); `CLAUDE.md` scale
table, View System, Site Selection System, grid-system section, auto-
context row, testing table; `tools/lunarmap/README.md`; both roadmaps
(the ❌ line, debt items 13 and "two flows", the polar frame and the
generator tuning as open items).

---

## 5. Verification matrix

| Check | Instrument | Runs where |
|---|---|---|
| cursor geometry, snap, stack | `survey_cursor_test` | any build |
| controller, refusals, identity, ground determinism | `ctest` | `-DBUILD_TESTS=ON` |
| ground economics at two regions | `c1_test`, `colony_inspect --pick` | any build |
| every rung as the game draws it | `preview.sh --view survey …` | headless, software GL |
| the whole ladder, both directions, two colonies | `viewtest.sh --shots` | headless, software GL |
| the instrument drives the same controller | `lunarmap.sh --siteshot` + `compare.py` | headless, software GL |
| CPU vs GPU ground for a rung, polar included | `terrain_probe` | headless |
| nothing else regressed | `preview.sh --all`, all targets, six CI workflows checked individually | CI |
| heap, tap rules, aim-ability | shell badge + a person with the device | device only |

A fresh container fetches raylib but lacks the X11 development headers
GLFW needs (`RandR headers not found`). `apt-get update` and then the
package list in `tools/preview/README.md` fixes it; without the refresh
the install 404s on a stale index. Verified 2026-09-18: after that,
`colony_game`, `lunar_map` and `survey_cursor_test` build here and the
self-test passes all 42 checks.

## 6. Risks

| Risk | Mitigation |
|---|---|
| A1 touches the economy: quantities without clusters could make every region play alike or starve the c1 chains | A-D3 is tuned against `colony_inspect` at named regions and held by `c1_test` on a mare and a highland point before A1 is accepted |
| A hidden consumer of world units survives A2 and draws in the wrong place | the acceptance grep is the gate; `viewtest --shots` on the far side would show anything still assuming the old origin |
| Prospecting sub-grids change under existing saves or tests when the seed moves from cell indices to coordinates | there are no saves; tests fix the point and the seed and assert determinism, not specific values |
| The port drifts from the instrument and `--siteshot` stops proving anything | B0 makes the controller shared before any game work and feeds it through `SurveyInput`, never globals |
| The 200 km chain window reads flatter than the instrument's relief | render three demo sites in B1 and look; fallback is a DEM hillshade multiplied into the texture, still one camera |
| Polar windows draw stretched | D7 measures at Shackleton in B1; a tangent-plane frame is the follow-up if the pictures say so |
| Web heap grows with the DEM | B3 measures first and lands the shared decode in the same phase |
| `rendermanager.cpp` grows again | new drawing goes to `rendermanager_survey.cpp` only |

## 7. Deliberately out of scope

- Coherency chains C2–C4 (lunar night, PSR water, distance-priced
  transport), master design §5.0.
- A tangent-plane local frame for polar windows, unless D7's pictures
  demand it. *(They do — §8.)*

## 8. As built (2026-09-21)

Five commits on this branch, in the plan's order: B0 (`a98b947`), A1
(`20a5a6f`), A2 (`859d7f9`), B1 (`e95b4af`), then B2–B4 together. What
differs from the text above:

- **The colony's centre is the site window, not the first sect.** A1 fixed
  the centre on the first sect; B1 found that the base then moved on the
  click (the Colony view recentred on it). The centre is the 25 km window
  the site rung showed, the first sect stands at the click inside it, and
  the Colony view opens with the window filling the height — so the base
  appears exactly where the cursor was, on the same texture
  (`GameManager::FoundColony(point, windowCentre, claimed)`).
- **The ground under the rungs is one cache.** The terrain cache is keyed
  by (place, span): the district asks for a 200 km window, the site and
  the Colony view for the same 25 km window, both widened to the screen's
  aspect; the Sect view keeps the game chain with the site disturbance.
  Zoom-out draws the same cache's 1/zoomMin window (D8).
- **Flights were not deferred to B2.** The controller already flew them;
  drawing the rung being left under the moving camera (`SurveyDrawFlight`)
  was a dozen lines, so B1 shipped with them. `viewtest --shots` renders
  `flight1_25/50/80` (globe turn and zoom) and `flight2_25/50/80` (the dive).
- **D7 measured, and it bites.** At Shackleton the windows smear and the
  survey frame and the local frame disagree about east (D7, with
  pictures). Claims inside 80° are refused at the globe with the strip
  saying the window is not drawn truthfully yet. The tangent-plane frame
  is the open item.
- **The archetype is the claimed region's**, as D6 says, with a console
  note when the ground at the click reads differently.
- **Cards keep the instrument's default-font drawing.** The restyle to the
  game's fonts and the dark-kit tokens is not done; the layout is shared
  through `SurveyLayout` so it can be restyled in one place.
- **Not built from B2:** the labels toggle and the ghosted previous-region
  card (§4.5). Tap-to-aim, BACK, the narrow layout, hints and per-rung zoom
  are in.
- **B3:** the global LDEM_16 moved to `src/assets/planet/lola/`, so the
  game's and the harness's web preload carry it (32 MB; the SLDEM overlays
  stay with the prototypes); the globe builds its albedo from the
  synthesizer's one grey decode (`TerrainWacGrey`), ending the double
  decode; this branch is listed in `deploy-web.yml` for the playtest. The
  heap was **not** measured on a device from this container — the shell
  badge on an iPad is still the acceptance check.
- **SurveyFlow** (`src/Engine/survey_flow.*`) is the piece the
  architecture sketch in B.3 called "if view ∈ {…}: controller.Update":
  the Engine, `colony_viewtest` and `colony_preview` run the descent
  through it, and `SurveyScript` (`src/SiteSelection/survey_script.h`)
  drives it headlessly for the tools and the tests.
- Save/load (which now has a natural unit: colonies as `LunarPoint`s).
- DEM relief lit by the game's own sun; WebGL2; real PSR distance.

---

## Appendix A — `lunarmap_main.cpp` symbol inventory and destination

| Lines | Symbol(s) | Goes to |
|---|---|---|
| 70–152 | `DemoSite`, `DEMO_SITES` | stays (`--demo`) |
| 165–176 | `SITE_LEVELS`, `LEVEL_QUESTION` | `site_selection_constants.h` |
| 479–670 | GLSL shaders | stays (DEM renderer) |
| 681–1000 | `TerrainScene`, mesh, textures, `BuildScene` | stays |
| 1025–1090 | window cache | stays |
| 1107–1328 | chain layer over the DEM | stays |
| 1357–1395 | speculation | stays (D8) |
| 1859–1876 | `PlacementVerdict`, `JudgeSite` | `site_verdict` |
| 1878–1934 | `DrawPlacementCursor` | stays (`--place`) |
| 1951–2126 | data-layer instruments | stays (the archived model's test) |
| 2152–2192 | `GroundStats`, `CursorGroundStats` | `site_verdict` |
| 2200–2286 | wrapped text, hint underline, hint tooltip, mini bar | `rendermanager_survey.cpp` |
| 2288–2512 | `DrawRegionCard`, `DrawLevelCard` | `rendermanager_survey.cpp` |
| 2514–2529 | `DrawTestNote` | stays |
| 2531–2580 | `LadderViewport`, `LadderViewportZoomed`, `ZoomApproach` | controller |
| 2582–2676 | `DrawSurveyCursorNav` | stays |
| 2703–2963 | features, PKT, `FeatureAt`, `RegionIdentity`, `IdentifyRegion`, `SiteFromIdentity` | `region_identity` (+ `lunar_regions` for the composition table) |
| 3133–3230 | `GlobeCircleAt`, `DrawGlobeRing`, `DrawGlobeFeatureOutlines`, `DrawGlobeCursorBox` | `rendermanager_survey.cpp`, shared header so `lunar_map` draws them too |
| 3254–3350 | fake pointer, press gesture, click/escape, view toggles | `SurveyInput` + `InputManager`; toggles stay |
| 3353–3379 | `RegionCardHintAt` | controller |
| 3381–3510 | wide window, speculation | stays (D8) |
| 3513–3587 | `BuildSiteScene` | stays |
| 3589–3758 | flight constants, `BeginGlobeDescent`, `BeginDescentZoom`, `RunDescentZoom` | interpolation → controller; drawing → each renderer |
| 3760–3805 | `SyncWebCanvasToViewport` | stays |
| 3807–4401 | `UpdateSiteSelect` | state → controller; DEM draw stays; cards → shared |
| 4617–4750 | `DrawGlobeHud`, `DrawHoverChip`, `DrawFeatureArcsInWindow`, `DrawCursorCallout`, `DrawLadderCursor` | `rendermanager_survey.cpp` |
| 4753–5027 | `RenderLadder` | stays |
| 5029–5535 | `main`, `--flyshot`, `--siteshot` | stays; harnesses feed `SurveyInput` |

## Appendix B — What the first draft got wrong, kept as reasoning

The first draft accepted the 20x20 playfield as fixed and derived three
decisions from it: lay the grid so the founded site becomes cell (10, 10)
(`anchorLat = siteLat + 0.5·cellDeg`, the inverse of
`TerrainGridCellToLatLon`); found later colonies only from the Planet view
on that grid; refuse founding above 78° because the lat/lon grid smears.
Each was internally sound and each was a consequence of a world model the
design had already left behind — the globe is the planet, and the rungs
below it are windows on the same Moon, so there is nothing to anchor.
Removing the grid removes the decisions, which is the surest sign they
were never design.
