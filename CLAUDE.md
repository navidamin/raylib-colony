# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
Planet (20x20 grid, resource generation)
  └─ Colony (collection of sects, resource pooling)
      └─ Sect (settlement with units, local storage)
          └─ Unit (production/extraction buildings, modules)
```

Each level can be viewed and interacted with by zooming in (double-click) or out (Escape key).

### View System

The game operates in different views defined in `game_enums.h`:
- **Menu** - Initial menu (not yet implemented)
- **Planet** - Strategic view showing all colonies on the planet surface
- **Site_Selection** - Orbital survey view for informed colony placement (instrument panels: GRS, Neutron, Thermal, Site Assessment)
- **Colony** - Shows all sects within a colony and their connections
- **Sect** - Shows individual units within a settlement
- **Unit** - Detailed view of a specific production unit and its modules

View transitions are handled by `ViewManager::SwitchTo*View()` methods which adjust camera zoom and target.

### Site Selection System

Colony placement uses an informed site selection flow (src/Engine/gamemanager.cpp):
- Ctrl+click in Planet view enters `View::SITE_SELECTION` instead of placing immediately
- `DrawSiteSelectionView()` renders orbital instrument panels (GRS bar charts, neutron spectrometer, thermal mapper)
- Each grid cell is classified with a `SiteArchetype` (MARE_INDUSTRIAL, HIGHLAND_CONSTRUCTION, POLAR_VOLATILE, KREEP_SCIENTIFIC, LAVA_TUBE, MIXED)
- Confirming a site creates a Colony with archetype-specific bonus multipliers
- Sect placement within a Colony shows a resource preview tooltip (Ctrl+hover)

### Resource System

Resources are managed at multiple levels:

**ResourceManager** (src/ResourceManager/):
- Generates procedural resource distribution across the planet grid using cluster-based generation
- Each grid cell has resource abundances (0.0-1.0) for different ResourceTypes
- Tracks resource depletion as units extract materials
- ResourceTypes defined in `resource_types.h` include: ENERGY, H2, O2, C, Fe, Si, Ti, Al, Ca, WATER, FOOD, BIOFUEL, SCIENCE, MANPOWER, MACHINERY, ELECTRONICS, ALLOYS, CONSTRUCTION_MATERIALS
- Generates `OrbitalSurveyData` per grid cell (elemental composition, hydrogen signal, solar illumination, terrain slope, earth visibility)
- Classifies grid cells into `SiteArchetype` based on composition thresholds

**ResourceDescriptor table** (`resource_types.h`):
- `ResourceDescriptor` struct is the single source of truth for each resource's name, color, category (`SINGULAR` or `TYPED`), and subtypes
- `GetResourceDescriptors()` returns the full table; `GetResourceDescriptor(type)` looks up one entry
- `ResourceTypeToString`, `GetResourceCategory`, and `ResourceUtils::*` are thin wrappers around the descriptor lookup

**Resource flow:**
- Planet grid stores natural resources (H2, O2, C, Fe, Si, Ti, Al, Ca)
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

**Other unit types** have 5 stub-named modules each (Farming, Energy, Manufacture, Research) using generic production logic.

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
re-sharpens, relights and adds regolith grain. Deterministic per
location — the same coordinates always regenerate the same ground, so
nothing is stored.

**Scale system** (anchored on the sect being 5 km across):

| | |
|---|---|
| 1 world unit | 50 m |
| grid cell (sect + units) | 5 km = 100 world units |
| PLANET view | 20x20 cells = 100 km |
| COLONY view | 5x5 cells = 25 km |
| SECT view | 1 cell = 5 km |

**One chain feeds three views.** `GenerateTerrainChain` walks
100 → 25 → 5 km, each level the centre crop of the one above, and emits
all three: level 0 is the Planet backdrop, 1 the Colony, 2 the Sect.
Because they are registered to each other by construction, zooming
approaches the same ground instead of cutting to a different scene.
`RenderManager` caches one chain per grid cell.

**Real coordinates.** The 20x20 grid is anchored on a real lat/lon
(`SetTerrainAnchor`, settable — clicking the orbital disc re-anchors the
playfield there via `OrbitalPickToLatLon`). `TerrainGridCellToLatLon`
gives any cell its true coordinates. Elevation/slope ground truth from
NASA's LOLA model lives in `prototypes/planet_visuals/elevation.py`.

**Occupied sites** get `TerrainSiteDisturbance`: the natural ground is
levelled off (relief and imagery contrast damped toward local means,
partially — not a platform) and then worked with undulations plus
alterations around each dome. A graded construction platform was tried
and rejected; see SITE_SYNTHESIS.md before re-proposing one.

Design record: `prototypes/planet_visuals/SITE_SYNTHESIS.md`.

### Unlock Registry

`UnlockRegistry` (src/UnlockRegistry/unlock_registry.h) is a header-only singleton that stubs the tech dependency system until Research units are fully implemented. Contains 14 available techs (Spectroscopy, Geophysics, SwarmAI, etc.). Debug key F5 cycles through unlocks.

### Extraction UI Font Scaling

The extraction unit view uses `Exo 2` (Regular + Bold) loaded at 48pt texture size with bilinear filtering. All `DrawTextEx`/`MeasureTextEx` size parameters in extraction view methods are wrapped with `FS()` — a simple multiplier returning `baseSize * 1.30f` (XL preset). This keeps text comfortably readable at the dark-themed panel layout. `FS()` is defined in `RenderManager` and only applies to extraction view methods, not site selection or other views.

## Visual Testing Instruments

Never claim a visual result without rendering it. Two headless tools
drive the real `RenderManager`, so what they export is what the game
draws:

| Tool | Use |
|------|-----|
| `tools/preview/preview.sh` | one view in isolation (`--view orbital\|planet\|sect`, `--cell X,Y`) |
| `tools/viewtest/viewtest.sh` | the whole Orbital → Planet → Colony → Sect descent, with per-view issue notes; `--pick LAT,LON` lands it anywhere on the moon |

Both need software GL: `LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
xvfb-run -a ...` (the scripts apply it). `colony_viewtest` also deploys
to `/viewtest/` on GitHub Pages for phone/tablet playtesting — see
`tools/viewtest/README.md`.

## Coding Conventions

**Critical: Follow CONVENTIONS.md strictly.** This project uses C-style naming conventions:

- **Functions**: TitleCase (e.g., `InitWindow()`, `CalculateProduction()`)
- **Variables/members**: lowerCase (e.g., `screenWidth`, `resourceManager`)
- **Structs/Classes**: TitleCase (e.g., `Colony`, `ResourceManager`)
- **Enums**: TitleCase with ALL_CAPS members (e.g., `enum class View`, `View::Planet`)
- **Constants/Defines**: ALL_CAPS (e.g., `PLANET_SIZE`, `SECT_CORE_RADIUS`)
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

**Working with the grid system:**
- Planet uses a 20x20 grid (PLANET_SIZE)
- Each cell is SECT_CORE_RADIUS * 2 units wide (100 units)
- Conversion functions: `WorldToGrid()` / `GridToWorld()` in relevant classes
- World coordinates are used for rendering, grid coordinates for resource lookup

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

## Design Documents

Module-specific design planning lives in `docs/design/<module-name>/`. Each module has a README.md that serves as the entry point and table of contents.

**Auto-context rule:** When working on a module's code, read its design README first:

| Module | Design Directory | Context Trigger |
|--------|-----------------|-----------------|
| Prospecting | `docs/design/prospecting/README.md` | Working on prospecting methods in `unit.cpp`, `DrawProspectingPanel` in `rendermanager.cpp`, or prospecting input handling |

See `docs/design/README.md` for the full planning method explanation.
