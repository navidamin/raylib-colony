# Data Inspector

Prints real generated game data to the terminal, so value bugs are diagnosed
from numbers instead of from reading generation code.

```bash
cmake --build build --target colony_inspect
./build/src/colony_inspect                  # Mare Imbrium, tier 3
./build/src/colony_inspect -43.3 -11.4      # Tycho: any LAT LON on the Moon
./build/src/colony_inspect -43.3 -11.4 1    # ...at tier 1
./build/src/colony_inspect --pick 32.8,-15.6
```

## Output

1. **The region** the point falls in and its orbital survey — who the
   ground is, and the composition the region card would show.
2. **ResourceManager raw quantities** at that point, per depth layer —
   the absolute numbers the ground generator produced.
3. **ProspectingGrid sub-cell view** — composition fractions (should sum to
   ~1.00) plus absolute quantity per sub-cell, i.e. exactly what the
   prospecting chain consumes.

Uses the same fixed world seed as the preview tool, so the numbers describe
the same world the preview screenshots render.

## Why this exists

Samples were showing `0%` richness and empty compositions. Reading the
generation code suggested the logic was fine, and two rounds of theorising
blamed the wrong component. Dumping the actual values found both causes in
minutes:

- the test harness never seeded the world, so the planet was empty;
- abundances are **quantities in the hundreds to thousands**, while the whole
  prospecting chain treated them as **0-1 composition fractions**.

Neither was visible from reading code. When a displayed value looks
implausible, run this first.
