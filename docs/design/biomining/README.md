# Biomining — Parked Seed

**Status:** PARKED SEED (not being designed; stored for future maturation)
**Origin:** farming maturation node **F5**, git branch
`claude/farming-unit-design-4rz536` — the circled "Biomining" on the
original reMarkable note.

## The idea

Use engineered fungi / bacteria to **leach metals from regolith**
(bioleaching / biooxidation — real ISRU-adjacent tech): cultures secrete
acids or accumulate metal ions, turning low-grade regolith into
recoverable Fe / Ti / Al / rare elements at low energy cost but
biological speed.

## Why it's parked here and not inside farming or extraction

It is a genuine **crossover**: the culture infrastructure (strains,
myco-bays, living stock) belongs to farming's biology layer
(`docs/design/farming/maturation/CONCEPT_TREE.md` nodes F1/F2), while
the output and siting logic belong to extraction's chain (beneficiation
already has an MRE node type). A dedicated directory keeps it findable
from both when its time comes.

## Sketch (unmatured, one paragraph)

A biomining module could slot in as: a beneficiation `SeparationNode`
alternative (BIOLEACH node — slow, near-zero energy, needs water +
culture supply from a farming unit), or a standalone low-tier extraction
path for colonies rich in biology but poor in power — the inverse of
DEEP REGISTER's "solve it with uranium". Tension to design around:
throughput vs energy vs culture upkeep, plus a contamination /
sterility risk shared with the farm.

## When maturing this, start from

- Farming nodes F1/F2 (inoculant + living stock systems) — its substrate.
- `src/Unit/separation_node.h` — where a BIOLEACH node would live.
- The farming trade-off table (node B4) for cost-axis consistency.
