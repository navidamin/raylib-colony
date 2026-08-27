# Drill Tuning — Campaign Results and the Constant Ledger

**Status: MEASURED** — every number here comes from running the model, not
from feel. Method: a Python mirror of `ProspectingSystem::UpdateLineHole`
(the exact heat/rpm/advance equations), swept over sustained click rates and
strata; screen-space numbers from the dock geometry (~134 px per band).
Re-run the sweep whenever `DRILL_*` constants move — it is ten lines.

## 1. The clicking campaign

Sustained click rate → equilibrium rpm `IDLE + KICK·rate·TAU` (capped), and
the full 120 m column with heat dwells included:

| clicks/s | old rpm (x idle) | old column | tuned rpm (x idle) | tuned column |
|---|---|---|---|---|
| 0 (idle) | 0.60 (1.00x) | 106 s (33 s dwell) | 0.65 (1.00x) | 105 s (38 s dwell) |
| 1 | 0.79 (1.31x) | 101 s | 0.76 (1.17x) | 100 s |
| 2 | 0.97 (1.62x) | 95 s | 0.88 (1.35x) | 98 s |
| 4 | 1.35 (2.25x) | 92 s (55 s dwell) | 1.10 (1.69x) | 95 s |
| 6 | capped | 90 s | 1.25 (1.92x) | 93 s |

The two findings that drove the change:

- **The lurch was real.** One click bumped speed **+42%** instantly
  (`KICK/IDLE = 0.25/0.60`). Tuned to **+23%** (`0.15/0.65`), ceiling
  1.92x instead of 2.25x.
- **Deep clicking buys dwell, not depth.** At 4+ clicks/s more than half the
  basalt leg is spent cooling — the heat gate works. Clicking pays where it
  should: a surface hole runs 3.7 s idle, 2.6 s driven (1.4x); the soft
  layers are where hands-on wins.

Chosen: `DRILL_RPM_IDLE 0.65, KICK 0.15, MAX 1.25, TAU 0.75`.

## 2. The spin law (why the auger finally correlates)

An auger only reads as *screwing itself down* if one on-screen revolution
descends one thread pitch. Screw-true rotation is therefore derived, not
styled:

```
spin (rad/s) = advance(px/s) * 2*pi / PITCH_px
advance(px/s) = DRILL_ADVANCE_MPS[layer] * rpm * pxPerM(layer band)
```

Measured against the old fixed rate (9.0 rad/s per rpm):

| stratum | px/m | advance px/s/rpm | screw-true rad/s/rpm |
|---|---|---|---|
| regolith | 11.2 | 55.8 | **18.5** (old rate was HALF — looked pushed, not drilled) |
| megaregolith | 6.1 | 21.3 | 7.1 |
| fractured | 3.9 | 15.0 | 5.0 |
| basalt | 2.6 | 5.2 | 1.7 |

The renderer now computes screw-true per layer, **floored at 6.0·rpm**: soft
ground locks the screw illusion exactly; hard rock turns faster than it
bites, which is honest grinding. A dwell winds the spin to a creep (1.1).

## 3. The 16x16 ledger — every constant the finer lattice touched

The rule that made the migration survivable: **metric constants never move**
(halo RANGE 20 m, energy per metre, layer thicknesses, heat). Everything
denominated in *cells* had to follow the cell:

| constant | 8x8 | 16x16 | why |
|---|---|---|---|
| `PROSPECTING_GRID_SIZE` | 8 | 16 | the ask |
| `SUBCELL_SIZE_M` | 12.5 | 6.25 | 100 m cell / 16 |
| `PROSPECTING_REACH_PER_TIER` | {2,4,6,8} | {4,8,12,16} | same metres, same fraction per tier |
| `PROSPECTING_MAX_GRID_SIZE` | 8 (literal) | = GRID_SIZE | was a stack buffer bound; literal 8 smashed the stack on construction |
| `SURVEY_SAMPLE_COVERAGE_TARGET` | 0.25 | 0.25·(8²/16²) | it is a fraction of the LATTICE; unscaled it silently demanded 4x the holes for the same progress and flipped colony_sim's survey-beats-blind claim |
| `EXCAVATION_SUPPORT_RANGE_M` | 10 | **4** | scales with the face (one sub-cell across). At 10 m every neighbour of a dug spot went Indicated (support 0.68 at 6.25 m) and **blind digging self-mapped past the surveyor** — sim: blind 11470 vs surveyor 10731. At 4 m a neighbour reads 0.09, a sub-classification hint; surveyor wins again (10913 vs 10779) |
| survey plan (sim player) | every cell | every 4th cell (25 m) | a hole speaks for its 20 m halo; mid-gap support 0.68. Also removed two sim-side falsehoods the finer lattice exposed: the surveyor stopped coring when its TRAY filled (the engine's own rule says it must not), and it ground ~19 ticks of XRF for a lab stage the design retired |

The classification ladder in the new cell units (metric footprint identical
— the Measured column around one hole is ~12.5 m wide on both lattices):

| distance | cells (16x16) | support | class |
|---|---|---|---|
| 0 | cored | 1.00 | MEASURED |
| 6.25 m | 1 | 0.91 | MEASURED |
| 12.5 m | 2 | 0.68 | INDICATED |
| 25 m | 4 | 0.21 | INFERRED |
| 37.5 m | 6 | 0.03 | UNCLASSIFIED |

Also rewritten: the vertical-continuity test. Its global-peak-drift
statistic broke honestly — with two shoots per field the global maximum can
switch shoots between layers, reading as huge drift over continuous ground,
and its resource pick silently compared against FLAT deeper layers where the
element does not exist at depth. It now asserts Pearson correlation of the
two layers' yield fields (> 0.35 in at least 3/4 of cells) on an element
present in both.

## 4. The plate-row depth mapping

A plate is a **slab**: its iso rows (`i+j`) span its stratum top to bottom —
`CellRowDepthM = LayerTop + ((i+j+1)/2N)·thickness`. Used by aiming (the
second click's cell IS the hole's end depth), coring (each crossing lands at
the crossed cell's own row), the hover tick on the borehole strip, and
per-metre pricing (`DrillEnergyToDepthMetres`). Depth resolution at 16x16:
31 rows per layer, 0.4–1.7 m per row.

## 5. Advance and heat (from the previous pass, unchanged here)

`DRILL_ADVANCE_MPS {5.0, 3.5, 3.8, 2.0}` m/s at rpm 1.0;
heat `+ rpm·(0.35+hard·0.75)·0.50 − 0.15` per second, cool 0.25,
auto-peck at 1.0 down to 0.45. `LAYER_HARDNESS {0.25, 0.55, 0.45, 0.95}`.
