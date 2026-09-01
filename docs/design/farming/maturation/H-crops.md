# H — CROPS: the alphabet the farm spells with

**Status:** DRAFT v0.1 (branch H, grafted 2026-09-01)
**Tree:** [`CONCEPT_TREE.md`](CONCEPT_TREE.md) § H · **Siblings:** [`B2-hydroponics.md`](B2-hydroponics.md) · [`B3-media-roster.md`](B3-media-roster.md)
**Visual:** [`crop_medium_matrix.html`](crop_medium_matrix.html) / [`.png`](crop_medium_matrix.png)

---

## Why crops are a major branch, not a table inside B

B decides *how* you grow. H decides *what*, and the two are not independent:
a medium's affordances and a crop's root architecture either meet or they
don't. The register (C) cycles over crops; conditioning (B1.b) cares about
crop *pairs*; morale (G2) and the food ledger (G4) care about what the crop
actually contains. Crops are the alphabet every other branch spells with —
which is why they get their own letter despite arriving late in the tree.

Branch H is grafted after G; codes are addresses, not reading order.

---

## H1 — `CropDescriptor`: the data shape ●

Named already in **A2**. This is what fills it. Mirrors `ResourceDescriptor`:
one table, thin accessors, lives in `game_types.toml`.

- **H1.a Identity** — name, botanical family (family matters: it drives
  disease sharing and rotation rules in H7), cultivar (H5).
- **H1.b Requirements** — the *gates and draws* of A1.a, at crop scale:
  `dli` (mol·m⁻²·d⁻¹), `ecBand` (dS/m), `phBand`, `tempBand` (°C),
  `rootDepth` (cm), `rootVolume` (L/plant), `supportNeed` (0–1),
  `organFormation` (NONE | SWELLING | TUBER | GEOCARPIC | FLOODED).
  The last field is small and does most of the work in H2.
- **H1.c Yield vector** — the note's *"Yield { calories, biomass, oxygen }"*
  plus its margin scribble *"nutrient pack"*, made concrete:
  `kcal`, `protein_g`, `fat_g`, `micronutrients[]`, `edibleFraction`,
  `residueMass` (→ MYCO/INSECT feedstock), `o2`. **Residue is not waste**;
  it is the heterotrophic families' input, so it must be a named output.
- **H1.d Cycle** — `daysToMaturity`, `harvestMode`
  (SINGLE | CUT_AND_COME_AGAIN | CONTINUOUS). Harvest mode changes the
  register's rhythm as much as duration does: a cut-and-come-again crop
  occupies its plot without ever freeing it.
- **H1.e Medium fit** — *not stored*. Derived by the H2 rule from H1.b.
- **H1.f Culinary/morale value** — what it is worth to a person, which is
  not correlated with its calories (see H3, H4).

## H2 — Medium fit: computed, not authored ◆

**Decided.** A hand-written 17×8 compatibility matrix is 136 numbers nobody
can keep true. Instead each medium declares what it *offers* and each crop
what it *needs*, and fit is a function:

```
Medium offers:  depth (cm) · volume (L/plant) · anchoring (0-1)
                · allowsSwelling (bool) · allowsPegging (bool) · flooded (bool)

fit(crop, medium):
    depth   = medium.depth  >= crop.rootDepth
    volume  = medium.volume >= crop.rootVolume
    anchor  = medium.anchoring >= crop.supportNeed
    organ   = crop.organFormation == NONE
              || (SWELLING|TUBER  && medium.allowsSwelling)
              || (GEOCARPIC       && medium.allowsPegging)
              || (FLOODED         && medium.flooded)
    → NATIVE if all pass with margin · WORKABLE if all pass ·
      MARGINAL if one fails softly · INCOMPATIBLE otherwise
```

Adding a crop costs one row of traits, not thirteen judgement calls. And
every "no" carries a *reason the player can learn*: a carrot forks because
nothing let its taproot run straight; a peanut fails in water because its
pegs have nothing to push into.

### What the rule produces

Order the media by failure latency (slow → fast) and the crops by root
demand, and the compatible cells form a **triangle that thins to the right**.
That shape is the finding:

```
             SOIL  SUBSTR  EBB  DRIP  DWC  NFT  AERO
  lettuce      ●     ●      ●    ●     ●    ●    ●      leaf, no organ
  tomato       ●     ●      ●    ●     ○         ○      needs anchoring
  wheat        ●     ●      ●    ◐     ○    ◐    ○      area-hungry
  potato       ●     ●      ●    ◐              ◐       tuber needs volume
  peanut       ●     ●      ○                           pegs need substrate
  carrot       ●     ◐      ○                           taproot needs depth
```

**Soil grows everything** — its cost is latency, mass and time, never
capability. The fast media are *specialists*, and what they specialise in is
crops that store nothing.

## H3 — Crop roles ●

What a crop is *for*. A colony needs every row filled; no crop fills two well.

| Role | Crops | Note |
|---|---|---|
| **Calorie staple** | potato, sweet potato, wheat, rice | the ones that need slow media |
| **Protein** | soybean, cowpea, peanut | legumes also **fix nitrogen** — see H8 |
| **Fat** | peanut, soybean, sunflower | the scarcest macro in closed loops |
| **Fresh / morale** | lettuce, tomato, strawberry, herbs | high value per gram, near-zero calories |
| **Micronutrient** | brassica greens, sprouts | vitamins the staples lack |
| **Industrial** | biofuel feedstock, fibre | feeds the existing FOOD→BIOFUEL chain |
| **Service** | legumes (N), cover crops (conditioning) | grown for what they leave behind, not what they yield |

**Service crops** are the interesting category: a crop the player plants to
*improve the plot*, not to eat. That is ASHFIELD's whole hypothesis (S2.4)
turned into a routine decision.

## H4 — The diet ledger ●

The nutrient pack, at colony scale. Complete nutrition is a **portfolio
problem**, and three hard facts enforce it:

1. **B12 does not exist in plants.** At all. It comes from microbes and the
   animals that host them — so INSECT, AQUA, or a VAT strain, or imported
   supplements. A purely botanical farm has a deficiency clock running.
2. **Fat is scarce.** Most CEA crops are near-fat-free. Peanut, soy and
   insect larvae are the realistic sources.
3. **The microbial ceiling** (B6) caps single-cell protein near 20–25 g/day.

Together these mean the efficient farm and the *survivable* farm are
different farms. G2 and G4 get a real ceiling to design against instead of a
calorie counter.

## H5 — Cultivars & genetics ○

Space agronomy has always bred **dwarfs** — NASA's Superdwarf wheat is the
canonical case: shorter straw, faster cycle, less area per grain, at some
cost in absolute yield. That is a clean research axis: a cultivar is the same
crop with a shifted trait vector (height ↓, cycle ↓, DLI ↑, yield ↓).

Fits the tech tree without new machinery: research unlocks *rows*, not rules.

## H6 — Seed stock & viability ○

The note's **"128 seeds."** Seeds are a stock, not an infinite spawner:

- Seed decays under cosmic radiation — the premise THE VAULT (S2.6) exists to
  solve, and the reason the archive must stay *alive and cycling*.
- Regenerating stock means letting a cohort **bolt and go to seed**, which
  costs that plot its harvest. A recurring, legible sacrifice.
- Losing a cultivar is permanent unless another sect still carries it →
  the register's diversity becomes an actual asset, not a flourish.

## H7 — Pathogens & monoculture ○

A sealed environment is sterile until it isn't; after that it is a paradise.
Monoculture amplifies, shared botanical families share diseases (H1.a), and
a recirculating solution (B2) distributes a root pathogen to every plant on
the loop in hours — **the same loop topology that makes hydro efficient makes
it an epidemic vector.**

Release valves: rotation, family diversity, loop segmentation (a D-branch
placement decision), and F1's **defense mold** inoculant.

## H8 — Crops as the register's alphabet ●

This is where H hands off to C:

- The register cycles over crops; **n** is how many the catalog exposes.
- Crop *pairs* (parent → child) are what B1.b conditioning scores — the
  ASHFIELD matrix is literally an H×H table.
- Legumes make the pairing non-arbitrary: a nitrogen-fixing parent genuinely
  improves its child's plot, which is the first *real* reason a rotation
  order matters rather than a designed one.
- Answers the open question from B: the register runs over **crops**, and
  medium is a property of the *plot* the register schedules onto. Crops ×
  media only matters where FORTNIGHT interleaves light and dark alphabets —
  and there the "dark" letters are organisms, not crops (MYCO, INSECT, VAT),
  so it is two registers sharing a clock, not one bigger one.

## H9 — The calorie–fragility coupling ◆

**Decided, and it is the payoff of grafting H at all.**

Staples need volume, depth and time; the media that provide those are exactly
the slow, heavy, forgiving ones. The fast media grow leaves — high value per
gram, near-zero stored calories. Aeroponic potato is real, but it is a
*seed-tuber* technique, capital-intensive and not how anyone feeds a colony.

> **Therefore the portfolio (B5) is not merely insurance — it is
> nutritionally mandatory.** You cannot eat from the fast half of the farm,
> and you cannot react quickly with the slow half.

This closes the loop between B and H: the buffer↔speed frontier is also a
calories↔responsiveness frontier, and the player is standing on both.

---

## Preliminary catalog

Seventeen crops spanning every role and every organ-formation case. Cycle in
days under CEA conditions; DLI in mol·m⁻²·d⁻¹; depth in cm; edible = harvest
index. Numbers are **design starting points calibrated to real horticulture**,
to be tuned against play.

| Crop | Role | Cycle | DLI | Depth | Organ | Edible | Notes |
|---|---|---|---|---|---|---|---|
| Lettuce | fresh | 30–40 | 12–17 | 10 | none | 0.90 | the reference hydroponic crop |
| Mizuna / brassica greens | micronutrient | 21–30 | 12–16 | 10 | none | 0.90 | fastest useful leaf |
| Basil / herbs | morale | 30–40 | 14–20 | 12 | none | 0.50 | cut-and-come-again; morale per gram is the highest on this table |
| Kale / chard | micronutrient | 40–55 | 15–20 | 20 | none | 0.70 | tolerates abuse |
| Strawberry | morale | 60–90 | 17–22 | 18 | none | 0.15 | PILGRIM's crop; nutritionally trivial, psychologically not |
| Tomato | fresh | 80–110 | 20–30 | 30 | none | 0.50 | needs trellis **and hand pollination** — no insects up here |
| Pepper | fresh | 90–120 | 20–30 | 30 | none | 0.40 | as tomato |
| Radish | micronutrient | 22–30 | 14–18 | 12 | swelling | 0.60 | tops edible too; fastest root |
| Beet | staple-ish | 50–65 | 15–20 | 22 | swelling | 0.80 | root + greens, dual harvest |
| Dwarf wheat | staple | 60–90 | 30–45 | 30 | none | 0.45 | area-hungry, power-hungry — and its **55% straw is MYCO's substrate** |
| Soybean | protein / fat | 90–120 | 25–35 | 40 | none | 0.40 | **N-fixing service crop** |
| Rice | staple | 110–140 | 25–35 | 25 | flooded | 0.45 | wants standing water → EBB is its natural medium |
| Potato | staple | 90–120 | 17–25 | 35 | tuber | 0.80 | best calories per m²·day here; aeroponic only for seed tubers |
| Sweet potato | staple | 110–150 | 18–26 | 45 | tuber | 0.75 | **leaves are edible too** — a staple that also yields greens |
| Peanut | fat / protein | 120–150 | 20–30 | 35 | geocarpic | 0.35 | pods form *underground*; needs a medium to peg into. Soil-family only. |
| Carrot | micronutrient | 70–90 | 15–20 | 40 | none (deep taproot) | 0.80 | forks on any obstruction — the hardest crop to site |
| Salicornia | halophyte | 60–90 | 18–24 | 20 | none | 0.60 | the only crop that eats **HALO**'s brine |

**Not on this table, because they are not crops:** the organism media grow
their own stock — AQUATIC (spirulina, chlorella, duckweed), MYCO (oyster,
shiitake on residue), INSECT (mealworm, black soldier fly), AQUA (tilapia),
VAT (*Cupriavidus*), CULTURE (cell lines), PIONEER (cyanobacteria).
SPROUT takes any grain seed and is a *mode*, not a medium with a crop list.

## Open questions

1. **Catalog size.** 17 here; the note says 128. Is 128 the *lifetime*
   catalog across all research, with ~16–24 available at any one time
   (which is what the register sizes in S2 actually assume)?
2. Does `organFormation` need a fifth case for vining/climbing, or is that
   just `supportNeed` at the top of its range?
3. Are cultivars (H5) separate `CropDescriptor` rows or a modifier applied to
   one? Rows are simpler; modifiers keep the catalog from tripling.
4. Pollination (H1.b gate?) — a real labour cost for every fruiting crop, or
   an abstracted efficiency term? It is one of the few places crew *time*
   would enter farming directly.
5. Does H4's B12 clock actually tick, or is it a one-time gate that unlocks
   when the player builds any animal/microbial medium?
