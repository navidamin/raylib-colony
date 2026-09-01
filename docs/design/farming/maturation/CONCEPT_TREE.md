# Farming Unit — Concept Tree

**Status:** DRAFT v0.1 (maturation in progress)
**Branch:** `claude/farming-unit-design-4rz536`
**Sources:** [`SOURCES.md`](SOURCES.md) · **Method:** [`BRANCH_MATURATION.md`](BRANCH_MATURATION.md)
**Visual:** [`concept_tree.html`](concept_tree.html) / [`concept_tree.png`](concept_tree.png)

This is the navigable map of the farming-unit design space for this
branch. Address nodes by code in conversation — *"focus B2"*, *"graft a
child under D3"*, *"park F5"*. One line of state per node:

> ○ seed (named, unexplored) · ● maturing (being worked) ·
> ◆ design-ready (harvestable into a design doc) · ⏸ parked (stored for later)

---

## ROOT — Farming Unit

The unit where the colony turns light, water, regolith and biology into
calories, oxygen and morale — and the pilot case for a **uniform
definition of what a unit is**. Design must satisfy the three project
principles (scientific coherence, multi-scale control, progressive
disclosure — `docs/guides/module-architecture.md` Part I).

---

## A. CHARTER — the uniform unit definition ●

The reMarkable note's boxed idea (S1): one struct that every unit type
can inhabit, composed here first for farming. Follows the codebase's own
`ResourceDescriptor` precedent — a single-source-of-truth table plus thin
accessors — and feeds the `game_types.toml` data-driven layer.

- **A1. Field grammar ●** — generalizing "Requirements / Yield / Cycle"
  into terms that survive all eight unit types:
  - **A1.a Draws vs Gates** — split "requirements" into *draws*
    (continuous consumption rates: water L/tick, power kW) and *gates*
    (thresholds that must hold but aren't consumed: light level, crew
    present, temperature). A farm both draws water and is gated by light.
  - **A1.b Yields: stocks vs services ○** — farming yields stocks
    (calories, biomass, O2); Transport/Communication yield *services*
    (capacity, coverage). The uniform struct must express both or the
    "uniform" claim quietly dies. Stress-test node.
  - **A1.c Cycle** — duration + phase structure (prepare → grow →
    harvest → recondition), against `TimeManager` ticks (20 ticks =
    1 day). Every unit gets a rhythm, even "continuous" ones (cycle → 0).
  - **A1.d Medium** — the generalized "type of planting" (S1's
    "Soil? Hydroponics?"). Uniform term: **Medium** — the substrate/mode
    a unit works through. Farming values in branch B; extraction analog:
    ore body type; energy analog: solar vs nuclear.
  - **A1.e Couplings** — the note's "Look at" list as a first-class
    field: which colony gauges the unit reads/writes (crew morale, power,
    water reserve, O2 reserve). Uniformly exposed so colony logic and UI
    read any unit the same way.
- **A2. Naming decisions ◆** — decided this session: struct
  **`UnitDescriptor`** (mirrors `ResourceDescriptor`), fields
  `draws / gates / yields / cycle / medium / couplings`; crop catalog
  entries **`CropDescriptor`**. "Requirements" survives only as UI copy.
- **A3. Data-driven catalog ●** — descriptors live in `game_types.toml`
  (module production/consumption entries already exist there). The
  note's "128 seeds" = the CropDescriptor catalog; register sizes
  n ∈ {4, 14, 16, 24, 128} (S2) are *views into* that catalog, not
  separate data. Includes S1's "nutrient pack" as a crop-level field
  (what a crop returns beyond calories).
- **A4. Migration path ○** — order of adoption: Farming pilots the
  descriptor → Energy/Manufacture (simple) → Research/Transport (service
  yields, hard) → Extraction retrofit last (richest bespoke logic).
  Existing `Unit::parameters` string-map is the thing being replaced.
- **A5. FOOD resource upgrade ○** — FOOD is currently `SINGULAR` and
  grey. Likely → `TYPED` with subtypes (grains / greens / protein /
  fungi …) so variety, morale (G2) and the register (C) have something
  to grip. Follows the "Adding a new resource type" recipe in CLAUDE.md.

## B. MEDIA — cultivation substrates ●

The farming values of `medium` (A1.d). Each medium is a distinct build
cost, rhythm, and failure mode — the mix a player chooses *is* the
siting answer.

- **B1. SOIL — regolith agriculture ●**
  - **B1.a Regolith input types** — soil is *made*, from extraction's
    output: REGOLITH (balanced) / MEGAREGOLITH (mineral-rich, +yield) /
    FRACTURED (fast, poor) / INTACT BASALT (hostile) (S3), joined to
    beneficiation's separation chain and site archetype geology.
  - **B1.b Conditioning & fertility** — fertility as persistent
    *per-plot* state that biomass amendment, fungi (F1) and crop history
    improve — replacing today's decay-to-0.2-floor with a real
    degrade/restore loop (release valve, guide §8). ASHFIELD's
    conditioning matrix (S2.4) is this system's hidden ground truth.
  - **B1.c Profile** — cheap in power, expensive in mass and time;
    degrades gracefully (no pumps to fail); slow cycles.
- **B2. HYDRO — hydroponics ◆** *(elaborated: [`B2-hydroponics.md`](B2-hydroponics.md))*
  - **B2.a Technique family** — DWC / NFT / EBB / DRIP, each a point on
    (water inventory, pump dependence, failure latency, gravity sensitivity).
    AERO promoted out to its own medium — a different class of fragile.
  - **B2.b Solution chemistry** — EC (dS/m) and pH as live dials the *plants
    themselves* push out of range: a real degrade/restore pair. Nutrient
    sourcing is the good part — **N from the crew loop** (regolith has none
    that is bioavailable; a conserved, leaky, initially-imported stock),
    **K+P from KREEP** (the acronym is literally the fertilizer), micros from
    beneficiation with **Al/Cr toxicity removed**, not just supplied.
  - **B2.c Root zone** — **dissolved O₂ is the real limiter**, not nutrients;
    temperature-coupled, and drawn from the colony's own air. Capillary rise
    scales 1/g, so at 0.16 g the fringe is **~6× taller** — Earth-graded
    substrate waterlogs. Fix is coarser particles → **`SIZE_SORT` becomes the
    farm's substrate mill.**
  - **B2.d Light regime** — PPFD / DLI as the real units; intensity and
    photoperiod substitutable (the dial DEEP REGISTER turns). **The spectrum
    tax**: efficient red+blue makes a magenta room crew cannot inspect or bear
    — white light is watts spent on morale. Feeds off existing
    `solarIllumination`.
  - **B2.e Atmosphere** — CO₂ enrichment the habitat cannot tolerate →
    **zoned atmospheres and time-boxed crew access**; ethylene accumulation as
    friction that only arrives once the loop is properly sealed; transpiration
    recovery makes the farm a **water purifier**, not only a sink.
  - **B2.f Failure latency** — the designed, surfaced number: hours from power
    loss to first crop death. Buffer is mass; mass is the lunar problem.
- **B3. The medium roster ●** *(grafted: [`B3-media-roster.md`](B3-media-roster.md))*
  — thirteen media on two real axes: **trophic route** (photo / chemo /
  hetero) × **which loop it closes** (carbon / nitrogen / water / waste /
  mineral). Validated by ESA's MELiSSA, which is exactly this shape.
  - **B3.a Phototrophic ●** — SOIL · **SUBSTRATE** (inert graded regolith +
    solution — the answer the physics actually recommends) · HYDRO · AERO ·
    AQUATIC (algae PBR + duckweed).
  - **B3.b Chemotrophic ●** — **VAT** (H₂+CO₂ → single-cell protein,
    *Cupriavidus*; watts to calories with no photosynthesis in the path, and
    lightless by nature) · **LITHO** (mineral-oxidizing mats → the F5 bridge).
  - **B3.c Heterotrophic ●** — **MYCO** (eats the inedible half of every
    harvest, runs through the lunar night) · **INSECT** (fats and B12 that
    greens cannot supply) · AQUA (closes **nitrogen** via nitrification) ·
    CULTURE (late luxury).
  - **B3.d Special-loop ○** — SPROUT (vitamins, honestly a net calorie *loss*)
    · **HALO** (halophytes eating the **brine dead-end**) · **PIONEER**
    (cyanobacteria making soil biologically → feeds B1.b).
  - **B3.e Ship order ◆** — Core: SOIL · SUBSTRATE · HYDRO · MYCO · VAT (one
    per family + the hybrid). Unlock: AERO · AQUATIC · INSECT. Stretch: the
    rest.
- **B4. Trade-off table ●** — one table on the two axes that matter: **growth
  rate × failure latency**, plus water/power/mass and morale. The roster
  collapses onto one line, because fast systems are fast precisely by holding
  no reserve.
- **B5. The portfolio rule ◆** — *decided.* Every medium's latency is measured
  against **the same failure: power**. So diversity is not flavour, it is
  computable insurance — and the lightless families (chemo, hetero) survive
  what kills every phototroph at once (the night, an eclipsed solar farm).
  **Never put the whole farm in one medium.** This is what C's six programs
  are portfolios *of*.
- **B6. The microbial ceiling ◆** — *decided, and it is biology not balance.*
  Single-cell protein carries a high nucleic-acid load; uric-acid handling caps
  human intake near **20–25 g/day**. The most energy-efficient food source
  cannot feed the crew alone — diet diversity is forced by the body, which is
  the honest answer to "why not just build VATs" (→ G2, G4, A5).

## C. REGISTER — the implicit farming algorithm ●

The six-programs material (S2), matured into a game system whose rule #1
is: **the player never fills out the register as homework**.

- **C1. The invariant ●** — a rotation register over crop pairs
  (Eulerian circuit / de Bruijn structure): every crop-follows-crop
  pairing has consequences (via B1.b conditioning), and the algorithm is
  the same everywhere; siting only instantiates (n, tiers, service rate).
- **C2. Constraint → program map ●** — master constraints bind to
  existing `SiteArchetype`s: eternal-light polar → PHAROS
  (POLAR_VOLATILE); equatorial day/night → FORTNIGHT (MARE_INDUSTRIAL);
  reactor-lit lava tube → DEEP REGISTER (LAVA_TUBE); research →
  ASHFIELD (KREEP_SCIENTIFIC); crew/hospitality → PILGRIM; continuity →
  VAULT. Programs are *attractors the simulation produces*, not menu
  options.
- **C3. Implicitness principle ◆** — decided: constraints + feedback
  make good registers emerge; the UI shows rhythm and health, never the
  combinatorics. The player arranges plots and cycles; the game
  recognizes what they've built. (How the nudge actually works —
  gradients, advisors, defaults — is the open child: **C3.a ○**.)
- **C4. Time binding ○** — 20 ticks = 1 game day; a synodic month ≈
  29.5 game days. Open: does the planet model a day/night cycle at all
  (solar illumination is currently static per cell)? FORTNIGHT needs it;
  PHAROS needs polar light geometry. Likely the single biggest engine
  ask this design makes.
- **C5. Recognition & reward ○** — the game *names* a settled rhythm
  ("your farm has found a FORTNIGHT program"): codex entry, stat
  bonuses, crew flavor. Discovery moment instead of configuration
  screen. Grand cycles as long-horizon milestones.

## D. ARRAY — spatial plot gameplay ●

The user's ideation (4): the farming unit's interior as a placeable
array where **placement has impactful meaning** — light × water ×
cycles interfering on a grid.

- **D1. The plot grid ●** — the unit view's centre is a grid of plots;
  planters have footprints by medium (soil bed 1×1, hydro rack 2×3,
  S3); the array is the module's hero visual (guide §12).
- **D2. Flow networks ●** — light field (windows / heliostat patches /
  LED strings), water lines (feed chains, drainage cascades), power
  buses laid over the grid. Topology matters: a drain feeding the next
  rack, a shadowed corner, a bus at capacity.
- **D3. Time made spatial ●** — staggered planting turns the register
  (C) into geography: harvest fronts sweep the array; the farm doubles
  as the colony's visible clock (PILGRIM's liturgy, S2.5). This is
  where C's implicitness cashes out — the player *sees* rhythm.
- **D4. Neighborhood effects ○** — fungal spread between adjacent soil
  plots (F1), shading by tall crops, pest/disease propagation, myco
  firebreaks. Adjacency rules give placement its texture.
- **D5. Stakes ○** — same parts, different layout → measurably
  different yield and resilience. Needs the guide's §10 test: every
  placement choice a real trade-off, or it's decoration.

## E. FORM — module system evolution ●

The user's ideation (6)(7): couple the concept to its visual/module
form, and use farming to *deform the module convention deliberately*.

- **E1. Current chrome ◆** (known) — five farming stubs (Irrigation,
  Greenhouse, Hydroponics, Harvest, Storage), tiers 0–3, all rendered
  through `DrawModularUnitView` → `DrawGenericModulePanel` fallback.
- **E2. Stations reframe ●** — replace the five stubs with stations
  *serving the array*: **Seedbank & Nursery** (crop catalog, starts),
  **Substrate Works** (soil making, B1), **Fluids & Atmosphere** (water
  loop, gas exchange, B2/F4), **Harvest & Processing** (yields,
  biomass return), **Program Registry** (the register brain, C — where
  automation tiers live).
- **E3. Lens model ●** — the deformation: the centre panel is *always
  the array* (D); selecting a module switches **lenses/overlays**
  (water lens, light lens, cycle lens, biology lens) instead of
  replacing the view — vs extraction's dashboard-per-module. Decide how
  much of the shared chrome survives this.
- **E4. Tier arcs ○** — per station, tiers unlock capability (guide
  §6): new media (B3), larger array, grounding automation, register
  autopilot (the AI hook, guide §13 — read
  `docs/design/ai-automation/README.md` before finalizing).
- **E5. Visual language ○** — pixel planter tools (S3) as array
  placeables; growth-stage sprites as the living hero visual; panel
  styling via `docs/guides/ui-panels.md` tokens; `FS()` question for
  farming view text.

## F. BIOLOGY — fungi, microbes & nutrients ●

The additive layer (user ideation 3b, 5) — living inputs that cut
across media.

- **F1. Inoculant slot ●** — per-plot additive: MYCORRHIZAL (+growth,
  +water efficiency) / NITROGEN FIXER (+yield, +fertility) / DEFENSE
  MOLD (+disease & pest resist) / NONE (S3). Applies to soil *and*
  hydro (and B3 media).
- **F2. Living stock ○** — additives are cultures, not consumables from
  nowhere: kept in a myco-bay (E2's Substrate Works?), consumed on
  application, can die out; strain quality as a slow-burn asset.
- **F3. Nutrient chemistry ○** — NUTRIENT_PACK recipes from extraction
  outputs; compost loop biomass → conditioner (closes ASHFIELD's loop);
  quality grades feeding B1.b/B2.b.
- **F4. Atmosphere & waste loops ○** — O2 out / CO2 in with sect life
  support; water reclaim from biomass processing; the farm as a
  life-support organ, not just a food factory (→ G1).
- **F5. BIOMINING ⏸** — fungi/bacteria leaching metals from regolith; a
  farming × extraction crossover. **Parked** to its own seed doc:
  [`../../biomining/README.md`](../../biomining/README.md). Matures
  separately; F1/F2's culture infrastructure is its future substrate.

## G. STAKES — colony coupling ●

The note's "Look at" list (S1) as gameplay: why the farm matters beyond
its own panel. The read/write surface is A1.e; this branch is the
*dynamics*.

- **G1. Life-support ledger ●** — farm as O2 producer and water sink;
  reserve dynamics size the farm before calories do. A farm failure is
  an air problem, not a menu problem.
- **G2. Crew ●** — morale from diet variety (needs A5's FOOD subtypes)
  and fresh food; labor per grounding (service rate as crew cost);
  the farm as psychological anchor space (PILGRIM).
- **G3. Power politics ○** — the farm vs every other unit on the energy
  budget; light regime as siting consequence (C2/C4). DEEP REGISTER is
  what "solving farming with power" costs.
- **G4. Food ledger ○** — calories vs variety vs storage/spoilage
  (today's Storage stub, E2's Harvest & Processing); BIOFUEL chain
  already exists (FOOD → BIOFUEL) and should join the biomass loop (F3).

## H. CROPS — the alphabet the farm spells with ●

*(grafted 2026-09-01: [`H-crops.md`](H-crops.md). Codes are addresses, not
reading order — H arrives late but sits at the centre.)*

B decides *how* you grow; H decides *what*, and they are not independent —
a medium's affordances and a crop's root architecture either meet or they
do not. C cycles over crops, B1.b scores crop *pairs*, G weighs what a crop
actually contains.

- **H1. `CropDescriptor` — the data shape ●** *(named in A2, filled here)*
  - **H1.a Identity** — name, **botanical family** (drives shared disease
    and rotation in H7), cultivar.
  - **H1.b Requirements** — A1.a's gates and draws at crop scale: `dli`,
    `ecBand`, `phBand`, `tempBand`, `rootDepth`, `rootVolume`,
    `supportNeed`, and **`organFormation`** (NONE / SWELLING / TUBER /
    GEOCARPIC / FLOODED) — a small field that does most of H2's work.
  - **H1.c Yield vector** — the note's *"calories, biomass, oxygen"* plus
    *"nutrient pack"*: kcal, protein, fat, micronutrients, edible fraction,
    **residue mass** (MYCO/INSECT feedstock — residue is an output, not
    waste), O₂.
  - **H1.d Cycle** — days to maturity + `harvestMode` (SINGLE /
    CUT_AND_COME_AGAIN / CONTINUOUS). Harvest mode shapes the register's
    rhythm as much as duration: a cut-and-come-again crop never frees its
    plot.
  - **H1.e Medium fit** — *not stored*; derived by H2.
  - **H1.f Morale value** — uncorrelated with calories, which is the point.
- **H2. Medium fit — computed, not authored ◆** — *decided.* Media declare
  what they **offer** (depth, volume, anchoring, allowsSwelling,
  allowsPegging, flooded); crops declare what they **need**; fit is a
  function returning NATIVE / WORKABLE / MARGINAL / INCOMPATIBLE. A new crop
  costs one row of traits, not thirteen judgement calls — and every "no"
  carries a reason the player can learn (a carrot forks because nothing let
  its taproot run straight; a peanut fails in water because its pegs have
  nothing to push into). Ordered slow→fast, the compatible cells form a
  **triangle that thins to the right**: *soil grows everything; the fast
  media are specialists in crops that store nothing.*
- **H3. Crop roles ●** — calorie staple · protein · **fat** (the scarcest
  macro) · fresh/morale · micronutrient · industrial · **service crops**
  (grown for what they leave behind: legumes fixing N, cover crops
  conditioning a plot — ASHFIELD's hypothesis as a routine decision).
- **H4. The diet ledger ●** — complete nutrition is a portfolio, enforced by
  three hard facts: **B12 does not exist in plants at all** (it comes from
  microbes and their hosts — INSECT/AQUA/VAT or imports); **fat is scarce**
  in CEA crops; and B6's microbial ceiling. The efficient farm and the
  survivable farm are different farms.
- **H5. Cultivars & genetics ○** — space agronomy breeds **dwarfs** (NASA's
  Superdwarf wheat): same crop, shifted trait vector — height ↓, cycle ↓,
  DLI ↑, absolute yield ↓. Research unlocks *rows*, not rules.
- **H6. Seed stock & viability ○** — the note's **"128 seeds."** Seed decays
  under cosmic radiation (THE VAULT's whole premise); regenerating stock
  means letting a cohort bolt, which costs that plot its harvest; a lost
  cultivar is gone unless another sect carries it — diversity becomes an
  asset, not a flourish.
- **H7. Pathogens & monoculture ○** — sealed environments are sterile until
  breached, then ideal. Shared botanical families share diseases, and **the
  recirculating loop that makes hydro efficient makes it an epidemic
  vector** — one root pathogen reaches every plant on the loop in hours.
  Valves: rotation, family diversity, loop segmentation (a D placement
  decision), F1's defense mold.
- **H8. Crops as the register's alphabet ●** — H hands off to C: **n** is how
  many crops the catalog exposes; crop pairs are what B1.b scores (ASHFIELD's
  matrix is literally H×H); legumes make pairing *non-arbitrary*, the first
  real reason rotation order matters. **Answers B's open question:** the
  register runs over **crops**, with medium a property of the plot it
  schedules onto — except FORTNIGHT, where the dark letters are organisms
  (MYCO/INSECT/VAT), so it is two registers sharing a clock, not one bigger
  one.
- **H9. The calorie–fragility coupling ◆** — *decided, and the payoff of
  grafting H at all.* Staples need volume, depth and time — exactly what the
  slow, heavy, forgiving media provide. The fast media grow leaves: high
  value per gram, near-zero stored calories. So **B5's portfolio is not
  merely insurance, it is nutritionally mandatory**: you cannot eat from the
  fast half of the farm, and you cannot react quickly with the slow half.
  The buffer↔speed frontier is also a calories↔responsiveness frontier.

---

## Cross-links worth holding

| Link | Why it matters |
|---|---|
| B1.a ↔ extraction/beneficiation | soil is made from beneficiation output — first deep unit-to-unit production chain |
| C2 ↔ site selection archetypes | siting already classifies exactly the constraints the programs need |
| C4 ↔ planet light model | the one engine-level dependency; decide early |
| A3 ↔ `game_types.toml` | the charter lands as data, not code |
| F5 ↔ `docs/design/biomining/` | parked seed; do not let it bloat this branch |
| H2 ↔ B3 | the medium roster's affordances are H2's other half — one rule, two branches |
| H8 ↔ C1 | crops are the register's alphabet; legume pairs make rotation order mean something |
| H1.c ↔ B3.c | residue mass is MYCO/INSECT's input — wheat straw is the loop closing visibly |
| E4 ↔ `docs/design/ai-automation/` | register autopilot must fit the global AI-tree pattern |

## Open questions (the next conversations)

1. **C4** — does the planet get a real day/night + polar light model,
   or do programs fake it per-archetype? (Biggest dependency.)
2. **A1.b** — one descriptor for stock *and* service yields, or a
   two-variant charter?
3. **E3** — how far can the lens model bend the shared module chrome
   before unit views stop feeling like one game?
4. **D1** — array size and granularity: plots per farming unit? Does
   array area grow with tier (E4)?
5. **A5** — FOOD subtypes: which four-to-six, and what does each feed
   (morale? nutrition? register roles?)
