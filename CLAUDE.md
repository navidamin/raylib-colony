# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Read this first: there is ONE level ladder

**Globe → District (200 km) → Site (25 km).** It is how a colony is
sited, and it is the only thing in this repository called a *level*.

| Level | Window | Cursor | Question |
|-------|--------|--------|----------|
| 1 ORBITAL | the globe | 200 km, snapped | Which economy? |
| 2 DISTRICT | 200 km | 25 km, snapped | Which mix? |
| 3 SITE | 25 km | 1.5 km base footprint, free | Which ground? |

- **Authority:** `SURVEY_LEVEL_COUNT = 3` and the table in
  `src/TerrainGen/survey_cursor.{h,cpp}`. `survey_cursor_test` fails if
  it changes.
- **Where it runs:** **in the game** — `View::Orbital` is level 1,
  `View::District` level 2, and `View::Colony` with no colony under it is
  level 3, where a click founds the colony — and in `lunar_map`, the
  instrument, on the same `SiteSelectionController`
  (`src/SiteSelection/`). On Pages: `/ladder/` is the game,
  `/ladder/walk/` walks it with notes, `/lunarmap/` is the instrument. (The wiring was built on
  `lunarmap-wiring-site-selection` and merged here on 2026-09-23; the old
  grid picker and the 100 km Planet view went with it.)
- **Design:** `docs/design/site-selection/README.md`; how it went into
  the game: `docs/design/site-selection/game-integration-plan.md`.

**Not levels**, though every one of these has been taken for one:

- the game's **Colony (25 km) → Sect (5 km) views** once a colony
  exists — where it is managed (the Colony view shares level 3's window,
  which is why the base lands exactly where the cursor was);
- the terrain chain's **steps** (the 100 / 25 / 5 km crops
  `GenerateTerrainChain` walks to build one picture);
- the **crater bench**'s free zoom (`prototypes/planet_visuals/`,
  `/regolith/`) — a design prototype.

Four other ladders lived here until 2026-09-23 — a five-level draft, a
browser descent page, a `--ladder`/`--demo` walker, a flat-map explorer —
and sessions kept building on them. They are gone; `docs/graveyard.md`
entry 10 says what each was. **Do not add a second ladder.** If the
design needs a level changed, change the one table and the tests and
docs that name it.

**Before touching the ladder, the terrain or site-selection code, check
for other branches doing the same:**
`git fetch -q && git branch -r --sort=-committerdate | head`, then look
at anything recent touching `src/TerrainGen`, `src/SiteSelection` or
`tools/lunarmap`. If there is one, tell the user before starting. On
2026-09-23 two branches had diverged on exactly this code for five days
without either knowing, and a bug fixed on one was still live on the
other.

**The Pages site is shared.** Every deploy publishes the whole site, and
the excavation branch deploys too. Each branch builds only its own
folders and carries the other's: this branch owns `/`, `/viewtest/`,
`/ladder/`, `/lunarmap/` and `/regolith/`; the excavation branch
`/playtest/`, `/extraction/` and `/excavation/` (its own copy of the
game). Every page's browser-tab title — and the game's title screen —
names the branch and commit that built it: read it before judging a
playtest. `/ladder/` is the address to hand out for the ladder, because
no other branch builds one. (On 2026-09-24 a playtest of the ladder
turned out to be the excavation branch's month-old game at `/`, which it
used to build too.)

Two checks guard the ladder: `survey_cursor_test` (run by CI's ctest as
`level_ladder`) fails if the ladder's shape changes, and
`tools/lunarmap/web_site_level_test.mjs` (run before every deploy) fails
if the site level comes up without its ground — regolith included — in
a browser at 1656×960. It walks `lunar_map`, the game, and the game with
`?terrain=gpu`, the path a device with a real GPU takes.

---

## Session Catchup Procedure

**IMPORTANT: At the beginning of each session with this project, follow this procedure:**

### 1. Recent Changes Recap
Review recent development activity using git:
```bash
# Get last 5 commits
git log --oneline -5

# Get summary of recent changes
git diff --stat HEAD~5..HEAD

# Identify modified files
git status
```

Present to the user:
- Summary of recent commits (1-2 sentences each)
- Key files modified
- New features/fixes added
- Any notable changes to architecture

### 2. Roadmap Overview
Read and summarize current progress:
```
1. Read ROADMAP_IMMINENT.md for current sprint details
2. Read ROADMAP_OVERALL.md for phase context
3. Present:
   - Current phase and completion percentage
   - Active checkpoints and their status
   - Next 3-5 immediate tasks
   - Any blockers or risks
```

### 3. Work Session
Proceed with implementing tasks or addressing user requests.

### 4. Session Conclusion
When user says "update roadmaps" or at end of session:
```
1. Update ROADMAP_IMMINENT.md:
   - Mark completed tasks as done
   - Update checkpoint progress
   - Add any new issues discovered
   - Update testing checklist

2. Update ROADMAP_OVERALL.md (if phase changed):
   - Update phase completion percentages
   - Move completed items to "Completed" section
   - Adjust timelines if needed

3. Commit roadmap changes:
   git add ROADMAP_*.md
   git commit -m "Update roadmaps: [brief description of progress]"
```

**Example Session Start:**
```
"Hi! Let me catch you up on recent work:

Recent Changes (last 5 commits):
- Engine refactored into manager subsystems (Input, View, Game, Render)
- Terrain rendering enhanced with tile-based system
- BuildNewColony/Sect functionality added
- UI improvements for navigation and selection

Current Phase: PHASE 0 (Foundation & Architecture) - 60% complete

Active Checkpoints:
1. Data-Driven Type System (game_types.toml) - Not started
2. Multiple Simultaneous Active Modules - Not started
3. Storage Capacity System - Not started

Next Immediate Tasks:
- Choose and integrate TOML parser
- Implement GameTypesLoader class
- Create initial game_types.toml
- Refactor Unit::activeModule to support multiple modules

No current blockers.

Ready to work! What would you like to tackle today?"
```

---

## Build System

This project uses CMake with CPM for dependency management. The build is configured for C99 (for raylib) and C++17 (for the game code).

**Build commands:**
```bash
# Configure and build from repository root
cmake -B build
cmake --build build

# Run the game (from repository root)
./build/src/colony_game
```

The executable is generated at `build/src/colony_game`.

**Key dependencies:**
- raylib (3D game library) - automatically fetched via CPM from master branch
- raymath (math utilities)

## Architecture Overview

This is a planet colonization game built with raylib. The architecture follows a manager-based pattern with hierarchical game entities.

### Core Engine Structure

The `Engine` class (src/Engine/) is the main entry point that coordinates four manager subsystems:

1. **InputManager** - Handles user input, double-clicks, dragging, and keyboard shortcuts
2. **ViewManager** - Manages camera control and view transitions between different zoom levels
3. **GameManager** - Owns game state, entity selection, and building placement logic
4. **RenderManager** - Responsible for drawing all views and UI elements

The engine runs a standard game loop: `HandleInput()` → `Update()` → `Draw()`

### Hierarchical Entity System

The game world has a nested hierarchy representing different scales of management:

```
Planet (the Moon: the ground truth, and the colonies on it)
  └─ Colony (founded at a LunarPoint; a 25 km window; resource pooling)
      └─ Sect (a 5 km footprint at its own LunarPoint; units, local storage)
          └─ Unit (production/extraction buildings, modules)
```

**Positions are places on the Moon.** Every colony, sect and unit
carries a `LunarPoint` (lat/lon, `game_structs.h`); nothing carries a
world-unit position. Each view draws in a `LocalFrame`
(`src/TerrainGen/lunar_frame.h`) about what it is looking at — origin at
the colony's centre, +x east, +y south, 1 unit = 50 m — so a sect's
`GetPosition()` is where it sits in *its colony's* frame, and
`GetPoint()` is where it is on the Moon. There is no grid: sects are
placed freely, a footprint (5 km) apart at least, anywhere inside the
colony's window, and two colonies can stand on opposite sides of the
Moon at once.

Each of these can be viewed and interacted with by zooming in (double-click) or out (Escape key).

### View System

The game operates in different views defined in `game_enums.h`:
- **Menu** - Initial menu (not yet implemented)
- **Orbital** - The Moon as a globe (`lunar_globe.h`): level 1 of the ladder. Hover names the region, click claims it; a colony's marker opens it
- **District** - Level 2: a 200 km window with a 25 km snapping cursor
- **Colony** - A colony's 25 km window: its sects and their connections, drawn in the colony's local frame. With no colony under it, it is level 3, the site rung (1.5 km cursor, live verdict, click founds)
- **Sect** - Shows individual units within a settlement
- **Unit** - Detailed view of a specific production unit and its modules

View transitions are handled by `ViewManager::SwitchTo*View()` methods which adjust camera zoom and target.

### Site Selection System

A colony is founded through **the level ladder** (top of this file): the
globe (`View::Orbital`, hover names the region, click claims it), the
200 km district (`View::District`, a 25 km snapping cursor: which mix of
ground), and the 25 km site window (`View::Colony` with no colony under
it: a 1.5 km cursor that is the base's own footprint, judged live by
`EvaluateSite` + `JudgeSite` from real LOLA elevation). A green click
calls `GameManager::FoundColony(point, windowCentre, &region)`: the
first sect stands at the point, the colony's window is the site window,
and the archetype is the claimed region's. The state machine is
`SiteSelectionController` (src/SiteSelection, shared with `lunar_map`);
`SurveyFlow` (src/Engine) runs it a frame at a time for the Engine, the
harness and the preview tool; `rendermanager_survey.cpp` draws it. The
levels' ground is the terrain chain's (a window keyed by place and span in
the terrain cache); the DEM only judges. Claims inside the polar cap
(`SITE_POLAR_FRAME_LAT_DEG`) are refused until a tangent-plane frame
exists (plan D7). The old grid picker (`View::SITE_SELECTION`) and the
100 km Planet view are gone.
- Each place is classified with a `SiteArchetype` (MARE_INDUSTRIAL, HIGHLAND_CONSTRUCTION, POLAR_VOLATILE, KREEP_SCIENTIFIC, LAVA_TUBE, MIXED) from its region's real composition
- `FoundSect(point)` refuses a sect inside another colony's territory, closer than `SECT_MIN_SPACING_KM` to any sect, or with its footprint outside the `COLONY_WINDOW_KM` window
- Sect placement within a Colony shows a resource preview tooltip (Ctrl+hover)

### Resource System

Resources are managed at multiple levels:

**ResourceManager** (src/ResourceManager/):
- The ground truth is a function of location: `GroundAt(point)` generates the resources under one sect footprint from the region's real composition (`IdentifyRegion`), a hashed 20 km variation and the depth-bias table, on first ask, and remembers it (keyed by `LunarQuantise(point)`); one world seed per Moon
- Absolute quantities (hundreds to thousands) per element; the prospecting chain normalises them to composition fractions
- Tracks resource depletion per place as units extract materials
- ResourceTypes defined in `resource_types.h` include: ENERGY, H2, O2, C, Fe, Si, Ti, Al, Ca, WATER, FOOD, BIOFUEL, SCIENCE, MANPOWER, MACHINERY, ELECTRONICS, ALLOYS, CONSTRUCTION_MATERIALS
- `SurveyAt(point)` gives `OrbitalSurveyData` (composition, hydrogen signal, solar illumination, real terrain slope from LOLA, earth visibility); `ArchetypeAt(point)` the region's `SiteArchetype`
- `colony_inspect LAT LON` dumps all of it for a place

**ResourceDescriptor table** (`resource_types.h`):
- `ResourceDescriptor` struct is the single source of truth for each resource's name, color, category (`SINGULAR` or `TYPED`), and subtypes
- `GetResourceDescriptors()` returns the full table; `GetResourceDescriptor(type)` looks up one entry
- `ResourceTypeToString`, `GetResourceCategory`, and `ResourceUtils::*` are thin wrappers around the descriptor lookup

**Resource flow:**
- The ground holds the natural resources (H2, O2, C, Fe, Si, Ti, Al, Ca)
- Sects have local storage for processed/extracted resources
- Units consume resources from sect storage during production cycles
- Production costs defined in `game_constants.h` (e.g., EXTRACTION_PRODUCTION_COSTS, FARMING_PRODUCTION_COSTS)
- Sects push/pull typed resources to/from colony reserves via `Colony::ReceiveTypedSurplus()` / `Colony::ProvideTypedResource()`
- Colony auto-balance and deficit transport iterate descriptors via `GetResourceDescriptors()` (not raw `static_cast<int>` loops)

### Time Management

**TimeManager** (src/TimeManager/):
- Manages game time progression with configurable time scale
- Tick-based system where 20 ticks = 1 game day (TICKS_PER_DAY)
- Each tick is 1 second (TICK_DURATION)
- Handles pause/resume functionality
- Units track their production cycles and construction timers relative to game time

### Unit Module System

Units have a modular upgrade system where each unit type has specialized named modules. Each `UnitModule` defines:
- `moduleType` (e.g., "PROSPECTING", "EXCAVATION", "BENEFICIATION")
- `tier` (0-3) with tier-specific stats, dependencies, and energy requirements
- `tierDependencies` (tech strings checked against `UnlockRegistry`)
- Production/consumption rates, efficiency, upgrade costs per tier

**Extraction unit modules** (5 specialized):
1. **Prospecting** - LIBS scanning, site marking, scan history (`ScanResult` struct), survey progress (0-100%)
2. **Excavation** - Excavator fleet management (`Excavator` struct), depth/rate control, wear
3. **Beneficiation** - Separation chain (`SeparationNode` structs: SIZE_SORT, MAGNETIC, ELECTROSTATIC, THERMAL, MRE, DIRECT_OUTPUT)
4. **Operations** - Efficiency modifier (tier 0=0.85 penalty, tier 3=1.2 bonus)
5. **Directives** - Autonomous control (PRIORITIZE, MAXIMIZE, CONSERVE, EXPLORATION_MODE, EMERGENCY_HARVEST, THERMAL_SYNC)

**Other unit types** have 5 stub-named modules each, using generic production logic. There are **eight** unit types in total — `Sect::CreateInitialUnits` (`src/Sect/sect.cpp`) is the authoritative list, not the `UnitType` enum, since units are constructed from strings: Extraction, Farming, Energy, Manufacture, Research, Construction, Transport, Communication. All 40 modules are reachable through the module menu; only Extraction's five have bespoke panels.

**Extraction pipeline** (`ProcessExtraction()`):
1. Survey-gated efficiency: `scanMultiplier = 0.35 + 0.65 × surveyProgress` (+ 0.15 if marked, × objective bonus). Each scan adds progress via diminishing returns formula.
2. Excavation stage: base rate × scanMultiplier × operations modifier × directive modifier × excavator count
3. Beneficiation stage: raw regolith processed through separation chain nodes
4. Storage stage: processed resources added to sect storage

**Tier upgrades** (`UpgradeModuleTier()`): Check `UnlockRegistry` for required techs, deduct resource costs, increment tier.

### Terrain Generation (src/TerrainGen/)

The planet's surface is **generated from real lunar imagery**, not from
tile art. `terrain_synthesis.{h,cpp}` amplifies the shipped LROC WAC
mosaic (`src/assets/planet/wac_global.jpg`): the real imagery supplies
every landform, and below its ~1.3 km/px resolution floor the synthesizer
re-sharpens, relights, and builds a **world-anchored regolith** — a
fractal residual, a clustered crater population, clast bands, grit and
albedo mottle, all inside `TextureModulate`, concentrated by a roughness
field measured off the mosaic itself (`BuildRoughField`) so a mare gets
less of it than crater ejecta — Procellarum 0.90x Mare Imbrium, Tycho
4.29x. Deterministic per location
— the same coordinates always regenerate the same ground, from any
window that frames them, so nothing is stored. `detail_noise.h` holds
the one lattice: `terrain_synthesis.cpp` and `lola_dem.cpp` include it,
and `terrain_gpu.cpp` reproduces the same hash chain in GLSL — change
the header and the shader must change with it.

**Scale system** (anchored on the sect being 5 km across):

| | |
|---|---|
| 1 local unit | 50 m (`LOCAL_UNITS_PER_KM = 20`) |
| sect footprint (sect + units) | 5 km = 100 units (`TERRAIN_CELL_KM`) |
| COLONY view | 25 km window = 500 units (`COLONY_WINDOW_KM`) |
| SECT view | 5 km |
| ORBITAL view | the whole Moon, as a globe |

**One chain feeds the views.** `GenerateTerrainChain` walks
100 → 25 → 5 km about one `LunarPoint`, each step the centre crop of
the one above: step 1 is the Colony window, 2 the Sect (step 0, the
100 km, was the retired Planet view's backdrop). (These are the chain's
*steps* — not levels; the code calls them "levels" internally, for
historical reasons.) Because they are registered to each other by
construction, zooming approaches the same ground instead of cutting to a
different scene. `RenderManager` caches one chain per place
(`EnsureTerrainAt`, keyed by `LunarQuantise`) — the colony being looked
at plus its sects, which `RequestTerrainAt` builds before they are asked
for. The ladder's district and site levels draw single windows from the
same cache, keyed by place and span.

**Two synthesizers, one chosen at startup.** `terrain_gpu.{h,cpp}` runs
the same chain as fragment-shader passes (both GLSL 330 and ES 100, so
it is the path the browser and phone take). `GetTerrainPath()` decides
once: `COLONY_TERRAIN=cpu|gpu` overrides; every platform — the browser
included, since WebGL there may be a software rasterizer — times one
512 px chain and picks
GPU at 1024 (≤ 12 ms), GPU at 512 (≤ 40 ms) or the threaded CPU path
(a software rasterizer such as WSL's llvmpipe); in a browser
`?terrain=cpu|gpu` is the same override. `COLONY_TERRAIN_RES` forces the
GPU resolution. The GPU chain is *fused* — no float
textures, the height field is never stored — and its noise is hashed
rather than drawn from the CPU's xorshift stream, so it has the same
texture statistics without the same pixels. `terrain_probe` builds a
location both ways and reports the difference; run it after touching
either synthesizer (CPU vs GPU currently 3.4 / 7.5 / 3.5 out of 255).

**The shader cannot do the regolith on WebGL1.** GLSL ES 1.00 has no
`uint`, no bitwise operators, and a `highp int` guaranteed only to 2^16
where the lattice indices reach millions. So **who builds a chain is one
question, `TerrainChainOnGpu()`** — the GPU path *and* shaders that can
run the regolith — and every consumer asks it: the game's terrain cache
and `lunar_map`'s layer. On WebGL1 the answer is the CPU, sized to
`TERRAIN_CHAIN_BUDGET_MS` by a measured cost model
(`TerrainCpuChainResFor`), and nothing is prefetched, since a browser
has no threads to hide it on. Asking anything else — the path alone, the
platform — is how the site level came up grey in `lunar_map` and
craterless in the game on 2026-09-23. WebGL2 would end the split.

**Real coordinates.** Everything is a real lat/lon: a click on the
globe is inverted by `OrbitalPickToLatLon`, and the chain is built for
that point. Elevation/slope ground truth from NASA's LOLA model is read
in-game through `GetLunarDem()` (`lunar_dem_shared.h`; the global model
ships in `src/assets/planet/lola/`, the optional SLDEM overlays stay in
`prototypes/planet_visuals/data/lola/`); `elevation.py` there is the
Python original.

**Occupied sites** get `TerrainSiteDisturbance`: the natural ground is
levelled off (relief and imagery contrast damped toward local means,
partially — not a platform) and then worked with undulations plus
alterations around each dome. A graded construction platform was tried
and rejected; see SITE_SYNTHESIS.md before re-proposing one.

Design record: `prototypes/planet_visuals/SITE_SYNTHESIS.md`. The same
chain also runs in JavaScript in
`prototypes/planet_visuals/regolith_craters.html` — an interactive bench
that puts the mosaic beside what the chain made of it, over eight real
regions and a free 200 km → 500 m zoom. Its regolith stack — craters
included — **is** the shipped chain's: it was designed there and ported
to `terrain_synthesis.cpp`, and the two agree to RMS 0.71 out of 255 at
all three steps. (An older, separate crater layer was removed from the
chain on 2026-08-13; what is there now arrived with the port.) It
carries its own WAC blocks, renders headlessly through
`regolith_craters_render.mjs`, and its port is checked against
`colony_preview`'s output rather than assumed.

### Unlock Registry

`UnlockRegistry` (src/UnlockRegistry/unlock_registry.h) is a header-only singleton that stubs the tech dependency system until Research units are fully implemented. Contains 14 available techs (Spectroscopy, Geophysics, SwarmAI, etc.). Debug key F5 cycles through unlocks.

### Extraction UI Font Scaling

The extraction unit view uses `Exo 2` (Regular + Bold) loaded at 48pt texture size with bilinear filtering. All `DrawTextEx`/`MeasureTextEx` size parameters in extraction view methods are wrapped with `FS()` — a simple multiplier returning `baseSize * 1.30f` (XL preset). This keeps text comfortably readable at the dark-themed panel layout. `FS()` is defined in `RenderManager` and only applies to extraction view methods, not site selection or other views.

## Visual Testing Instruments

Never claim a visual result without rendering it. `preview` and
`viewtest` drive the game's real `RenderManager`, so what they export is
what the game draws; `lunar_map` is the level ladder itself, so what it
exports is what the ladder shows:

| Tool | Use |
|------|-----|
| `tools/preview/preview.sh` | one view in isolation (`--view orbital\|colony\|sect`, `--pick LAT,LON`) |
| `tools/viewtest/viewtest.sh` | **the game, walked:** the level ladder through the game's own `SurveyFlow` (Globe → District → Site), then the founded colony's Colony → Sect views, with per-view issue notes; `--pick LAT,LON --aim DX,DY` scripts the claim, the dive, the founding and a second colony |
| `tools/lunarmap/lunarmap.sh` | **the level ladder as an instrument**, on the same `SiteSelectionController` as the game: bare `lunar_map` plays Globe → District → Site over the LOLA DEM; `--chain` lays the synthesizer over it, `--siteshot` renders every step. Deploys to `/lunarmap/`. |

All three need software GL: `LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
xvfb-run -a ...` (the scripts apply it). `colony_viewtest` also deploys
to `/viewtest/` on GitHub Pages for phone/tablet playtesting — see
`tools/viewtest/README.md`.

## Coding Conventions

**Critical: Follow CONVENTIONS.md strictly.** This project uses C-style naming conventions:

- **Functions**: TitleCase (e.g., `InitWindow()`, `CalculateProduction()`)
- **Variables/members**: lowerCase (e.g., `screenWidth`, `resourceManager`)
- **Structs/Classes**: TitleCase (e.g., `Colony`, `ResourceManager`)
- **Enums**: TitleCase with ALL_CAPS members (e.g., `enum class View`, `View::Colony`)
- **Constants/Defines**: ALL_CAPS (e.g., `COLONY_WINDOW_KM`, `SECT_CORE_RADIUS`)
- **float literals**: Always use `.0f` suffix (e.g., `1.0f`, `0.5f`)
- **Braces**: Always aligned opening/closing on separate lines
- **Spacing**: 4 spaces (no tabs), spaces around `+/-` but not `*//`
- **Control flow**: Space after keyword (e.g., `if (condition)`, `while (!done)`)
- **File/directory names**: snake_case

## Common Patterns

**Adding a new unit type:**
1. Add enum value to `UnitType` in `game_enums.h`
2. Implement production logic in `Unit::Process*()` methods
3. Define production costs in `game_constants.h`
4. Create module definitions in `Unit::InitializeModules()`

**Adding a new module panel:** (see [`docs/guides/ui-panels.md`](docs/guides/ui-panels.md))
1. Add `DrawFooPanel(Unit*, int x, int y, int w, int h)` to `RenderManager`
2. Dispatch to it by `moduleType` in `DrawExtractionModuleCenter`
3. Add the module's icon to the `ExtIcon` enum and `ExtModuleIcon()`
4. Build the layout from the existing widget helpers and design tokens
5. Keep persistent UI state on the module's facade, not the renderer
6. Render every state (`--tier`, `--state`, `--energy`) and **look at the PNGs**
7. Check against [`docs/guides/feature-completeness.md`](docs/guides/feature-completeness.md)

*Every unit type now draws through the shared modular chrome
(`DrawModularUnitView`). Modules without a bespoke centre panel fall back to
`DrawGenericModulePanel`, which renders the module's real data (tier arc,
throughput, energy, tech deps) and marks itself PRELIMINARY. Replacing that
fallback with a real panel is step 2 above — the legacy `unit->DrawInUnitView()`
path in `unit_ui.cpp` is no longer reached.*

**Building a new gameplay module:** (see [`docs/guides/module-architecture.md`](docs/guides/module-architecture.md))
1. `src/<Module>/` with constants / types / pure-logic engines / facade
2. Name units at data boundaries (quantity vs fraction vs rate)
3. Tier capability in constant tables, queried via `Can*()` methods
4. Keep the contract with the rest of the game narrow
5. Add sources to `COLONY_CORE_SOURCES` in `src/CMakeLists.txt`

**Adding a new view:**
1. Add enum to `View` in `game_enums.h`
2. Create `RenderManager::Draw*View()` method
3. Add `ViewManager::SwitchTo*View()` method
4. Handle camera setup and input in respective managers

**Adding a new resource type:**
1. Add enum value to `ResourceType` in `resource_types.h`
2. Add a `ResourceDescriptor` entry in the `GetResourceDescriptors()` table (name, color, category, subtypes)
3. If `TYPED`: populate the `subtypes` vector with valid subtype strings
4. Initialize storage in `Sect` constructor and `Colony` constructor
5. All wrapper functions (`ResourceTypeToString`, `GetResourceCategory`, `ResourceUtils::*`) automatically work via descriptor lookup

**Working with places and frames:**
- A place is a `LunarPoint`; ask the ground about it (`ResourceManager::GroundAt`, `SurveyAt`) and the terrain about it (`EnsureTerrainAt`)
- Distances and offsets in km: `LunarDistanceKm`, `LunarOffsetKm`, `LunarOffsetPoint` (`lunar_frame.h`)
- Drawing happens in a view's `LocalFrame`: `ToLocal(point)` / `FromLocal(local)`, 1 unit = 50 m; the Colony view's frame is `colony->GetFrame()`
- A sect is SECT_CORE_RADIUS * 2 = 100 units across; the colony window is `COLONY_WINDOW_KM * LOCAL_UNITS_PER_KM` = 500 units
- Never store a local position as if it were the world: it is only meaningful in the frame that produced it

## File Organization

- `src/` - Main source directory
- `src/Engine/` - Core engine managers (Input, View, Game, Render)
- `src/Colony/`, `src/Sect/`, `src/Unit/`, `src/Planet/` - Game entities
- `src/ResourceManager/` - Resource generation, tracking, and orbital survey data
- `src/TimeManager/` - Game time and production scheduling
- `src/TerrainGen/` - Real-imagery terrain synthesis (see Terrain Generation above)
- `src/UnlockRegistry/` - Stub tech dependency system (header-only singleton)
- `src/Unit/separation_node.h` - Beneficiation separation node types and processing
- `src/InquiryManager/` - (Purpose unclear from headers, investigate if modifying)
- `assets/` - Game assets (textures, etc.)
- `game_*.h` - Shared definitions (enums, structs, constants)

**Note:** CMakeLists.txt in src/ may be incomplete - not all .cpp files are listed in target_sources. Verify compilation if adding new files.

## Development Guides

General instructions distilled from building the prospecting module and the
extraction UI. Read the relevant one **before** starting, not after.

| Guide | Read when |
|-------|-----------|
| [`docs/guides/ui-panels.md`](docs/guides/ui-panels.md) | Building or restyling any module panel — design tokens, semantic colours, widget helpers, control semantics, touch feedback, IMGUI discipline |
| [`docs/guides/module-architecture.md`](docs/guides/module-architecture.md) | Starting a new module or unit — **a 13-aspect design brief to work through before writing code** (loop, contract, multi-scale control, tier arc, economy, friction, decision texture, scale, AI hook), then the implementation shape: engine/facade structure, **declaring units at data boundaries**, tier tables, hero visuals |
| [`docs/guides/feature-completeness.md`](docs/guides/feature-completeness.md) | You think a feature is done — the six questions that catch "engine-implemented but not player-reachable" |
| [`docs/dev-workflow.md`](docs/dev-workflow.md) | Any UI or gameplay work — the testing instruments and the working loop |
| [`docs/web-deploy-mobile.md`](docs/web-deploy-mobile.md) | Touching `minshell.html`, the Pages deploy, or the phone build |
| [`docs/graveyard.md`](docs/graveyard.md) | Before rebuilding something that feels missing, or before deleting something that looks unused — what was removed on purpose, with the numbers needed to reproduce it |

Three rules worth stating up front, each learned expensively:

1. **Never claim a visual result without rendering it.** `preview.sh` takes
   ~5 seconds.
2. **When a value looks wrong, dump the data before theorising.**
   `colony_inspect` found in one step what reading the generator twice missed.
3. **Engine-implemented is not player-reachable.** Correct code that no input
   path reaches is not a finished feature.

## Dev Workflow & Testing Instruments

**Read `docs/dev-workflow.md` before starting UI or gameplay work.** The
repo has purpose-built instruments so changes can be seen and verified
without a display:

| Tool | Use it for |
|------|-----------|
| `tools/preview/preview.sh` | Render any module panel to a PNG headlessly (~5s). Real RenderManager, fixed world seed, so screenshots are faithful and reproducible. |
| `tools/playtest/` | Interactive prospecting sandbox; also builds for Web and deploys to `/playtest/` for phone testing. |
| `tools/sectwalk/` | Walk the Sect view by hand — open every unit and all 40 modules in sequence. The only harness that covers the whole tree. |
| `tools/inspect/` | Dump real generated data (`colony_inspect`). Use when a value looks wrong — **before** theorising about the cause. |
| `tools/lunarmap/` | `lunar_map`: **the level ladder** on real lunar coordinates, Globe → District (200 km) → Site (25 km). The only harness for it; `--help` lists the flags. |
| `tools/surveycursor/` | `survey_cursor_test`: headless self-test for the level ladder — its count, spans and geometry. No GL, no DEM — run it after touching `survey_cursor.*`. |
| `tools/terrainprobe/` | `terrain_probe`: one location's terrain chain built on the GPU and the CPU, timed, every level as PNG, per-level statistics and the mean difference between the two. |
| `tools/shell-test/` | Canvas-fit regression test for `minshell.html`. Run after any shell change. |

The working loop: **change → render preview → look at the PNG and iterate
→ show the images → `--all` + build all targets → commit/push (deploy is
automatic) → playtest on device.** Never claim a visual result without
rendering it first.

## Web Builds & Mobile

Before touching `src/minshell.html`, the Pages deploy workflow, or
anything about the web/phone builds, read `docs/web-deploy-mobile.md` —
it documents the three-layer canvas sizing problem (CSS size vs
framebuffer attributes vs game render size), the SHELL v4 enforcer that
fixes it, the on-page diagnostic badge, and the deploy/caching gotchas.

## Design Documents

Module-specific design planning lives in `docs/design/<module-name>/`. Each module has a README.md that serves as the entry point and table of contents.

**Auto-context rule:** When working on a module's code, read its design README first:

| Module | Design Directory | Context Trigger |
|--------|-----------------|-----------------|
| Prospecting | `docs/design/prospecting/README.md` | Working on prospecting methods in `unit.cpp`, `DrawProspectingPanel` in `rendermanager.cpp`, or prospecting input handling |
| Sect View | `docs/design/sect-view/README.md` | Working on `Sect::DrawInSectView` and its visual helpers in `sect.cpp`, `DrawSectView` in `rendermanager.cpp`, or sect view input handling |
| Site Selection | `docs/design/site-selection/README.md` | Working on the level ladder: `src/SiteSelection/*`, `src/Engine/survey_flow.*`, `rendermanager_survey.cpp`, `survey_cursor.*`, `lunar_map`, the Orbital / District views, or founding in `gamemanager.cpp` |
| Core (habitat/command) | `docs/design/core/README.md` | Working on `Sect::core`, crew or life-support logic, the centre dome in `Sect::DrawInSectView`, or Core module panels |

See `docs/design/README.md` for the full planning method explanation.
