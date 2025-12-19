# ROADMAP_OVERALL.md

## Vision
Create a scalable, data-driven colony management game with deep resource logistics, modular production systems, and emergent strategic gameplay across multiple hierarchical scales (Planet → Colony → Sect → Unit → Module).

---

## Overall Development Phases

### **PHASE 0: Foundation & Architecture** ⚡ CURRENT
**Status:** IN PROGRESS (75% complete)
**Timeline:** Started - Week 6
**Last Updated:** 2025-11-25

#### Completed ✅
- ✅ Basic game loop and view system (Planet/Colony/Sect/Unit views)
- ✅ Camera control and zoom transitions
- ✅ Entity hierarchy (Planet → Colony → Sect → Unit)
- ✅ Module system with production/consumption rates
- ✅ TimeManager for tick-based progression
- ✅ ResourceManager for planet grid resources
- ✅ Engine refactoring into manager subsystems (InputManager, ViewManager, GameManager, RenderManager)
- ✅ Basic terrain rendering with tiles
- ✅ BuildNewColony and BuildNewSect functionality
- ✅ Double-click navigation between views
- ✅ **Data-driven type system (game_types.toml)** - *Completed 2025-11-09*
  - ✅ tomlplusplus v3.4.0 integration via CPM
  - ✅ GameTypesLoader singleton implementation
  - ✅ 11 singular resources, 2 typed resources, 5 unit types, 14 modules defined
  - ✅ Asset directory structure for units and modules
- ✅ **Build system cleanup** - *Completed 2025-11-09*
  - ✅ Fixed multiple definition errors
  - ✅ All source files properly listed in CMakeLists.txt
  - ✅ Successful compilation and linking
- ✅ Resource flow architecture design (planning completed, implementation pending in Phase 1)

#### In Progress 🔄
- 🔄 **Graphics enhancement - texture-based rendering** (Checkpoint 0 - NEW 2025-11-25)
  - Replace circle primitives with textures in Sect view
  - Dome_off.png for central dome
  - Unit-specific textures for buildings (Extraction, Farming, Energy, Water, Manufacturing)
  - Proof of concept with graceful fallback for missing textures
- 🔄 Storage capacity system (design completed, implementation in Phase 1)
- 🔄 Production priority system (basic structure exists, needs enhancement in Phase 1)

#### Remaining Tasks 📋
- 📋 Multiple simultaneous active modules per unit (next immediate task)
- 📋 Strategy-based resource distribution (Energy/Manpower) - Phase 3
- 📋 Production strategy pattern implementation - Phase 3

**Exit Criteria Progress:**
- ✅ All manager subsystems fully functional
- ✅ Data-driven type loading system operational
- 📋 Multiple modules can run simultaneously (next task)
- 📋 Basic resource flow working (Unit → Sect → Colony) - Phase 1

---

### **PHASE 1: Core Resource System**
**Status:** PLANNED
**Timeline:** Week 7-10

#### Objectives
- Implement hierarchical storage with capacity limits
- Create push/pull resource flow mechanisms
- Add typed resources (Machinery, Electronics, Alloys)
- Implement graceful degradation for resource scarcity
- Add sect-level direct generation (population, ambient solar)

#### Major Components

**1.1 Storage & Capacity System** (Week 7)
- [ ] Add storage capacity to Sect class
- [ ] Add strategic reserves to Colony class
- [ ] Implement storage overflow handling
- [ ] Create unit-level overflow buffers
- [ ] Add storage upgrade mechanics

**1.2 Resource Classification** (Week 7-8)
- [ ] Extend ResourceType enum with Tier 2/3 resources
- [ ] Implement singular vs typed resource system
- [ ] Create TypedResource struct with subtypes
- [ ] Add resource category/priority enums
- [ ] Update resource visualization for new types

**1.3 Flow Mechanisms** (Week 8-9)
- [ ] Implement Unit→Sect production deposit
- [ ] Implement Sect→Colony surplus push (dynamic timing)
- [ ] Implement Colony→Sect deficit pull
- [ ] Add transport time calculations
- [ ] Create flow event system

**1.4 Consumption & Distribution** (Week 9-10)
- [ ] Implement priority-based allocation system
- [ ] Add graceful degradation for partial resources
- [ ] Create ResourceAllocator class
- [ ] Add sect-level population resource generation
- [ ] Add ambient energy generation at sect level

**Exit Criteria:**
- Resources flow correctly through all hierarchy levels
- Storage capacities enforced
- Typed resources (Machinery variants) working
- Units degrade gracefully when resources scarce
- Sect generates MANPOWER from population

---

### **PHASE 2: Transport Network**
**Status:** PLANNED
**Timeline:** Week 11-14

#### Objectives
- Implement intersect transport via roads
- Create automatic (adjustable) transport scheduling
- Add visual representation of resources in transit
- Implement distance-based transport efficiency
- Add Transport module effects on timing

#### Major Components

**2.1 Transport Network Core** (Week 11-12)
- [ ] Create TransportNetwork class
- [ ] Implement TransportJob struct
- [ ] Add pathfinding algorithm (A* on road network)
- [ ] Create transport job queue system
- [ ] Implement distance/speed calculations

**2.2 Dynamic Transport Parameters** (Week 12-13)
- [ ] Remove hardcoded 10-tick interval
- [ ] Implement technology-based transport multipliers
- [ ] Add Transport module effects on speed
- [ ] Implement distance-based request routing
- [ ] Add transport capacity limits

**2.3 Automatic Transport Scheduling** (Week 13)
- [ ] Create automatic intersect transport system
- [ ] Implement smart routing (closest surplus to deficit)
- [ ] Add player-adjustable transport priorities
- [ ] Create transport job visualization

**2.4 Visual Representation** (Week 14)
- [ ] Draw animated resource packets on roads
- [ ] Color-code by resource type
- [ ] Show transport progress indicators
- [ ] Add traffic congestion visualization
- [ ] Create transport statistics UI panel

**Exit Criteria:**
- Resources automatically transport between sects via roads
- Transport timing affected by technology and modules
- Visual feedback shows resources in transit
- Player can adjust transport priorities
- System routes efficiently based on distance

---

### **PHASE 3: Advanced Production & Intelligence**
**Status:** PLANNED
**Timeline:** Week 15-18

#### Objectives
- Implement production strategy pattern
- Create strategy-based Energy/Manpower distribution
- Add implicit player decision learning
- Implement production policy framework
- Add module chaining for vertical integration

#### Major Components

**3.1 Production Strategy System** (Week 15)
- [ ] Implement ProductionStrategy base class
- [ ] Create unit-specific strategies (Extraction, Farming, Energy, etc.)
- [ ] Refactor Unit::Process*() methods to use strategies
- [ ] Add strategy selection based on UnitType
- [ ] Create strategy factory pattern

**3.2 Intelligent Resource Distribution** (Week 16)
- [ ] Implement ResourceDistributionPolicy class
- [ ] Add player action tracking system
- [ ] Create priority learning from player behavior
- [ ] Implement adaptive Energy distribution
- [ ] Implement adaptive Manpower distribution

**3.3 Production Policy Framework** (Week 17)
- [ ] Create ProductionPolicy enum (SURVIVAL, GROWTH, RESEARCH, etc.)
- [ ] Implement policy application logic
- [ ] Add player policy selection UI
- [ ] Create auto-balancing system
- [ ] Add production suggestions to player

**3.4 Module Chaining** (Week 18)
- [ ] Add internal buffer system to Units
- [ ] Implement module dependency chains
- [ ] Create chain validation logic
- [ ] Add chaining UI for advanced units
- [ ] Optimize buffer flush mechanisms

**Exit Criteria:**
- Unit-specific logic cleanly separated via strategies
- Energy/Manpower distributed based on player's implicit priorities
- Production policies affect colony behavior
- Module chaining enables vertical integration
- System learns and adapts to player's playstyle

---

### **PHASE 4: Data-Driven Architecture**
**Status:** PLANNED
**Timeline:** Week 19-22

#### Objectives
- Create game_types.toml definition file
- Implement type loading system
- Enable modding and expansion without recompilation
- Add asset path management
- Create balancing configuration system

#### Major Components

**4.1 Type Definition System** (Week 19-20)
- [ ] Design game_types.toml schema
- [ ] Implement TOML parser (or use library)
- [ ] Create GameTypesLoader singleton
- [ ] Define all resource types in TOML
- [ ] Define all unit types in TOML
- [ ] Define all module types in TOML

**4.2 Dynamic Type Registration** (Week 20-21)
- [ ] Implement resource type registration from file
- [ ] Implement unit type registration from file
- [ ] Implement module type registration from file
- [ ] Add asset path resolution system
- [ ] Create type validation system

**4.3 Balancing & Configuration** (Week 21-22)
- [ ] Move all game constants to TOML
- [ ] Create difficulty presets
- [ ] Add balance testing tools
- [ ] Create mod loading system
- [ ] Document TOML schema for modders

**Exit Criteria:**
- Game types defined entirely in game_types.toml
- New units/modules/resources added without code changes
- Asset paths auto-resolved from TOML
- Modding supported
- Balancing tweaks possible via config edits

---

### **PHASE 5: Module System Enhancement**
**Status:** PLANNED
**Timeline:** Week 23-26

#### Objectives
- Enable multiple simultaneous active modules
- Add per-module leveling system
- Expand module variety for each unit
- Implement module dependencies and unlocks
- Create module upgrade UI

#### Major Components

**5.1 Simultaneous Module Support** (Week 23)
- [ ] Replace single activeModule pointer with activeModuleIndices set
- [ ] Update ProcessModuleEffects for multiple modules
- [ ] Implement per-module resource consumption
- [ ] Add module activation/deactivation UI
- [ ] Create module conflict resolution

**5.2 Per-Module Leveling** (Week 24)
- [ ] Implement independent level tracking per module
- [ ] Add per-module upgrade costs
- [ ] Create module-specific enhancement system
- [ ] Add module level visualization
- [ ] Implement level-based unlock system

**5.3 Module Expansion** (Week 25)
- [ ] Design 3-5 modules per unit type (defined in TOML)
- [ ] Create module variety (e.g., Mining: H2/O2/Fe/Si/C extractors)
- [ ] Add specialized modules for advanced play
- [ ] Implement module prerequisites
- [ ] Create module tech tree

**5.4 Module UI & Feedback** (Week 26)
- [ ] Create module selection interface
- [ ] Add module status indicators
- [ ] Implement module upgrade confirmation
- [ ] Add module efficiency visualization
- [ ] Create module tooltip system

**Exit Criteria:**
- Multiple modules run simultaneously per unit
- Each module has independent level progression
- 3-5 unique modules per unit type
- Module upgrades provide meaningful choices
- Clear UI for module management

---

### **PHASE 6: Research & Technology**
**Status:** PLANNED
**Timeline:** Week 27-30

#### Objectives
- Implement research tree system
- Add technology unlocks for new modules/units
- Create SCIENCE resource accumulation
- Implement colony-wide tech bonuses
- Add research priority system

#### Major Components

**6.1 Research Infrastructure** (Week 27)
- [ ] Create ResearchManager class
- [ ] Implement ResearchProject struct
- [ ] Add SCIENCE accumulation at colony level
- [ ] Create research queue system
- [ ] Implement research progress tracking

**6.2 Technology Tree** (Week 28-29)
- [ ] Design tech tree structure (in TOML)
- [ ] Implement tech prerequisites
- [ ] Create tech unlock effects
- [ ] Add colony-wide bonuses (transport speed, efficiency, etc.)
- [ ] Implement new unit/module unlocks

**6.3 Research UI** (Week 30)
- [ ] Create research tree visualization
- [ ] Add research project selection
- [ ] Implement research progress display
- [ ] Add tech tooltip descriptions
- [ ] Create unlocked technology indicators

**Exit Criteria:**
- Research projects consume SCIENCE and unlock techs
- Tech tree provides strategic choices
- Technologies unlock new modules, units, and bonuses
- Research system integrated with production priorities

---

### **PHASE 7: Polish & Optimization**
**Status:** PLANNED
**Timeline:** Week 31-35

#### Objectives
- Performance optimization for large colonies
- Comprehensive UI/UX improvements
- Visual polish and animations
- Sound effects and music
- Tutorial and player onboarding

#### Major Components

**7.1 Performance** (Week 31-32)
- [ ] Profile resource system performance
- [ ] Optimize tick processing for many units
- [ ] Implement spatial partitioning for rendering
- [ ] Add level-of-detail system
- [ ] Optimize pathfinding

**7.2 UI/UX Polish** (Week 33)
- [ ] Improve resource dashboard
- [ ] Add contextual tooltips
- [ ] Create keyboard shortcuts
- [ ] Implement notification system
- [ ] Add settings menu

**7.3 Visual & Audio** (Week 34)
- [ ] Add particle effects for production
- [ ] Create smooth camera transitions
- [ ] Add ambient sound effects
- [ ] Implement background music
- [ ] Polish terrain rendering

**7.4 Onboarding** (Week 35)
- [ ] Create tutorial sequence
- [ ] Add contextual hints
- [ ] Write game manual/wiki
- [ ] Create example scenarios
- [ ] Add achievement system

**Exit Criteria:**
- Game runs smoothly with 10+ colonies
- UI is intuitive and polished
- Visual/audio feedback enhances experience
- New players understand core mechanics

---

### **PHASE 8: Advanced Features** (Future)
**Status:** PLANNED (Post-Launch)
**Timeline:** TBD

#### Potential Features
- Multi-colony trade and diplomacy
- Planetary events and disasters
- Advanced automation systems
- Multiplayer/co-op mode
- Procedural scenario generation
- Combat/defense systems
- Orbital structures and space expansion

---

## Current Status Summary

**Current Phase:** PHASE 0 (Foundation & Architecture) - 75% complete
**Next Phase:** PHASE 1 (Core Resource System)

**Recent Updates (2025-11-25):**
- 🔄 **NEW PRIORITY:** Graphics enhancement - texture-based rendering (Checkpoint 0)
  - Goal: Replace circle primitives with custom textures
  - Scope: Dome center + unit visuals in Sect view
  - Assets: Dome_off.png + 5 unit textures ready
  - Impact: Major visual improvement, establishes texture pipeline

**Recent Completions (2025-11-09):**
- ✅ Data-driven type system (game_types.toml) with tomlplusplus integration
- ✅ GameTypesLoader singleton class implementation
- ✅ Build system cleanup (fixed multiple definition errors)
- ✅ All source files properly configured in CMakeLists.txt
- ✅ Successful compilation (3.0 MB executable)

**Previous Completions:**
- Engine refactor into manager subsystems
- Terrain rendering with tiles
- BuildNewColony/BuildNewSect functionality

**Immediate Priorities (Next 1-2 Weeks):**
1. ✅ ~~Complete data-driven type system (game_types.toml)~~ DONE
2. 🔄 **Graphics enhancement with texture rendering (Checkpoint 0)** IN PROGRESS
3. Implement multiple simultaneous active modules (Checkpoint 2)
4. Add storage capacity limits to Sect/Colony (Phase 1)
5. Implement typed resources (Machinery, Electronics) (Phase 1)
6. Create sect-level direct generation (population MANPOWER) (Phase 1)

---

## Technical Debt & Architectural Improvements

**High Priority:**
- [ ] Move from switch-based unit logic to Strategy Pattern
- [x] ~~Implement game_types.toml for data-driven design~~ ✅ COMPLETED 2025-11-09
- [ ] Refactor production priority from vector<string> to structured system
- [ ] Add comprehensive logging system
- [ ] Create unit testing framework

**Medium Priority:**
- [ ] Optimize rendering for large entity counts
- [ ] Add serialization/deserialization for save/load
- [ ] Improve error handling throughout codebase
- [ ] Add debug visualization modes
- [ ] Create profiling tools

**Low Priority:**
- [ ] Consider migration to ECS (Entity Component System) for scalability
- [ ] Add scripting support (Lua/Python) for advanced modding
- [ ] Implement network protocol for multiplayer
- [ ] Create level editor

---

## Dependencies & Prerequisites

**By Phase:**
- Phase 1 requires: game_types.toml, multiple module support
- Phase 2 requires: Phase 1 complete (resource flow working)
- Phase 3 requires: Phase 1 complete, Strategy Pattern implemented
- Phase 4 can run in parallel with Phase 2/3
- Phase 5 requires: Phase 4 complete (type system)
- Phase 6 requires: Phase 1-5 complete
- Phase 7 requires: All core systems functional
- Phase 8 requires: Phase 7 complete, game released

---

**Last Updated:** 2025-11-25
**Maintained By:** Development Team
**Review Cycle:** Weekly updates, major revisions at phase boundaries
