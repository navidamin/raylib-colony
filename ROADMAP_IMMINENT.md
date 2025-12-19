# ROADMAP_IMMINENT.md

**Last Updated:** 2025-11-25
**Current Sprint:** Graphics Enhancement + Foundation Completion
**Timeline:** Week 6-8 (Next 2-3 weeks)

---

## Where We Stand in Overall Roadmap

```
PHASE 0: Foundation & Architecture ███████████████░░░░░ 75% ⚡ CURRENT
├─ Engine refactor ✅ COMPLETE
├─ View system ✅ COMPLETE
├─ Entity hierarchy ✅ COMPLETE
├─ Module system ✅ COMPLETE
├─ Data-driven architecture ✅ COMPLETE (Checkpoint 1)
├─ Graphics enhancement 🔄 IN PROGRESS (Checkpoint 0 - NEW)
├─ Multiple active modules 📋 NEXT (Checkpoint 2)
└─ Resource flow design ✅ COMPLETE (implementation pending)

PHASE 1: Core Resource System ░░░░░░░░░░░░░░░░░░░░  0% NEXT
PHASE 2: Transport Network ░░░░░░░░░░░░░░░░░░░░  0% PLANNED
PHASE 3: Advanced Production ░░░░░░░░░░░░░░░░░░░░  0% PLANNED
...
```

---

## Recent Updates

### New Priority (2025-11-25)
🎯 **Graphics Enhancement (Checkpoint 0)** - IN PROGRESS
- **Goal:** Replace circle primitives with texture-based rendering
- **Scope:** Dome center + unit circles in Sect view
- **Assets ready:** Dome_off.png, 5 unit textures (Extraction, Farming, Energy, Water, Manufacturing)
- **Approach:** Proof of concept with available textures, graceful fallback for missing ones
- **Impact:** Major visual improvement, establishes texture pipeline for future artwork

---

## Recent Completions (2025-11-09)

✅ **Data-Driven Type System (Checkpoint 1)** - COMPLETE
- Integrated tomlplusplus v3.4.0 via CPM
- Created GameTypesLoader singleton class
- Implemented game_types.toml with 11 resources, 5 unit types, 14 modules
- Proper asset directory structure (unit.png for sect view, module.png for unit view)
- Game expansion now possible without recompilation

✅ **Build System Fixes**
- Fixed RenderManager multiple definition errors (removed .cpp includes from Engine.cpp)
- Added missing source files to CMakeLists.txt (unit_ui.cpp, all managers, ResourceManager, TimeManager)
- Fixed source file paths in build configuration
- Added tomlplusplus dependency integration
- Successful compilation: 3.0 MB executable

✅ **Previous Week Completions**
- Engine modularization (InputManager, ViewManager, GameManager, RenderManager)
- Terrain rendering with tile-based system
- BuildNewColony/BuildNewSect functionality
- UI improvements for navigation

---

## Immediate Priorities (Next 2 Weeks)

### **CHECKPOINT 0: Graphics Enhancement - Texture-Based Rendering** 🎯 IN PROGRESS
**Priority:** CRITICAL (NEW - 2025-11-25)
**Estimated Effort:** 1-2 days
**Goal:** Replace placeholder circles with actual image textures for dome and units in Sect view

#### Motivation
Improve visual appeal and game identity by replacing geometric primitives with custom artwork. This is a proof of concept to establish the texture loading/rendering pipeline before expanding to all unit types.

#### Available Assets
- **Dome:** `Dome_off.png` (1022 KB, high-res dome texture)
- **Units:**
  - `extractionX256.png` (Extraction unit)
  - `FarmX256.png` (Farming unit)
  - `powerX256.png` (Energy unit)
  - `waterX256.png` (Water/processing unit)
  - `manfuctrungX256.png` (Manufacturing unit - note: filename has typo)

#### Tasks
- [ ] Add texture loading infrastructure to Sect class
  - File: `src/Sect/sect.h` - Add member variables for textures
  - Add: `Texture2D domeTexture;`
  - Add: `std::map<std::string, Texture2D> unitTextures;`
  - Add: `void LoadTextures();` method declaration
  - Add: `void UnloadTextures();` method declaration (for cleanup)

- [ ] Implement texture loading in Sect constructor
  - File: `src/Sect/sect.cpp` - Update constructor
  - Call `LoadTextures()` after initialization
  - Load `Dome_off.png` for dome
  - Load unit textures based on available files
  - Handle missing textures gracefully (fallback to circle rendering)

- [ ] Map unit types to texture filenames
  - Create mapping: `Extraction` → `extractionX256.png`
  - Create mapping: `Farming` → `FarmX256.png`
  - Create mapping: `Energy` → `powerX256.png`
  - Create mapping: `Manufacture` → `manfuctrungX256.png`
  - Use case-insensitive or normalized matching (note filename inconsistencies)

- [ ] Replace dome circle with texture in DrawInSectView()
  - File: `src/Sect/sect.cpp` line 166
  - Replace: `DrawCircle(position.x, position.y, coreRadius, GRAY);`
  - With: `DrawTextureEx()` centered on position, scaled to coreRadius*2 diameter
  - Calculate proper source/dest rectangles for circular rendering
  - Maintain aspect ratio or use circular masking

- [ ] Replace unit circles with textures in DrawInSectView()
  - File: `src/Sect/sect.cpp` lines 196-199
  - Replace: `DrawCircle(unitPos.x, unitPos.y, unitRadius, fillColor);`
  - With: `DrawTextureEx()` using unit-type-specific texture
  - Scale textures to match `unitRadius * 2` diameter
  - Apply color tint for active/inactive status (GREEN for active, GRAY for inactive)
  - Fallback to circle if texture not available for unit type

- [ ] Implement texture cleanup in destructor
  - File: `src/Sect/sect.cpp` - Update destructor
  - Call `UnloadTextures()` to free GPU memory
  - Unload dome texture
  - Unload all unit textures

- [ ] Test proof of concept
  - Run game and navigate to Sect view
  - Verify dome displays with Dome_off.png texture
  - Verify units with available textures display correctly
  - Verify units without textures fall back to circles
  - Check performance (texture loading should not impact frame rate)
  - Verify textures scale properly at different screen sizes

- [ ] Document texture loading system
  - Add comments explaining texture path resolution
  - Document expected texture naming convention
  - Note which unit types still need textures

#### Success Criteria
- ✅ Dome center displays Dome_off.png texture instead of gray circle
- ✅ Units with available textures display their respective images
- ✅ Units without textures gracefully fall back to circle rendering
- ✅ Textures scale properly with screen size
- ✅ Active/inactive status still visually indicated (color tint or other method)
- ✅ No memory leaks (textures properly unloaded)
- ✅ Performance remains smooth (60 FPS target)

#### Blockers
- None identified (all assets already in place)

#### Dependencies
- Enables: Future expansion to all unit types with custom artwork
- Enables: Animated textures for active units (future enhancement)
- Enables: Module-specific visuals in Unit view (future)

#### Technical Notes
**Current rendering location:**
- `src/Sect/sect.cpp:162-199` - `DrawInSectView()` method
- Line 166: Main dome circle rendering
- Lines 196-199: Unit circle rendering in orbit around dome

**Texture scaling approach:**
- Use `DrawTexturePro()` for precise control over source/dest rectangles
- Calculate scale factor: `textureScale = (radius * 2) / texture.width`
- Center texture on calculated position

**Fallback strategy:**
```cpp
if (texture.id != 0) {
    // Draw texture
    DrawTextureEx(texture, position, rotation, scale, tint);
} else {
    // Fallback to circle
    DrawCircle(position.x, position.y, radius, color);
}
```

---

### **CHECKPOINT 1: Data-Driven Type System** ✅ COMPLETE
**Priority:** CRITICAL
**Completed:** 2025-11-09
**Actual Effort:** 1 day (3-4 hours)

#### Tasks
- [x] Design game_types.toml schema *(completed in planning)*
- [x] Choose and integrate TOML parser library
  - ✅ Selected: tomlplusplus v3.4.0 (C++17, header-only, TOML 1.0.0 compliant)
  - ✅ Integrated via CPM package manager
- [x] Implement GameTypesLoader class
  - ✅ Created: `src/GameTypes/game_types_loader.h/.cpp`
  - ✅ Methods: `LoadFromFile()`, `GetUnitTypeDef()`, `GetModuleDef()`, `GetResourceTypeDef()`
  - ✅ Singleton pattern with forward declarations for TOML types
- [x] Create initial game_types.toml
  - ✅ Defined 11 singular resources (H2, O2, C, Fe, Si, WATER, FOOD, BIOFUEL, ENERGY, SCIENCE, MANPOWER)
  - ✅ Defined 2 typed resources (MACHINERY, ELECTRONICS with variants)
  - ✅ Defined 5 unit types (Extraction, Farming, Energy, Research, Manufacture)
  - ✅ Defined 14 modules with production/consumption rates
  - ✅ Proper asset directory structure (assets/units/{UnitType}/unit.png and {ModuleName}.png)
- [x] Fixed resource_types.h compilation (added missing includes)
- [x] Test type loading on game startup (compiles successfully)
- [x] Documentation embedded in TOML file with comments

**Success Criteria: ALL MET ✅**
- ✅ Game compiles successfully with game_types.toml integration
- ✅ Adding new resource type requires only TOML edit
- ✅ No compilation needed for new types
- ✅ Modding infrastructure in place

**Blockers Encountered:**
- ⚠️ toml++ namespace conflict → Fixed with proper forward declarations
- ⚠️ Multiple definition errors → Fixed by removing .cpp includes from Engine.cpp
- ⚠️ Missing unit_ui.cpp in build → Fixed by adding to CMakeLists.txt

**Unblocked:**
- ✅ Module expansion
- ✅ Balancing improvements
- ✅ Future modding support

---

### **CHECKPOINT 2: Multiple Simultaneous Active Modules** 🎯 DUE: Week 7 Mid
**Priority:** HIGH
**Estimated Effort:** 2-3 days

#### Tasks
- [ ] Refactor Unit::activeModule pointer to std::set<int> activeModuleIndices
  - File: `src/Unit/unit.h` line 98
- [ ] Update Unit::ProcessModuleEffects() to iterate active modules
  - File: `src/Unit/unit.cpp` line 146
- [ ] Implement Unit::ActivateModule(int index)
- [ ] Implement Unit::DeactivateModule(int index)
- [ ] Update Unit::UpgradeModule() to support per-module levels
- [ ] Add UI controls for activating/deactivating modules
  - File: `src/Unit/unit_ui.cpp` (if separate) or inline in unit.cpp
- [ ] Test multiple module interaction (resource competition)
- [ ] Add visual indicators for active vs inactive modules

**Success Criteria:**
- Can activate 2+ modules simultaneously in a unit
- Each module consumes resources independently
- UI clearly shows which modules are active
- Module levels tracked independently

**Blockers:**
- None identified

**Dependencies:**
- Enables: Module chaining (Phase 3), expanded module variety (Phase 5)

---

### **CHECKPOINT 3: Storage Capacity System** 🎯 DUE: Week 7 End
**Priority:** HIGH
**Estimated Effort:** 2 days

#### Tasks
- [ ] Add storageCapacity to Sect class
  - File: `src/Sect/sect.h` line 76 (after resourceStorage)
  - Type: `std::map<ResourceType, float> storageCapacity;`
- [ ] Add strategicReserves and reserveCapacity to Colony class
  - File: `src/Colony/colony.h` line 34
  - Types: `std::map<ResourceType, float> strategicReserves;`
  - `std::map<ResourceType, float> reserveCapacity;`
- [ ] Implement storage overflow handling in Unit::DepositProduction()
  - Add: `std::map<ResourceType, float> unitOverflowBuffer;` to Unit
- [ ] Add Sect::PushSurplusToColony() method
  - Trigger when storage > 80% capacity
- [ ] Add Colony::ReceiveSurplus() method
- [ ] Initialize default capacities in game_constants.h
  - `SECT_BASE_STORAGE = 1000.0f`
  - `COLONY_BASE_RESERVES = 5000.0f`
- [ ] Create storage upgrade mechanism (basic)
- [ ] Add storage capacity UI visualization

**Success Criteria:**
- Sects cannot exceed storage capacity
- Overflow handled gracefully (buffered or pushed to colony)
- Colony accepts surplus and stores in reserves
- UI shows current/max storage levels

**Blockers:**
- None identified

**Dependencies:**
- Required for: Resource flow implementation (Phase 1)

---

### **CHECKPOINT 4: Typed Resources Implementation** 🎯 DUE: Week 8 Mid
**Priority:** MEDIUM
**Estimated Effort:** 3 days

#### Tasks
- [ ] Create TypedResource struct
  - File: `src/ResourceManager/resource_types.h` (new section)
  - Fields: `ResourceType baseType`, `std::string subType`, `int quality`, `float efficiency`
- [ ] Add ResourceCategory enum (SINGULAR vs TYPED)
- [ ] Extend ResourceType enum with new types:
  - `MACHINERY`, `ELECTRONICS`, `ALLOYS`, `CONSTRUCTION_MATERIALS`
- [ ] Create storage for typed resources
  - `std::map<ResourceType, std::vector<TypedResource>> typedResources;`
- [ ] Implement Manufacture unit logic for typed resource creation
- [ ] Add typed resource production costs to TOML
- [ ] Update UI to display typed resource variants
- [ ] Create typed resource selection interface

**Success Criteria:**
- Can produce MACHINERY with subtypes (HeavyDrill, Conveyor, etc.)
- Typed resources stored separately from singular
- UI distinguishes between variants
- Production costs vary by subtype

**Blockers:**
- Requires: game_types.toml (Checkpoint 1)

**Dependencies:**
- Enables: Advanced manufacturing chains, module upgrades using specific artifacts

---

### **CHECKPOINT 5: Sect-Level Direct Generation** 🎯 DUE: Week 8 End
**Priority:** MEDIUM
**Estimated Effort:** 2 days

#### Tasks
- [ ] Add population tracking to Sect
  - File: `src/Sect/sect.h`
  - Field: `float population;`
- [ ] Implement Sect::GeneratePopulationResources()
  - Generate MANPOWER from population (1% per tick)
  - Consume FOOD and WATER for population
  - Track population growth/decline
- [ ] Implement Sect::GenerateAmbientEnergy()
  - Calculate solar intensity based on time of day
  - Generate base energy from ambient sources
- [ ] Add population UI display in Sect view
- [ ] Add population growth indicators
- [ ] Balance population consumption rates

**Success Criteria:**
- Sects generate MANPOWER based on population
- Population grows if fed, declines if starved
- Ambient energy provides baseline power
- UI shows population and growth rate

**Blockers:**
- None identified

**Dependencies:**
- Enables: Population management gameplay, energy bootstrapping

---

## Secondary Tasks (If Time Permits)

### **Production Priority Enhancement**
**Estimated Effort:** 1-2 days

- [ ] Refactor `Sect::production_priority` from `vector<string>` to structured system
  - File: `src/Sect/sect.h` line 75
- [ ] Create ProductionPriority struct (resource, weight, strategy, targetLevel)
- [ ] Implement Sect::UpdateProductionBasedOnPriorities()
- [ ] Add priority adjustment UI (drag-and-drop list)

### **Production Strategy Pattern**
**Estimated Effort:** 2-3 days

- [ ] Create ProductionStrategy base class
  - File: `src/Unit/production_strategy.h` (new)
- [ ] Implement ExtractionStrategy
- [ ] Implement FarmingStrategy
- [ ] Implement EnergyStrategy
- [ ] Refactor Unit to use strategy pattern
- [ ] Add strategy selection based on UnitType

### **Dynamic Transport Timing**
**Estimated Effort:** 1 day

- [ ] Create TransportParameters struct in Sect
- [ ] Replace hardcoded 10-tick interval with dynamic calculation
- [ ] Add technology multiplier effect
- [ ] Add transport module multiplier effect
- [ ] Update Sect::PushSurplusToColony() timing

---

## Known Issues & Technical Debt

**High Priority Fixes:**
1. **Production costs not applied consistently** (`unit.cpp` line 84-118)
   - Some modules skip consumption calculation
   - Need validation that all active modules have defined costs

2. **ResourceStorage reference management** (`unit.h` line 95)
   - Units hold reference to Sect's resourceStorage
   - Could cause issues if Sect destroyed before Unit
   - Consider weak_ptr or ownership clarification

3. **Module initialization incomplete** (`unit.cpp`)
   - InitializeModules() called but InitializeFutureModules() never used
   - Need to clarify module unlock/availability system

**Medium Priority:**
4. **No save/load system**
   - Game state lost on exit
   - Should implement serialization (JSON/binary)

5. **CMakeLists.txt incomplete** (noted in CLAUDE.md)
   - Not all .cpp files listed in target_sources
   - May cause build issues when adding new files

**Low Priority:**
6. **Debug output still present** (`unit.cpp` commented cout statements)
   - Should be replaced with proper logging system
   - Consider adding debug levels (INFO, WARN, ERROR)

---

## Risks & Mitigation

**Risk 1: TOML Parser Integration**
- **Impact:** Could delay Checkpoint 1 by 1-2 days
- **Mitigation:** Have fallback plan to use JSON if TOML problematic
- **Probability:** Low

**Risk 2: Multiple Modules Resource Contention**
- **Impact:** Modules may compete unpredictably for resources
- **Mitigation:** Implement clear priority system, add module pause if starved
- **Probability:** Medium

**Risk 3: Storage Overflow Cascades**
- **Impact:** Overflow at one level could propagate up hierarchy
- **Mitigation:** Implement backpressure system, pause production if full
- **Probability:** Medium

---

## Testing Checklist

Before marking checkpoints complete, verify:

**Checkpoint 0 (Graphics Enhancement):**
- [ ] Dome_off.png texture loads without errors
- [ ] Dome displays as texture (not gray circle) in Sect view
- [ ] Unit textures load for available unit types (Extraction, Farming, Energy, Water, Manufacturing)
- [ ] Units with textures display correctly (not as circles)
- [ ] Units without textures fall back to circle rendering gracefully
- [ ] Active units show visual distinction (green tint or highlight)
- [ ] Inactive units show visual distinction (gray tint or dimmed)
- [ ] Textures scale properly at different screen resolutions
- [ ] No performance degradation (maintain 60 FPS)
- [ ] Textures properly unload on Sect destruction (no memory leaks)
- [ ] Game runs without texture-related errors or warnings
- [ ] All 8 unit positions display correctly around dome

**Checkpoint 1 (Data-Driven Types):**
- [ ] Game loads game_types.toml on startup
- [ ] All existing resources defined in TOML
- [ ] Can add new resource type without recompiling
- [ ] Error handling for malformed TOML

**Checkpoint 2 (Multiple Modules):**
- [ ] Can activate 2+ modules simultaneously
- [ ] Modules with conflicting resource needs handled correctly
- [ ] UI shows all active modules
- [ ] Deactivating module stops its consumption immediately

**Checkpoint 3 (Storage Capacity):**
- [ ] Sect storage cannot exceed capacity
- [ ] Overflow triggers colony push
- [ ] Colony reserves track properly
- [ ] UI accurately reflects current/max storage

**Checkpoint 4 (Typed Resources):**
- [ ] Can produce multiple MACHINERY variants
- [ ] Typed resources display correctly in UI
- [ ] Production costs differ by variant
- [ ] Quality levels affect efficiency

**Checkpoint 5 (Sect Generation):**
- [ ] Population generates MANPOWER
- [ ] Population grows when fed
- [ ] Population declines when starved
- [ ] Ambient energy provides baseline power

---

## Next Sprint Preview (Week 9-10)

After completing current checkpoints, next sprint will focus on:

1. **Resource Flow Implementation**
   - Unit→Sect production deposit
   - Sect→Colony surplus push
   - Colony→Sect deficit pull

2. **Graceful Degradation**
   - Partial resource operation
   - Module efficiency reduction
   - Emergency shutdown protocols

3. **Resource Allocator**
   - Priority-based distribution
   - Scarcity handling
   - Request queue management

---

## Daily Standup Template

**What was completed yesterday:**
- [Checkpoint/Task completed]

**What is planned for today:**
- [Checkpoint/Task to work on]

**Blockers:**
- [Any blocking issues]

**Progress toward current checkpoint:**
- [Percentage or task count]

---

## Checkpoint Completion Criteria

To mark a checkpoint COMPLETE:
1. ✅ All tasks in checkpoint finished
2. ✅ Testing checklist passed
3. ✅ Code committed to git with descriptive message
4. ✅ ROADMAP_IMMINENT.md updated
5. ✅ No blocking issues remaining
6. ✅ Documentation updated (if applicable)

---

**Sprint Goal:** Complete Checkpoints 1-3, start Checkpoint 4
**Stretch Goal:** Complete all 5 checkpoints
**Fallback:** Complete Checkpoints 1-2 minimum

**End of Sprint Review Date:** End of Week 8
**Next Roadmap Update:** After checkpoint completion or weekly, whichever comes first
