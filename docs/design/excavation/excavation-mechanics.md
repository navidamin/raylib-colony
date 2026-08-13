# Excavation Mechanics — Science Review & Design Alternatives

> Status: REVIEWED — Part 3 alternatives decided, see [dig-plan-design.md](dig-plan-design.md)
> Last Updated: 2026-08-13
> Parent: [README.md](README.md)
>
> **Decision:** Design B chosen as the spine, with Design A reduced to machinery *selection*
> (presets, not parameter tuning) and Design C deferred. Part 1 remains the science reference
> for both development levels. Part 5's gaps are superseded by
> [dig-plan-design.md §6](dig-plan-design.md#6-gaps).

---

## Part 1: Real Excavation Technologies → Game Mechanics

Concise review of planetary-ISRU and mining-engineering excavation methods, with their
game mechanic translations. Same format as
[prospecting/sampling-mechanics.md](../prospecting/sampling-mechanics.md).

### 1. The Reaction-Force Problem (the constraint everything else answers)

On Earth an excavator digs by pushing against its own weight. At 1/6 g — and with launch
mass budgets in the tens of kilograms, not tens of tonnes — a lunar machine's weight is
"of little or no value in terms of generating a reaction force," so a conventional blade
or scoop simply cannot penetrate the regolith. Every viable design either cancels the
reaction force or lowers the force required.

**Game translation:** A hard **force budget** per machine: `availableTraction = mass × g × μ`.
Any tool setting demanding more horizontal force than that produces *slip*, not throughput.
This is what stops "set depth to max, set rate to max" from being the dominant strategy.
It also makes machine mass a real design axis rather than flavor text.

*Sources: [Zero horizontal reaction force excavator (US 9,027,265)](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/9027265), [RASSOR — NASA T2 Portal](https://technology.nasa.gov/patent/KSC-TOPS-7)*

### 2. Counter-Rotating Bucket Drums (RASSOR / IPEx)

Hollow drums with small staggered scoops around the perimeter and internal baffles that
keep captured regolith from falling back out. Two drums rotate in opposition so the
digging forces cancel: **net-zero horizontal reaction force**, so vehicle traction is not
needed. The drum integrates excavation, loading, and haulage in one tool, and its
continuous mode keeps peak force low. NASA's ISRU Pilot Excavator targets moving 10 metric
tons of regolith with a **30 kg-class** robot.

**Game translation:** The "safe" mid-tier tool. Low force, continuous flow, self-hauling
(no separate haul robot needed), but **shallow** — a drum only cuts to about its own bucket
depth per pass, so reaching depth means many passes. Trades depth access for reliability.

*Sources: [RASSOR 2.0 design (NTRS)](https://ntrs.nasa.gov/citations/20210011366), [IPEx bucket drum scaling (NASA)](https://www.nasa.gov/wp-content/uploads/2024/07/asce-es-2022-isru-pilot-excavator-bd-scaling.pdf), [Modular bucket drum excavator for lunar ISRU](https://arxiv.org/pdf/2511.00492)*

### 3. Bucket Wheel

Continuous rotary excavation with many small buckets. Force per bucket stays low, but
excavation force scales non-linearly with wheel size — small devices behave differently
from scaled-up ones, so "just build it bigger" does not linearly buy throughput.

**Game translation:** Throughput tool with **diminishing returns on scale**. Good for
soft surface regolith at volume; energy cost per kg rises as you push size or rate.

*Sources: [Measurement of force to excavate extraterrestrial regolith with a small bucket-wheel device](https://www.sciencedirect.com/science/article/abs/pii/S0022489809001013), [Effect of bucket-wheel scale on excavation forces and soil motion](https://www.researchgate.net/publication/271892582_Effect_of_bucket-wheel_scale_on_excavation_forces_and_soil_motion)*

### 4. Blade / Dozing / Scoop, and the Balovnev Force Model

The classical analytical model for tool force. Horizontal force rises with **soil
cohesion**, **internal friction angle**, **tool–soil adhesion**, **soil density**,
**digging depth**, **rake angle**, **gravity**, and **surcharge**. Reviews found Balovnev
the most reliable model across conditions. At a 72° rake angle the horizontal component
dominates; as rake approaches 90° (vertical blade) the vertical force falls toward zero.

**Game translation:** This is the **formula the game should actually use** for Design A.
It gives a physically-motivated, tunable equation with player-facing knobs (depth of cut,
rake angle, advance rate) and site-driven parameters (cohesion, density) that prospecting
already knows. Cheapest tool, highest force, most slip-prone.

*Sources: [Comparison of ISRU Excavation System Model Blade Force Methodology (NTRS)](https://ntrs.nasa.gov/api/citations/20100012787/downloads/20100012787.pdf), [Evaluation of an Analytical Earthmoving Model (LPSC 2024)](https://www.hou.usra.edu/meetings/lpsc2024/pdf/2089.pdf), [Review of Resistive Force Models for Earthmoving Processes](https://www.researchgate.net/publication/245307528_Review_of_Resistive_Force_Models_for_Earthmoving_Processes)*

### 5. Percussive & Ultrasonic Excavation

Adding percussion reduces the shear strength of dry regolith simulant (JSC-1A), cutting
the force needed to excavate it. Ultrasonic resonance in a blade demonstrably reduces
resistive force, including under reduced gravity.

**Game translation:** A **force-for-energy trade**: percussion multiplies the effective
force budget but costs extra energy and accelerates wear. The natural answer to hard,
deep, or ice-cemented ground. Maps cleanly onto the existing `"percussive"` tier-2 method
string.

*Sources: [Reducing extra-terrestrial excavation forces with percussion](https://www.researchgate.net/publication/261248221_Reducing_extra-terrestrial_excavation_forces_with_percussion), [Effect of reduced gravity on tool forces of an ultrasonically vibrating blade](https://www.sciencedirect.com/science/article/abs/pii/S0094576525004795)*

### 6. Pneumatic / Vacuum Excavation

Gas jets fluidize and lift regolith; a vacuum/conveyor system carries it away. Almost no
mechanical reaction force and no large moving contact parts.

**Game translation:** Exotic high-tier tool. Ignores the traction budget entirely and is
gentle on wear, but **consumes a consumable gas budget** — which the colony must produce.
Excellent on loose surface fines, useless on cemented ice.

*Sources: [Apparatus for pneumatic excavation (US 5,487,229)](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/5487229), [Pneumatic Excavator and Regolith Transport System for Lunar ISRU](https://www.researchgate.net/publication/268569543_Pneumatic_Excavator_and_Regolith_Transport_System_for_Lunar_ISRU_and_Construction)*

### 7. Conveyance: Augers, Screw Conveyors, Hoppers

Excavation is not finished until the material reaches the processor. A hopper with a
helical flexible screw conveyor is the most effective filling approach found for regolith;
augers and impellers are favored for cohesive granular material because of simplicity and
industrial heritage. Oversize fragments are mechanically rejected at slot openings.

**Game translation:** A **throughput bottleneck separate from the dig rate**. If conveyance
capacity < dig rate, the dig stalls or spills. Introduces a stockpile/buffer between
excavation and beneficiation — a natural place for the player to see backpressure.

*Sources: [Working with lunar surface materials: dust mitigation and regolith conveyance technologies](https://www.sciencedirect.com/science/article/pii/S0094576522001965), [Vertical-screw-auger conveyer feeder (US 9,334,693)](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/9334693)*

### 8. Depth-Dependent Geotechnics

Lunar regolith bulk density is ~**1.30 g/cm³ at the surface**, rising asymptotically to
~**1.92 g/cm³ below ~100 cm**. Cohesion increases exponentially with bulk density; near-surface
cohesion is ~**0.1–1.0 kPa**, and both cohesion and friction angle rise markedly with depth.

**Game translation:** Depth should be a **continuous cost curve, not a tier unlock**. The
current code treats depth as a bucket lookup (`>=100 → DEEP`). Replacing that with
`density(depth)` → `cohesion(density)` → Balovnev force means deeper ground is genuinely
harder, richer, and slower — and the transition is smooth. Directly reuses the depth-layer
work already done for prospecting.

*Sources: [Geotechnical Properties of Lunar Soil — Carrier (LPI)](https://www.lpi.usra.edu/lunar/surface/carrier_lunar_soils.pdf), [An Engineering Guide to Lunar Geotechnical Properties (NTRS)](https://ntrs.nasa.gov/api/citations/20220014634/downloads/Final%20IEEE%20paper%20formatted%20footnote%20added.pdf), [Cohesion and shear strength of compacted lunar and Martian regolith simulants](https://www.sciencedirect.com/science/article/abs/pii/S0019103524000010)*

### 9. Icy Regolith in Permanently Shadowed Regions

PSR ice may exist as sheets, as a cement bonding grains, or loosely mixed — it is genuinely
unknown. Measured behavior: strength and brittleness vary strongly with water content and
cryogenic temperature; higher water content gives greater brittleness and faster crack
growth. Drill-and-heat extraction power rose from **15 W to 40 W as water content went
2% → 6%**.

**Game translation:** A **second material class** requiring a different tool and a
different cost curve — mechanical tools bog down, percussive/thermal tools win. Pairs
directly with the existing `POLAR_VOLATILE` site archetype and the H2/WATER bonuses, and
gives polar colonies a distinct excavation game.

*Sources: [Geotechnical Properties of Icy Lunar Regolith in Cryogenic Environments (ASCE)](https://ascelibrary.org/doi/10.1061/JAEEEZ.ASENG-5253), [Water extraction from icy lunar regolith by drilling-based thermal method](https://www.sciencedirect.com/science/article/abs/pii/S0094576522006099), [Development of icy regolith simulant for lunar PSRs](https://www.sciencedirect.com/science/article/pii/S0273117724000322)*

### 10. Dust Abrasion, Seals, and Wear

Lunar dust is highly abrasive and increases friction on sliding components, limiting
operational lifespan. Bearings, bushings, and sliding cylinders can be impaired or halted
outright by fine dust ingress — Apollo suit joints jammed. Failure mechanisms include
adhesion, cold welding, lubricant starvation, cryogenic friction rise, and abrasion.
Mitigations: felt seals at moving interfaces, materials harder than the dust.

**Game translation:** Justifies and *shapes* the existing `wear` field. Wear should not be
linear-with-time; it should scale with **force**, **percussion**, **duty cycle**, and
**abrasive fines fraction of the ground being cut** — and be reducible by tool choice and
maintenance. Gives `EMERGENCY_HARVEST` its teeth.

*Sources: [Current lunar dust mitigation techniques and future directions](https://www.sciencedirect.com/science/article/pii/S0094576523004939), [Wear investigation of PTFE, PEEK and UHMWPE-based reciprocating shaft seal materials with regolith simulants](https://www.sciencedirect.com/science/article/abs/pii/S0043164825000602), [Dust Mitigation Gap Assessment Report (ISECG)](https://www.globalspaceexploration.org/wordpress/docs/Dust%20Mitigation%20Gap%20Assessment%20Report.pdf)*

### 11. Autonomous Dig Cycles & Multi-Robot Fleets

The IPEx autonomy work treats excavation as a repeating **dig cycle** and instruments it:
excavated mass, dumped mass, cycle time, and work done per cycle, compared across ~30
cycles on flat vs sloped terrain. RASSOR is intended to operate as a coordinated
multi-unit fleet, each robot functioning independently while raising aggregate throughput;
swarm layouts optimize spatial positioning to shorten material transport.

**Game translation:** The **cycle is the atomic simulation unit**, not a continuous rate.
Every stat the player sees (kg/hr, energy/kg, wear/cycle) falls out of the cycle. Fleet
size then produces real coordination decisions — spacing, haul distance, congestion —
rather than the current flat `× activeExcavatorCount` multiplier.

*Sources: [ISRU Pilot Excavator — Autonomous Excavation Algorithms (NTRS)](https://ntrs.nasa.gov/api/citations/20220005125/downloads/SRR-Autodig-2022-v3.pdf), [Autonomous Multirobot Excavation for Lunar Applications](https://arxiv.org/pdf/1701.01657), [Learning Tool-Aware Adaptive Compliant Control for Autonomous Regolith Excavation](https://arxiv.org/pdf/2509.05475)*

### 12. Grade Control, Selective Mining, and Dilution

Mining-engineering core concept. The **Selective Mining Unit (SMU)** is the smallest block
you can actually distinguish ore from waste with — set by equipment selectivity, not by
geology. **Dilution** happens when waste blocks inside the planned dig area cannot be
separated, because of operational limits or positioning inaccuracy. Contributing factors
split into controllable (equipment selectivity, dig design) and uncontrollable (spatial
heterogeneity of ore and waste). Better orebody models reduce it.

**Game translation:** The single strongest link between prospecting and excavation. Digging
ground you have not surveyed does not just cut efficiency — it **dilutes the feed grade**
into beneficiation, so the separation chain does more work for less product. Big machines
have coarse SMUs (fast, dirty); small machines have fine SMUs (slow, clean). This turns
"survey first" from a numeric penalty into a legible economic story.

*Sources: [Controlling operational dilution in open-pit mining](https://www.researchgate.net/publication/325289477_Controlling_operational_dilution_in_open-pit_mining), [Grade control drillhole spacing and mining selectivity determination](https://journals.sagepub.com/doi/10.1177/25726668241270400), [Spatial Entropy for Quantifying Ore Loss and Dilution in Open-Pit Mines](https://link.springer.com/article/10.1007/s42461-023-00881-4)*

### 13. Power & Thermal Duty Cycle

Lunar ISRU at meaningful scale is estimated at **10–20 kW** to produce metric-ton
quantities of propellant. The lunar night lasts ~**350 hours** at ~**95 K**. Photovoltaic
architectures win on mass *only if* the system can run daytime-only with minimal night
survival power; continuous night operation at daytime power levels favors reactors.

**Game translation:** Excavation is the colony's biggest load, so it is the natural place
to make the **day/night duty cycle** visible. A daytime-only excavation schedule is cheap
but caps throughput; continuous operation demands a much larger energy investment. Ties
excavation to the Energy unit as a genuine dependency instead of a flat consumption number.

*Sources: [Lunar ISRU energy storage and electricity generation](https://www.sciencedirect.com/science/article/abs/pii/S0094576520300679), [Strategies and prospects for energy storage in future lunar base](https://journals.sagepub.com/doi/10.1177/16878132251362316), [Thermal Energy Process Heat for Lunar ISRU](https://www.academia.edu/19315216/Thermal_Energy_Process_Heat_for_Lunar_ISRU_Technical_Challenges_and_Technology_Opportunities)*

---

## Part 2: Candidate Functions

Mechanics from Part 1, expressed as functions the module could expose. Grouped by which
design needs them (Part 3). Names follow CONVENTIONS.md TitleCase.

### Core / shared by all designs

| Function | Purpose | From |
|----------|---------|------|
| `GetRegolithDensityAtDepth(float depthCm)` | 1.30 → 1.92 g/cm³ asymptotic curve | §8 |
| `GetCohesionAtDepth(float depthCm)` | Exponential in density, 0.1–1.0 kPa near surface | §8 |
| `GetGroundClass(gridX, gridY, depth)` | DRY_REGOLITH / COMPACTED / ICE_CEMENTED / BEDROCK | §9 |
| `ComputeExcavationForce(tool, depth, rakeAngle, ground)` | Balovnev horizontal force | §4 |
| `GetTractionBudget(const Excavator&)` | `mass × g × μ` — the ceiling force must respect | §1 |
| `ComputeSpecificEnergy(tool, ground)` | J per kg — the real efficiency metric | §3, §4 |
| `AccumulateWear(Excavator&, force, percussion, finesFraction, dt)` | Force- and abrasion-driven, not time-driven | §10 |

### Design A additions (machine scale)

| Function | Purpose | From |
|----------|---------|------|
| `RunDigCycle(Excavator&, dt)` | State machine: APPROACH → PENETRATE → CUT → HAUL → DUMP → RETURN | §11 |
| `SetToolParameters(id, depthOfCut, rakeAngle, advanceRate)` | Player-facing knobs feeding the force model | §4 |
| `SetPercussion(id, bool, float amplitude)` | Force multiplier for energy + wear | §5 |
| `EvaluateSlip(requiredForce, tractionBudget)` | Stall / slip / nominal outcome of a cycle | §1 |
| `GetCycleMetrics(id)` | Mass excavated, cycle time, work per cycle | §11 |

### Design B additions (pit scale)

| Function | Purpose | From |
|----------|---------|------|
| `BuildBlockModel(gridX, gridY)` | Sub-cell × depth blocks, grade + confidence per block from prospecting | §12 |
| `PaintDigPlan(const std::vector<BlockRef>&)` | Player selects blocks / bench sequence | §12 |
| `GetSelectiveMiningUnit(tool)` | Block resolution a given tool can actually resolve | §12 |
| `ComputeDilution(plannedBlocks, smu)` | Waste fraction pulled into the feed | §12 |
| `GetBenchGeometry(plan)` / `ValidateSlopeStability(plan)` | Legal cut geometry, angle of repose | §12 |
| `GetHaulDistance(block, stockpile)` | Cycle-time driver, feeds fleet layout | §11 |

### Design C additions (operation scale)

| Function | Purpose | From |
|----------|---------|------|
| `GetDutyCycleState()` | DAY_OPERATION / NIGHT_SURVIVAL / THERMAL_HOLD | §13 |
| `GetSealIntegrity(const Excavator&)` | Separate degradation track from cutting-edge wear | §10 |
| `ScheduleMaintenance(id, MaintenanceType)` | Spend resources/time to reset wear tracks | §10 |
| `GetConveyanceCapacity()` / `GetStockpileLevel()` | Backpressure between dig and beneficiation | §7 |
| `AssignFleetRole(id, FleetRole)` | DIG / HAUL / SUPPORT under a coordination policy | §11 |
| `GetMeanTimeBetweenFailures(const Excavator&)` | Reliability readout driving maintenance decisions | §10 |

---

## Part 3: Three Alternative Designs

Each is a complete, self-sufficient answer to "what is the excavation module?" — they are
alternatives, not phases. All three keep the Stage 1 → Stage 2 → Stage 3 pipeline intact.

---

### Design A — "Dig Cycle": The Machine

**Player fantasy:** you are tuning a machine against ground that fights back.

**Core loop:** pick tool → set cut parameters → watch cycles run → read the force/slip
readout → retune as ground hardens with depth.

**Mechanics:**

1. **The dig cycle is the atomic unit.** Each excavator runs a state machine
   (APPROACH → PENETRATE → CUT/FILL → HAUL → DUMP → RETURN). Throughput is
   `bucketMass × fillFactor / cycleTime` — every visible stat derives from this. Replaces
   the current continuous `rate` field.
2. **Balovnev force model with live parameters.** Player controls depth of cut, rake angle,
   and advance rate. Ground supplies cohesion, density, and friction angle from the
   depth curve (§8) and ground class (§9).
3. **Traction budget as a hard ceiling.** `requiredForce > mass × g × μ` → the machine slips:
   cycle time balloons, fill factor collapses, wear spikes. The player *sees* the lunar
   gravity constraint instead of being told about it.
4. **Tool classes with distinct force signatures:**

| Tool | Reaction force | Depth reach | Energy/kg | Wear rate | Notes |
|------|---------------|-------------|-----------|-----------|-------|
| Scoop / blade | High (traction-limited) | Shallow | Low | Moderate | Cheap, slip-prone |
| Bucket wheel | Low, continuous | Shallow–mid | Moderate | Moderate | Volume tool, scale penalty |
| Bucket drum | **Net-zero** | Shallow (per pass) | Moderate | Low | Self-hauling, very reliable |
| Percussive | Reduced by percussion | Deep | High | **High** | Unlocks hard/cemented ground |
| Pneumatic | Effectively none | Surface only | High + gas | Very low | Consumes gas budget |

5. **Percussion as an explicit toggle.** Multiplies the effective force budget, costs energy
   and wear. The correct answer to depth and to ice cement — and the wrong answer to soft fines.

**Tier progression:** T0 scoop, fixed parameters → T1 bucket wheel/drum, depth-of-cut slider
→ T2 percussive, full parameter set + live force gauge → T3 auto-tuning (AI finds the
force-optimal setting, at the usual default-mode efficiency penalty).

**Strengths:** most tactile and legible; strongest science grounding (one real equation
drives everything); smallest data model — extends `Excavator` and needs no new grid.
**Weaknesses:** shallow strategic depth — once you find the optimal parameters they rarely
change; risks becoming a solved slider. Weak coupling to prospecting.
**Effort:** Low–Medium. Mostly `unit.cpp` + one panel.

---

### Design B — "Mine Plan": The Pit

**Player fantasy:** you are a mine planner deciding where to cut, and paying for what you
didn't survey.

**Core loop:** read the confidence map → paint a dig plan over blocks → fleet executes →
watch delivered grade vs planned grade → re-survey the ground that surprised you.

**Mechanics:**

1. **Block model.** Reuses the prospecting 5×5 sub-cell grid, extended in depth
   (Regolith / Megaregolith / Fractured Bedrock / Intact Bedrock). Each block carries
   `grade` (per-resource abundance), `confidence` (from prospecting), and `hardness`
   (from §8/§9).
2. **Dig plan painting.** The player selects blocks and an extraction order. Bench geometry
   is validated: you cannot dig a block whose neighbors leave an unstable slope, so cuts
   proceed in legal benches. Gives excavation a spatial puzzle.
3. **Grade control vs dilution — the central tension.** Each tool has a Selective Mining
   Unit (§12). If the SMU is coarser than the block, waste is pulled in with ore:

   ```
   deliveredGrade = plannedGrade × (1 − dilution)
   dilution       = f(smuSize / blockSize, 1 − confidence, gradeHeterogeneity)
   ```

   Digging unsurveyed ground now has a *mechanism*, not just a multiplier: low confidence
   means you misplaced the ore boundary, so you hauled waste. Beneficiation then burns
   energy separating rock you should never have loaded.
4. **Haul distance and stockpile placement.** Cycle time rises with distance from the
   active face to the stockpile. Blocks far from the stockpile are richer per kg but slower
   — a real ordering decision, and it makes fleet layout matter (§11).
5. **Depletion becomes spatial.** Blocks empty individually, so the pit visibly advances
   and the face migrates outward and downward over the colony's lifetime.

**Tier progression:** T0 single-block manual dig → T1 multi-block plan, coarse SMU
→ T2 bench sequencing + finer SMU + dilution readout → T3 auto-planner optimizing
grade-vs-haul (default-mode penalty applies).

**Strengths:** by far the strongest integration with prospecting — it *consumes* the
confidence map rather than reducing it to one float, and it gives beneficiation meaningful
variable input. Highest long-term replay value; the pit tells the colony's story visually.
**Weaknesses:** largest new data model (block grid, plan state, depletion per block); needs
the most UI; risks feeling like spreadsheet work if the visualization is weak.
**Effort:** High.

---

### Design C — "Fleet & Uptime": The Operation

**Player fantasy:** you are a mine superintendent keeping a fleet alive in a environment
that eats machines.

**Core loop:** assign fleet roles → run through the day cycle → watch wear and seal
integrity degrade → schedule maintenance against production targets → survive the night.

**Mechanics:**

1. **Machines as agents with health.** Two independent degradation tracks per excavator:
   **cutting-edge wear** (abrasive, driven by force × fines fraction) and **seal/bearing
   integrity** (driven by exposure time and dust load, §10). They fail differently: worn
   edges lose throughput; failed seals cause hard stops.
2. **Force- and abrasion-driven wear.** Replaces the current linear-with-time accrual:
   ```
   wearRate = k × excavationForce × (1 + percussionFactor) × finesFraction × dutyFraction
   ```
   `EMERGENCY_HARVEST` becomes a genuine gamble instead of a flat +50%.
3. **Maintenance economy.** Scheduled maintenance costs MACHINERY/time and resets a track;
   deferred maintenance is cheaper now and catastrophic later. Player is always trading
   uptime against throughput.
4. **Power–thermal duty cycle (§13).** Three states: DAY_OPERATION (full rate),
   NIGHT_SURVIVAL (dig halted, survival power drawn), THERMAL_HOLD (forced cooldown after
   sustained high-load operation). Continuous night operation is purchasable but expensive
   — a direct, legible dependency on the Energy unit.
5. **Conveyance backpressure (§7).** Dig rate and conveyance capacity are separate numbers.
   Exceed conveyance and the stockpile saturates, cycles stall, and throughput is capped by
   the *slowest* stage — teaching the player to balance a line rather than max one stat.
6. **Fleet roles and coordination.** Assign machines to DIG / HAUL / SUPPORT. Self-hauling
   drums need no haulers; blade machines do. Fleet composition becomes the decision.

**Tier progression:** T0 one machine, manual repair → T1 two machines + role assignment
→ T2 four machines, maintenance scheduling, thermal management → T3 eight-unit autonomous
fleet with self-scheduling maintenance.

**Strengths:** makes the existing `wear` field and the Directives module actually matter;
creates real cross-module dependencies (Energy, Manufacturing for spares); strong
mid-to-late-game texture and pressure.
**Weaknesses:** management-heavy, low tactile appeal on its own; can read as pure attrition
tax if it isn't paired with something the player is actively *doing*. Weakest link to
prospecting.
**Effort:** Medium.

---

### Comparison

| Dimension | A — Dig Cycle | B — Mine Plan | C — Fleet & Uptime |
|-----------|--------------|--------------|-------------------|
| Scale | Single machine | Site / pit | Whole operation |
| Player question | *How do I cut?* | *Where do I cut?* | *Can I keep cutting?* |
| Central tension | Force vs traction vs wear | Grade vs dilution vs haul | Throughput vs wear vs power |
| Prospecting coupling | Weak | **Very strong** | Weak |
| Beneficiation coupling | Weak (fixed grade) | **Strong** (variable grade) | Medium (variable flow) |
| Energy coupling | Medium | Low | **Strong** |
| New data model | Small | **Large** | Medium |
| UI surface | One tuning panel | Grid + plan editor + readouts | Fleet roster + schedule |
| Replay value | Low | **High** | Medium |
| Implementation effort | Low–Med | **High** | Medium |

---

## Part 4 — Recommendation

**Layer them, with B as the spine.**

Design B is the only one of the three that makes the prospecting investment pay off through
a *mechanism* rather than a multiplier, and the only one that gives beneficiation variable
input worth reacting to. It is also the one that keeps producing decisions after hour ten.
That makes it the backbone.

A and C then layer on without conflict, because each supplies what B lacks:

```
Design B  (spine)     Where to cut — block model, dig plan, grade vs dilution
   + Design A         How to cut  — per-machine tuning, opt-in fine control
   + Design C         Keep cutting — wear, maintenance, day/night duty cycle
```

This mirrors the prospecting module's **multi-scale control** philosophy: B's dig plan has
an auto-planner, A's tool parameters have auto-tuning, C's maintenance has auto-scheduling
— and in each case the automatic option costs efficiency relative to player attention. The
player chooses their engagement depth per layer.

**Suggested build order:**

| Step | Content | Why first |
|------|---------|-----------|
| 1 | B block model + grade/dilution (§8, §12) | Establishes the data model everything else attaches to; immediately makes prospecting pay |
| 2 | A force model + traction budget + dig cycle (§1, §4, §11) | Turns the block model into a simulation with real throughput numbers |
| 3 | C wear + duty cycle (§10, §13) | Adds pressure once there is something to pressure |
| 4 | Tool classes + ice ground class (§2–6, §9) | Content layer on a working system; gives archetypes distinct excavation identity |
| 5 | Default/AI modes for all three layers | Shared pattern with prospecting AI automation |

**If only one can be built:** B alone is a complete module. A alone is a good toy that will
be solved. C alone is a tax with no game attached.

---

## Part 5 — Gaps Inventory

Open questions to resolve before implementation.

### Must resolve before starting
- `[?]` Which design (or layering) is approved?
- `[?]` Block resolution — reuse prospecting's 5×5 sub-cells, or a coarser excavation grid?
- `[?]` Does dilution reduce delivered mass, delivered grade, or add a waste `ResourceType`?
- `[?]` Do excavators become individually addressable entities in the UI, or stay a fleet aggregate?
- `[?]` Should depth remain the `DepthLayer` enum, or become a continuous float driving density/cohesion?

### Should resolve before UI work
- `[?]` Is the dig plan painted on a dedicated excavation grid view, or overlaid on the prospecting grid?
- `[?]` How is dilution surfaced — a number, a color on the feed bar, or a beneficiation-side symptom?
- `[?]` Does the pit render visually in Unit view as it advances?

### Deferrable
- `[?]` Pneumatic tool and its gas budget — needs an Energy/gas production chain first
- `[?]` Ice-cemented ground class — needs polar archetype gameplay to be worth building
- `[?]` Fleet spatial layout and haul congestion — only meaningful at T2+ fleet sizes
- `[?]` Spare-parts economy linking maintenance to the Manufacturing unit
