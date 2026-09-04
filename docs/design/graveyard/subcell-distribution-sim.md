# `subcell_distribution_sim.py` — the Python model of the generator

**Lived:** `docs/design/excavation/subcell_distribution_sim.py` (53 lines)
**Removed:** the commit carrying this record — *Phase 4 groundwork: re-measure the survey payoff*
**Replaced by:** `tools/inspect/measure_clusters.cpp` — the survey-payoff table, measured off the real generator

## What it was

A standalone simulation of the sub-cell distribution: 1–2 Gaussian shoots on a
lattice, normalised to mean 1.0, clamped to `[0.3, 2.0]`, sampled 200,000 times
to answer one question — how much better is the best spot inside a tier's reach
than an average one? Its output was the survey-payoff table in
`excavation-design.md`, and through that table it is the reason the design says
surveying is worth paying for.

## Why it went

**It modelled the generator instead of measuring it, and the two drifted.**

Its first two lines were `G, MIN, MAX = 8, 0.3, 2.0` and `REACH = [2,4,6,8]`.
Both were true when it was written. The lattice then grew 8 → 16 → 32 and the
reach rings became `{8,16,24,32}`, and nothing connected the script to either —
it had its own copy of the world, so the world could change underneath it
silently and did.

The numbers it produced were wrong by about a third at the top end (**+130%**
against a measured **+89%**), and two of the conclusions drawn from it were not
merely imprecise but backwards:

- It had the mean reachable spot **falling** 1.03 → 0.87 with tier, which the
  design read as "more ground is not better ground; it's more varied ground".
  Measured, the mean is near-flat 1.07 → 1.00. The fall was an artifact of a
  four-cell T0 window in an eight-cell model.
- It had the best reachable spot **pinned to the 2.0 clamp** from T2 on, which
  the design read as "raise `SUBCELL_VARIATION_MAX` if late game needs headroom".
  Measured, it tops out at 1.89 and never reaches the clamp — so that lever does
  nothing.

A model that has its own copy of the constants will always eventually answer a
question about a game that no longer exists.

## What survived

**The question, and the shape of the answer.** Surveying payoff still rises
monotonically with tier; the field's best spot is still reachable only ~5% of the
time at T0; reach still raises the ceiling while surveying finds it. Every
structural conclusion the design drew held up — it was the magnitudes and two
sub-claims that did not.

**The method moved into `colony_measure_clusters`**, which builds a real
`ProspectingGrid` over a real `ResourceManager` and reads `IsSubCellInReach` and
`GetReachForTier` from the same headers the game does. It cannot drift from the
game without failing to compile.

## What would bring it back

Nothing in this form. A fast analytical model is genuinely useful for *sweeping*
generator parameters without a rebuild — but it would have to import its lattice
size, clamps and reach rings from the C++ headers rather than restating them,
and it would still owe its headline numbers to a measurement of the real thing.
