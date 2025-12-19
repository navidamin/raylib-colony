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
- **Colony** - Shows all sects within a colony and their connections
- **Sect** - Shows individual units within a settlement
- **Unit** - Detailed view of a specific production unit and its modules

View transitions are handled by `ViewManager::SwitchTo*View()` methods which adjust camera zoom and target.

### Resource System

Resources are managed at multiple levels:

**ResourceManager** (src/ResourceManager/):
- Generates procedural resource distribution across the planet grid using cluster-based generation
- Each grid cell has resource abundances (0.0-1.0) for different ResourceTypes
- Tracks resource depletion as units extract materials
- ResourceTypes defined in `resource_types.h` include: H2, O2, C, Fe, Si, WATER, FOOD, ENERGY, SCIENCE, MANPOWER

**Resource flow:**
- Planet grid stores natural resources (H2, O2, C, Fe, Si)
- Sects have local storage for processed/extracted resources
- Units consume resources from sect storage during production cycles
- Production costs defined in `game_constants.h` (e.g., EXTRACTION_PRODUCTION_COSTS, FARMING_PRODUCTION_COSTS)

### Time Management

**TimeManager** (src/TimeManager/):
- Manages game time progression with configurable time scale
- Tick-based system where 20 ticks = 1 game day (TICKS_PER_DAY)
- Each tick is 1 second (TICK_DURATION)
- Handles pause/resume functionality
- Units track their production cycles and construction timers relative to game time

### Unit Module System

Units have a modular upgrade system where each unit type can have multiple modules. Modules define:
- Production/consumption rates for various resources
- Efficiency and current level
- Upgrade costs for next level
- Special enhancements at different levels

Production is processed per-module with costs deducted before outputs are generated.

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

**Working with the grid system:**
- Planet uses a 20x20 grid (PLANET_SIZE)
- Each cell is SECT_CORE_RADIUS * 2 units wide (100 units)
- Conversion functions: `WorldToGrid()` / `GridToWorld()` in relevant classes
- World coordinates are used for rendering, grid coordinates for resource lookup

## File Organization

- `src/` - Main source directory
- `src/Engine/` - Core engine managers (Input, View, Game, Render)
- `src/Colony/`, `src/Sect/`, `src/Unit/`, `src/Planet/` - Game entities
- `src/ResourceManager/` - Resource generation and tracking
- `src/TimeManager/` - Game time and production scheduling
- `src/InquiryManager/` - (Purpose unclear from headers, investigate if modifying)
- `assets/` - Game assets (textures, etc.)
- `game_*.h` - Shared definitions (enums, structs, constants)

**Note:** CMakeLists.txt in src/ may be incomplete - not all .cpp files are listed in target_sources. Verify compilation if adding new files.
