# Reference Survey — Production & Inventory

Status: RESEARCH — survey only, no design decided

Gathered to answer one question: **if this game is fundamentally inventory
management, how do comparable games and real space programs structure a
manufacturing unit, and how does it know what to make?**

Two halves: real space logistics, then game UI patterns. Links go to the
sources; screenshots could not be embedded (see Gaps at the end).

---

# Part 1 — Real space logistics

## 1.1 Classes of Supply (NASA / MIT)

The canonical inventory taxonomy for spaceflight. Ten functional classes, each
with subclasses, built specifically because no existing scheme fitted
interplanetary exploration — NASA's own ISS scheme (CCART) was judged
inadequate for lacking categories for propellants, habitation infrastructure,
and surface exploration equipment.

| COS | Class |
|---|---|
| 1 | Propellants and fuels |
| 2 | Crew provisions |
| 3 | Crew operations |
| 4 | Maintenance and upkeep |
| 5 | Stowage and restraint |
| 6 | Exploration and research |
| 7 | Waste management and disposal |
| 8 | Habitation and infrastructure |
| 9 | Transportation and carriers |
| 10 | Miscellaneous |

**The insight worth stealing:** the classification is **by function, not by
material**. What something is *for* determines its class, not what it is made
of. Our `ResourceType` is purely material (Fe, Si, ALLOYS…). A function axis is
orthogonal and currently missing — and it is exactly the axis a *demand* system
would want, because demand arises from function ("we need maintenance stock"),
not from chemistry.

Source: [MIT Space Logistics — Classes of Supply](http://strategic.mit.edu/spacelogistics/classes_supply.php) ·
[AIAA 2006 paper (NTRS PDF)](https://ntrs.nasa.gov/api/citations/20170011140/downloads/20170011140.pdf)

## 1.2 SpaceNet (MIT / NASA)

A discrete-event simulator for space logistics: models the flow of vehicles,
crew, and supply items through a supply network, tracking pressurised and
unpressurised cargo capacity, fuel, and time. Used to model Apollo 17, ISS
assembly and resupply, lunar-surface ISRU, and a **lunar outpost build-up of 17
flights achieving continuous presence over eight years**.

Supports trade studies on mission planning, vehicle performance, and **demand
uncertainty** — i.e. the tool treats "how much will we actually need" as a
first-class modelled variable rather than a fixed number.

Source: [SpaceNet: Modeling and Simulating Space Logistics (NTRS)](https://ntrs.nasa.gov/search.jsp?R=20150014813) ·
[SpaceNet Cloud](https://www.researchgate.net/publication/355908378_SpaceNet_Cloud_Web-based_Modeling_and_Simulation_Analysis_for_Space_Exploration_Logistics)

## 1.3 ISS Inventory Management System (IMS)

The most directly relevant real system, and the most surprising.

- **~20,000 items tracked.** Every item barcoded.
- Crew **manually scan** items and log final stowage location; both flight and
  ground crews update the database **daily**.
- **Inventory and Stowage Officers (ISOs)** work in mission control every day
  purely to manage this.
- NASA is retrofitting **RFID** (the REALM experiments) specifically because
  line-of-sight scanning is such a burden.
- There is a genuine, documented problem of **losing things on the ISS**.

**The insight worth stealing:** in real spaceflight the hard problem is not
producing goods, it is *knowing what you have and where it is*. Every factory
game assumes perfect, instantaneous, global inventory knowledge. If this game's
core is inventory management, that assumption is the most obvious thing to
question — and no comparable game questions it.

Source: [SSP 50007 — Space Station IMS Bar Code spec (PDF)](http://www.metal-works.com/pdfs/SSP50007RB.pdf) ·
[Inventory and Stowage Officer Lessons Learned (National Academies PDF)](https://sites.nationalacademies.org/cs/groups/depssite/documents/webpage/deps_063600.pdf) ·
[NASA RFID on ISS](https://www.atlasrfidstore.com/rfid-insider/nasa-rfid-never-lost-space-58b2a4/)

## 1.4 ISRU — what a lunar factory actually makes

- Feedstock is **regolith**: abrasive powder, dust, small rocks.
- Regolith is **~45 wt% oxygen** in returned Apollo samples, bound in ilmenite,
  olivine, pyroxene, and plagioclase. Liberation is **energy-intensive**.
- Products: **oxygen** (life support *and* propellant oxidiser) plus **Fe, Ti,
  Al** for construction and manufacturing.
- NASA is expanding work on **combined oxygen-and-metal extraction** — the same
  process yields both.

**The insight worth stealing:** on the Moon, **oxygen is a by-product of metal
extraction**, not something farmed. Our current model has Farming produce food
and Extraction produce metals as separate chains, with O₂ appearing from
nowhere. A combined regolith→(O₂ + metals) process is both more accurate and a
better chain: it couples life support directly to industrial throughput.

Source: [NASA ISRU overview](https://www.nasa.gov/overview-in-situ-resource-utilization/) ·
[NASA Lunar ISRU Technology Overview (NTRS PDF)](https://ntrs.nasa.gov/api/citations/20220006072/downloads/LIVE-ISRU%20-Overview-RevB.pdf) ·
[System analysis of an ISRU production plant](https://www.sciencedirect.com/science/article/abs/pii/S0094576522006579)

---

# Part 2 — How games structure it

## 2.1 Comparison

| Game | Production model | How it knows what to make | Inventory surface |
|---|---|---|---|
| **RimWorld** | Bills queued on a workbench | **"Do until you have X"** with pause/unpause thresholds; ingredient filters; bills run top-down in priority order | Stockpile zones. **Items not in a stockpile are not counted** |
| **Factorio** | One machine = one recipe | Player sets the recipe. Requester chests pull ingredients; copy-pasting a recipe onto a requester chest auto-requests **30 s of continuous crafting** | Logistic network + production statistics graphs |
| **Satisfactory** | One machine = one recipe, fixed rate | Player plans ratios by hand; a mismatch throttles the whole chain to its slowest element | Storage containers, no global pool |
| **Captain of Industry** | Machine holds **multiple recipes**; activate / deactivate / prioritise each | Player toggles per machine. Throughput shown normalised **per 60 s** | Per-machine buffers + global stats |
| **Anno 1800** | Building = one fixed chain, no recipe choice | Nothing to decide — you build the right *ratio of buildings* | Island-wide warehouse pool |
| **Surviving Mars** | Production building, fixed recipe, worker **shifts** | Nothing to choose; building shows predicted production on click | Global colony resource pool |
| **The Crust** | Lunar base automation, conveyors + drones | Research trees gate capability; colonists have traits/skills affecting output | (closest genre match — see below) |

## 2.2 The closest existing game: The Crust

A **lunar base automation and colony manager** — "Moonpunk" setting. Early
Access July 2024, 1.0 scheduled September 2026, 74% positive over ~1,700
reviews. Mine lunar resources, craft processing machinery, build automated
production chains, run rover expeditions, manage colonists' health/mood/needs
and hire experts whose traits optimise production and research.

This is the nearest thing to what we are building. Worth playing before
finalising the Manufacture design.

Source: [The Crust on Steam](https://store.steampowered.com/app/1465470/The_Crust/) ·
[VEOM Studio](https://veomstudio.com/the-crust/) ·
[PC Gamer coverage](https://www.pcgamer.com/this-combo-of-surviving-mars-with-factory-building-has-you-automate-and-expand-a-lunar-colony-in-the-wake-of-disaster/)

Also announced: **Possible One: Lunar Industries** — a "Surviving Mars meets
Anno 2205" lunar city builder.
[PCGamesN](https://www.pcgamesn.com/possible-one-lunar-industries/announcement)

## 2.3 The three patterns that matter for us

### A. The thermostat — RimWorld's "Do until you have X"

The single most transferable mechanic found. A bill says *make this until stock
reaches N; pause when satisfied; unpause when it drops to M.* It answers "how
does the factory know what to make" with almost no data — one target number per
product — and it converts production from a thing the player micromanages into
a **standing policy** they set once and adjust occasionally.

It is also almost free for us: the target is a number, the current stock is
already in `resourceStorage`, and the delta is the order.

The hysteresis (separate pause and unpause thresholds) is the detail that makes
it work — without it, production oscillates around the target.

### B. One recipe per machine vs many recipes per machine

The structural fork.

- **One recipe** (Factorio, Satisfactory) — the machine *is* its purpose. The
  player's decision is spatial and quantitative: how many, arranged how.
- **Many recipes with priority** (Captain of Industry) — the machine is a
  general capability, and the player's decision is *allocation over time*.

For us, "many recipes with priority" fits better: we have no spatial factory
floor, so there is nothing to arrange. Allocation over time is the decision
that survives our abstraction level.

### C. Rate normalisation is a UI decision

Captain of Industry normalises every recipe display to **per 60 seconds**,
including a toggle for it, rather than showing per-cycle numbers. This is the
same problem our own guide flags as the most expensive mistake in this codebase
(`docs/guides/module-architecture.md` Part II §2 — declaring units at
boundaries). A production UI must pick one rate unit and hold it everywhere.

### D. Inventory visibility as a mechanic

RimWorld: **items not in a stockpile do not count** toward a bill's target.
That is a deliberate design choice making *where things are* matter, not just
how many exist — and it is the only game mechanic found that rhymes with the
ISS's real problem.

---

# Part 3 — What this suggests for us

Not decisions, just what the survey points at.

1. **Steal the thermostat.** "Keep N of X on hand" is the demand signal, and
   it costs one number per product. It answers the open question from the
   Manufacture discussion directly.
2. **Multi-recipe with priority** beats one-recipe-per-machine at our
   abstraction level, because we have no factory floor to lay out.
3. **The function axis is missing.** COS classifies by purpose; our
   `ResourceType` classifies by material. Demand is expressed in function terms.
   Worth considering a light second axis rather than more resource types.
4. **Couple O₂ to metal extraction.** It is what really happens, and it wires
   life support into industrial throughput instead of leaving it a separate
   farm chain.
5. **The unexploited niche is knowing what you have.** Every factory game gives
   the player perfect global inventory. Real space logistics is defined by the
   opposite. If our core is inventory management, that gap is the most
   defensible thing to build on — and it connects directly to the Core's
   Monitoring module.

---

# Gaps in this survey

- **No screenshots.** The session's network egress policy blocked every domain
  to direct fetch (`WebFetch` and `curl` both refused Wikipedia, NASA NTRS,
  Steam, and the game wikis); only search-engine access was available. Every
  entry links to where the UI can be seen. To fix, widen the environment's
  network policy — see
  https://code.claude.com/docs/en/claude-code-on-the-web — or paste screenshots
  into the session directly.
- **Second-hand detail.** Findings come from search summaries rather than
  primary pages, so specific numbers should be re-checked against the linked
  source before being relied on for tuning.
- **Not covered:** Dyson Sphere Program and Oxygen Not Included yielded no
  specifics on demand-driven production; Workers & Resources and Ixion returned
  nothing useful. Anno's and Satisfactory's *planning* tools (community
  calculators) may be worth a separate look, since they reveal what players
  find hard to reason about unaided.
