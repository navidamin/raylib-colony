# B3 — The medium roster

**Status:** DRAFT v0.1 (node B3, grafted 2026-09-01)
**Node:** [`CONCEPT_TREE.md`](CONCEPT_TREE.md) § B3 · **Sibling:** [`B2-hydroponics.md`](B2-hydroponics.md)

---

## The organizing principle

A list of thirteen media is unusable. Two axes make it a design space, and
both are real biology:

**Axis 1 — trophic route: where the energy comes from.**

| Family | Energy source | Needs light? |
|---|---|---|
| **Phototrophic** | photons → biomass | yes (the sun, or watts) |
| **Chemotrophic** | H₂ / electricity / minerals → biomass | **no** |
| **Heterotrophic** | existing biomass → other biomass | no |

**Axis 2 — which loop it closes.** Carbon, nitrogen, water, waste-biomass, or
mineral. A mature farm is not a bigger greenhouse; it is a **portfolio of
loop-closers**.

> This is not invented structure. ESA's **MELiSSA** — the canonical closed
> life-support architecture — is exactly this: five compartments, each a
> different organism class closing a different loop, with the crew as
> compartment V. The multi-medium farm *is* the real design.

---

## The roster

Latency = time from power loss to first crop damage (design targets).

### Phototrophic — photons in

| Code | Medium | Eats | Closes | Latency | Real basis |
|---|---|---|---|---|---|
| **SOIL** | regolith agriculture (B1) | conditioned regolith | carbon, mineral | weeks | terrestrial agronomy; the slow, forgiving default |
| **SUBSTRATE** | inert graded regolith + delivered solution | aggregate + solution | carbon | 1–3 d | **the answer the physics actually recommends** — no soil-building decades, no full plumbing mass |
| **HYDRO** | solution culture: DWC/NFT/EBB/DRIP (B2) | solution | carbon, water | 1 h – 3 d | commercial CEA |
| **AERO** | mist culture — roots in air | fine mist | carbon, water | **15–60 min** | NASA-studied; highest growth, lowest water, clogging is the killer |
| **AQUATIC** | photobioreactor + duckweed raft | CO₂, wastewater | carbon, **water/waste** | hours | MELiSSA IVa is literally an *Arthrospira* PBR; *Lemna* is ~40% protein and doubles in days |

### Chemotrophic — no light at all

| Code | Medium | Eats | Closes | Latency | Real basis |
|---|---|---|---|---|---|
| **VAT** | gas fermentation → single-cell protein | **H₂ + CO₂ + O₂ + N** | **carbon (hardest)** | hours | *Cupriavidus necator*, ~70% protein. NASA-derived, now commercial (Solar Foods, Air Protein). **Watts → calories with no photosynthesis in the path** — several times better than plants at converting energy to food. |
| **LITHO** | chemolithotrophic mats on mineral substrate | reduced minerals | **mineral** | days | Fe/S oxidizers. Not food — this is the bridge to **BIOMINING (F5)** |

### Heterotrophic — eats biomass

| Code | Medium | Eats | Closes | Latency | Real basis |
|---|---|---|---|---|---|
| **MYCO** | fungiculture on lignocellulose | **the inedible ~50%** of crop mass | waste-biomass | days | oyster mushrooms on straw; dark-tolerant → runs through the lunar night |
| **INSECT** | mealworm / black soldier fly | crop residue, food waste | waste-biomass | days | China's *Yuegong-1* used mealworms as crew protein. Supplies **fats and B12**, which greens cannot |
| **AQUA** | aquaponics — fish + nitrifying bacteria | feed | **nitrogen** | hours | ammonia→nitrite→nitrate closes the N loop elegantly; huge water mass; very high morale |
| **CULTURE** | cultivated animal cells | expensive growth media | — | hours | late-game luxury; the media itself needs amino acids from somewhere |

### Special-loop — each eats something nothing else will

| Code | Medium | The point |
|---|---|---|
| **SPROUT** | 7-day sprouted fodder, water only | Emergency and vitamins. Honest caveat: it is a **net calorie loss** (dry matter falls) — it buys micronutrients and morale, not food. |
| **HALO** | halophytes (*Salicornia*) on reclaim brine | Eats the **brine dead-end** — the genuine unsolved bottleneck in ISS water recovery. Turns a disposal problem into a crop. |
| **PIONEER** | cyanobacterial regolith conditioning | Makes soil **biologically** instead of industrially — the slow, nearly-free path into B1.b. (*Anabaena* fixes N₂ *if* there is N₂ buffer gas to fix; on the Moon that is a supplied resource, not free air.) |

---

## The spine: buffer ↔ speed

Plot the roster on (failure latency × growth rate) and it collapses onto one
line. That is not a coincidence — **fast systems are fast because they deliver
nutrients and oxygen aggressively and hold no reserve.**

```
  fast   AERO ── HYDRO ── AQUATIC ── SUBSTRATE ── SOIL   slow
  20min    hours     hours       days         weeks
  └────────── growth rate falls, forgiveness rises ──────────┘
```

## B5 — the portfolio rule

Every medium's failure latency is measured against **the same failure**: power.
A colony-wide outage kills AERO in an hour, HYDRO in three, SOIL in a month.

> **Therefore: never put your whole farm in one medium.** Not for flavour —
> because latency correlates with a *shared* failure mode. Diversity is
> insurance you can compute.

The chemotrophic and heterotrophic families are insurance of a second kind:
they do not need light at all, so they survive the thing that kills every
phototroph simultaneously — the lunar night, or an eclipse of the solar farm.

This is the mechanic that makes B a real branch rather than a menu, and it is
what C's six programs are *portfolios of*.

## Programs → portfolios (→ C2)

| Program | Constraint | Portfolio it implies |
|---|---|---|
| **PHAROS** | eternal polar light | SUBSTRATE + HYDRO — steady photons, small crew, minimal machinery |
| **FORTNIGHT** | 14-day night | phototrophs sprint by day; **MYCO + INSECT + VAT** eat the batch-grounded biomass through the dark. Literally the light and dark alphabets. |
| **DEEP REGISTER** | reactor watts, no sun | HYDRO + VAT — photons from uranium; the only site where AERO's fragility is affordable, because power never fails |
| **ASHFIELD** | research campaign | SOIL + PIONEER + LITHO — the media that *are* the experiment |
| **PILGRIM** | guests, morale | AQUA + SOIL + SPROUT — the expensive, delightful, nutritionally trivial ones |
| **VAULT** | continuity, diversity | SUBSTRATE at scale — boring, robust, maximal register width |

Each program falls out of siting rather than being chosen from a list. That is
C3's implicitness principle doing its job at the medium layer.

## Ship order

| Wave | Media | Why |
|---|---|---|
| **Core** | SOIL, SUBSTRATE, HYDRO, MYCO, VAT | one per family, plus the hybrid. Proves the portfolio mechanic with five things. |
| **Unlock** | AERO, AQUATIC, INSECT | tier/tech rewards that change the shape of a portfolio |
| **Stretch** | AQUA, CULTURE, HALO, PIONEER, LITHO, SPROUT | flavour, late game, and the F5 bridge |

## The hard ceiling worth keeping

Microbial protein (VAT, AQUATIC) carries high **nucleic acid** load; human
uric-acid handling caps intake at roughly 20–25 g of such protein per day.
A well-documented constraint from the 1970s single-cell-protein literature.

**Design consequence:** the most energy-efficient food source *cannot* feed
the crew alone, by biology, not by balance patch. Diet diversity is mechanically
forced. That is the cleanest possible answer to "why not just build VATs" —
and it hands G2 and G4 a real ceiling to design against.

## Open questions

1. Is **SUBSTRATE** a distinct medium or a technique inside B1/B2? It is
   arguably the most realistic lunar answer, which argues for making it the
   *default* rather than an option.
2. Does the register (C) run over crops, or over **crops × media**? The latter
   is where FORTNIGHT's paired alphabets actually live, and it is a much bigger
   combinatorial object.
3. Do heterotrophic media need their own resource type (BIOMASS as feedstock),
   or does FOOD-going-TYPED (A5) cover it?
4. **LITHO** sits half in F5 (biomining). Does it ship as farming, extraction,
   or the thing that forces the two units to talk?
