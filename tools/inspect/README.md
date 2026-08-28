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
