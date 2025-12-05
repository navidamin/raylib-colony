# SNAPSHOT 001 | 5-Dec-25
## Current Game Features & Mechanics

---

### 1. ENTITY HIERARCHY

```
Planet (20x20 grid)
  └─ Colony (groups sects, pools resources)
      └─ Sect (settlement with units, local storage)
          └─ Unit (production building with modules)
              └─ Module (upgradeable production component)
```

**Planet**
- 20x20 grid (PLANET_SIZE)
- Cell size: 100 units (SECT_CORE_RADIUS * 2)
- Procedural resource distribution (cluster-based)
- Resource abundances per cell: 0.0-1.0

**Colony**
- Groups multiple Sects
- Strategic reserves storage (5000 capacity per resource)
- Jurisdiction radius calculation
- Road network between sects

**Sect**
- 8 unit slots around central dome
- Local resource storage (1000 capacity per resource)
- Surplus push to Colony at 80% capacity
- Texture-based rendering (dome + unit icons)

**Unit**
- Types: Extraction, Farming, Energy, Construction, Transport, Manufacture, Research, Commerce
- Multiple simultaneous active modules
- Production cycle system
- Construction state tracking

---

### 2. RESOURCE SYSTEM

**Tier 1 - Raw Materials**
| Resource | Source | Color |
|----------|--------|-------|
| H2 | Extraction | Light Blue |
| O2 | Extraction | Light Red |
| C | Extraction | Dark Gray |
| Fe | Extraction | Brown |
| Si | Extraction | Greenish Gray |

**Tier 2 - Processed**
| Resource | Source |
|----------|--------|
| WATER | Processing |
| FOOD | Farming |
| BIOFUEL | Farming |
| ENERGY | Energy units |

**Tier 3 - Abstract**
| Resource | Source |
|----------|--------|
| SCIENCE | Research |
| MANPOWER | Population (planned) |

**Storage Hierarchy**
- Unit → Sect storage (direct deposit)
- Sect → Colony reserves (when >80% full)
- Capacities: Sect=1000, Colony=5000 per resource

---

### 3. PRODUCTION MECHANICS

**Production Costs (per unit output)**
```
Extraction (any raw):  1.0 ENERGY
Farming FOOD:          0.5 WATER + 0.2 ENERGY
Farming BIOFUEL:       0.5 WATER + 1.0 FOOD
```

**Module System**
- Each unit has upgradeable modules
- Modules have: level, efficiency, production/consumption rates
- Multiple modules can be active simultaneously
- Consumption calculated per-module, then aggregated

**Time System**
- 1 tick = 1 second (TICK_DURATION)
- 20 ticks = 1 game day (TICKS_PER_DAY)
- Production processed per tick

---

### 4. VIEW SYSTEM

| View | Scale | Navigation |
|------|-------|------------|
| Menu | - | Game start |
| Planet | Strategic | ESC from Colony |
| Colony | Tactical | Double-click colony / ESC from Sect |
| Sect | Operational | Double-click sect / S key |
| Unit | Detail | Double-click unit / U key |

**Camera Controls**
- Zoom: Mouse wheel
- Pan: Arrow keys / drag
- Double-click: Drill down
- ESC: Zoom out

---

### 5. ENGINE ARCHITECTURE

```
Engine (main loop)
  ├─ InputManager   (mouse, keyboard, double-click detection)
  ├─ ViewManager    (camera, view transitions)
  ├─ GameManager    (entity selection, building placement)
  └─ RenderManager  (drawing all views, UI)
```

**Game Loop:** HandleInput() → Update() → Draw()

---

### 6. DATA-DRIVEN SYSTEM

**game_types.toml**
- 11 singular resources defined
- 5 unit types with descriptions
- 14 modules with production rates
- Asset paths for textures

**GameTypesLoader**
- Singleton pattern
- Methods: LoadFromFile(), GetUnitTypeDef(), GetModuleDef(), GetResourceTypeDef()
- Enables modding without recompilation

---

### 7. VISUAL RENDERING

**Texture-Based (Sect View)**
- Dome: Dome_off.png
- Units: extractionX256.png, FarmX256.png, powerX256.png, waterX256.png, manfuctrungX256.png
- Fallback: Circle primitives if texture missing
- Active indicator: Green ring around unit

**Tile-Based (Planet/Colony View)**
- Moon surface tiles: moonsurface_tile1/2/3.png
- Procedural tile pattern generation

---

### 8. UI ELEMENTS

**Sect View**
- Central dome with development percentage
- 8 orbital unit positions
- Resource storage panel (right side)
- Unit status indicators

**Unit View**
- Module list with levels
- Production/consumption rates table
- Resource sliders for rate control
- Upgrade buttons

---

### 9. BUILDING SYSTEM

**BuildNewColony**
- Click empty area in Planet view
- Checks jurisdiction overlap
- Creates initial sect

**BuildNewSect**
- Click within colony in Colony view
- Creates sect with 8 default units
- Auto-starts Extraction unit

---

### 10. CONSTANTS

```cpp
SECT_CORE_RADIUS      = 50.0f
PLANET_SIZE           = 20
SECT_BASE_STORAGE     = 1000.0f
COLONY_BASE_RESERVES  = 5000.0f
STORAGE_SURPLUS_THRESHOLD = 0.8f (80%)
TICK_DURATION         = 1.0f
TICKS_PER_DAY         = 20
```

---

**Build:** cmake -B build && cmake --build build
**Run:** ./build/src/colony_game
