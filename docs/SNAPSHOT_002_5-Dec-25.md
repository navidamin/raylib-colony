# SNAPSHOT 002 | 5-Dec-25
## Current Game Features & Mechanics

---

### 1. ENTITY HIERARCHY

```
Planet (20x20 grid)
  └─ Colony (strategic reserves, resource distribution)
      └─ Sect (local storage, 8 unit slots, deficit/surplus management)
          └─ Unit (production with graceful degradation)
              └─ Module (multiple can be active simultaneously)
```

---

### 2. RESOURCE SYSTEM

**Singular Resources (11 types)**
| Tier | Resources |
|------|-----------|
| Raw | H2, O2, C, Fe, Si |
| Processed | WATER, FOOD, BIOFUEL |
| Abstract | ENERGY, SCIENCE, MANPOWER |

**Typed Resources (4 categories, 14 subtypes)**
| Category | Subtypes |
|----------|----------|
| MACHINERY | HeavyDrill, Conveyor, Assembler |
| ELECTRONICS | Sensor, Controller, Computer |
| ALLOYS | Steel, Bronze, Aluminum, Titanium |
| CONSTRUCTION_MATERIALS | Beam, Panel, Pipe, Cable |

---

### 3. RESOURCE FLOW (Phase 1)

**Hierarchy Flow:**
```
Unit produces → Sect storage → Colony reserves
                    ↑               ↓
              (deficit pull)  (surplus push)
```

**Thresholds:**
- Surplus push: When Sect storage > 80% → push to Colony
- Deficit pull: When Sect storage < 10% → request from Colony (fill to 30%)

**Storage Capacities:**
- Sect: 1000 per resource type
- Colony: 5000 per resource type
- Typed resources: 50 items per type (Sect), 100 items (Colony)

---

### 4. PRODUCTION MECHANICS

**Graceful Degradation:**
- Units operate at reduced efficiency when resources scarce
- Efficiency curve: 0% resources → ~50% efficiency
- Production and consumption scale proportionally
- Only stops when 0 resources available

**MANPOWER:**
- Constant 10 per Sect (no population system)
- Available for unit operations

**Ambient Energy:**
- Solar-based generation varies by time of day
- Peak (noon): 2x multiplier
- Night: 0.1x multiplier

---

### 5. MODULE SYSTEM

**Multiple Active Modules:**
- Units can run multiple modules simultaneously
- Each module has independent consumption/production rates
- `activeModuleIndices` set tracks which modules are running

**Manufacturing Modules (new):**
| Module | Output Type | Variants |
|--------|-------------|----------|
| MachineryFab | MACHINERY | HeavyDrill, Conveyor, Assembler |
| ElectronicsFab | ELECTRONICS | Sensor, Controller, Computer |
| Smelter | ALLOYS | Steel, Bronze, Aluminum, Titanium |
| Prefabricator | CONSTRUCTION_MATERIALS | Beam, Panel, Pipe, Cable |

---

### 6. DATA-DRIVEN SYSTEM

**game_types.toml contains:**
- 11 singular resources with colors and values
- 4 typed resource categories with 14 subtypes
- Variant-specific production costs
- 5 unit types with module lists
- 18 module definitions with production/consumption rates

---

### 7. VIEW SYSTEM

| View | Key | Action |
|------|-----|--------|
| Planet | - | Strategic overview |
| Colony | ESC from Sect | Tactical view |
| Sect | S or double-click | Operational view |
| Unit | U or double-click | Detail view |

**Texture Rendering:**
- Dome and units render with textures (fallback to circles)
- Active units show green ring indicator
- Moon surface tiles for background

---

### 8. ENGINE ARCHITECTURE

```
Engine
  ├─ InputManager   (double-click, keyboard)
  ├─ ViewManager    (camera, transitions)
  ├─ GameManager    (selection, building, SelectDefaultUnit)
  └─ RenderManager  (views, UI, moon tiles)
```

---

### 9. KEY CONSTANTS

```cpp
// Storage
SECT_BASE_STORAGE = 1000.0f
COLONY_BASE_RESERVES = 5000.0f
STORAGE_SURPLUS_THRESHOLD = 0.8f   // 80%
STORAGE_DEFICIT_THRESHOLD = 0.1f   // 10%
DEFICIT_REQUEST_AMOUNT = 0.3f      // Fill to 30%

// Manpower & Energy
SECT_BASE_MANPOWER = 10.0f
BASE_AMBIENT_ENERGY = 1.0f
SOLAR_PEAK_MULTIPLIER = 2.0f
SOLAR_MIN_MULTIPLIER = 0.1f

// Time
TICK_DURATION = 1.0f
TICKS_PER_DAY = 20
```

---

### 10. COMMITS THIS SESSION

```
741a057 Implement Phase 1 core resource flow
0d7ff0f Complete Phase 0: Typed resources and ambient energy
40f00e2 Implement multiple simultaneous active modules (Checkpoint 2)
9f0d189 Improve Engine with debug logging, auto-select, and render fixes
9a58b56 Add strategic reserves and resource management to Colony
b2ee8cc Add texture-based rendering and storage capacity to Sect
265508d Add data-driven type system with game_types.toml
```

---

**Build:** `cmake -B build && cmake --build build`
**Run:** `./build/src/colony_game`
