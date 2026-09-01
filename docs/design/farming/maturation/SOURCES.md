# Sources — raw inputs to this maturation session

**Status:** REFERENCE (verbatim capture, do not redesign here)
**Branch:** `claude/farming-unit-design-4rz536`

The three inputs that seeded this session, captured so the concept tree
(`CONCEPT_TREE.md`) can cite them without re-explaining. Nothing here is
decided design.

---

## S1. The reMarkable note — "The Uniform Definition of Farming Unit"

Transcription of the handwritten note (dot-grid tablet page):

```
The Uniform Definition of Farming Unit:

128 Seeds ──▶ Requirements { Light, Water, Soil, Oxygen }
          ──▶ Yield        { Calories, Biomass, Oxygen }   [margin: "Nutrient pack"]
          ──▶ Cycle

Look at:  crew morale
          power consumption
          water reserve
          O2 reserve

Soil? Hydroponics?

[boxed]    Uniform Definition
[circled]  Biomining
```

Reading: a unit is defined by what it needs (Requirements), what it gives
(Yield), and its rhythm (Cycle); a farming unit couples to four colony
gauges (morale, power, water, O2); the medium question (soil vs
hydroponics) is open; "Biomining" is a separate circled idea to keep.

## S2. The six constraint programs — "the farming algorithm"

User-provided ideation (lightly formatted, content verbatim in spirit).
Framing: *the algorithm is applied implicitly, not as homework; the game
should make the player tend toward the arrangement their location already
implies. A seed of ideation, not a spec — it introduces cycles, seed
types, desired yields.*

Six programs, each decided by a different **master constraint**. All run
the same grounding/rotation algorithm — the constraint picks n, the
tiers, and the rhythm:

1. **PHAROS — decided by location: polar peak of eternal light.**
   Four-person science outpost, farming a side duty. Continuous sun via
   heliostat → steady growth, minimal one-grounding-per-day register:
   14 types, 84 plants, tiers 2/4/8, grand cycle 128 days. Crew rotations
   set to 128 days so each crew inherits the register at state zero;
   handover ceremony = the day's single grounding, performed jointly.

2. **FORTNIGHT — decided by the equatorial day/night cycle.** No batteries
   big enough, so the program surrenders to the synodic month.
   Photosynthesis sprints 14 days; at sunset the whole mature cohort is
   grounded in one batch (15 groundings in one shift). The dark fortnight
   runs the *heterotroph register*: mushrooms, mealworms, gas-fermentation
   vats consume the grounded biomass. Two alphabets — light and dark —
   alternating; each plant's pair is (light parent → dark consumer →
   light child). Cycle: multiples of 29.5 days; they run 118
   (register 2×[8×8] interleaved).

3. **DEEP REGISTER — decided by the energy budget: lava-tube colony,
   50 crew, reactor-fed LEDs.** No sun, so maturation time becomes a
   *dial* — dim the fast crops, floodlight the slow ones until all 24
   types converge on one growth duration. That dissolves tiering: the
   full unified 24×24 register, 576 plants, all 576 ordered pairs
   realized, one grounding/day, 576-day grand cycle. The only colony
   where combinatorial completeness is affordable, because photons come
   from uranium instead of orbit.

4. **ASHFIELD — decided by a research initiative: regolith
   beneficiation.** The register isn't for food. Hypothesis: which parent
   plant's biomass best conditions raw regolith for which child crop.
   Every ordered pair is a datapoint — the de Bruijn sequence *is* the
   experimental design (every substrate-history tested exactly once,
   no wasted trials). n=16, 256 groundings, 256-day campaign, publish
   the 16×16 conditioning matrix, hand the three best columns to the
   food programs.

5. **PILGRIM — decided by other crew activities: a hospitality colony.**
   Guests stay 16 days; the farm is the experience. Fast tier only —
   n=4 (radish, baby greens, mizuna, strawberry runners), cycle 16 days:
   every guest witnesses one complete Eulerian circuit, arrival-day
   planting to a departure-day harvest they grind themselves. The
   register doubles as the colony's clock and liturgy. Nutritionally
   trivial, psychologically the highest-value square meters on the Moon.

6. **THE VAULT — decided by a continuity initiative.** A living genetic
   archive — the Moon's Svalbard — except stored seeds decay under cosmic
   radiation, so the archive must stay *alive and cycling*. Maximal
   diversity, cycle time irrelevant: n=128, 16,384 plants, one
   grounding/day, a 44.9-year grand cycle tended like a monastery keeps
   hours. The "absurd" first design was never wrong — it was the
   specification for a different institution.

**The pattern:** the constraint never changes the algorithm, only which
(n, tiers, service rate) triple instantiates it. The Eulerian structure
is the invariant; everything else is siting.

## S3. Concept images — pixel-art planter tools (ChatGPT sketches)

Two preliminary tool-card mockups, supplied as visual context only
(style reference, not committed art or numbers):

**"13. Soil Planter Tool"** — 1×1 footprint, −0.8 kW, 0.3 L/plant.
Drill-bodied capsule that plants seeds directly into prepared soil.
Choice of **regolith type**: REGOLITH (balanced), MEGAREGOLITH
(high mineral, +yield), FRACTURED ZONE (poor nutrients, faster growth),
INTACT BASALT (very dense, hard to cultivate). Choice of **fungi
additive**: MYCORRHIZAL (+growth, +water efficiency), NITROGEN FIXER
(+yield, +soil fertility), DEFENSE MOLD (+disease/pest resist), NONE.
Stat bars: planting speed / soil efficiency / fertility boost /
durability. Crop example: potato (soil: regolith, fungi: mycorrhizal).
Unlock: Farming Unit lvl 1. Cost in three materials.

**"12. Hydroponic Planter Tool"** — 2×3 footprint, −1.5 kW, 0.8 L/s.
Rack of channels; drag-placed on a grid; **must be connected to water
and power**. Connection ports: Power In, Water In, Water Out (drainage).
Stat bars: growth rate / yield / durability. Crop example: lettuce with
four growth-stage sprites. Unlock: Farming Unit lvl 1.

Salvage from these: footprint sizes differ by medium; soil is a
per-plant batch tool while hydro is a plumbed continuous rack; the
regolith-type and fungi-additive slots; growth-stage sprites as the
hero visual; connection topology as gameplay.

## S4. Code state at session start (for grounding)

- `Unit::InitializeFarmingModules()` (`src/Unit/unit.cpp:622`) — five stub
  modules: Irrigation, Greenhouse, Hydroponics, Harvest, Storage; only
  Irrigation has rates (FOOD out; WATER+ENERGY in).
- `Unit::ProcessFarming()` (`src/Unit/unit.cpp:306`) — food = rate ×
  fertility × growthBoost × water-efficiency; fertility decays 0.01/s to
  a 0.2 floor with no restore path (a **release-valve violation**, see
  `docs/guides/module-architecture.md` §8).
- `FARMING_PRODUCTION_COSTS` (`src/game_constants.h:129`) — FOOD ← 0.5
  WATER + 0.2 ENERGY; BIOFUEL ← 0.5 WATER + 1.0 FOOD.
- `game_types.toml` — Hydroponics/Greenhouse module entries with
  production/consumption already data-driven.
- `resource_types.h` — FOOD is `SINGULAR`, colour grey `{128,128,128}`;
  no subtypes yet.
- Site archetypes exist (`MARE_INDUSTRIAL`, `HIGHLAND_CONSTRUCTION`,
  `POLAR_VOLATILE`, `KREEP_SCIENTIFIC`, `LAVA_TUBE`, `MIXED`) — the
  natural hook for constraint programs (S2).
