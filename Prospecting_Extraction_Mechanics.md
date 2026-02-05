# EXTRACTION UNIT: Complete Gameplay Design v2

## Overview

This document presents the complete, scientifically-grounded gameplay design for the Extraction Unit in **Colony**. It covers:

1. **Pre-Game: Colony Site Selection** (before the first Sect exists)
2. **Five Integrated Modules** with organic progression
3. **Scientific Accuracy** based on real lunar ISRU research
4. **Dependency Chains** linking to other Units
5. **Strategic Focus Integration** with Sect/Colony doctrines

---

# PART 1: PRE-GAME — COLONY SITE SELECTION

## The Landing Decision

Before the game begins (or when establishing a new Colony), the player must select a landing site. This is a **critical strategic decision** that affects the entire Colony's resource profile.

### Site Selection Interface

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  COLONY SITE SELECTION                                    Press [ENTER] to confirm │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                          ORBITAL SURVEY DATA                                │   │
│  │                                                                             │   │
│  │      ░░░░░░▓▓▓▓▓▓░░░░░░░░░░░░░░░░                                         │   │
│  │    ░░░░░▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░                                        │   │
│  │   ░░░░▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░███████░░░░        LEGEND:                        │   │
│  │   ░░▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░████████████░         ▓ = Mare basalt (Fe, Ti rich) │   │
│  │   ░░▓▓▓▓▓▓▓(✦)▓▓▓▓░░░░░█████████░░░         █ = Highland (Si, Al, Ca)     │   │
│  │    ░░▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░██████░░░░         ░ = Mixed terrain             │   │
│  │     ░░░▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░          ● = PSR (water ice possible)  │   │
│  │       ░░░░░░░░░░░░░░░░░░░░●●●░░░░░          ✦ = Cursor position           │   │
│  │         ░░░░░░░░░░░░░░░●●●●●●●░░░░                                         │   │
│  │           ░░░░░░░░░░░●●●●●●●●●░░░░                                         │   │
│  │                                                                             │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                     │
│  ┌─ ORBITAL INSTRUMENTS ────────────────────────────────────────────────────────┐  │
│  │                                                                              │  │
│  │  GAMMA-RAY SPECTROMETER        NEUTRON SPECTROMETER       THERMAL MAPPER    │  │
│  │  ┌────────────────────┐        ┌──────────────────┐       ┌──────────────┐  │  │
│  │  │ Fe:  ████████░░ 14%│        │ H:  ███░░░░░ 0.8%│       │ Day: +127°C  │  │  │
│  │  │ Ti:  ██████░░░░ 11%│        │ (water proxy)    │       │ Night: -173°C│  │  │
│  │  │ Si:  ██████████ 21%│        │                  │       │              │  │  │
│  │  │ Al:  ████░░░░░░  7%│        │ Ice likelihood:  │       │ Solar:       │  │  │
│  │  │ Ca:  ███░░░░░░░  5%│        │ LOW              │       │ ███████░ 72% │  │  │
│  │  │ Th:  █░░░░░░░░░ 2ppm│       └──────────────────┘       └──────────────┘  │  │
│  │  │ K:   ██░░░░░░░░ 800ppm                                                   │  │
│  │  └────────────────────┘                                                     │  │
│  │                                                                              │  │
│  └──────────────────────────────────────────────────────────────────────────────┘  │
│                                                                                     │
│  ┌─ SITE ASSESSMENT ────────────────────────────────────────────────────────────┐  │
│  │                                                                              │  │
│  │  LOCATION: 18.7°S, 23.4°W (Mare Cognitum)                                   │  │
│  │                                                                              │  │
│  │  TERRAIN:        ████████░░ Flat (5° avg slope)     ✓ Safe landing          │  │
│  │  SOLAR ACCESS:   ███████░░░ 72% illumination        ✓ Good for energy       │  │
│  │  EARTH COMMS:    █████████░ 94% visibility          ✓ Reliable contact      │  │
│  │  RESOURCE SCORE: ████████░░ High Fe/Ti              ✓ Industrial focus      │  │
│  │  WATER ACCESS:   ██░░░░░░░░ Low (no nearby PSR)     ⚠ Will need import      │  │
│  │                                                                              │  │
│  │  RECOMMENDED DOCTRINE: INDUSTRIAL (manufacturing, export)                   │  │
│  │                                                                              │  │
│  └──────────────────────────────────────────────────────────────────────────────┘  │
│                                                                                     │
│  [SCAN DEEPER] (costs time)    [COMPARE SITES]    [CONFIRM LANDING]               │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### Orbital Survey Instruments (Scientific Basis)

| Instrument | Real Technology | Game Function |
|------------|-----------------|---------------|
| **Gamma-Ray Spectrometer** | Lunar Prospector GRS | Shows elemental composition (Fe, Ti, Si, Al, Ca, Th, K, U) |
| **Neutron Spectrometer** | Lunar Prospector NS | Detects hydrogen (proxy for water ice) |
| **Thermal Mapper** | LRO Diviner | Shows temperature range, solar illumination % |
| **Laser Altimeter** | LOLA | Shows terrain slope, crater density |
| **Radar** | Mini-RF | Detects subsurface ice deposits in PSRs |

### Site Selection Criteria

| Factor | Measurement | Trade-off |
|--------|-------------|-----------|
| **Terrain Slope** | Degrees average | Flat = safe landing & easy traversal, but crater rims have better illumination |
| **Solar Illumination** | % of lunar day with sun | Higher = more energy, but equatorial sites have 14-day nights |
| **Earth Visibility** | % of time with line-of-sight | Higher = better communications |
| **Resource Composition** | Elemental abundances | Mare = Fe/Ti rich; Highlands = Si/Al/Ca rich |
| **Water Proximity** | Distance to nearest PSR | Closer = easier volatile access, but PSRs are dark and cold |
| **KREEP Presence** | Th, K, rare earths | High = valuable rare elements, but radioactive |

### Site Archetypes

| Site Type | Location | Resources | Strategic Focus |
|-----------|----------|-----------|-----------------|
| **Mare Industrial** | Low-latitude mare | High Fe, Ti, moderate Si | Manufacturing, export |
| **Highland Construction** | Highland plains | High Si, Al, Ca | Construction materials, glass |
| **Polar Volatile** | Near south pole | Water ice access, variable | Life support, fuel production |
| **KREEP Scientific** | Procellarum KREEP Terrane | Rare earths, Th, K | Research, high-value exports |
| **Lava Tube Shelter** | Mare with skylights | Mixed, protected terrain | Habitat, radiation shielding |

### Site Selection Mechanics

| Action | Input | Effect |
|--------|-------|--------|
| **Pan Map** | Arrow keys / drag | Move view across planet |
| **Hover Location** | Mouse position | Shows orbital data for that point |
| **Scan Deeper** | Click button | Increases data resolution but costs game time |
| **Compare Sites** | Mark multiple locations | Side-by-side comparison panel |
| **Confirm Landing** | Click + Enter | Commits to site, game begins |

---

# PART 2: THE FIVE INTEGRATED MODULES

## Module Architecture

All five modules are **visible simultaneously** from game start. Modules begin in basic state and evolve through upgrades requiring resources from other Units.

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  EXTRACTION UNIT                                        Power: 425/500 kW    [?]    │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  ┌─ PROSPECTING ─────────────────┐  ┌─ EXCAVATION ───────────────────────────────┐ │
│  │  "Where to dig"               │  │  "How to dig"                              │ │
│  │  Analyze regolith composition │  │  Collect bulk regolith                     │ │
│  └───────────────────────────────┘  └────────────────────────────────────────────┘ │
│                                                                                     │
│  ┌─ BENEFICIATION ──────────────────────────────────────────────────────────────┐  │
│  │  "How to separate" — Extract usable resources from regolith mix              │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
│                                                                                     │
│  ┌─ OPERATIONS ─────────────────────┐  ┌─ DIRECTIVES ────────────────────────────┐│
│  │  "When to operate"               │  │  "Autonomous priorities"                ││
│  └───────────────────────────────────┘  └────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## MODULE 1: PROSPECTING

### Scientific Foundation

Real lunar prospecting uses multiple complementary techniques:

| Method | Range | Penetration | Elements Detected |
|--------|-------|-------------|-------------------|
| **LIBS** (Laser-Induced Breakdown Spectroscopy) | 1.5-7m stand-off | Surface only | Major elements (Si, Fe, Ti, Al, Ca, Mg) |
| **Raman Spectroscopy** | Contact-2m | Surface minerals | Mineral identification, hydration state |
| **Neutron Detection** | Ground-contact | 1m depth | Hydrogen (water proxy) |
| **Gamma-Ray** | Ground-contact | 30cm depth | Radioactive elements (U, Th, K) |
| **Ground-Penetrating Radar** | Mobile | 10m depth | Subsurface structure, ice deposits |

### Tier Structure

| Tier | Name | Capability | Dependencies |
|------|------|------------|--------------|
| **0** | Visual Estimation | Surface color hints only, 40% accuracy | None |
| **1** | LIBS Scanner | Point analysis at 2m, major elements | Energy: 50 kW |
| **2** | Multi-Spectral Suite | LIBS + Raman + Neutron, mineral ID | Energy: 150 kW, Research: Spectroscopy |
| **3** | Deep Survey Array | GPR + full spectroscopy, 10m depth | Energy: 300 kW, Research: Geophysics, Manufacture: Sensor Arrays |

### Tier 0 Interface (Game Start)

```
┌─────────────────────────────────────────────────────────────────┐
│  PROSPECTING MODULE                                             │
│  ──────────────────                                             │
│                                                                 │
│  Status: VISUAL ESTIMATION ONLY                                 │
│                                                                 │
│  ┌─────┬─────┬─────┬─────┬─────┐                               │
│  │     │     │  ▓  │     │     │   Surface albedo hints:       │
│  ├─────┼─────┼─────┼─────┼─────┤                               │
│  │     │  ▓  │  █  │  ▓  │     │   ░ = Light (highland-like)   │
│  ├─────┼─────┼─────┼─────┼─────┤   ▓ = Medium (mixed)          │
│  │     │  ▓  │  ▓  │     │     │   █ = Dark (mare-like, Fe/Ti) │
│  ├─────┼─────┼─────┼─────┼─────┤                               │
│  │     │     │     │     │     │   Darker ≈ more ilmenite      │
│  └─────┴─────┴─────┴─────┴─────┘   (but this is unreliable!)   │
│                                                                 │
│  ⚠ Accuracy: ~40%. Dig and analyze samples to learn more.     │
│                                                                 │
│  [LIBS Scanner requires: Energy Unit online]                   │
└─────────────────────────────────────────────────────────────────┘
```

### Tier 1 Interface (LIBS Available)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  PROSPECTING MODULE                                    Power: 50/50 kW      │
│  ──────────────────                                                         │
│                                                                             │
│  ┌─────────────────────────────────┐  ┌──────────────────────────────────┐ │
│  │     SURVEY GRID                 │  │ LIBS ANALYSIS                    │ │
│  │                                 │  │ ─────────────                    │ │
│  │  ┌───┬───┬───┬───┬───┐        │  │                                  │ │
│  │  │ · │ · │ ▓ │ · │ · │        │  │ Target: C3 (click to analyze)   │ │
│  │  ├───┼───┼───┼───┼───┤        │  │                                  │ │
│  │  │ · │ ▓ │[█]│ ▓ │ · │        │  │ LASER PULSE ════════▶ ⚡        │ │
│  │  ├───┼───┼───┼───┼───┤        │  │                                  │ │
│  │  │ · │ ▓ │ ▓ │ · │ · │        │  │ SPECTRAL ANALYSIS:              │ │
│  │  ├───┼───┼───┼───┼───┤        │  │ ┌────────────────────────────┐  │ │
│  │  │ · │ · │ · │ · │ · │        │  │ │ Fe:  ████████░░  16.2%     │  │ │
│  │  └───┴───┴───┴───┴───┘        │  │ │ Ti:  ██████░░░░  10.8%     │  │ │
│  │                                 │  │ │ Si:  █████████░  19.4%     │  │ │
│  │  [█] = selected cell           │  │ │ Al:  ████░░░░░░   7.1%     │  │ │
│  │  Click any cell to LIBS scan   │  │ │ Ca:  ███░░░░░░░   5.3%     │  │ │
│  │                                 │  │ │ Mg:  ██░░░░░░░░   4.2%     │  │ │
│  └─────────────────────────────────┘  │ │ O:   ██████████████ 42.1%  │  │ │
│                                        │ └────────────────────────────┘  │ │
│  SCAN HISTORY:                         │                                  │ │
│  • C3: 16% Fe, 11% Ti (HIGH)          │ Ilmenite content: ~18% (GOOD)    │ │
│  • B2: 8% Fe, 4% Ti (LOW)             │                                  │ │
│  • D4: 12% Fe, 9% Ti (MEDIUM)         │ [MARK FOR EXCAVATION]            │ │
│                                        └──────────────────────────────────┘ │
│                                                                             │
│  Scan time: 3 sec/point   Accuracy: ±2%                                    │
│                                                                             │
│  [Multi-spectral requires: Research - Spectroscopy]                        │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Tier 2 Interface (Multi-Spectral Suite)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  PROSPECTING MODULE                                          Power: 150/150 kW      │
│  ──────────────────                                                                 │
│                                                                                     │
│  ┌─────────────────────────────┐  ┌─ INSTRUMENT PANEL ─────────────────────────────┐│
│  │     COMPOSITE MAP           │  │                                                ││
│  │                             │  │  [LIBS]  [RAMAN]  [NEUTRON]  ← click to switch ││
│  │    ░░▒▒▒▓▓▓░░░░░           │  │                                                ││
│  │   ░▒▒▒▓▓▓▓▓▓▒▒░░░          │  │  ┌─ RAMAN SPECTROSCOPY ─────────────────────┐ ││
│  │  ░▒▒▓▓▓███▓▓▓▒▒░░          │  │  │                                          │ ││
│  │  ░▒▓▓▓██[█]██▓▓▒░          │  │  │  MINERAL IDENTIFICATION:                 │ ││
│  │  ░▒▒▓▓▓███▓▓▓▒▒░░          │  │  │                                          │ ││
│  │   ░▒▒▒▓▓▓▓▓▓▒▒░░░          │  │  │  ████████████ Ilmenite (FeTiO₃)    34%  │ ││
│  │    ░░▒▒▒▓▓▓▒▒░░░░          │  │  │  ████████░░░░ Plagioclase          28%  │ ││
│  │                             │  │  │  ██████░░░░░░ Pyroxene             21%  │ ││
│  │  Overlay: [ILMENITE %]     │  │  │  ████░░░░░░░░ Olivine              11%  │ ││
│  │                             │  │  │  ██░░░░░░░░░░ Glass/Agglutinates   6%  │ ││
│  └─────────────────────────────┘  │  │                                          │ ││
│                                    │  │  Hydration: NONE DETECTED               │ ││
│  NEUTRON DETECTOR:                 │  └──────────────────────────────────────────┘ ││
│  ┌───────────────────────────┐    │                                                ││
│  │ Thermal neutrons: ███░░░░ │    │  EXTRACTABLE RESOURCES (with processing):     ││
│  │ Epithermal:       ████░░░ │    │  • Fe: ~14% (from ilmenite + pyroxene)        ││
│  │                           │    │  • Ti: ~6% (from ilmenite)                    ││
│  │ Hydrogen signal: LOW      │    │  • O₂: ~42% (bound in all oxides)             ││
│  │ (no significant water)    │    │  • Si: ~19% (from plagioclase)                ││
│  └───────────────────────────┘    │                                                ││
│                                    │  QUALITY RATING: ★★★★☆ (excellent for Fe/Ti)  ││
│  MARKED SITES:                     │                                                ││
│  ★ C3 (34% ilmenite) - PRIORITY   │  [MARK]  [UNMARK]  [COMPARE]                   ││
│  ★ D4 (28% ilmenite)              └────────────────────────────────────────────────┘│
│  ☆ B2 (12% ilmenite) - LOW                                                          │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### Prospecting Mechanics Summary

| Action | Input | Tier | Effect |
|--------|-------|------|--------|
| View surface hints | Hover | 0+ | Shows albedo-based estimate |
| LIBS point scan | Click cell | 1+ | Elemental analysis, 3 sec |
| Switch instrument | Click tab | 2+ | Change analysis mode |
| Raman mineral ID | Click cell (Raman mode) | 2+ | Identify minerals, hydration |
| Neutron hydrogen | Passive area scan | 2+ | Detect water signatures |
| GPR subsurface | Drag scan line | 3+ | 10m depth profile |
| Mark for excavation | Click MARK | 1+ | Adds to excavation queue |
| Compare sites | Select multiple, COMPARE | 1+ | Side-by-side view |

---

## MODULE 2: EXCAVATION

### Scientific Foundation

Real lunar excavation faces unique challenges:

| Challenge | Cause | Mitigation |
|-----------|-------|------------|
| **Compaction at depth** | Impact gardening | Percussive/vibratory tools |
| **Abrasive dust** | Sharp, glassy particles | Sealed bearings, replaceable parts |
| **Low gravity** | 1/6 Earth | Reduced traction, need anchoring |
| **Vacuum** | No atmosphere | No pneumatic tools (except with carried gas) |
| **Thermal cycling** | ±300°C swing | Material stress, equipment wear |

### Excavation Methods (Scientific Basis)

| Method | Real Technology | Game Stats |
|--------|-----------------|------------|
| **Scoop** | Viking/Phoenix style | Simple, ≤10cm depth, low throughput |
| **Bucket Wheel** | Continuous excavator | Medium depth, high throughput, complex |
| **Percussive** | Honeybee Robotics | Breaks compacted soil, ≤25cm, high energy |
| **Auger/Drill** | TRIDENT, Chang'e-5 | Deep sampling, core extraction |
| **Pneumatic** | Gas-assisted excavation | Reduces force needed, requires gas supply |

### Tier Structure

| Tier | Name | Capability | Dependencies |
|------|------|------------|--------------|
| **0** | Manual Scoop | 1 excavator, ≤10cm, 30 kg/hr | None |
| **1** | Mechanized | 3 excavators, ≤15cm, bucket wheel option | Manufacture: Excavator Parts, Energy: 100 kW |
| **2** | Heavy Equipment | 5 excavators, ≤25cm, percussive option | Manufacture: Heavy Systems, Construction: Pads, Energy: 250 kW |
| **3** | Autonomous Fleet | 12 drones, zone control, ≤30cm | Manufacture: Drones, Research: Swarm AI, Energy: 500 kW |

### Tier 0 Interface

```
┌─────────────────────────────────────────────────────────────────────────┐
│  EXCAVATION MODULE                                                      │
│  ─────────────────                                                      │
│                                                                         │
│  ┌─────┬─────┬─────┬─────┬─────┐    ┌──────────────────────────────┐  │
│  │     │     │     │     │     │    │ EXCAVATOR #1 (SCOOP)         │  │
│  ├─────┼─────┼─────┼─────┼─────┤    │ ──────────────────           │  │
│  │     │     │ [E] │     │     │    │                              │  │
│  ├─────┼─────┼─────┼─────┼─────┤    │ Site: C3 ★                   │  │
│  │     │     │     │     │     │    │ Composition: 34% ilmenite    │  │
│  ├─────┼─────┼─────┼─────┼─────┤    │                              │  │
│  │     │     │     │     │     │    │ DEPTH:                       │  │
│  └─────┴─────┴─────┴─────┴─────┘    │ ◀━━●━━━━━━━▶                 │  │
│                                      │ 5cm       10cm (MAX)         │  │
│  [E] = excavator    ★ = marked site │                              │  │
│                                      │ RATE:                        │  │
│  Drag [E] to any cell to relocate   │ ◀━━━━●━━━━▶                  │  │
│  (30 second travel time)            │ slow       fast              │  │
│                                      │                              │  │
│  ┌────────────────────────────┐     │ Output: 28 kg/hour          │  │
│  │ EQUIPMENT                  │     │ (bulk regolith)              │  │
│  │ [E] Scoop #1 (active)     │     │                              │  │
│  │ [○] (locked)              │     │ Wear: ████░░░░░░ 38%         │  │
│  │ [○] (locked)              │     │                              │  │
│  └────────────────────────────┘     │ ⚠ Hitting compacted layer   │  │
│                                      │   at 8cm. Deeper needs       │  │
│                                      │   percussive equipment.      │  │
│                                      └──────────────────────────────┘  │
│                                                                         │
│  REGOLITH COLLECTED: 28 kg/hour ─────▶ BENEFICIATION                   │
│                                                                         │
│  [More excavators require: Manufacturing Unit]                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Tier 2 Interface (Multiple Methods)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  EXCAVATION MODULE                                           Power: 250/250 kW      │
│  ─────────────────                                                                  │
│                                                                                     │
│  ┌─────┬─────┬─────┬─────┬─────┐    ┌────────────────────────────────────────────┐│
│  │     │ [1] │ ★   │     │     │    │ EXCAVATOR FLEET                            ││
│  ├─────┼─────┼─────┼─────┼─────┤    │                                            ││
│  │     │     │ [2] │ ★   │     │    │ #  SITE  METHOD     DEPTH   OUTPUT   WEAR  ││
│  ├─────┼─────┼─────┼─────┼─────┤    │ ─────────────────────────────────────────  ││
│  │     │ ★   │     │ [3] │     │    │ 1  B2    Bucket     12cm    87 kg/h  15%   ││
│  ├─────┼─────┼─────┼─────┼─────┤    │ 2  C3    Percuss.   22cm    45 kg/h  31%   ││
│  │     │     │ [4] │     │ [5] │    │ 3  D4    Bucket     15cm    92 kg/h  18%   ││
│  └─────┴─────┴─────┴─────┴─────┘    │ 4  C4    Scoop       8cm    34 kg/h   8%   ││
│                                      │ 5  E5    Bucket     14cm    78 kg/h  22%   ││
│  [1-5] = excavators                  │                                            ││
│  ★ = marked high-value sites        │ TOTAL: 336 kg/hour bulk regolith           ││
│                                      └────────────────────────────────────────────┘│
│                                                                                     │
│  ┌─ SELECTED: EXCAVATOR #2 ─────────────────────────────────────────────────────┐  │
│  │                                                                              │  │
│  │  METHOD:  [SCOOP]  [BUCKET WHEEL]  [PERCUSSIVE]                             │  │
│  │                                     ↑ selected                              │  │
│  │                                                                              │  │
│  │  Percussive: 40x force reduction, accesses compacted layers, high energy   │  │
│  │                                                                              │  │
│  │  DEPTH:    ◀━━━━━━━━━━●━━▶        RATE:    ◀━━━●━━━━━━▶                    │  │
│  │            5cm           25cm               slow     fast                    │  │
│  │                                                                              │  │
│  │  DEPTH PROFILE (from Prospecting GPR):                                      │  │
│  │  0-10cm:   Loose regolith ████████████████████                              │  │
│  │  10-20cm:  Compacted ░░░░░░░░░░░░ (needs percussive)                        │  │
│  │  20-30cm:  Dense + rocks ▓▓▓▓▓▓ (slow going)                                │  │
│  │                                                                              │  │
│  │  Composition at depth: Deeper = less weathered, richer minerals            │  │
│  └──────────────────────────────────────────────────────────────────────────────┘  │
│                                                                                     │
│  AUTO-ROTATION: [ON] — depleted sites auto-switch to next ★ in queue              │
│                                                                                     │
│  REGOLITH FLOW: 336 kg/hour ─────────────────▶ BENEFICIATION MODULE               │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### Excavation Mechanics Summary

| Action | Input | Tier | Effect |
|--------|-------|------|--------|
| Position excavator | Drag to cell | 0+ | Move to location (travel time) |
| Assign to marked site | Drag to ★ | 1+ | Auto-position at surveyed location |
| Adjust depth | Drag slider | 0+ | Deeper = harder, richer, more wear |
| Adjust rate | Drag slider | 0+ | Faster = more output, more wear |
| Select method | Click method button | 1+ | Change excavation technique |
| Multi-select | Shift+Click | 2+ | Apply settings to group |
| Enable auto-rotation | Toggle | 2+ | Auto-move to next queued site when depleted |
| Zone painting | Drag on map | 3+ | Define autonomous operation zones |

---

## MODULE 3: BENEFICIATION

### Scientific Foundation

This is the CRITICAL module where resources are actually separated. Real lunar beneficiation uses a multi-stage process:

```
RAW REGOLITH
     │
     ▼
┌─────────────┐
│ SIZE SORT   │ ← Remove oversized particles (>1mm)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ MAGNETIC    │ ← Extract ferromagnetic particles (metallic Fe, some ilmenite)
│ SEPARATION  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ELECTROSTATIC│ ← Tribocharging separates by conductivity
│ SEPARATION  │   (ilmenite = conductive, silicates = insulating)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ THERMAL     │ ← Heat to release volatiles (H₂O, H₂) and reduce oxides
│ PROCESSING  │   Ilmenite + H₂ → Fe + TiO₂ + H₂O
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ MOLTEN      │ ← Melt regolith, electrolyze to extract pure metals + O₂
│ ELECTROLYSIS│   Most energy-intensive but produces pure Al, Fe, Si, O₂
└─────────────┘
```

### Separation Node Details (Scientific)

| Node | Process | Input | Output | Energy | Temperature |
|------|---------|-------|--------|--------|-------------|
| **Size Sort** | Vibrating sieve | Raw regolith | Sized fractions | 10 kW | Ambient |
| **Magnetic Sep.** | Drum separator | <1mm regolith | Fe particles, ilmenite concentrate | 25 kW | Ambient |
| **Electrostatic** | Tribocharging + plate sep. | Non-magnetic fraction | Ilmenite, plagioclase, olivine | 45 kW | Ambient |
| **Thermal (H₂ Red.)** | Hydrogen reduction | Ilmenite concentrate | Fe, TiO₂, H₂O (→ H₂ + O₂) | 150 kW | 900-1100°C |
| **Thermal (Volatiles)** | Heating | Icy regolith | H₂O, CO₂, H₂S | 80 kW | 50-300°C |
| **Carbothermal** | Carbon reduction | Regolith + C | CO, metals | 200 kW | 1600°C |
| **Molten Electrolysis** | MRE process | Molten regolith | Fe, Si, Al, Ti, O₂ (pure) | 400 kW | 1600°C |

### Tier Structure

| Tier | Name | Capability | Dependencies |
|------|------|------------|--------------|
| **0** | Direct Output | No processing, raw regolith only | None |
| **1** | Basic Separation | Size sort + magnetic, 60% efficiency | Construction: Separator Base, Energy: 75 kW |
| **2** | Processing Chain | +Electrostatic +Thermal, configurable | Construction: Plant, Manufacture: Nodes, Research: Reduction Chemistry, Energy: 200 kW |
| **3** | Refinery Complex | +MRE, purity control, waste reclaim | Construction: Refinery, Manufacture: Precision Equip, Research: Molten Electrolysis, Energy: 500 kW |

### Tier 2 Interface

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  BENEFICIATION MODULE                                        Power: 280/300 kW      │
│  ────────────────────                                                               │
│                                                                                     │
│  INTAKE: 336 kg/hour (from Excavation)                                             │
│  Regolith composition: 34% ilmenite, 28% plagioclase, 21% pyroxene, 17% other     │
│                                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                    SEPARATION CHAIN (drag nodes to reorder)                 │   │
│  │                                                                             │   │
│  │   ┌────┐    ┌────────┐    ┌──────────┐    ┌─────────┐    ┌─────────┐      │   │
│  │   │BULK│    │  SIZE  │    │ MAGNETIC │    │ ELECTRO │    │ THERMAL │      │   │
│  │   │ IN │───▶│  SORT  │───▶│   SEP    │───▶│ STATIC  │───▶│  H₂RED  │      │   │
│  │   │    │    │        │    │          │    │         │    │         │      │   │
│  │   │336 │    │  94%   │    │   82%    │    │   71%   │    │   78%   │      │   │
│  │   │kg/h│    │        │    │          │    │         │    │         │      │   │
│  │   └────┘    └───┬────┘    └────┬─────┘    └────┬────┘    └────┬────┘      │   │
│  │                 │              │               │              │            │   │
│  │                 ▼              ▼               ▼              ▼            │   │
│  │            ┌────────┐    ┌────────┐      ┌────────┐    ┌──────────┐       │   │
│  │            │OVERSIZE│    │Fe DUST │      │ILMENITE│    │ Fe + O₂  │       │   │
│  │            │(waste) │    │ 12/h   │      │CONC.   │    │ + TiO₂   │       │   │
│  │            │ 20/h   │    └────────┘      │(to next│    │          │       │   │
│  │            └────────┘                    │ stage) │    │ Fe: 28/h │       │   │
│  │                                          └────────┘    │ O₂: 47/h │       │   │
│  │                                                        │TiO₂:15/h │       │   │
│  │                                                        └──────────┘       │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                     │
│  NODE DETAIL: [THERMAL H₂ REDUCTION]  ← click any node to configure               │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                                                                             │   │
│  │  Reaction: FeTiO₃ + H₂ → Fe + TiO₂ + H₂O                                   │   │
│  │            2H₂O → 2H₂ + O₂ (electrolysis)                                   │   │
│  │                                                                             │   │
│  │  Temperature:   ◀━━━━━━━●━━━━▶    (optimal: 1050°C)                        │   │
│  │                 900°C      1200°C                                           │   │
│  │                                                                             │   │
│  │  H₂ Recycling:  [■] ON  (recycles H₂ from electrolysis back to reactor)   │   │
│  │                                                                             │   │
│  │  Efficiency: 78%    Wear: ███░░░░ 34%    Energy: 150 kW                    │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                     │
│  AVAILABLE NODES (drag to add):                                                    │
│  [SIZE SORT] [MAGNETIC] [ELECTROSTATIC] [THERMAL H₂] [THERMAL VOL.] [MRE locked]  │
│                                                                                     │
│  MAINTENANCE: ○ ○ ● ○ ○   MAGNETIC needs service (efficiency: 82% → 92%)          │
│                                                                                     │
│  OUTPUT SUMMARY: Fe: 40/h  O₂: 47/h  TiO₂: 15/h  Si: 0/h (need electrostatic→MRE) │
│  WASTE: 234 kg/h (can add more stages to recover)                                  │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### Chain Sequence Optimization

The ORDER of nodes matters significantly:

| Sequence | Efficiency | Why |
|----------|------------|-----|
| SIZE → MAG → ELEC → THERMAL | Optimal | Remove debris first, concentrate ilmenite, then process |
| THERMAL → MAG | Poor | Heating unsorted material wastes 60% of energy |
| MAG → SIZE | Poor | Magnetic picks up oversized particles, jams |
| ELEC → MAG | Suboptimal | Electrostatic works better on already magnetically cleaned feed |

### Beneficiation Mechanics Summary

| Action | Input | Tier | Effect |
|--------|-------|------|--------|
| Adjust intake | Drag dial | 1+ | More regolith = more throughput, more energy |
| Add node | Drag from available | 1+ | Extend processing capability |
| Remove node | Drag off chain | 1+ | Simplify (and reduce energy) |
| Reorder nodes | Drag within chain | 2+ | Change sequence (affects efficiency) |
| Configure node | Click node | 2+ | Adjust temperature, parameters |
| Toggle recycling | Checkbox | 2+ | Recycle byproducts (H₂, heat) |
| Assign maintenance | Drag worker | 2+ | Restore efficiency |
| Adjust purity | Click output, drag slider | 3+ | Higher purity = slower, better quality |

---

## MODULE 4: OPERATIONS (Scheduling)

### Scientific Foundation

Lunar operations are constrained by:

| Factor | Challenge | Mitigation |
|--------|-----------|------------|
| **14-day night** (equatorial) | No solar power | Energy storage, nuclear, or polar location |
| **Thermal cycling** | Equipment stress | Schedule maintenance, thermal management |
| **Dust accumulation** | Abrasion, contamination | Regular cleaning cycles |
| **Solar flares** | Radiation, equipment damage | Shelter periods, hardened equipment |

### Thermal Windows

| Process | Optimal Time | Reason |
|---------|--------------|--------|
| Thermal processing | Lunar day | Solar assist, ambient heat helps |
| Volatile extraction (polar) | Constant | PSRs are always cold |
| Heavy excavation | Lunar night (equatorial) | Cooler = less thermal stress |
| Maintenance | Dawn/dusk transition | Moderate temperatures |

### Tier Structure

| Tier | Name | Capability | Dependencies |
|------|------|------------|--------------|
| **0** | Continuous | No scheduling, everything runs 24/7, -15% efficiency | None |
| **1** | Thermal Awareness | See thermal windows, manual start/stop | Energy: 100 kW (sensors) |
| **2** | Shift Scheduling | Drag-block scheduling, maintenance windows | Manufacture: Timers, Construction: Control Room |
| **3** | Predictive AI | AI suggests optimal schedules, auto-adjusts | Research: Operations AI |

### Tier 2 Interface

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  OPERATIONS MODULE                                                                  │
│  ─────────────────                                                                  │
│                                                                                     │
│  LUNAR CYCLE: [████████████░░░░░░░░░░░░░░░░]  Day 12 of 28                        │
│               ↑ NOW                                                                 │
│                                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │ SHIFT SCHEDULE (drag blocks to assign)                                      │   │
│  │                                                                             │   │
│  │              │D1│D2│D3│D4│D5│D6│D7│D8│D9│10│11│12│13│14│N1│N2│...│N14│     │   │
│  │ ─────────────┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼───┼───│     │   │
│  │ EXCAVATOR 1  │██│██│██│██│██│██│██│██│██│██│██│██│░░│░░│██│██│...│██ │     │   │
│  │ EXCAVATOR 2  │██│██│██│██│██│██│░░│░░│██│██│██│██│██│██│░░│░░│...│░░ │     │   │
│  │ EXCAVATOR 3  │██│██│██│██│██│██│██│██│██│██│██│██│██│██│██│██│...│██ │     │   │
│  │ ─────────────┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼───┼───│     │   │
│  │ MAGNETIC SEP │██│██│██│██│██│██│██│██│██│██│██│██│██│██│██│██│...│██ │     │   │
│  │ ELECTROSTATIC│██│██│██│██│██│██│██│██│██│██│██│██│██│██│░░│░░│...│░░ │     │   │
│  │ THERMAL PROC │██│██│██│██│██│██│██│██│██│██│██│██│██│██│░░│░░│...│░░ │ +30%│   │
│  │ ─────────────┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼───┼───│     │   │
│  │ MAINTENANCE  │  │  │  │  │  │  │MM│  │  │  │  │  │  │  │  │  │MM │   │     │   │
│  │                                                                             │   │
│  │  ██ = active   ░░ = standby   MM = maintenance window                      │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                     │
│  THERMAL BONUS: Thermal processing during day = +30% efficiency (solar assist)    │
│  NIGHT PENALTY: All processes during night cost +20% energy (heating required)    │
│                                                                                     │
│  TEMPLATES: [24/7] [Day Only] [Night Excavate + Day Process] [Energy Saving]      │
│                                                                                     │
│  CURRENT EFFICIENCY GAIN FROM SCHEDULING: +18%                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## MODULE 5: DIRECTIVES (Autonomous Control)

### Purpose

When player attention is elsewhere (managing other Sects, other Colonies), the Directive system keeps the Extraction Unit running intelligently.

### Directive Cards

| Directive | Effect | Trade-off |
|-----------|--------|-----------|
| **PRIORITIZE [Resource]** | 70% of processing targets that resource | Other resources reduced |
| **MAXIMIZE THROUGHPUT** | All systems at max rate | High wear, high energy |
| **CONSERVE EQUIPMENT** | Lower rates, more maintenance | Lower output |
| **CONSERVE ENERGY** | 60% power mode | Slower extraction |
| **QUALITY OVER QUANTITY** | High purity settings | Much slower output |
| **EXPLORATION MODE** | More prospecting, less extraction | Better future yields |
| **EMERGENCY HARVEST** | All-out extraction | Massive wear spike |
| **THERMAL SYNC** | Follow day/night optimization | Timing constraints |
| **BALANCE OUTPUTS** | Even resource distribution | No specialization |

### Tier 3 Interface (Full Autonomy)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  DIRECTIVES MODULE                                           Status: AUTONOMOUS     │
│  ─────────────────                                                                  │
│                                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │ AUTONOMOUS OPERATION SUMMARY                                                │   │
│  │                                                                             │   │
│  │ The Extraction Unit is self-managing based on:                             │   │
│  │ • Your active directives (below)                                           │   │
│  │ • Colony-wide resource needs (from Core)                                   │   │
│  │ • Current equipment status and wear levels                                 │   │
│  │ • Thermal conditions and energy availability                               │   │
│  │                                                                             │   │
│  │ AI ASSESSMENT: "Fe output meeting Manufacturing needs. Recommend           │   │
│  │ pivoting 20% capacity to O₂ for upcoming Transport Unit construction."    │   │
│  │                                                                             │   │
│  │ [ACCEPT RECOMMENDATION]  [DISMISS]  [DETAILS]                             │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                     │
│  ACTIVE DIRECTIVES (drag to reorder priority):                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │  1. [PRIORITIZE Fe    ] ██████████████████████████                         │   │
│  │  2. [THERMAL SYNC     ] ████████████████░░░░░░░░░░                         │   │
│  │  3. [CONSERVE EQUIPMT ] ██████████░░░░░░░░░░░░░░░░                         │   │
│  │  4. [─────────────────]                                                    │   │
│  │                         ↑ weight decreases down the list                   │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                     │
│  AVAILABLE DIRECTIVES (drag to active):                                            │
│  [PRIORITIZE ▼] [MAXIMIZE THROUGHPUT] [CONSERVE ENERGY] [QUALITY>QUANTITY]        │
│  [EXPLORATION MODE] [EMERGENCY HARVEST] [BALANCE OUTPUTS]                          │
│                                                                                     │
│  CONFLICT DETECTION: ⚠ None                                                       │
│  (Note: MAXIMIZE THROUGHPUT + CONSERVE EQUIPMENT would conflict)                   │
│                                                                                     │
│  EXCEPTION LOG (last 24 hours):                                                    │
│  • 2h ago: Excavator #3 auto-relocated (site C3 depleted)                         │
│  • 5h ago: Thermal processor maintenance auto-scheduled                           │
│  • 8h ago: Night mode activated, excavation continued, processing paused          │
│                                                                                     │
│  [MANUAL OVERRIDE]  [VIEW FULL LOGS]  [PERFORMANCE REPORT]                        │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

# PART 3: COMPLETE DEPENDENCY MATRIX

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                    EXTRACTION UNIT — COMPLETE DEPENDENCY MATRIX                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  MODULE          TIER 1                TIER 2                TIER 3                │
│  ───────────────────────────────────────────────────────────────────────────────── │
│                                                                                     │
│  PROSPECTING     Energy: 50kW          Energy: 150kW         Energy: 300kW         │
│                  (LIBS scanner)        Research:             Research:             │
│                                        Spectroscopy          Geophysics            │
│                                                              Manufacture:          │
│                                                              Sensor Arrays         │
│  ───────────────────────────────────────────────────────────────────────────────── │
│                                                                                     │
│  EXCAVATION      Manufacture:          Manufacture:          Manufacture:          │
│                  Excavator Parts       Heavy Systems         Extraction Drones     │
│                  Energy: 100kW         Construction:         Research:             │
│                                        Equipment Pads        Swarm AI              │
│                                        Energy: 250kW         Energy: 500kW         │
│  ───────────────────────────────────────────────────────────────────────────────── │
│                                                                                     │
│  BENEFICIATION   Construction:         Construction:         Construction:         │
│                  Separator Base        Processing Plant      Refinery Complex      │
│                  Manufacture:          Manufacture:          Manufacture:          │
│                  Magnetic Drum         Thermal Chamber       MRE Reactor           │
│                  Energy: 75kW          Electrostatic Plates  Precision Equipment   │
│                                        Research:             Research:             │
│                                        Reduction Chemistry   Molten Electrolysis   │
│                                        Energy: 200kW         Energy: 500kW         │
│  ───────────────────────────────────────────────────────────────────────────────── │
│                                                                                     │
│  OPERATIONS      Energy: 100kW         Manufacture:          Research:             │
│                  (thermal sensors)     Timer Systems         Operations AI         │
│                                        Construction:                               │
│                                        Control Room                                │
│  ───────────────────────────────────────────────────────────────────────────────── │
│                                                                                     │
│  DIRECTIVES      Research:             Research:             Research:             │
│                  Basic Automation      Advanced Automation   AI Governance         │
│                                        Manufacture:          + 100 hours of        │
│                                        Control Systems       autonomous operation  │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

# PART 4: STRATEGIC FOCUS INTEGRATION

## How Extraction Focus Affects Colony Doctrine

| Extraction Focus | Best Resources | Feeds Into | Supports Colony Doctrine |
|------------------|----------------|------------|-------------------------|
| **Heavy Metals** | Fe, Ti, Ni | Manufacturing | INDUSTRIAL |
| **Construction Materials** | Si, Al, Ca | Construction | EXPANSION |
| **Volatiles** | H₂O, H₂, O₂ | Energy, Life Support | SCIENTIFIC, BALANCED |
| **Rare Elements** | Pt, Au, REE | Commerce, Research | COMMERCIAL |

## Position-Based Focus

| Sect Position | Recommended Focus | Rationale |
|---------------|-------------------|-----------|
| **Core Sect** | Balanced/Construction | Feeds colony growth |
| **Edge Sect** | Heavy Metals | Export to other colonies |
| **Near PSR** | Volatiles | Access to water ice |
| **Mare Location** | Fe/Ti | High ilmenite content |
| **Highland Location** | Si/Al/Ca | Construction materials |

---

# PART 5: IMPROVEMENTS FROM PREVIOUS VERSION

| Aspect | Previous Version | This Version |
|--------|------------------|--------------|
| **Site Selection** | Not addressed | Full pre-game orbital survey system |
| **Prospecting Science** | Basic probes | LIBS, Raman, Neutron, GPR — real instruments |
| **Excavation Methods** | Generic "drills" | Scoop, Bucket Wheel, Percussive, Auger — scientifically accurate |
| **Beneficiation** | Basic pipeline | Multi-stage separation with real chemistry (H₂ reduction, MRE) |
| **Resource Targeting** | In Field Module (wrong) | In Beneficiation Module (correct) |
| **Depth Mechanics** | Not detailed | Compaction layers, depth vs. composition trade-offs |
| **Thermal Effects** | Basic day/night | Full thermal windows, processing bonuses, equipment stress |
| **Volatile Extraction** | Not addressed | Thermal mining, sublimation collection |
| **Chain Optimization** | Not addressed | Node sequence affects efficiency |
| **Strategic Integration** | Separate from colony | Integrated with Colony Doctrine and Sect Position |
