# Data Inspector

Prints real generated game data to the terminal, so value bugs are diagnosed
from numbers instead of from reading generation code.

```bash
cmake --build build --target colony_inspect
./build/src/colony_inspect             # parent cell (5,5), tier 3
./build/src/colony_inspect 12 7        # a specific parent cell
./build/src/colony_inspect 12 7 1      # ...at tier 1
```

## Output

1. **ResourceManager raw quantities** for the parent cell, per depth layer —
   the absolute numbers the world generator produced.
2. **ProspectingGrid sub-cell view** — composition fractions (should sum to
   ~1.00) plus absolute quantity per sub-cell, i.e. exactly what the
   prospecting chain consumes.

Uses the same fixed world seed as the preview tool, so the numbers describe
the same world the preview screenshots render.

## Why this exists

Samples were showing `0%` richness and empty compositions. Reading the
generation code suggested the logic was fine, and two rounds of theorising
blamed the wrong component. Dumping the actual values found both causes in
minutes:

- the test harness never called `GenerateResourceMap()`, so the planet was
  empty;
- abundances are **quantities in the hundreds to thousands**, while the whole
  prospecting chain treated them as **0-1 composition fractions**.

Neither was visible from reading code. When a displayed value looks
implausible, run this first.


---

## `colony_measure_clusters`

Answers *"how big is an ore body on the 8×8 lattice?"* from the real generator.

```bash
cmake --build build --target colony_measure_clusters
./build/src/colony_measure_clusters          # all 400 planet cells (the full population)
./build/src/colony_measure_clusters 100      # a 10x10 corner, for a quick look
```

Built for one question from excavation's Phase 9b: an access shaft opens a 3×3 block of
sub-cells — is that the right size? Reading the generator could not answer it; the field the
player actually optimises is `GetQuantity × GetGroundTruth[resource]`, and what that looks
like after normalisation and clamping is not obvious from the code.

Prints, over every (parent cell × depth × resource) field:

- **best-placed N×N capture** for N = 2..5 — what a shaft of that footprint, sited perfectly,
  would open
- **rich ground** — count and bounding box of sub-cells at ≥ 1.5× the field mean
- **the weight field `w`** with abundance divided out, and which of
  `SUBCELL_VARIATION_MIN` / `MAX` actually binds

It also dumps one raw 8×8 field at the end, so the summary can be checked against numbers
instead of trusted. Run it before changing `SUBCELL_VARIATION_*` or the shaft footprint.
