# Core Unit — Master Design

Status: DRAFT — designed, not built

Worked through the thirteen aspects of
[`docs/guides/module-architecture.md`](../../guides/module-architecture.md) Part I.

---

## 1. Identity

> **The dome converts life support into labour.**

Everything in the sect's ring runs on crew. The Core is what makes crew exist,
keeps them alive, and decides what they work on. It is the only unit that is
always present — it *is* the sect, in the sense that a sect without people is
just equipment on regolith.

**Fantasy:** running a station. Not a factory manager — a base commander who
watches the O₂ margin, argues with themselves about whether to pull the
geologist off survey duty, and orders everyone into the shelter when the
particle count spikes.

**Why it is not two units.** Habitat and Command were considered separately.
Real bases put the command post inside the pressurised module, and splitting
them produces one unit with all the physical content and one that is pure
abstraction. They are the same building.

**Why Safety is not its own unit.** Every ring unit converts inputs into
outputs. Safety produces nothing — it is a support function, and it is about
the crew, who live here. A separate Safety unit would recreate the same
duplication that merging Habitat and Command avoids.

---

## 2. The loop

The Core is a **cycle**, not a production chain. This is a deliberate departure
from the stage model in the guide, which assumes each stage feeds the next.

```
        ┌────────────────────────────────────────┐
        │                                        │
        ▼                                        │
   [Life Support] ──▶ habitable volume           │
        │                                        │
        ▼                                        │
     [Crew] ──▶ [Roster] ──▶ labour ──▶ [Ring units]
        │                                        │
        │                              FOOD, WATER, O2, ENERGY
        │                                        │
        └────────────────────────────────────────┘
```

The ring feeds the dome consumables; the dome feeds the ring people. Neither
half functions alone, which is what makes the sect a single organism rather
than eight independent buildings.

**Test applied:** remove any stage and something downstream breaks. Remove Life
Support and crew die. Remove Roster and labour is unassigned. Remove crew and
the ring stops. No decorative stages.

---

## 3. The contract

**Exports** — three values. Everything else stays inside.

```cpp
int   GetAvailableCrew() const;            // people not already assigned
float GetHabitability() const;             // 0-1, life-support health
float GetSectEfficiencyModifier() const;   // 0-1+, applied to every ring unit
```

`GetSectEfficiencyModifier()` is the one number the rest of the game feels.
Life-support failure, crew shortfall, and radiation dose all fold into it, so a
struggling sect degrades visibly everywhere at once without every ring unit
needing to know why.

**Imports**

| From | What |
|---|---|
| `Sect` | resource storage (FOOD, WATER, O2, ENERGY), ring unit list |
| `TimeManager` | tick cadence for consumption and dose accrual |
| `ResourceManager` | terrain shielding at the sect's cell (affects baseline dose) |
| `Colony` | specialist transfers in and out |

---

## 4. Data & units

**Name the unit of every value that crosses a boundary.** This is the most
expensive mistake made in this codebase so far (see the guide, Part II §2).

| Value | Unit | Meaning |
|---|---|---|
| `crewCount` | **people** (integer) | how many bodies are in the sect |
| `crewCapacity` | **people** (integer) | how many the dome can house |
| `labourPerTick` | **labour-units / tick** (rate) | what crew supply to the ring |
| `labourDemand` | **labour-units / tick** (rate) | what active ring units require |
| `habitability` | **fraction 0-1** | life-support health, not a percentage string |
| `closureRate` | **fraction 0-1** | share of O₂/water recovered by recycling |
| `consumptionPerCrew` | **quantity / person / tick** | raw draw before closure |
| `accumulatedDose` | **sievert** (quantity) | per specialist, monotonic |
| `doseRate` | **sievert / tick** (rate) | current exposure |

Consumption is computed as
`crewCount × consumptionPerCrew × (1 - closureRate)` for O₂ and water; FOOD and
ENERGY do not recycle and use the raw rate.

**Ground truth vs player knowledge.** The Monitoring module is exactly this
split. The sect's true state always exists; what the player *sees* depends on
Monitoring tier and whether they are physically viewing the sect. A sect can be
stalled for days before an untiered player notices.

---

## 5. Multi-scale control

| Module | Default / auto | What it misses | Penalty type |
|---|---|---|---|
| Life Support | Holds nominal setpoints | Pre-emptive margin before a shortfall | Missed opportunity |
| Roster | Auto-assigns by headcount only | Specialty matching bonuses | Missed opportunity |
| Command | Round-robin priority | Ordering work to match current needs | Missed opportunity |
| Monitoring | Alarms only on failure | Early warning from trends | Missed opportunity |
| Safety | Auto-shelters on every alert | Judging when to accept dose and keep working | Missed opportunity |

Every default is *worse*, never *broken*. A player who ignores the Core still
has a functioning sect at reduced output.

---

## 6. Progression — what each tier unlocks

Capability, not multipliers. The arc is crude/manual → precise/automated.

| Tier | Crew | Ring units runnable | Loop closure | Unlocks |
|---|---|---|---|---|
| 0 | ~6 | 2 | Open — haul in O₂ and water | Visit-to-see status only |
| 1 | ~12 | 3 | ~60% | Basic alarms; 1 specialist billet |
| 2 | ~20 | 4–5 | ~85% | Storm shelter; live telemetry; 2 billets |
| 3 | ~30 | 6 (full ring) | ~95% | Automated scheduling; full remote oversight; 3 billets |

Per-unit (tier) vs colony-global (research): dome tiers are per-sect. Specialist
*training* and the AI trees are colony-wide.

---

## 7. Economy

**Costs.** Crew consume FOOD, WATER, O₂, and ENERGY every tick. Dome tier
upgrades cost CONSTRUCTION_MATERIALS and machinery.

**The scarcity and the tension it creates:**

> More crew runs more ring units — but more crew burns more life support, which
> costs the very sockets you wanted for production. **The sect must decide how
> much of itself to spend staying alive.**

This is self-limiting without a tuning knob. A sect that grows its dome to run
six units discovers that two of those six are Farming and Energy, feeding the
dome that runs them.

**Income sources exist for every cost:** FOOD from Farming, WATER and O₂ from
Extraction, ENERGY from Energy. No cost is a wall.

---

## 8. Friction & release valves

Every degrading thing has a restore path. A decaying resource with no valve is
a dead end — this shipped once in prospecting and made the module unplayable.

| Degrades | Release valve |
|---|---|
| Life-support filters and scrubbers | Maintenance cycle (costs parts, brief habitability dip) |
| Recycling closure drifts down with use | Servicing restores it |
| Specialist radiation dose accumulates | Rotate them out to another sect, or stand down |
| Crew fatigue under sustained overtime | Reduce assigned units, or expand quarters |

**What can fail:** a life-support breach drops habitability sharply. Response is
shelter-in-place plus repair, not crew loss (see Open Questions).

---

## 9. Time

- **Per-tick:** consumption, dose accrual, labour supply.
- **Timed:** maintenance cycles block a module briefly. Specialist transfers take
  transport time.
- **Event-driven:** solar particle events arrive on a schedule the player cannot
  set, and are **colony-wide** — one event, one decision, applied everywhere.

**While the player is not looking:** the sect runs on standing orders from
Command. Consumption and dose continue. This is where an unmonitored sect can
quietly stall.

---

## 10. Decision texture

A named tradeoff per interaction. No obviously-correct answers.

| Decision | The tradeoff |
|---|---|
| How large to grow the dome | More units running vs more life support burden |
| Which ring units get crew when short | Output now vs keeping a slow build alive |
| Where the geologist goes this month | One sect surveys well, the others do not |
| Shelter during a flare, or push through | Lost production vs accumulated dose |
| Invest in closure, or haul supplies | Capital cost vs standing transport burden |

---

## 11. Scale — surviving ×20 sects

**This is the aspect most likely to sink the design, and it constrains the
Roster module absolutely.**

Twenty sects at thirty crew is six hundred individuals. Naming and managing all
of them is untenable. The crew therefore splits in two:

- **Complement** — the bulk. A count, never individually managed. Consumes life
  support, supplies baseline labour.
- **Specialists** — few and named, with 1–2 traits. Billets per sect are gated
  by dome tier (1 at T1, 3 at T3), so a large colony has perhaps a dozen or two
  colony-wide, not hundreds.

The decision count stays bounded no matter how large the colony grows. The
player is never assigning six hundred people; they are deciding where their one
good geologist goes.

Everything else in the Core is either per-tick automatic or colony-wide
single-decision (solar events), so nothing else multiplies with sect count.

---

## 12. Feedback & hero visual

**Hero visual: the dome in cutaway.** Crew inside, life-support loops running
green/amber/red, the shelter, the airlock. It is the sect's identity object and
it is already drawn at the centre of the sect view — it simply has no meaning
yet.

This is the strongest hero visual of any unit, because unlike crystals or
battery cells the player is looking at **people they can lose**.

**At-a-glance state:**

- Dome size in sect view → sect population
- Filled ring sockets → what the sect is *for*
- Habitability colour on the dome → whether it is in trouble

A player scanning the colony view should be able to read every sect's role and
health without opening anything.

---

## 13. AI automation hook

Per [`docs/design/ai-automation/README.md`](../ai-automation/README.md), costs
follow the 200/500/1000/2500 SCIENCE tiers, total ~6,000–7,000.

| Tier | Capability |
|---|---|
| Baseline (free) | Auto-assign crew by headcount; auto-shelter on every alert |
| Convenience | Auto-schedule maintenance before failure; auto-rotate high-dose specialists |
| Intelligence | Specialty-aware assignment; shelter decisions weighed against production value |
| Mastery | Predictive life-support balancing; cross-sect specialist routing |
| Full autonomy | Sect self-manages population and assignment against a player-set policy |

Designing the module against this from the start prevents mechanics that cannot
be automated sensibly later.

---

## The five modules

| Module | Owns | Decision it creates |
|---|---|---|
| **Life Support** | O₂/water/CO₂ loop closure | Invest in recycling vs haul raw supply |
| **Roster** | Crew, specialists, assignment | Who works where; where scarce specialists go |
| **Command** | Standing orders, unit priority | What the sect does unattended |
| **Monitoring** | Visibility into sect state | Whether you learn a sect stalled, or find out later |
| **Safety** | Dose, shelter, medical | Shelter and lose output, or push through and take dose |

**Monitoring** absorbs the one salvageable idea from the abandoned
Communication unit: information as a resource. It is what makes twenty sects
manageable, without a bandwidth economy.

---

## Sect structure

The Core changes the sect's shape:

```
        ┌─ 1 Core (dome, centre, always present)
Sect ───┤
        └─ 6 sockets in a ring at 60°, chosen from 7 production types
```

Seven production types into six sockets means **every sect is missing at least
one thing by construction**, even at full build. The transport network becomes
load-bearing by design rather than by balance tuning.

Sockets fill progressively — an early sect has two, and the empty ones are
visible on the dome. The ring doubles as a progress readout and a statement of
what the sect is.

---

## Open questions

| # | Question | Why it matters |
|---|---|---|
| 1 | On life-support failure, are crew **lost** or only degraded? | Permanent loss may be too punishing for a guide that prefers missed-opportunity penalties. Leaning: degrade to incapacitated, recoverable via Medical, with loss only on sustained neglect. |
| 2 | Do specialists have **skill levels** on top of specialty? | Levels add depth but a second number to weigh on every assignment. Leaning: start with specialty alone; add levels only if assignments feel obvious. |
| 3 | Is the flare/shelter decision interesting repeatedly? | It is colony-wide and infrequent, which helps — but if the answer is always "shelter", it is a button, not a mechanic. Needs a real cost to sheltering. |
| 4 | Does 6 sockets leave enough scarcity? | Six of seven is nearly self-sufficient. If the network feels optional in play, reduce effective sockets via crew capacity rather than changing the ring. |
| 5 | Should the Core be buildable/destructible, or implicit in the sect? | Currently `Sect::core` is a `Unit*` that is barely used. |

---

## Suggested specialties

Geologist · Agronomist · Engineer · Physician · Technician

One per broad unit family, so the assignment puzzle stays readable at a glance
rather than requiring a spreadsheet.
