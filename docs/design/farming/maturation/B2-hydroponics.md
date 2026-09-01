# B2 — HYDRO: solution culture

**Status:** DRAFT v0.1 (node B2, matured 2026-09-01)
**Node:** [`CONCEPT_TREE.md`](CONCEPT_TREE.md) § B2 · **Sources:** [`SOURCES.md`](SOURCES.md) S3
**Siblings:** [`B3-media-roster.md`](B3-media-roster.md)

---

## Thesis

Hydroponics is the **fast, brittle** end of the medium spectrum. It exists in
this game not because it grows more food per square metre (it does), but
because it makes the player buy speed with **buffer** — and buffer is mass,
and mass is the whole lunar problem.

> **The spine of the entire B branch:** every medium trades *growth rate*
> against *failure latency*. Water you carry is time you have when the pump
> stops. Soil is slow and forgives for weeks. Aeroponics is fast and forgives
> for twenty minutes.

Everything below is a consequence of that sentence.

---

## B2.a — Technique family

Hydroponics is not one thing. Four techniques, each a different point on
(water inventory, pump dependence, failure latency, gravity sensitivity):

| Technique | What it is | Water held | Failure latency* | Notes |
|---|---|---|---|---|
| **DWC** (deep water culture) | roots suspended in an aerated reservoir | high (≈30 L/m²) | 6–24 h | thermal mass; the reservoir is also **radiation shielding** |
| **NFT** (nutrient film) | thin film down sloped channels | very low (≈2 L/m²) | 1–4 h | lowest mass, highest pump dependence |
| **EBB** (flood & drain) | periodic flooding of an inert media bed | medium (≈15 L/m²) | 12–48 h | the robust middle; media buffers everything |
| **DRIP** (substrate culture) | emitters onto graded slabs | medium-low (≈8 L/m²) | 1–3 d | industry standard; per-plant control |

\* *time from total power loss to first crop damage — a design target, not a
measurement. These numbers are the mechanic.*

**AERO** (mist culture) is fast enough and fragile enough that it is promoted
out of B2 into its own medium in the roster — ~15–60 min latency, a different
class of thing.

## B2.b — Solution chemistry

Two live dials, both of which the plants themselves push out of range. This is
the module's **degrade / release-valve pair** (guide §8), and unlike today's
`FertilityLevel` decay it has a real restore path.

- **EC** (electrical conductivity, **dS/m**) — total dissolved salts. Rises as
  plants transpire water away and leave salts behind. Target 1.2–3.0 depending
  on crop. Valve: dilute with recovered condensate; or dump and remake.
- **pH** (unitless, target 5.5–6.5) — driven by the plants: nitrate uptake
  alkalizes, ammonium uptake acidifies. So *your nitrogen source form* sets
  the drift direction. Valve: acid/base dosing.

### The nutrient sourcing problem (this is the good part)

| Nutrient | Lunar source | Consequence |
|---|---|---|
| **N** | **none in regolith** (solar-wind N ≈50–100 ppm, not bioavailable) | must come from the **crew waste loop** or be imported. Nitrogen is a *conserved, leaky stock*: every kg vented is gone forever. |
| **K, P** | **KREEP terrane** — the acronym is literally **K**, **R**are **E**arth **E**lements, **P**hosphorus | `KREEP_SCIENTIFIC` sites are the fertilizer mines. The archetype already exists in code. |
| **Ca, Mg, S, Fe** | regolith leachate | abundant, easy |
| **micros** | regolith leachate | comes with **Al and Cr toxicity** — Al³⁺ mobilizes below pH 5.5. Raw leachate is a *bad* solution; beneficiation must **remove**, not just supply. |

**The structural consequence:** the farm cannot run open-loop. Nitrogen ties
farm size to crew size, and the first nitrogen is imported and finite. That is
the early-game pressure, and it makes G2 (crew) a farming input rather than a
farming customer.

*Grounding: the 2022 Apollo-regolith* Arabidopsis *study — plants grew, but
showed ionic-stress responses, worse in more space-weathered regolith.*

## B2.c — Root zone environment

**Dissolved oxygen is the real limiter**, not nutrients. Most hydroponic
failure is root hypoxia, and it is temperature-coupled: warm solution holds
less O₂. Target >6 mg/L; below ~4 mg/L is stress.

This means the farm **spends the colony's oxygen on its roots** — an O₂ draw,
not just an O₂ yield. G1's ledger runs both directions.

### Low gravity changes the plumbing

Capillary rise scales as **h ∝ 1/g**. At 0.16 g the capillary fringe is
**~6× taller** than Earth practice assumes, so a substrate that drains freely
on Earth stays waterlogged on the Moon — a perched water table sitting in the
root zone, producing exactly the hypoxia above. This was the persistent water
management problem in ISS plant growth hardware.

**The fix is particle size** (h ∝ 1/r): lunar substrate must be *coarser*,
by roughly the same factor.

> **Cross-link, concrete:** beneficiation's existing **`SIZE_SORT`** node is
> the farm's substrate mill. Grade determines drainage determines root-zone
> oxygen determines yield. One separation node, already in the code, becomes a
> farming input with a real curve behind it.

## B2.d — Light regime

Real units, and they should be the API's units:

- **PPFD** — µmol·m⁻²·s⁻¹, instantaneous photon flux
- **DLI** — mol·m⁻²·d⁻¹, the daily integral, which is what yield actually
  tracks. Leafy greens ≈12–17; fruiting crops ≈20–30.

DLI = PPFD × photoperiod, so **intensity and duration are substitutable** —
that is a genuine player lever (cheap slow photons vs expensive fast ones),
and it is the dial DEEP REGISTER turns to converge every crop onto one growth
duration (S2.3).

**The spectrum tax.** Red+blue LEDs are the most photosynthetically efficient
per watt, and they make the grow space magenta: crew cannot visually assess
plant health, and prolonged exposure is miserable. Adding white/green light is
watts spent on photons the plants barely use, **purely for crew morale and
inspection**. A pure-efficiency farm is a psychologically hostile one.

`OrbitalSurveyData::solarIllumination` (already per-cell, 0–1 fraction of the
lunar day with sun) is the natural input for how much of DLI is free.

## B2.e — Atmosphere

- **CO₂ enrichment** — 1000–1500 ppm meaningfully raises C3 crop yield over
  ambient ~420. But elevated CO₂ is not a comfortable place to work a shift.
  Consequence: **the farm wants an atmosphere the habitat does not**, so
  enriched zones are separated, and crew access becomes a scheduled,
  time-boxed thing. Spatial, and therefore D-branch, gameplay.
- **Ethylene** — plants emit it; in a sealed volume it accumulates and causes
  premature senescence and poor fruit set. A documented spacecraft problem,
  solved with catalytic scrubbers. **A failure mode that only appears once you
  seal the loop properly** — friction that arrives as a reward for progress.
- **Transpiration & recovery** — 2–5 L·m⁻²·d⁻¹ transpired, condensed on cold
  surfaces and recovered clean. **The farm is a water purifier**, not only a
  water sink. G1 must model both.

## B2.f — Failure latency & buffer

The designed number that makes B2 legible: **how long after a power loss until
the first crop dies.** Show it in the UI as a live countdown-capable value per
rack, not a hidden stat.

Because latency correlates with buffer, and buffer is mass, the player's real
question is never "which medium is best" but **"how much of my food do I dare
put in the fast one?"** — which is the portfolio rule (B5).

---

## Units at boundaries

Per `docs/guides/module-architecture.md` Part II §2 — the codebase's most
expensive past bug. Name these in the API:

| Value | Unit | Kind |
|---|---|---|
| `ec` | dS/m | concentration |
| `ph` | unitless | intensive |
| `dissolvedOxygen` | mg/L | concentration |
| `ppfd` | µmol·m⁻²·s⁻¹ | rate (flux) |
| `dli` | mol·m⁻²·d⁻¹ | quantity per day |
| `waterInventory` | L | absolute quantity |
| `transpirationRate` | L·m⁻²·d⁻¹ | rate per area |
| `co2` | ppm | fraction |
| `failureLatency` | hours | duration |

## Decision texture (guide §10)

| Decision | Tradeoff |
|---|---|
| Which technique per rack | speed vs failure latency vs mass |
| EC setpoint | growth vs osmotic stress — and deliberate mild stress raises dry matter and flavour (real practice) |
| Nitrogen form (NO₃⁻ / NH₄⁺) | yield vs which way pH drifts, i.e. which valve you spend |
| Spectrum & photoperiod | watts vs yield vs crew morale |
| CO₂ setpoint | yield vs crew access to the space |
| Buffer volume | mass and shielding vs minutes-to-failure |

None has an obviously correct answer. Each is a real horticultural decision.

## Tier arc (guide §6) — crude/manual → precise/automated

| Tier | Capability | The jump |
|---|---|---|
| 0 | one DWC tub, hand-mixed solution, **no instruments** | the plants *are* your instrument — you read EC/pH by symptom, after the damage |
| 1 | NFT channels + EC/pH probes | **measurement arrives** — the ground-truth/knowledge split, same philosophy as prospecting |
| 2 | automated dosing to setpoint, spectrum control, CO₂ zoning | setpoints instead of readings |
| 3 | ion-selective sensing (per-nutrient, not bulk EC), per-crop recipes, full condensate recovery | the solution becomes fully legible |

Tier 0's blindness is the design's best progressive-disclosure moment: a new
player sees plants and water, not a chemistry panel.

## Default / auto behaviour (guide §5)

| Stage | Auto does | What it misses | Penalty |
|---|---|---|---|
| Solution management | holds EC/pH in a wide safe band | crop-specific setpoints, deficit-stress quality | missed opportunity (~15% under optimal) |
| Light | flat DLI for every rack | wastes watts on low-light crops, starves high-light ones | missed opportunity |
| Buffer | keeps reservoirs full | lean, fast, mass-efficient running | mass cost, never a crash |

Auto never kills a crop. Auto is *worse*, not broken.

## What B2 asks of other systems

- **Extraction/beneficiation** — graded substrate (`SIZE_SORT`), K+P from
  KREEP sites, micronutrient leachate with Al/Cr **removed**.
- **Core/crew** — the nitrogen loop; crew as a farming *input*.
- **Energy** — DLI is the colony's largest discretionary load (G3).
- **Planet data** — `solarIllumination` per cell (exists).
- **Life support** — O₂ both ways, CO₂ zoning, water recovery credit.

## Open questions

1. Is solution state **per-rack** or **per-unit**? Per-rack makes D (array)
   matter and multiplies UI; per-unit is tolerable ×20 sects (guide §11).
   *Leaning: per-loop, where a loop spans several racks the player wires
   together — placement decides the blast radius.*
2. Does the farm draw O₂ from sect storage explicitly, or is root-zone O₂ an
   abstracted efficiency term? Explicit is more coherent and more punishing.
3. Ethylene: real accumulating stock, or a tier-2 unlock that simply gates a
   scrubber purchase?
4. How much chemistry is visible at tier 0 — is the "blind" tier fun or just
   opaque? Needs a playable test.
