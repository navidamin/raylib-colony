# Drill Tuning — Campaign Results and the Constant Ledger

**Status: MEASURED** — every number here comes from running the model, not
from feel. Method: the in-repo instrument
`./build/tests/colony_tests "[campaign]"` (a hidden Catch2 case in
`tests/test_line_hole.cpp` that drives the real `UpdateLineHole` at swept
click rates and prints the table). Re-run it whenever `DRILL_*` constants
move.

## 1. The clicking campaign (v2 — idle demoted to a crawl)

Playtest verdict on v1: idle carried too much of the drill — the string
made real progress with hands off, so clicking felt like a garnish, and a
single click still lurched. **v2 inverts the ownership: the clicks ARE the
drill.** Idle is a bare crawl (0.15 rpm), the per-click kick is smaller
(0.12), the decay window longer (1.2 s) so a *rhythm* is what holds speed.
Sustained f clicks/s equilibrates near `IDLE + KICK·f·TAU`.

Measured, full 79 m basalt column (surface → intact basalt), dwells
included:

| clicks/s | rpm held | column | dwell | note |
|---|---|---|---|---|
| 0 (AUTO) | 0.15 | 180 s | 0 s | never heats — bleed beats gain at idle, finishes cold |
| 1 | ~0.29 | 94 s | 0 s | |
| 2 | ~0.44 | 63 s | 0 s | the comfortable cruise |
| 4 | ~0.73 | 53 s | 14 s | heat gate engages in basalt |
| 6 | ~0.97 | 48 s | 18 s | furious clicking buys 5 s — dwell eats the rest |

Ownership check: hands-on is **3.8x** faster than AUTO (was 1.4x in v1),
and AUTO still finishes every hole (drilling-procedure.md Rule 1 — time,
never the run). Past ~4 clicks/s the heat gate converts extra drive into
dwell, so the ceiling is soft, not a wall. A test now pins the cold-AUTO
property (`an idle hole never cooks the bit`).

Chosen: `DRILL_RPM_IDLE 0.15, KICK 0.12, MAX 1.0, TAU 1.2`.

## 2. The spin law (why the auger finally correlates)

An auger only reads as *screwing itself down* if one on-screen revolution
descends one thread pitch. Screw-true rotation is therefore derived, not
styled:

```
spin (rad/s) = advance(px/s) * 2*pi / PITCH_px
advance(px/s) = DRILL_ADVANCE_MPS[layer] * rpm * pxPerM(layer band)
```

With the v2 advance rates (section 5) the spin — like the descent — steps
gently at the seams instead of collapsing:

| stratum | px/m | advance px/s/rpm | screw-true rad/s/rpm |
|---|---|---|---|
| regolith | 11.2 | 22.4 | 7.4 |
| megaregolith | 6.1 | 16.8 | 5.6 |
| fractured | 3.9 | 13.3 | 4.4 |
| basalt | 2.6 | 9.6 | 3.2 |

(v1 for comparison ran 18.5 → 7.1 → 5.0 → 1.7 — a 2.6x spin cliff at the
first seam alone.)

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

## 5. Advance rates are tuned against the DOCK (v2)

The strip draws every stratum at (near) equal band height while the strata
run 12/22/34/52 m thick, so px-per-metre falls ~4.3x from regolith to
basalt. The v1 rates (`{5.0, 3.5, 3.8, 2.0}` m/s) were tuned in *metres*
— on screen the bit slammed from 55.8 px/s to 21.3 px/s at the first seam
(x0.38), and to a fifth at the basalt seam. That was most of the reported
"synthetic stop at the border".

v2 chooses rates by **band traverse time** instead: ~6/8/10/14 s per band
at full spindle → `DRILL_ADVANCE_MPS {2.0, 2.75, 3.4, 3.7}`. On-screen
speed now steps x0.75 / x0.79 / x0.72 at the seams — a gentle slowdown
with depth, no cliff. Deeper rock being *faster in metres* is fine: its
cost is heat (pecks) and energy per metre, not a wall of slowness.

Heat unchanged: `+ rpm·(0.35+hard·0.75)·0.50 − 0.15` per second, cool
0.25, auto-peck at 1.0 down to 0.45.
`LAYER_HARDNESS {0.25, 0.55, 0.45, 0.95}`.

## 6. The seam blend

The other half of the border stop was a *step*: rate and hardness switched
the instant `LayerOfDepthM` flipped, and entering harder rock spiked heat
gain right at the seam — so the auto-peck dwell tended to land exactly on
the border, reading as the drill refusing the boundary. Now
`DrillAdvanceAtM` / `DrillHardnessAtM` lerp both values across
`DRILL_BLEND_M = 6 m` centred on each seam (the bit feels the next rock
coming). The renderer's sparks and screw-true spin read the same blended
values, so nothing about the cut — speed, spin, heat, sparks — changes
discontinuously at a plate border.

## 7. The bit: wear, fracture, and the trip

Redline's Tier 3 failure (redline-disposition.md §4), in the game. Wear
runs 0 → 1 through two channels and NEVER ends the run:

```
abrasion:          dW  = adv_m · hard · BIT_WEAR_PER_M            (0.008 /m·hard)
thermal fatigue:   dW/dt = BIT_FATIGUE_RATE · x²,                 (0.065 /s)
                   x = (heat − BIT_FATIGUE_ONSET)/(1 − ONSET),    (onset 0.60)
                   accruing whether or not the bit advances — hot is hot
fracture at W=1:   trip of BIT_TRIP_BASE_S + depth · BIT_TRIP_S_PER_M
                   (3 s + 0.30 s/m — 27 s from 79 m), no advance, fast
                   cool, fresh bit (W=0) after
```

Quadratic-above-onset is the shape that makes "too long, too hot" the
trigger rather than heat per se: cruising warm accrues nothing, riding
the auto-peck cycle (0.45→1.0 sawtooth) accrues ~0.24·RATE per second of
it. Measured on the 79 m basalt column (campaign instrument, now printing
trips and end wear):

| clicks/s | column | trips | wear at end |
|---|---|---|---|
| 0–2 | 180–63 s | 0 | 0.33 (abrasion only — a cool bit does two columns) |
| 4 | 53 s | 0 | 0.95 |
| 6–8 | 48 s | 0 | **0.97 — the fastest play finishes ON the redline** |
| 12 (spam) | 75 s | 1 | fractured at depth, paid the 27 s trip |

The gamble scales with depth twice over: a deeper hole gives fatigue more
time to reach 1.0, and the trip it forces costs more from further down.
The same 6/s pace that survives 79 m fractures on a longer column.

Not ported with it (still out by decision or blocker): Tier 2 lost
intervals need the pressure band (excluded for now); Tier 1 cooked
evidence is blocked on the assay-quality term (disposition doc §5);
voluntary tripping and wireline await a UI affordance.

### 7b. The fine core log (v3 of the lane)

The lane's sticks are 5 m (`PROS_LOG_INTERVAL_M`), equal height (a log is
a document), graded by the thermal DOSE they were cut under -- the mean
over metres cut of x², x = heat excess above the fatigue onset. Smoked
(PARTIAL) at dose >= `PROS_LOG_SMOKE_DOSE` (0.0005); LOST is the stick the
bit fractured in. The threshold is a near-zero floor: in effect any
hot metre smokes the stick -- a trough-cut stick with a sliver of hot
metres must not read intact between two smoked ones. Advance-gated -- a dwell off the face marks nothing
(recovery is a property of metres cut, disposition doc section 4b).

v2 graded by the WORST INSTANT and the auto-peck sawtooth made the record
alternate `PLPLPL` -- a stick that held the redline peak logged LOST, the
next one, cut while cooling, PARTIAL. Real, but it read as random breaks
(playtest report). Dose per metre reads the same cycle as one sustained
band. Measured columns (campaign, I/P/L per stick):

```
0-1/s  IIIIIIIIIIIIIIII   clean
2/s    IIIIIIIIIIIIIIIP   one smoked stick at the bottom
4/s    IIIPPPPPPPPPPPPP   the hot run, as one band
12/s   IIPPPPPPPPPPPPPL   spam: smoked throughout, LOST where the bit let go
```

The grade is a record only -- no survey term reads it yet (the
disposition doc's named blocker).

Shake (same pass): rumble is now `rpmN^2 * (0.25 + 1.2 * hardness)` --
no base term, ground-scaled; the old flat `0.35 + 1.1*rpmN^2` was
reported too strong at idle and identical in regolith and basalt.
