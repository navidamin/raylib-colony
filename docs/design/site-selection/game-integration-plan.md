# Site Selection — Game Integration Plan

**Status: PROPOSED** — written 2026-09-18 against `161ad96` on
`claude/lunarmap-wiring-site-selection-b9lwaw`. Nothing in this document
is built yet.

**Scope:** wire the survey descent that lives in `lunar_map --site` into
`colony_game` as *the* way a colony is founded, retire the grid picker
behind `View::SITE_SELECTION`, and leave one implementation of every
piece that both the game and the instrument need.

**Read first:** [`README.md`](README.md) (the design in two sentences),
[`site-selection-master-design.md`](site-selection-master-design.md)
§2–§5 (what each level decides, and the coherency contract), and
[`../../guides/feature-completeness.md`](../../guides/feature-completeness.md)
(the six questions this plan is organised around, because the descent
is today's clearest case of *engine-implemented, not player-reachable*).

---

## 0. Where the two flows stand

Everything below was read from the code, not from the roadmaps. Line
numbers are for `161ad96`.

| | The game: `View::SITE_SELECTION` | The instrument: `lunar_map --site` |
|---|---|---|
| **Entry** | Ctrl+click in Planet view (`Engine.cpp:308–316`) → `GameManager::BuildNewColony` → `EnterSiteSelection` | Menu-less: the web build boots straight into `--site` (`lunarmap_main.cpp:5029+`) |
| **Level 1** | none — the Orbital view draws the globe and a lat/lon readout but a click does nothing (`rendermanager.cpp:6347–6405`); ENTER jumps to Planet at the default anchor (Mare Imbrium) | the globe (`lunar_globe.*`), hover names the region under the pointer, click claims it and flies down (`UpdateSiteSelect`, `:3807–4401`) |
| **Level 2** | the 20x20 grid, cells tinted from synthetic `OrbitalSurveyData` over the three legacy moon tiles (`DrawSiteSelectionView`, `:1713–1937`) | 200 km DISTRICT: a LOLA elevation window shaded through a lunar GLSL shader, WAC albedo, optional chain layer; 25 km snapping cursor |
| **Level 3** | — | 25 km SITE: same renderer, free 1.5 km cursor = the base's footprint |
| **Region data** | per-cell Fe/Ti/Si/Al/Ca/Th/K/H bars — synthetic, random clusters, not geographic (`resource_manager.cpp:256–312`); Th is `(Fe+Ca)*0.5` | `IdentifyRegion` (`:2860`): zones.json feature (real), PKT polygon, mare/highland from real elevation; frozen at the claim |
| **Terrain data** | slope / illumination / Earth visibility per cell from `EvaluateSite` at the **default anchor** (`:313–330`) | live `EvaluateSite` at the cursor, `GroundStats` over the cursor, `JudgeSite` verdict with named blocker |
| **Commit** | ENTER → `ConfirmSiteSelection` (`gamemanager.cpp:563–616`): Colony + Sect at the cell centre, archetype set, Colony view | `app.founded = true` — a flag. Nothing is created. |
| **Camera / input** | the Planet camera (`viewmanager.cpp:26`), keyboard ENTER/ESC | drag-vs-click gesture, release-to-commit, touch "tap to aim, tap to commit", BACK button, wheel zoom bounded per rung, descent flights, resolution ladder, scene cache, wide-window zoom-out, speculative prebuild thread |
| **Hints** | none | `survey_hints.h` on hover, per card row |
| **Harness** | none (`colony_preview --view` has no `site`) | `--siteshot`, `--flyshot`, `--ladder`, `--demo`, `survey_cursor_test` |
| **Web** | ships at `/` with `src/assets` only — **no DEM**, so the survey's terrain rows fall back to the synthetic branch there | ships at `/lunarmap/` with the DEM (32 MB), the WAC and zones.json |
| **Size** | ~330 lines across four files | 5,535 lines in one file; the reusable parts are `static` |

Three facts that decide the shape of the work:

1. **`survey_cursor.*` is already in every game target** but has no
   caller in `src/Engine/`. The geometry is shared; nothing else is.
2. **The game has one terrain anchor** (`SetTerrainAnchor`,
   `terrain_synthesis.cpp:1723`) for the whole 20x20 grid, clamped to
   ±78° latitude, and the survey grid is generated **once at startup at
   the default anchor**. Re-anchoring invalidates the terrain cache but
   leaves the survey grid describing Mare Imbrium. Founding must
   regenerate it.
3. **`Colony::GetArchetypeBonus` has no caller** (`colony.cpp:557`), and
   the old picker's "Bonus: +20% Fe/Ti" text describes it. The master
   design already says not to bolt multipliers on (§4.6, §8).

---

## 1. The target

One founding flow, three rungs, one commitment:

```
Menu ─ENTER─▶ ORBITAL (level 1: which economy?)
                 │ hover: region chip + frozen-on-claim region card
                 │ click (release, no drag): claim → flight
                 ▼
              SITE_SELECTION · DISTRICT (200 km, 25 km snapping cursor: which mix?)
                 │ click: descend → flight
                 ▼
              SITE_SELECTION · SITE (25 km, free 1.5 km cursor: which ground?)
                 │ click on a green cursor: FOUND
                 ▼
              GameManager::FoundColony ─▶ anchor laid, survey regenerated,
                                          Colony + Sect created
                 ▼
              Colony view  (Planet / Sect / Unit as today)

Esc / right-click / BACK at any rung backs out one rung with the cursor
where it was left (SurveyDescent is a stack). Nothing binds before FOUND.
```

After the first colony exists the globe is a place to look, not to
choose: the playfield is marked on it, clicking the mark (or ENTER)
descends to Planet, and further colonies are founded **from the Planet
view on the existing grid** (§2, D2).

"Unified" here means: one controller, one region identity, one verdict,
one set of cards, one hint layer, one geometry — used by `colony_game`,
`colony_viewtest`, `colony_preview` and `lunar_map`. It does **not** mean
one terrain renderer; D3 explains why the game keeps drawing ground the
way its other views do.

---

## 2. Decisions to settle before code

Each has a recommendation. Overriding any of them changes tasks in §4,
so settle them first.

### D1 — How founding lays the grid

The site cursor is free-moving at 1.5 km so the verdict is measured over
the ground the base actually covers. The game places Sects at 5 km cell
centres, and the terrain cache, the site disturbance and the Sect view
are all per cell. Snapping the player's chosen spot to a cell of a
pre-existing grid would move the base off the ground that was judged.

**Recommendation: derive the anchor from the site, not the site from
the grid.** On FOUND, set the anchor so the cursor centre becomes the
centre of cell (10, 10):

```
cellDeg   = TERRAIN_CELL_KM / MOON_KM_PER_DEG          // 0.164893°
anchorLat = siteLat + 0.5 * cellDeg
anchorLon = siteLon - 0.5 * cellDeg / max(0.2, cos(anchorLat))
```

which inverts `TerrainGridCellToLatLon(10, 10)` (`terrain_synthesis.cpp:1871`:
offsets `gx - 9.5`, `gy - 9.5`, longitude widened by `1/cos(anchorLat)`).
The first Sect then sits at world `(1050, 1050)` and the playfield
surrounds the base with room on every side. A unit test asserts the round
trip to 1e-9°.

### D2 — Colonies after the first

Only one anchor exists, so a second descent from orbit cannot found
anywhere else without moving colony 1's ground.

**Recommendation:** with colonies present the Orbital view is browse-only
(hover still names regions; clicking the playfield mark or ENTER goes to
Planet; clicking elsewhere shows "the playfield is here" on the chip, no
descent). Later colonies are founded in the **Planet view**: Ctrl+click a
cell → the SITE-rung card appears in place (frozen region card +
`EvaluateSite` over that 5 km cell + `JudgeSite` verdict + FOUND/CANCEL),
drawn in the Planet camera. This is the site rung with a 5 km snapped
cursor — `SurveyCursor` already carries `footprintKm` and `snapToGrid`
per instance — and it needs no separate view. Refusal reasons: verdict
red, cell inside another colony's jurisdiction (today's
`CheckCollisionPointCircle` test in `ConfirmSiteSelection`), cell
occupied.

*Alternative considered:* a full DISTRICT → SITE descent clamped to the
100 km playfield. More motion for no new decision; the region card is
already fixed. Not recommended.

### D3 — What draws the ground at the DISTRICT and SITE rungs

`lunar_map` draws a LOLA elevation mesh through its own lunar shader
(`BuildScene`, `:1396–1615`; shaders `:479–670`; mesh and textures
`:755–1000`). The game draws every geographic view as a 2D chain
texture in world space (`DrawWorldTerrainLayer`, `EnsureTerrainForCell`).

**Recommendation: the game draws both rungs with the terrain chain, and
the DEM supplies only numbers** — the verdict, the level card, the
ground stats. Reasons:

- The SITE window (25 km) **is** chain level 1, the COLONY view's ground.
  The handover from the descent to the playfield becomes the same
  imagery of the same place at the same scale, which is the whole claim
  the chain makes ("zooming approaches the same ground instead of
  cutting to a different scene").
- At 200 km the WAC mosaic (1.33 km/px) out-resolves LDEM_16
  (1.9 km/px); at 25 km neither resolves anything and the chain's
  regolith is the picture — [`site-ground-texture.md`](site-ground-texture.md)
  §2.1–2.3 measured this, and the web instrument already lays the chain
  over the DEM there for that reason.
- `TerrainChainSpansForWindow(spanKm)` (`terrain_synthesis.cpp:2156`)
  already builds arbitrary spans, on both paths; the GPU builds one in
  milliseconds, the CPU pool off-thread in ~0.5 s.
- No second renderer, second shader, or second camera enters
  `RenderManager`, and WebGL1 needs no new work.

What is given up: the DEM's shaded relief at 200 km (105 real samples,
explicit sun) and the tilt view. Both stay in `lunar_map` as instrument
features. If the game ever wants relief lit by its own sun, the route is
site-ground-texture.md Phase 2 (the chain's unlit height + albedo
export, `GenerateTerrainFields`) applied to the game's views — a
separate track, not this one.

### D4 — Where the shared code lives

**Recommendation:** a new module `src/SiteSelection/`, in the shape
[`../../guides/module-architecture.md`](../../guides/module-architecture.md)
Part II prescribes (constants / types / pure engines / facade), added to
`COLONY_CORE_SOURCES` and to `lunar_map`'s sources:

| File | Holds | From `lunarmap_main.cpp` |
|---|---|---|
| `site_selection_constants.h` | verdict thresholds (8° mean, 25° peak, 40 m rough, 400 m relief), horizon km, flight rates, drag threshold, jumped-pointer distance, layout constants shared by draw and hit-test | `JudgeSite` literals, `SITE_TRANS_*`, `SITE_DRAG_THRESHOLD_PX`, `24.0f` jump |
| `region_identity.{h,cpp}` | `RegionIdentity`, `IdentifyRegion(dem, lat, lon)`, PKT polygon, `SiteArchetype` descriptor table (name, tint, gives / costs) | `:2703–2963` |
| `site_verdict.{h,cpp}` | `PlacementVerdict`, `JudgeSite`, `GroundStats`, `CursorGroundStats` | `:1859–1876`, `:2152–2192` |
| `survey_input.h` | `SurveyInput { pointer, click, escape, wheel, dt, pointerJumped }` — the seam the game, `viewtest` and `--siteshot` all feed | `FakePointer`, `PressGesture`, `SiteClick`, `SiteEscape` (`:3254–3350`) |
| `site_selection_controller.{h,cpp}` | `SiteSelectionState` + `SiteSelectionController::Update(const SurveyInput&, w, h)`: rungs, `SurveyDescent` stack, claim, zoom within rung, flights, touch mode, hover hint key, pending verdict. **No GL, no drawing.** | the state half of `UpdateSiteSelect`, `BeginGlobeDescent`, `BeginDescentZoom`, `RunDescentZoom`'s interpolation, `RegionCardHintAt` |
| `lunar_dem_shared.{h,cpp}` (in `TerrainGen/`) | `const LolaDem* GetLunarDem()`, one load, one path constant, optional override for `--dem` | `resource_manager.cpp:12–26` (`RealMoon`), `lunarmap_main.cpp:62`, `:5085` |

Composition fallbacks for named features (`BUILTIN_FEATURES`,
`:2713–2733`) move into `lunar_regions.cpp` so a `LunarRegion` always
carries Fe/Ti/Th and no second table exists.

Drawing goes to `RenderManager`, in a new translation unit
`src/Engine/rendermanager_survey.cpp` (the class is already 6,405 lines
in one file; CMake lists sources explicitly, so a second file costs
nothing): region card, level card, hint tooltip, hover chip, globe
feature outlines, globe cursor box, feature arcs in window, ladder
cursor, cursor callout, prompt strip, BACK button. Restyled to the
game's fonts and the dark kit tokens per
[`../../guides/ui-panels.md`](../../guides/ui-panels.md) — the instrument
draws with raylib's default font.

The controller exports exactly what `GameManager` needs:
`Founded()`, `FoundLat/Lon()`, `FoundRegion()`, `FoundVerdict()`.
That is the whole contract.

### D5 — The DEM in the game build

The game already loads the DEM on desktop (`RealMoon`) from
`prototypes/planet_visuals/data/lola/`; the Linux/macOS/Windows release
zips copy only `src/assets` and so ship without it; the web game
preloads only `src/assets`.

**Recommendation:** move `ldem_16_uint.tif` (32 MB) to
`src/assets/planet/lola/` so every build that copies assets carries it,
update `fetch-dem.yml`'s target directory, and add it to the game's
`--preload-file` list. Leave the two SLDEM overlays (51 MB) where they
are: optional on desktop, absent on web. Web download grows ~32 MB; heap
grows ~33 MB (the raw 16-bit grid). Measure the game's web heap before
and after with the shell badge — the 271 MB figure in the roadmap is
`lunar_map`'s, not the game's, and the game currently decodes the WAC
**three** times (`EnsureWacLoaded`, `LoadAlbedo`, `LoadPlanetMap`; debt
item 13 counts two). Sharing one decode is the enabling task for the
phone (§4, Phase 5).

### D6 — Archetype, bonus, composition

**Recommendation:** `IdentifyRegion` returns the `SiteArchetype` enum
(it produces POLAR_VOLATILE, KREEP_SCIENTIFIC, MARE_INDUSTRIAL,
HIGHLAND_CONSTRUCTION; MIXED emerges from position per §2.1; LAVA_TUBE is
never produced and stays an enum value). `Colony::SetArchetype` keeps
receiving it. **Delete `GetArchetypeBonus`** and the bonus text with the
old panel; record both in `docs/graveyard.md`. Composition coherency —
the region card's numbers being the numbers the planet grid then holds —
is Phase 4, done the cheap honest way first (§4).

### D7 — The poles

`SetTerrainAnchor` clamps to ±78°; the grid's longitude widening uses a
cosine floor of 0.2. `lunar_map` happily claims Shackleton at −89.7°
(`DEMO_SITES`). In the game, clamping silently would put the colony
~12° from where the player pointed.

**Recommendation:** FOUND is refused above the clamp with a named reason
on the verdict ("PLAYFIELD GRID CANNOT REACH THIS LATITUDE"), the same
way slope refuses. The polar strategy therefore does not exist in play
until the grid becomes a local tangent-plane projection about the anchor
(a change to `TerrainGridCellToLatLon`, `PlanetMapWorldRect` and the
chain's cell placement). That is a follow-up (§7), and the refusal
wording must say so honestly rather than pretend the ground is bad.

### D8 — Zoom within a rung

The instrument's zoom-out needs a second, wider DEM window with swap
logic, a speculative worker and a cache (`BuildWideWindow`,
`SpeculationStart`, `:3387–3510`) because the mesh frames the window's
full width. With chain textures a wider view is a wider chain.

**Recommendation:** keep `SurveyZoomMax` (one notch in) and
`SurveyZoomMin` (2x out) as the bounds, implement zoom-in as drawing the
rung's texture larger, and zoom-out as requesting a 2x-span chain from
the same ground cache. Drop the wide-window swap and the speculation
thread from the port; they stay in `lunar_map` for the DEM renderer.

---

## 3. Architecture of the port

### 3.1 One frame of the descent in the game

```
Engine::HandleInput
  ├─ InputManager::Update                (press gesture: down/moved/released,
  │                                        pointer-jumped, wheel)
  ├─ SurveyInput in = inputManager.Survey()
  └─ if view ∈ {Orbital, SITE_SELECTION}:
        controller.Update(in, w, h, dt)  ← pure state: hover → identity,
                                            cursor track, zoom, flight t,
                                            claim / descend / ascend / found
        if controller.WantsGround(lat, lon, spanKm):
            renderManager.RequestSurveyGround(lat, lon, spanKm)   (prefetch)
        if controller.Founded():
            gameManager.FoundColony(controller)  → anchor, survey regen,
                                                    Colony + Sect
            viewManager.SwitchToColonyView(...)
Engine::Draw
  └─ renderManager.DrawSurveyView(controller, planet, dem)
        rung 0: DrawLunarGlobe + outlines + cursor box + chip + region card
        rung 1/2: ground texture (chain window, cached) + feature arcs
                  + ladder cursor + callout + region card + level card
        flight: the previous rung's texture under the interpolated camera
        hint tooltip last, prompt strip and BACK on top
```

The controller never touches GL. `--siteshot` and `colony_viewtest
--shots` drive it with a scripted `SurveyInput`, which is what makes the
harness verify the shipping state machine rather than a copy (the
instrument's stated reason for `--siteshot`).

### 3.2 Ground for the rungs

A small cache in `RenderManager` beside the per-cell one: three entries
keyed by `(lat, lon, spanKm, anchorVersion-independent)`, each one chain
built with `TerrainChainSpansForWindow(spanKm)` at
`GetTerrainPathResolution()` (1024 GPU / 512 CPU), with
`TerrainSiteDisturbance` off (nothing is built yet). On the GPU path it
builds on request. On the CPU path `TerrainPool::Build` gains a job kind
that carries `(lat, lon, spans)` instead of `(gx, gy)`, and the request
is issued **when the flight begins**, so the 1–3 s of flight hides the
~0.5 s build the instrument pays *after* landing. `TerrainWarmMosaic()`
runs in `Engine::InitGame` so the first descent never pays the JPEG
decode.

### 3.3 Engine changes

| File | Change |
|---|---|
| `game_enums.h` | `View::SITE_SELECTION` kept, meaning changes to "the descent below orbit". (Rename to `Survey` is optional; ~10 sites.) |
| `Engine.cpp` | Orbital case: replace ENTER/ESC-only handling with the controller (ENTER = claim at the sub-point, the keyboard equivalent of a click). SITE_SELECTION case: replace hover/ENTER/ESC with the controller; on `Founded()` call `FoundColony`. Remove the Ctrl+click → `BuildNewColony` path in Planet (Phase 3 replaces it). Draw: `DrawSurveyView` for both views. Keep F6. |
| `ViewManager` | `HandleCameraControls`: SITE_SELECTION no longer runs the Planet camera. Remove the SITE_SELECTION case from `ResetCameraForCurrentView`. Add `SwitchToSurveyView()`. |
| `InputManager` | press gesture (down / moved past 5 px / released), `PointerJumped()` (24 px), `SurveyInput Survey() const`. |
| `GameManager` | replace `EnterSiteSelection` / `UpdateSiteSelectionHover` / `ConfirmSiteSelection` / `CancelSiteSelection` / `inSiteSelection` / `hoveredGridPos` / `selectedSite` with `FoundColony(lat, lon, const RegionIdentity&, const TerrainBuildability&)` and, in Phase 3, `TryFoundColonyAtCell(gx, gy)`. Owns the `SiteSelectionController`. |
| `RenderManager` | `rendermanager_survey.cpp`; delete the old `DrawSiteSelectionView` body; survey ground cache; the Orbital view's readout moves into the level-1 drawing. |
| `resource_manager.cpp` | `RealMoon()` → `GetLunarDem()`. |
| `src/CMakeLists.txt` | new sources in `COLONY_CORE_SOURCES`, `colony_viewtest`, `c1_test`, `colony_inspect`; `lunar_map` links `SiteSelection/`; DEM preload for the web game. |

### 3.4 What `lunar_map` becomes

The instrument keeps everything the game does not want: the DEM mesh and
shader, `--nearside`, `--pick/--span`, `--tilt`, `--style`, `--survey`,
`--place`, `--ladder`, `--demo`, the data-layer test, the chain layer
over the DEM, the wide window. `UpdateSiteSelect` becomes: feed the
shared controller, build/draw its DEM scene for the rung the controller
says it is on, draw the shared cards. About 1,700 lines leave the file.
Its README line "links no game code" is retired.

---

## 4. Phases

Each phase leaves every target building and the game playable end to
end. Sizes are relative (S < M < L); no dates.

### Phase 0 — Extraction, no behaviour change (M)

- [ ] Create `src/SiteSelection/` per D4; move `RegionIdentity`,
      `IdentifyRegion`, PKT polygon, `JudgeSite`, `PlacementVerdict`,
      `GroundStats`, `CursorGroundStats` out of `lunarmap_main.cpp`.
- [ ] `IdentifyRegion` returns `SiteArchetype`; add the descriptor table
      (name, tint, gives / costs from master design §2.1). `lunar_map`
      reads the name from the table.
- [ ] Fold `BUILTIN_FEATURES` composition fallbacks into
      `lunar_regions.cpp`; `LunarRegion` always has Fe/Ti/Th.
- [ ] `TerrainGen/lunar_dem_shared.{h,cpp}`: one `LolaDem`, one path;
      `ResourceManager` and `lunar_map` use it (`--dem` sets the path
      before first use).
- [ ] `SurveyInput` + `SiteSelectionController`: lift the state half of
      `UpdateSiteSelect` (claim, descend, ascend, zoom bounds, touch mode,
      hint row, flights as pure interpolation). `lunar_map` drives it;
      its draw half stays put for now.
- [ ] `tests/test_site_selection.cpp` (Catch2; add
      `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}` to `catch_discover_tests`
      so `zones.json` and the DEM resolve): descent stack semantics;
      `IdentifyRegion` at Mare Imbrium → MARE_INDUSTRIAL, Tycho →
      HIGHLAND_CONSTRUCTION, Shackleton rim → POLAR_VOLATILE; `JudgeSite`
      on hand-built `TerrainBuildability` values at each threshold.

**Accept when:** all targets build (`colony_game colony_preview
colony_playtest colony_sectwalk colony_viewtest colony_inspect lunar_map
terrain_probe survey_cursor_test c1_test`), `ctest` green,
`lunar_map --siteshot` renders the same twelve steps and `compare.py`
reports no pixel change beyond text antialiasing, `lunarmap_main.cpp` is
shorter by the moved code and contains no `static` copy of anything now
in `src/SiteSelection/`.

### Phase 1 — The first colony is founded through the descent (L)

- [ ] `InputManager`: press gesture, jumped pointer, `Survey()`.
- [ ] `RenderManager::RequestSurveyGround` / cache (§3.2); CPU pool job
      kind for `(lat, lon, spans)`.
- [ ] `rendermanager_survey.cpp`: port the cards, chip, outlines, arcs,
      cursor, callout, strip, BACK, hint tooltip. Game fonts, dark-kit
      tokens, layout constants shared with the controller's hit-test.
- [ ] `Engine.cpp`: Orbital = rung 0 with the controller (ENTER claims at
      the sub-point; no idle drift while choosing, as the instrument
      does). SITE_SELECTION = rungs 1–2. No flights yet: cut straight, as
      the headless harness does.
- [ ] `GameManager::FoundColony`: D1 anchor, `SetTerrainAnchor`,
      `planet->GetResourceManager().GenerateOrbitalSurveyData()`,
      Colony (archetype from identity) + Sect at cell (10,10), switch to
      Colony view. D7 refusal for |lat| above the clamp, surfaced on the
      verdict before the click.
- [ ] With colonies present: Orbital browse-only (D2), playfield mark
      drawn with `OrbitalLatLonToScreen` (as `viewtest` does today).
- [ ] Keep the old Ctrl+click grid picker for **additional** colonies in
      this phase only, so multi-colony play does not regress.
- [ ] `colony_preview --view survey --rung orbital|district|site --pick
      LAT,LON [--aim DX,DY]` renders through the real `RenderManager`.
- [ ] `colony_viewtest`: replace its own orbital pick prototype
      (`viewtest_main.cpp:317–558`) with the controller; `--shots` walks
      hover → claim → district → site → found → planet → colony → sect
      with a scripted `SurveyInput`; update its issue notes.
- [ ] Unit tests: anchor round trip (D1) to 1e-9°; FOUND refused on a red
      verdict; FOUND refused above the latitude clamp with the right
      reason; survey grid rows change after re-anchor.

**Accept when:** a player who starts the game can turn the globe, read a
region name, claim it, descend twice, see a green or red 1.5 km cursor
with the blocking limit named, found, and arrive in a Colony view whose
ground is the ground they judged (the `--shots` sequence shows the site
window and the Colony view as the same imagery). `survey_cursor_test`,
`ctest`, `preview.sh --all` clean. **Look at the PNGs.**

### Phase 2 — The feel (M)

- [ ] Flights: globe turn + zoom to `OrbitalZoomForSpan(200)` on claim;
      log-space dive with the straight-approach centre law between rungs
      (`RunDescentZoom`, `:3675–3758`, as pure interpolation in the
      controller; the renderer draws the previous rung's texture under the
      interpolated camera). Ground for the next rung requested at the
      start of the flight.
- [ ] Touch: tap-to-aim then tap-to-commit once a jumped pointer is seen;
      prompt strip wording switches; BACK button; narrow layout branch
      (< 720 px) kept even though the game's web canvas is pinned at
      1280x720 today.
- [ ] Hints on hover for every card row (`survey_hints.h` already ships
      in `src/`); dotted underline affordance; tooltip drawn last.
- [ ] Labels toggle (annotations on/off) and the region-card ghost of the
      previously viewed region (master design §4.5, "keep the previous
      region's panel on screen, ghosted").
- [ ] Zoom within rung per D8.

**Accept when:** `--shots` includes three flight frames at 25/50/80 %
progress and they show the target falling straight to centre; a scripted
jumped-pointer tap does not claim on the first tap and does on the
second; every card row renders a hint in preview.

### Phase 3 — Later colonies, and the grid picker goes (M)

- [ ] Ctrl+click in Planet view → in-place SITE-rung card for that cell
      (D2): frozen region card, `EvaluateSite(cell, TERRAIN_CELL_KM)`,
      `JudgeSite`, occupancy and jurisdiction refusals, FOUND / CANCEL.
      `GameManager::TryFoundColonyAtCell`.
- [ ] Delete the old `DrawSiteSelectionView` body, `EnterSiteSelection`
      and friends, the `SITE_SELECTION` branches in `ViewManager`, and
      `GetArchetypeBonus` (D6).
- [ ] `docs/graveyard.md` entry 10: the instrument-panel grid picker —
      the mare/highland tint formula (`60 + highland*140 …`, `:1742–1750`),
      the bonus table, the archetype recommendation panel, why it went.
- [ ] Colony-view Ctrl+hover "ORBITAL SURVEY" tooltip
      (`rendermanager.cpp:620–640`) stays; it now reads a grid generated
      for the real anchor.

**Accept when:** a second colony can be founded on a free flat cell and
is refused on a steep one, inside another jurisdiction, and on an
occupied cell, each with its reason shown before the click;
`grep -rn "SITE_SELECTION" src tools` finds only the descent.

### Phase 4 — Coherency: the card is true (M)

The region card must show the numbers the planet then holds (master
design §4.6, §5.0 rule 1). Cheapest honest version first; the generator
inversion stays a follow-up.

- [ ] `ResourceManager::GenerateResourceMap` takes the founding
      `RegionIdentity`: cluster `maxAbundance` for Fe/Ti scaled by the
      region's Fe/Ti, Al/Ca inversely (plagioclase vs mafic, §4.6), so a
      mare playfield really holds more iron.
- [ ] `GenerateOrbitalSurveyData`: Fe/Ti/Th rows set from the identity,
      uniform across the grid (one gamma-ray pixel is wider than the
      playfield); terrain rows from `EvaluateSite` at the real anchor;
      drop the `(Fe + Ca) * 0.5` thorium.
- [ ] Defer `Planet::GenerateMap()` from `InitGame` to `FoundColony`, or
      regenerate there — Planet view is only reachable with a colony
      after Phase 1. Harnesses that call `GenerateResourceMap` directly
      are unaffected.
- [ ] Test: the frozen card's Fe/Ti/Th equal
      `GetOrbitalSurveyAt(10,10)` after founding; `colony_inspect` prints
      the region it was generated for.

**Accept when:** founding Mare Imbrium and founding a highland region
produce visibly different `colony_inspect` dumps in the direction the
cards said, and the c1 economics test still passes on both.

### Phase 5 — Web and device (M)

- [ ] DEM under `src/assets/planet/lola/`, `fetch-dem.yml` retargeted,
      game `--preload-file` extended (D5). `lunar_map`'s preload updated
      to the new path.
- [ ] One WAC decode shared by `terrain_synthesis`, `lunar_globe` and
      `LoadPlanetMap` (the globe's 2048-wide grayscale equirect can serve
      as the planet map layer directly). Measure heap before/after with
      the shell badge.
- [ ] `deploy-web.yml`: this branch in `on.push.branches` while
      playtesting (and in the `github-pages` environment rules — see
      `docs/web-deploy-mobile.md`).
- [ ] Device playtest of the game at `/`, not `/lunarmap/`: arrival pause
      per rung, tap rules, whether the 1.5 km cursor can be aimed at
      1280x720 scaled onto a phone.

**Accept when:** the badge reports a game heap that stays under
`lunar_map`'s 271 MB on the same iPad, and a colony is founded on device.

### Phase 6 — Documents (S)

- [ ] This file: status → IMPLEMENTED, with "as built" notes where
      phases diverged.
- [ ] `README.md` progress table: steps 2–4 done; document list.
- [ ] `site-selection-master-design.md`: §5 "as built" for D1/D2/D3/D7.
- [ ] `CLAUDE.md`: View System bullets, the Site Selection System
      section, the auto-context row, the Visual Testing table
      (`--view survey`).
- [ ] `tools/lunarmap/README.md`: shares `src/SiteSelection/`; flags
      table re-checked against `--help`.
- [ ] `ROADMAP_IMMINENT.md` / `ROADMAP_OVERALL.md`: the ❌ line becomes
      ✅ with the phase that closed it; debt items 13 and "two flows"
      resolved; D7 polar grid and the generator inversion added as open
      items.

---

## 5. Verification matrix

| Check | Instrument | Runs where |
|---|---|---|
| cursor geometry, snap, stack | `survey_cursor_test` | any build |
| controller state machine, anchor math, refusals, identity | `ctest` (`test_site_selection`) | any build with `-DBUILD_TESTS=ON` |
| every rung as the game draws it | `tools/preview/preview.sh --view survey --rung …` | headless, software GL |
| the whole descent + handover to Planet/Colony/Sect | `tools/viewtest/viewtest.sh --shots` | headless, software GL |
| the instrument still drives the same controller | `tools/lunarmap/lunarmap.sh --siteshot` + `compare.py` | headless, software GL |
| CPU vs GPU ground for a rung | `terrain_probe` | headless |
| nothing else regressed | `preview.sh --all`, all targets, six CI workflows (check each, per debt item 10) | CI |
| heap, tap rules, aim-ability | shell badge + a person with the device | device only |

A fresh container fetches raylib but lacks the X11 development headers
GLFW needs (`RandR headers not found`). `apt-get update` and then the
package list in `tools/preview/README.md` (`libxrandr-dev libxinerama-dev
libxcursor-dev libxi-dev libgl-dev mesa-common-dev`) fixes it; without
the refresh the install 404s on a stale index.

---

## 6. Risks

| Risk | Mitigation |
|---|---|
| The port drifts from the instrument and `--siteshot` stops proving anything | Phase 0 makes the controller shared *before* any game work; the harness feeds `SurveyInput`, never globals |
| Re-anchoring after founding leaves stale state somewhere the terrain cache does not cover | `FoundColony` is the one place the anchor moves; the survey regen and the cache invalidation both key on `GetTerrainAnchorVersion()`; test asserts rows change |
| The 200 km chain window looks worse than the DEM relief the instrument shows | Render both at three demo sites in Phase 1 and look; if the WAC-only district reads flat, the fallback is `DrawPlanetMapLayer`-style albedo plus the DEM hillshade as a multiply — still 2D, still one camera |
| A polar claim is refused and the player does not know why | D7 wording names the grid, not the ground; the roadmap carries the tangent-plane follow-up |
| Web heap grows past the iPad ceiling with the DEM added | Phase 5 measures first and ships the one-decode change in the same phase |
| `rendermanager.cpp` grows again | new drawing goes to `rendermanager_survey.cpp`; nothing new lands in the 6,405-line file |
| Cards restyled to the dark kit stop matching the instrument's screenshots | acceptable: the instrument keeps its own look for its own figures; the *content* and layout constants are shared |

---

## 7. Deliberately out of scope

- **Coherency chains C2–C4** (lunar night, PSR water, distance-priced
  transport) — master design §5.0. The panels stay measured-only until
  each system exists.
- **Generator inversion** so that the region decides abundance
  (§4.6 `[?]`). Phase 4 does the uniform-row version only.
- **Polar playfields**: a tangent-plane grid about the anchor (D7).
- **DEM relief lit by the game's own sun** in Planet/Colony/Sect —
  site-ground-texture.md Phase 2.
- **WebGL2**, and the CPU/GPU regolith split on the web.
- **Real PSR distance** on the region card; the instrument's
  `psrKm = (|lat| > 80) ? 4 : 999` placeholder is carried as-is and
  labelled as such in code.

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
| 1878–1934 | `DrawPlacementCursor` | stays (`--place`); the cursor drawing it duplicates is `DrawLadderCursor` |
| 1951–2126 | data-layer instruments (`INSTRUMENTS`, `FieldMean`, `FieldBand`) | stays (Appendix A test of the master design) |
| 2152–2192 | `GroundStats`, `CursorGroundStats` | `site_verdict` |
| 2200–2286 | wrapped text, hint underline, hint tooltip, mini bar | `rendermanager_survey.cpp` (game fonts) |
| 2288–2512 | `DrawRegionCard`, `DrawLevelCard` | `rendermanager_survey.cpp` |
| 2514–2529 | `DrawTestNote` | stays |
| 2531–2580 | `LadderViewport`, `LadderViewportZoomed`, `ZoomApproach` | controller |
| 2582–2676 | `DrawSurveyCursorNav` | stays (the archived instrument-floor readout) |
| 2703–2963 | features, PKT, `FeatureAt`, `RegionIdentity`, `IdentifyRegion`, `SiteFromIdentity` | `region_identity` (+ `lunar_regions` for the composition table) |
| 3133–3230 | `GlobeCircleAt`, `DrawGlobeRing`, `DrawGlobeFeatureOutlines`, `DrawGlobeCursorBox` | `rendermanager_survey.cpp` (shared header so `lunar_map` draws them too) |
| 3254–3350 | fake pointer, press gesture, click/escape, view toggles | `SurveyInput` + `InputManager`; toggles stay |
| 3353–3379 | `RegionCardHintAt` | controller (layout constants shared with the card) |
| 3381–3510 | wide window, speculation | stays (D8) |
| 3513–3587 | `BuildSiteScene` | stays |
| 3589–3758 | flight constants, `BeginGlobeDescent`, `BeginDescentZoom`, `RunDescentZoom` | interpolation → controller; drawing → each renderer |
| 3760–3805 | `SyncWebCanvasToViewport` | stays (the game's canvas is pinned) |
| 3807–4401 | `UpdateSiteSelect` | state → controller; DEM draw stays; cards → shared |
| 4617–4750 | `DrawGlobeHud`, `DrawHoverChip`, `DrawFeatureArcsInWindow`, `DrawCursorCallout`, `DrawLadderCursor` | `rendermanager_survey.cpp` |
| 4753–5027 | `RenderLadder` (`--ladder`, `--demo`) | stays |
| 5029–5535 | `main`, `--flyshot`, `--siteshot` | stays; the harnesses feed `SurveyInput` |

## Appendix B — Anchor arithmetic (D1)

`TerrainGridCellToLatLon(gx, gy)` is

```
lat = A_lat − (gy − 9.5) · cellDeg
lon = A_lon + (gx − 9.5) · cellDeg / max(0.2, cos A_lat)
```

For the site `(φ, λ)` to be the centre of cell (10, 10), `gx − 9.5 =
gy − 9.5 = 0.5`, so `A_lat = φ + 0.5·cellDeg` and then
`A_lon = λ − 0.5·cellDeg / max(0.2, cos A_lat)`. `A_lat` is solved
first because the longitude term depends on it. With `cellDeg =
0.164893°` the anchor sits 4.6 km NNW of the site (2.5 km north,
2.5 km west at the equator). `SetTerrainAnchor` clamps `A_lat` to ±78°;
D7 refuses founding whenever `|φ + 0.5·cellDeg| > 78°` so the clamp
never engages silently.
