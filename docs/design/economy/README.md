# Colony Economy — Master Frame

Status: DRAFT — economic frame, sits above the per-unit designs

> **Auto-context rule:** When working on trade, colony reserves, sect
> specialization, site selection scoring, or any pricing/value question, read
> this first. It defines *why* a sect is worth building somewhere, which the
> unit designs assume but do not state.

This document is the layer above `core/`, `manufacture/`, and `prospecting/`.
Those describe how a unit works. This describes why a colony exists and what
makes one different from another.

---

## 1. The organizing principle: the gravity well is the price system

Every product's value is set by where it must travel from and to. There is no
separate "economy design" — the well *is* the economy.

```
                          EARTH-BOUND  ── must beat terrestrial supply
                               ▲          → only extreme value/kg survives
                               │
        ┌──────────────────────┴──────────────────────┐
        │                 TRANSLUNAR                  │  ── worth what lifting
        │        (cislunar space, transit crews)      │     it out of Earth's
        └──────────────────────┬──────────────────────┘     well would cost
                               │
                          INTRA-LUNAR   ── worth up to import parity:
                    (sect ↔ sect, colony ↔ colony)   what Earth would charge
                               ▲                      to land it here
                               │
                          EARTH IMPORTS ── the value floor under everything
                          (polymers, computers, crew)
```

**Import parity** is the single most useful number in the game. Anything a
colony makes locally is worth *at most* what Earth would charge to deliver it.
That gives every product on the manufacturing list a ceiling price without any
hand-tuned price table, and it makes substitution research directly legible:
each unlock lowers a bill you were paying in launch mass.

**The fourth flow is a posture, not a market.** Earth→Moon imports are the
dependency every strategy must take a position on: pay it, substitute it away,
or earn enough exporting to stop caring. See `references/production-and-inventory.md`
§3 for why import mass is the natural denominator.

---

## 2. Limits — each limit founds a defensive specialty

A limit is not only a constraint. It is a market for whoever specialises
against it. This is the arc that makes a colony feel like it grows up: *early
on a hazard is your problem; later it is your neighbour's problem and your
revenue.*

| Limit | Scope | Specialty it founds |
|---|---|---|
| Micrometeorite flux | intra-lunar | **Safety** — shielding blocks, hardened stowage, repair response |
| Solar particle events, GCR dose | intra-lunar | Shelter capacity, burial services, medical — refuge sold to neighbours |
| Thermal cycling (±280 °C, 14-day period) | intra-lunar | Maintenance & materials — fatigue-resistant parts, refit yards |
| The lunar night | intra-lunar | **Energy storage and firm power** — power sold through the dark |
| Dust — abrasive, electrostatic | intra-lunar | Filters, seals, bearings; cleanroom volume as a service |
| Pressurised volume scarcity | intra-lunar | **Habitable volume as a commodity** — build and lease volume |
| Terrain and distance | intra-lunar | Transport corridors; passes and chokepoints make hub siting a play |
| Crew isolation and health | intra-lunar | **Livability hub** — hospital, R&R, training, dose recovery rotation |
| Import mass cost | all | The floor under every local price; substitution research as strategy |

---

## 3. Opportunities by scope

### Intra-lunar — selling to other sects and colonies

| Focus | Core flow |
|---|---|
| **Metal & water extraction** | regolith / ice → Fe, Ti, Al, H₂O to everyone |
| Food & closed-loop agriculture | FOOD and O₂ surplus to industrial sects that skipped Farming |
| Energy & grid | firm power through the night; ENERGY exported over cable |
| Manufacturing hub | tools, spares, wear parts — everything the thermostats keep reordering |
| Logistics hub | throughput, warehousing, cache staging for expansion |
| Safety & medical services | refuge, dose recovery, rescue — from the limits column |
| Research & licensing | SCIENCE and process unlocks sold to colonies that skipped Research |

### Translunar — selling to space, not to the Moon

| Focus | Core flow |
|---|---|
| **LOX for traverse** | oxygen is ~80% of propellant mass — the anchor product |
| LH₂ from polar ice | the other 20%. Polar sites only — this is what makes poles contested |
| Spaceport | pads, dust standoff, landing and refuel services; every exporter needs one |
| Depot & consumables | water, food, spares to stations and transit crews |
| Vehicle assembly & servicing | building deep-space craft above the well instead of lifting them |
| Cislunar comms / nav | infrastructure services for everything moving in the volume |

### Earth-bound — only extreme value density survives the trip

| Focus | Core flow |
|---|---|
| **He3** | late-game, speculative, enormous capex, strip-mines vast area |
| KREEP rare earths / isotopes | value-dense cargo from KREEP terrain (`KREEP_SCIENTIFIC` exists) |
| Science data | far-side astronomy, samples — the massless export |
| Vacuum-made precision goods | ultra-pure wafers, exotic fibre — the "made in vacuum" premium |
| Beamed power / solar hardware | lunar-made cells feeding space solar — the megaproject economy |

---

## 4. Scarce sites — what makes specialisation competitive

Specialisation only creates tension if the good sites are few. The Moon
obliges, and `SiteArchetype` is already half of this system.

| Site | Why scarce | What it enables |
|---|---|---|
| **Peaks of eternal light** | A few km² of near-continuous sun at the poles | Best energy real estate on the Moon; whoever holds it sells power |
| **Polar cold traps** | The only H₂O/H₂ at scale | The only LH₂ business; the reason poles are contested |
| **Lava tubes** | Rare, but free shielding volume | Safety and volume-leasing without a burial campaign (`LAVA_TUBE`) |
| **Far-side radio quiet** | A *regulatory* scarcity — needs distance from everyone's noise | Science export. The one focus that wants to be **far from the network** |
| **Terrain chokepoints** | Geometry of passes and traverses | Logistics hubs |
| **KREEP terrain** | Geochemically restricted | Rare earths, isotopes, the science economy |

Far-side radio quiet is worth special attention: every other siting rule pulls
sects *together* for trade. This one pushes them apart, and pays for the
isolation. It is the only anti-network incentive in the design.

---

## 5. Sect geometry follows strategic focus

The sect's shape should state its strategy before any label does. This gives
the hexagon a second job: not only *which* sockets are filled, but how far out,
how buried, and how elongated the whole figure sits.

| Focus | Geometry |
|---|---|
| Safety | Buried, compact, thick — low profile, dome dominant, ring tucked in |
| Extraction | Sprawling, open — wide ring, pits and spoil beyond it |
| Energy | Widest footprint — panel fields and mast lines along the ridge |
| Spaceport | Linear, one axis — pads held at dust-plume standoff distance |
| Farming | Glass-heavy dome cluster, maximum lit surface |
| He3 harvest | Not a point but a **front** — mobile equipment sweeping area over time |
| Research (far-side) | Deliberately alone — distance from the network is part of the build |
| Logistics | Star-shaped — road convergence is the visual identity |

---

## 6. Three dynamics that fall out

**1. Strategies interlock, so trade is structural.** A He3 colony imports
metals, food, safety and spaceport services. An Earth-bound strategy *requires*
an intra-lunar economy beneath it. The scopes stack like a pyramid — intra-lunar
is the base, translunar the middle, Earth-bound the peak — and a player cannot
skip to the top.

**2. Risk rises with scope.**

| Scope | Demand | Margin | Risk |
|---|---|---|---|
| Intra-lunar | Steady | Thin | Low — your customers are next door |
| Translunar | Window-driven | Better | Contract and timing risk |
| Earth-bound | Speculative | Enormous | Enormous capex against a market that may move |

A multi-colony run becomes a portfolio decision, not a tech-tree race.

**3. Limits mature into exports.** Every entry in §2 starts as something that
hurts you and ends as something you sell. That is the clearest growth arc
available, and it costs no extra systems — it is the same hazard model read
twice.

---

## 7. What this implies for existing systems

| System | Implication |
|---|---|
| `SiteArchetype` | Already encodes part of §4. Needs peaks-of-eternal-light and far-side-quiet as distinct archetypes |
| Site selection UI | Currently shows composition and Earth-comms. Should also read as *"which strategies does this site support?"* |
| Sect specialization | `SECT_DEFAULT_LOADOUT` is a placeholder for §3 — the loadout should be the strategic choice |
| Colony reserves | The intra-lunar market clears here; pricing needs import parity as its anchor |
| Manufacture | Its product list (`references/production-and-inventory.md`) is priced by §1 |
| Transport | Roads serve intra-lunar; a spaceport is the translunar equivalent and does not exist yet |

---

## 8. Open questions

| # | Question |
|---|---|
| 1 | Is import parity computed from a single launch-cost constant, or does launch cost fall over the campaign? Falling cost is historically accurate and quietly deflates every local industry over time — interesting, or punishing? |
| 2 | Are other colonies AI-run competitors, neutral markets, or the player's own? Portfolio play (§6.2) needs at least the third |
| 3 | Does He3 need a demand *event* on Earth (fusion breakthrough) to become viable, making it a gamble rather than a tech unlock? |
| 4 | Should geometry (§5) be player-authored or derived from the socket loadout? Derived is cheaper and self-consistent; authored is more expressive |
| 5 | Far-side quiet penalises transport and comms — is that a real tradeoff, or just a worse site nobody picks? Needs the science export to be genuinely lucrative |

---

## Cross-references

| Document | Relationship |
|---|---|
| [`../references/production-and-inventory.md`](../references/production-and-inventory.md) | Product list this prices; Classes-of-Supply and import-mass framing |
| [`../core/README.md`](../core/README.md) | Crew capacity gates how much strategy a sect can run |
| [`../prospecting/README.md`](../prospecting/README.md) | Site knowledge is how the player finds the scarce sites in §4 |
| [`../../guides/module-architecture.md`](../../guides/module-architecture.md) | §7 Economy of the per-module brief inherits its scarcity from here |
