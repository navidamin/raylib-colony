# ROADMAP_OVERALL.md

## Vision
Create a scalable, data-driven colony management game with deep resource logistics, modular production systems, and emergent strategic gameplay across multiple hierarchical scales (Planet → Colony → Sect → Unit → Module).

---

## Overall Development Phases

### **PHASE 0: Foundation & Architecture**
**Status:** IN PROGRESS (85% complete)
**Timeline:** Started - Week 6
**Last Updated:** 2026-01-31

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
- ✅ **Transport system** (Phase 2.5) - *Completed 2025-12*
  - ✅ Road construction mode with UI feedback
  - ✅ Transport packets with visual rendering
  - ✅ Rate limiting and multiple packets per road
  - ✅ AUTO_BALANCE, MANUAL, DEFICIT_TRIGGERED modes
- ✅ **Multiple simultaneous active modules** - *Completed 2026-01*
  - ✅ std::set<int> activeModuleIndices tracking
  - ✅ ActivateModule/DeactivateModule methods
  - ✅ Per-module resource consumption and production

#### In Progress 🔄
- 🔄 **Graphics enhancement** (Checkpoint 0 - ~95% complete, needs polish)
- 🔄 Storage capacity system (basic implementation complete)

#### Remaining Tasks 📋
- 📋 Graphics polish pass (texture scaling, active/inactive tint refinement)

**Exit Criteria Progress:**
- ✅ All manager subsystems fully functional
- ✅ Data-driven type loading system operational
- ✅ Multiple modules can run simultaneously
- ✅ Transport network functional
- 📋 Graphics polish pass

---

### **PHASE 1.5: Extraction Unit Overhaul** ✅ COMPLETE
**Status:** COMPLETE (100%)
**Timeline:** Week 8-12
**Last Updated:** 2026-02-21

Based on `Prospecting_Extraction_Mechanics.md` design document.

#### Completed ✅
- ✅ **Phase A: Survey Data & Unlock Registry**
  - ✅ OrbitalSurveyData struct (elemental composition, hydrogen, solar, slope, earth visibility)
  - ✅ GetOrbitalSurveyAt() and GetSiteArchetype() methods
  - ✅ New resource types: Ti, Al, Ca added to ResourceType enum
  - ✅ UnlockRegistry singleton with 14 available techs
  - ✅ Debug key: F5 (unlock techs)

- ✅ **Phase B: Site Selection UI**
  - ✅ View::SITE_SELECTION enum and state machine
  - ✅ Full orbital survey instrument panels (GRS, Neutron, Thermal, Assessment)
  - ✅ Color-coded grid overlay (mare/highland/hydrogen)
  - ✅ Colony archetype system (5 types with production bonuses)
  - ✅ Sect placement resource preview overlay (Ctrl+hover in Colony view)

- ✅ **Phase C: Module Architecture Overhaul**
  - ✅ UnitModule extended with moduleType, tier, tierDependencies, energyRequired
  - ✅ 5 specialized extraction modules: PROSPECTING, EXCAVATION, BENEFICIATION, OPERATIONS, DIRECTIVES
  - ✅ Stub specialized modules for Farming (5), Energy (5), Manufacturing (5), Research (5)
  - ✅ UpgradeModuleTier() with tech dependency checking via UnlockRegistry
  - ✅ game_types.toml updated with 5 extraction modules × 4 tiers

- ✅ **Phase D: Prospecting Module (Tiers 0-3)**
  - ✅ ScanResult struct with elements, minerals, hydrogen, quality rating, scanTier, categories
  - ✅ PerformLIBSScan() with tier-dependent noise (T0: categories, T1: ±15%, T2: ±5%, T3: exact)
  - ✅ MarkSiteForExcavation/UnmarkSite tracking
  - ✅ Tier progression: Visual Estimation → LIBS → Multi-Spectral → Deep Survey
  - ✅ Tier 0 scanning enabled (categories only, 5s cooldown, 10 energy)

- ✅ **Phase E: Excavation Module**
  - ✅ Excavator struct (id, gridPos, method, depth, rate, wear)
  - ✅ MoveExcavator, SetExcavatorDepth, SetExcavatorRate
  - ✅ Tier-based max depth and excavator counts
  - ✅ Wear accumulation per excavator

- ✅ **Phase F: Beneficiation Separation Node System**
  - ✅ SeparationNode struct with type, efficiency, wear, energy, input/output maps
  - ✅ 7 node types: DIRECT_OUTPUT, SIZE_SORT, MAGNETIC, ELECTROSTATIC, THERMAL, CHEMICAL, MRE
  - ✅ Tier upgrades rebuild separation chain (Tier 0→1→2→3)
  - ✅ SwapSeparationNodes for node reordering

- ✅ **Phase G: Operations & Directives**
  - ✅ Operations efficiency modifier (Tier 0: -15%, Tier 3: +20%) + geological confidence bonus (+10% max)
  - ✅ DirectiveType enum (PRIORITIZE, MAXIMIZE, CONSERVE, EXPLORATION_MODE, etc.)
  - ✅ SetDirective with tier gating
  - ✅ Directive modifiers applied in extraction pipeline

- ✅ **Phase H: Integration**
  - ✅ ProcessExtraction rewritten with full pipeline:
    Excavation → Beneficiation → Storage
  - ✅ Operations and Directives modifiers applied
  - ✅ Excavator count scales extraction output
  - ✅ Scan-gated extraction efficiency (0.35 unscanned, 1.0 scanned, 1.15 marked)

- ✅ **Extraction Unit UI Rendering (Display Panels)**
  - ✅ Dark-themed RenderManager-based UI for extraction units
  - ✅ 5 module-specific center panels (Prospecting, Excavation, Beneficiation, Operations, Directives)
  - ✅ Left panel module list with hover/selection, status borders
  - ✅ Right panel controls (Build/Upgrade/Activate/Deactivate buttons, cost breakdown)
  - ✅ Resource overview with production/consumption table and storage bars
  - ✅ Bottom bar message fade system
  - ✅ Non-extraction units fall back to old white UI

- ✅ **Interactive Controls (2026-02-01)**
  - ✅ Prospecting: 5x5 clickable scan grid with cooldown overlay, tier gate, mark/unmark sites
  - ✅ Excavation: [-] value [+] buttons for depth (tier-dependent step) and rate per excavator
  - ✅ Beneficiation: up/down reorder arrows, clickable ON/OFF toggle per node
  - ✅ Directives: selectable directive cards with tier gating, PRIORITIZE resource chip selector

- ✅ **Balance Pass (2026-02-01)**
  - ✅ Fixed beneficiation double-multiply bug (efficiency was compounding per-node)
  - ✅ Raised separation node efficiencies (SIZE_SORT→0.92, MAGNETIC→0.88, ELECTROSTATIC→0.85, THERMAL→0.82, MRE→0.90)
  - ✅ Added missing directive handlers: EMERGENCY_HARVEST, EXPLORATION_MODE, THERMAL_SYNC
  - ✅ Bumped Operations Tier 3 modifier: 1.15→1.20
  - ✅ Dynamic energy consumption scaling
  - ✅ Removed `energyRequired` dead code

- ✅ **Prospecting Gameplay Overhaul (2026-02-21)**
  - ✅ Scan-gated extraction: 35% unscanned / 100% scanned / 115% marked site
  - ✅ Tier-dependent scan noise (T0: categories only, T1: ±15%, T2: ±5%, T3: exact)
  - ✅ Tier 0 scanning enabled (was blocked at tier>=1)
  - ✅ Colony Ctrl overlay nerfed — shows only LOW/MED/HIGH categories, no exact values
  - ✅ Geological confidence: 5x5 scan coverage → up to +10% Operations bonus
  - ✅ Tier-aware UI: panel titles, accuracy labels, category vs bar display, confidence meters

- ✅ **Prospecting Phase 2 Expansion (2026-02-21)**
  - ✅ Scan Profiles: Quick/Standard/Deep presets with configurable power, pulses, cooldown, energy, survey multiplier
  - ✅ Survey Progress Model: per-cell surveyProgress (0-100%) with diminishing returns, replaces scanCount/3 hard cap
  - ✅ Calibration Drift: quality degrades per scan (floor 0.5), directly multiplies survey gain, 30s recalibration, T3 auto-cal
  - ✅ Depth Profiling: 4 depth layers (SURFACE/SHALLOW/MID/DEEP), depth-biased resource generation, layer-based extraction
  - ✅ Adaptive Infill Campaign: queued auto-scanning (T2+ cap 10, T3 unlimited), +5% confidence on completion
  - ✅ Prospecting Objectives: THRESHOLD/COVERAGE/GRADIENT conditions with timed extraction/confidence rewards
  - ✅ AI Auto-Management: auto profile selection, auto calibration, T3 auto campaign; toggle UI

- ✅ **Survey Progress Rework + Panel UI Fixes (2026-02-22)**
  - ✅ Replaced scanCount/3 hard cap with survey progress model (diminishing returns, every scan helps)
  - ✅ Extraction efficiency = 0.35 + 0.65 × surveyProgress (+ 0.15 marked, × objective bonus)
  - ✅ Calibration and profiles feed directly into survey progress instead of hidden noise multipliers
  - ✅ Panel UI: deferred tooltip z-order fix, depth bands limited to tier, AI toggles relocated inline

#### Remaining Tasks 📋
- 📋 Tune upgrade costs per tier (resource amounts)
- 📋 Manual playtesting: full pipeline throughput verification at each tier
- 📋 Dependency validation with clear error messages

**Design Decisions Recorded:**
- Site selection: Full orbital survey for Colony, light preview for Sect
- Module system: 5 specialized per Extraction, stub specialized names for other units
- Dependencies: Stub UnlockRegistry until Research unit implemented
- Checkpoint 0 (Graphics): ~95% complete, needs polish pass

---

### **PHASE 1: Core Resource System**
**Status:** PARTIALLY ADDRESSED (by Phase 1.5)
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
- [x] Extend ResourceType enum with Tier 2/3 resources *(done in Phase 1.5 — MACHINERY, ELECTRONICS, ALLOYS, CONSTRUCTION_MATERIALS)*
- [x] Implement singular vs typed resource system *(ResourceDescriptor table in resource_types.h)*
- [x] Create TypedResource struct with subtypes *(descriptor-driven, subtypes in table)*
- [x] Add resource category/priority enums *(ResourceCategory::SINGULAR / TYPED)*
- [ ] Update resource visualization for new types

**1.3 Flow Mechanisms** (Week 8-9)
- [ ] Implement Unit→Sect production deposit
- [x] Implement Sect→Colony surplus push (dynamic timing) *(includes typed resources via ReceiveTypedSurplus)*
- [x] Implement Colony→Sect deficit pull *(includes typed resources via ProvideTypedResource)*
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
**Status:** LARGELY COMPLETE (~90%)
**Timeline:** Week 11-14 (completed early)

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

**Current Phase:** PHASE 1.5 (Extraction Unit Overhaul) - 100% COMPLETE
**Next Phase:** PHASE 1 (Core Resource System)

**Recent Completions (2026-02-22):**
- ✅ Survey progress rework — replaced scanCount/3 hard cap with survey progress model (0-100%, diminishing returns, every scan helps), calibration/profiles feed into survey gain, panel UI fixes (tooltip z-order, depth bands, AI relocation)
- ✅ Prospecting Phase 2 expansion — scan profiles (Quick/Standard/Deep), survey progress, calibration drift & standards, depth profiling (4 layers), adaptive infill campaigns, prospecting objectives (threshold/coverage/gradient), AI auto-management
- ✅ Prospecting gameplay overhaul — survey-gated extraction (35% unscanned to 100% fully surveyed + 15% marked), tier-dependent noise, Tier 0 enabled, Colony overlay nerfed, geological confidence system (+10% Operations bonus)
- ✅ ResourceDescriptor table refactor — single source of truth for resource metadata; typed resource flow (Sect↔Colony push/pull) wired up
- ✅ Interactive extraction controls - scan grid, excavator +/- buttons, beneficiation reorder/toggle, directive card selector
- ✅ Balance pass - fixed beneficiation double-multiply bug, raised node efficiencies, added 3 missing directive handlers, bumped Ops Tier 3
- ✅ Extraction UI font scaling - 48pt texture, FS() 1.30x multiplier on all extraction text, enlarged scan log bars with element percentages
- ✅ Extraction Unit UI Overhaul - dark-themed display panels for all 5 modules
- ✅ Extraction pipeline (Phases A-H) - data model, site selection, module architecture, integration
- ✅ Transport network (~90%) - road construction, transport packets, rate limiting
- ✅ Multiple simultaneous active modules
- ✅ Data-driven type system (game_types.toml)
- ✅ Graphics enhancement (~95%)

**Previous Completions:**
- Engine refactor into manager subsystems
- Terrain rendering with tiles
- BuildNewColony/BuildNewSect functionality

**Immediate Priorities:**
1. 📋 Tune upgrade costs per tier (resource amounts)
2. 📋 Manual playtesting: full pipeline throughput verification (including Phase 2 mechanics)
3. Phase 1: Core Resource System (storage capacity, resource flow, graceful degradation)
4. Phase 3: Advanced Production (manufacturing chains, research trees)

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

**Last Updated:** 2026-02-22
**Maintained By:** Development Team
**Review Cycle:** Weekly updates, major revisions at phase boundaries
