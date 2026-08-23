# Manufacture Unit — Design

Status: DRAFT — frame settled, structure proposed, nothing implemented

> **Auto-context rule:** When working on Manufacture code (`Unit::InitializeManufactureModules`,
> its module panels in `rendermanager.cpp`, recipes, or anything touching the
> `TYPED` resource subtypes in `resource_types.h`), read this first.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | This README | Unit frame, product catalogue, flow classes, candidate structures | DRAFT |
| 2 | *(pending)* `manufacture-master-design.md` | Full design against the 13-aspect brief — **write once §9 is resolved** | — |

---

## 1. Unit level, before modules

The method here is deliberate: **state what the unit does, and its inputs and
outputs, before inventing modules.** An earlier pass at this design started
from "find five modules" and produced five plausible names attached to nothing.

> **Manufacture converts refined elements into the typed goods every other unit
> needs to operate and expand.**

| | |
|---|---|
| **Inputs** | Fe · Si · Ti · Al · Ca (Extraction) · ENERGY (Energy) · MANPOWER (Core) · its own ALLOYS and ELECTRONICS as intermediates |
| **Outputs** | the typed goods in §2 |
| **Consumers** | Extraction · Transport · Core · Research · Construction · **itself** — and every module's build cost in `MODULE_BASE_COSTS` |

**The structural fact:** Manufacture is the only unit whose output is
denominated in *everyone else's growth*. Every other unit converts inputs into
a resource; this one converts inputs into other units' capability. That is what
makes it the hub, and it is already half-true in code — `MODULE_BASE_COSTS`
prices every module in `CONSTRUCTION_MATERIALS`, `MACHINERY`, and `ELECTRONICS`.

The implied chain, latent in the existing resource tiers:

```
Fe Ti Al Ca ──▶ ALLOYS ──┬──▶ CONSTRUCTION_MATERIALS
                          └──▶ MACHINERY ◀── ELECTRONICS ◀── Si + Al
```

---

## 2. Product catalogue

Fourteen subtypes already exist in `resource_types.h`. Nothing reads them yet.

| Typed resource | Existing subtypes |
|---|---|
| `ALLOYS` | Steel · ~~Bronze~~ · Aluminum · Titanium |
| `CONSTRUCTION_MATERIALS` | Beam · Panel · Pipe · Cable |
| `MACHINERY` | HeavyDrill · Conveyor · Assembler |
| `ELECTRONICS` | Sensor · Controller · Computer |

> ⚠️ **`Bronze` is wrong for the Moon.** Bronze is Cu+Sn and neither exists in
> usable quantity in lunar regolith. Replace with **Magnesium** (present in
> olivine/pyroxene) or **Cast Iron**.

### 2.1 The feedstock constraint that shapes everything

Lunar regolith gives **O, Si, Fe, Al, Ca, Mg, Ti** in abundance. It gives
**C, H, N only in traces** (solar-wind implanted, ~100–200 ppm) unless at a
polar ice site. **Cu, Sn, Zn, Pb are effectively absent.**

So metals, glass, ceramics and oxygen are cheap; anything needing carbon,
hydrogen or polymers is a genuine bottleneck. This is what keeps Earth relevant
and makes polar sites worth contesting — see [`../economy/README.md`](../economy/README.md) §4.

### 2.2 Proposed expansion

Additions worth making, grouped by what they unlock. Full annotated list with
feedstock and consumers is in the session record; the load-bearing ones:

| Product | Why it matters |
|---|---|
| **Sintered regolith block** | Cheapest possible product — regolith + energy only. The bootstrap item |
| **Cast basalt / basalt fibre** | Lunar substitute for steel reinforcement; real ISRU proposal |
| **Solar cell** | Closes the energy loop (see §3, compounding) |
| **Excavator cutting head** | **Wear part** — the archetype for usage-driven demand |
| **CO₂ scrubber cartridge / filter** | Recurring life-support consumable; dust makes it constant |
| **Seals & gaskets** | Needs polymer → the import dependency made concrete |
| **Heat exchanger / radiator** | Thermal control is the real lunar engineering problem |
| **Spare part kit** | The ISS's dominant cargo class; feeds Construction's Integrity |
| **Propellant (LOX/LH₂)** | The translunar export product |

---

## 3. Flow classes — the design backbone

Product *category* is the wrong primary axis. **Turnover class** is the right
one: how fast something flows and what drives its consumption determines what
gameplay it can support.

| Class | Products | Flow driver | Gameplay it creates |
|---|---|---|---|
| **Continuous** (per tick) | O₂, water, energy, food | Crew count | Margins and buffers; interruption is a crisis |
| **Fast consumables** (days) | Filters, cartridges, seals, fasteners | Time + dust | Safety-stock policy — the thermostat's natural home |
| **Wear parts** (usage-driven) | Cutting heads, bearings, tools | **Activity elsewhere** | Coupling — mining harder eats parts faster |
| **Capital goods** (pulses) | Beams, panels, machinery, glass | Expansion decisions | Production campaigns with lead times |
| **Compounding** | Assemblers, solar cells, drills | Reinvestment choice | Exponential growth curves |
| **Import / export** | Polymers, computers ← / propellant → | Launch windows | Commerce with a different clock |

**Two of these are the design's real assets:**

- **Wear parts** make demand *emergent from throughput*. No other factory game
  does this — demand is normally set by the player or by a fixed recipe. Here,
  running Extraction harder automatically generates Manufacture load. It is the
  single mechanic that makes the units feel like one organism.
- **Import/export** gives a second economy on a launch-window clock, which is
  where commerce, dependency and substitution research all live.

---

## 4. The demand signal

The open question from the outset was *how does Manufacture know what to make.*
The answer, taken from RimWorld's bill system (see
[`../references/production-and-inventory.md`](../references/production-and-inventory.md) §2.3A):

> **A thermostat: "keep N of X on hand; reorder at M."**

One number per product. Current stock is already in `resourceStorage`; the
delta is the order. The hysteresis — separate pause and unpause thresholds — is
the detail that stops production oscillating around the target.

This converts production from something the player micromanages into a
**standing policy** they set once and revise occasionally, which is what makes
it survive ×20 sects (`module-architecture.md` §11).

**Three demand sources, in implementation order:**

| # | Source | Cost | What it gives |
|---|---|---|---|
| 1 | **Mechanical** — sum unfilled `upgradeCosts` of unbuilt/upgradeable modules | Zero new data; derivable today | Manufacture becomes reactive to real game state |
| 2 | **Standing orders** — player sets targets via Core's Command module | Small | Player authorship of policy |
| 3 | **External** — colony reserves below threshold; buyer contracts | Needs trade | Commerce |

Start at 1. It costs nothing and immediately makes the unit responsive.

---

## 5. Three candidate structures

### A · Process stages — modules are transformations

Smelting → Forming / Electronics → Assembly, plus Tooling for capability.

- **Pro:** maps 1:1 onto existing resource tiers. Cheapest to build, most legible.
- **Con:** a fixed pipeline. No build/disassemble decision at all — modules are
  stages, not lines.

### B · Production lines — modules are the line lifecycle ★ recommended

The unit owns *N line slots*. A line is a data object: one recipe, a rate, an
input feed, an output sink. The player builds and tears down lines.

| Module | Owns |
|---|---|
| Line Bay | line slots — build / reconfigure / disassemble |
| Process Library | which recipes exist (SCIENCE-gated) |
| Throughput | runs configured lines; batch size, changeover |
| Feedstock | staged inputs per line; starvation visible |
| **Order Book** | the demand signal from §4 |

- **Pro:** the sect's manufacturing identity becomes player-authored. Pairs with
  the 6-socket specialization — the *ring* says what the sect does, the *lines*
  say what it makes.
- **Con:** needs a recipe system and a line struct. Real implementation cost.

### C · Order-driven — modules are the demand pipeline

Order Book → Requirements Planning → Fabrication → Inventory → Contracts.

- **Pro:** makes Manufacture the economic brain; scales straight into trade.
- **Con:** the *making* collapses into one module. Risks feeling like admin.

### Recommendation

**B as the skeleton, with C's Order Book as one of its five modules.**

Not a compromise — demand is the *input* to the line-building decision. Lines
without demand are arbitrary; demand without lines is a spreadsheet. Neither
half stands alone, and neither needs its own unit.

**Machine model:** multi-recipe with per-recipe priority (Captain of Industry),
not one-recipe-per-machine (Factorio/Satisfactory). We have no factory floor to
lay out, so the spatial version has nothing to decide; allocation over time is
the decision that survives our abstraction level.

---

## 6. Mechanics this structure supports

Drawn from the flow classes. The five worth building first:

| # | Mechanic | Class it comes from |
|---|---|---|
| 1 | **Wear coupling** — extraction rate ↑ → cutting-head burn ↑ → Manufacture load ↑ | Wear parts |
| 2 | **The 14-day night** — solar is pulsed; batteries are energy inventory | Continuous |
| 3 | **The launch manifest** — fixed mass budget every N days; one brutal list | Import/export |
| 4 | **Assemblers build assemblers** — reinvestment ratio as the growth dial | Compounding |
| 5 | **Uncounted inventory** — stock only counts if Monitoring can see it | *(see below)* |

Others held in reserve: bill-of-materials pulses for expansion, pre-positioned
caches, quality tiers that *lower* colony-wide consumption, substitution
research measured in reduced import mass, recycling loops, cold-start costs,
maintenance debt, cannibalization.

### The unclaimed niche

Every factory game surveyed grants perfect, instant, global inventory
knowledge. Real space logistics is defined by the opposite — the ISS tracks
~20,000 barcoded items, needs crew *and* ground updating daily, employs
dedicated Inventory and Stowage Officers, is being retrofitted with RFID, and
things still get lost.

The only game mechanic found that rhymes with it is RimWorld's rule that items
outside a stockpile don't count toward a bill.

If this game's core is inventory management, **that gap is the most defensible
ground available** — and it connects directly to the Core's Monitoring module,
which currently has no mechanical job.

---

## 7. Controlling the network

Manufacture sits at the centre of a directed graph: most edges run one way
(units consume its output), but a few run back the other way — **Drills** to
Extraction, **Shredders** to Recycle, lab equipment to Science. Those loop-back
edges are where deadlock lives, and they are the reason this unit can degenerate
into a queue of unfillable requests.

### 7.1 The loops are the gameplay, not the obstacle

"Spend this titanium on a drill — more ore forever — or on a beam, the thing I
need now" is investment-versus-consumption, and it is the decision this unit
exists to pose. Removing the loops produces an acyclic, safe, inert economy.

Three properties keep a loop from becoming a trap:

| Property | Rule |
|---|---|
| **Gain > 1** | One drill must enable enough ore to build *more than one* drill. Gain < 1 is a slow death that looks like progress |
| **Named and few** | Four loop-back edges is a system; forty is a maze. Loop-backs are an explicit, small, enumerated set — never emergent |
| **Always an exit** | Imports are the deadlock-breaker of last resort. This is their structural job, beyond flavour |

### 7.2 Four rules that keep it frictionless

**Time-phasing dissolves the circularity.** The dependency is circular in
topology but not in time — the drill built today uses ore mined yesterday.
Real MRP handles exactly this. There is no paradox, only a schedule.

**Thermostats do not queue.** A request accumulates; a *condition* does not.
"Keep 5 drills" is either satisfied or short — no backlog, no ageing list, no
unattended pile. There cannot be 47 pending orders if there are no orders, only
target numbers. This is the primary defence against the unit becoming clerical
work.

**Aggregate by resource, never by requester.** Show `MACHINERY — 5 short`, not
"Extraction wants 3 drills, Transport wants 2 conveyors, Core wants 5 filters."
Fourteen subtypes is a readable list; forty requests from eight units is
bookkeeping. Requester identity is diagnostic detail available on demand, never
the primary view.

**Doctrine resolves priority, not per-order clicks.** One sect-level setting —
*Survival / Growth / Export* — deterministically settles every shortage
conflict: life support before maintenance before expansion before export. One
choice, hundreds of conflicts resolved, fully predictable.

### 7.3 The gameplay is diagnosis, not clerking

The player never processes orders. They set policy and **find the constraint**.

```
read state ──▶ spot what is starved ──▶ diagnose why ──▶ one decision ──▶ leave
```

"Titanium is short" — is the line off, is Extraction not producing Ti, or did
the drill break unnoticed? Three answers, three fixes. Locating a system's
bottleneck is the activity; filling orders is not.

**The bottleneck must move as the colony grows.** If it is always energy, the
player fixes it once and the unit goes quiet. The intended progression is
energy → titanium → crew → transport capacity → polymer imports, each growth
phase revealing a new constraint. This is what makes the unit worth revisiting
late in a run.

Expected action frequency:

| Action | Frequency |
|---|---|
| Read the unit's state | Every visit |
| Respond to a flagged exception | Occasionally |
| Reassign a line to a recipe | Rarely — changeover costs time and spares, so the choice sticks |
| Adjust a stock target | Rarely |
| Change doctrine | Almost never |

### 7.4 Scale: star topology, not mesh

Within a sect, eight units trade instantly and without friction — local, ~14
edges, no transport cost.

**Between sects, sects must not talk to sects.** Twenty sects meshed is 400
edges and genuinely unmanageable. Sects trade only with the **colony pool**,
which already exists in code (`Colony::ReceiveTypedSurplus` /
`Colony::ProvideTypedResource`). That is 20 edges, not 400. A sect resolves
demand internally first, then pushes surplus and pulls deficit across one
boundary.

The graph therefore stays a constant, readable size regardless of colony scale.

### 7.5 Deadlock: name it and offer the exits

The game must never silently strand the player. When a loop breaks, it says so
and lists the ways out:

> **Extraction stalled — no HeavyDrill.**
> Ti stock: 0. Options: import a drill (mass 40) · salvage the Conveyor line
> (recovers 30 Ti) · switch doctrine to Survival

Deadlock detection that names the cycle and enumerates the exits converts the
genre's worst frustration into a decision with stakes.

### 7.6 Open: how this is presented

The organising principle above is settled. **How the player reads it is not** —
node-link graph, proportional flow diagram, input/output matrix, tier ladder,
stock thermostats, line bay, or constraint feed are all candidates, and the
choice materially changes what the unit feels like. See open question 6 in §8.

---

## 8. Pricing

Manufacture does not set prices. [`../economy/README.md`](../economy/README.md) §1
does: every product is worth at most **import parity** — what Earth would
charge to land it here. That gives the whole catalogue a ceiling with no
hand-tuned price table, and makes substitution research legible as a
launch-mass bill going down.

---

## 9. Decided vs open

**Decided (this frame):**

- Unit verb, inputs, outputs, and the hub role
- Flow classes as the organizing axis, not product categories
- Thermostat as the demand primitive, sourced mechanically first
- Multi-recipe with priority over one-recipe-per-machine
- Loop discipline: gain > 1, few named loop-backs, imports as the exit (§7.1)
- Aggregate demand by resource; doctrine resolves priority (§7.2)
- Star topology via the colony pool, never sect-to-sect mesh (§7.4)

**Open — resolve before writing the master design:**

| # | Question |
|---|---|
| 1 | With ~14+ subtypes and a handful of line slots, is the optimal loadout *computable*? If a known-best set exists, build/disassemble becomes setup, not gameplay. **Guard:** demand must shift over time (construction pushes, contracts) so this month's right answer is wrong next month |
| 2 | Does `Recipe` live in data (`game_types.toml`, already has an unread schema) or in code? |
| 3 | How many line slots, and does dome tier / crew gate them like ring sockets? |
| 4 | Rate unit — per tick, per day, or CoI's per-60s? Pick once and hold it everywhere (`module-architecture.md` Part II §2) |
| 5 | Does Manufacture own O₂ production? Real ISRU makes oxygen a **by-product of metal extraction**, which would rewire it away from Farming and couple life support to industrial throughput |
| 6 | **How is the network presented?** Node-link graph · proportional flow · I/O matrix · tier ladder · stock thermostats · line bay · constraint feed. The organising rules (§7) hold for any of them, but the choice decides whether the unit reads as a *system*, a *ledger*, or a *workshop* |
| 7 | Where does **Recycle** live? It appears in the flow sketches but is not one of the seven production units. Candidates: a Manufacture module, or Construction's Salvage |

---

## Cross-references

| Document | Relationship |
|---|---|
| [`../references/production-and-inventory.md`](../references/production-and-inventory.md) | Source survey — games and real space logistics |
| [`../economy/README.md`](../economy/README.md) | Prices this unit's output; defines scope tiers |
| [`../core/README.md`](../core/README.md) | Supplies MANPOWER; Command holds standing orders; Monitoring gates inventory visibility |
| [`../../guides/module-architecture.md`](../../guides/module-architecture.md) | The 13-aspect brief the master design must answer |

### Code

| File | Relevance |
|---|---|
| `src/ResourceManager/resource_types.h` | The 14 subtypes; `Bronze` needs replacing |
| `src/Unit/unit.cpp` | `InitializeManufactureModules` — current stub modules |
| `src/game_constants.h` | `MODULE_BASE_COSTS` — every module priced in this unit's output |
| `game_types.toml` | Has an unread recipe/cost schema; relevant to open question 2 |
