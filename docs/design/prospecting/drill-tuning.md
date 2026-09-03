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

Measured on the then-79 m basalt column (surface → intact basalt), dwells
included. **Section 4 later made the deepest hole a fixed 94 m**, which
retuned two constants — see 4b for the current numbers:

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

## 3b. The 32x32 ledger — the same rule, applied twice

"Double the resolution of the layer planes." The playbook above held
without amendment: metric constants never move, cell-denominated ones follow
the cell. Everything below was found by running the tests and the sim, not
by reading the code.

| constant | 16x16 | 32x32 | why |
|---|---|---|---|
| `PROSPECTING_GRID_SIZE` | 16 | 32 | the ask |
| `SUBCELL_SIZE_M` | 6.25 | 3.125 | 100 m cell / 32 |
| `PROSPECTING_REACH_PER_TIER` | {4,8,12,16} | {8,16,24,32} | same metres, same fraction per tier |
| `EXCAVATION_SUPPORT_RANGE_M` | 4 | 2 | one sub-cell across, as before |
| `SURVEY_SAMPLE_COVERAGE_TARGET` | 0.25·(8²/16²) | 0.25·(8²/N²) | now written against `PROSPECTING_GRID_SIZE`, so it never needs touching again |
| survey plan (sim player) | every 4th cell | every 8th cell | still 25 m — a hole speaks for its 20 m halo |
| `DigSite::GRID` | **8 (literal)** | = `PROSPECTING_GRID_SIZE` | see below |
| test aims | (3,3) | (7,6) | the same 79 m basalt column, re-celled; the halo ladder re-celled likewise |
| block-model relief | 0.45·diamondH | 0.60·diamondH | "the curvature is not enough to be clearly visible" — height alone never was; the plates are now slope-shaded and textured, [Dark Plating §5](../graphics/dark-plating.md) |

**The bug the lattice found.** `DigSite::GRID` was a literal 8 that never
followed the 16x16 migration: any dig spot with x or y ≥ 8 read as
exhausted, so three quarters of the lattice was undiggable — and nobody
saw it, because the sim's players dig next to their survey holes and the
16x16 numbers in section 3 were measured on the quarter that worked. At
32x32 the tier-0 sim stalled outright (total 0), which is how it surfaced.
It is now `PROSPECTING_GRID_SIZE`, with a test that digs the whole lattice
(`tests/test_dig_site.cpp`).

The classification ladder in 32x32 cells (metric footprint identical again):

| distance | cells (32x32) | support | class |
|---|---|---|---|
| 0 | cored | 1.00 | MEASURED |
| 3.125 m | 1 | > 0.91 | MEASURED |
| 6.25 m | 2 | 0.91 | MEASURED |
| 12.5 m | 4 | 0.68 | INDICATED |
| 25 m | 8 | 0.21 | INFERRED |
| 37.5 m | 12 | 0.03 | UNCLASSIFIED |

`colony_sim`, 18/18 checks, after the fix (tier 3, one season): IDLE 10493 ·
BLIND 14589 · SURVEYOR 17745 · EXPERT 18481 · HANDS-ON 22011; tier 0 ~930,
HANDS-ON 1805; the surveyor's survey progress 44%. The ordering section 3
argued for holds, with a wider gap between blind and surveyed than the
16x16 run showed — which is what one expects once the whole lattice can be
dug. Frame cost of the prospecting panel at 32x32 under `--bench`: 17 ms/frame
when this was written, ~27 ms after the panel gained generated rock, flatter
plates and textured interbands. Only ~3.5 ms of that is attributable by
isolation (see dark-plating §5, where the surprise is that the TEXTURED path
is 8x faster than the untextured fallback); the remainder does not reproduce
against a same-session baseline and is most likely host variance between
sessions — software-raster numbers are only comparable within one sitting.
The estimate field is still built once per frame and nothing scales with N⁴.

## 4. One plate, one depth (the slab mapping, and why it went)

A plate **was** a slab: its iso rows (`i+j`) spanned its stratum top to
bottom, so a cell's depth was `LayerTop + ((i+j+1)/2N)·thickness`. Aiming,
coring, the hover tick and per-metre pricing all read it. It bought
continuous depth control — 31 rows per layer, 0.4–1.7 m apart — from the
same click that chose x and y.

**It was wrong, and the playtest named it exactly:** *"as I move the mouse on
each of the 4 layer planes the vertical position in the drill bar changes,
implying that the location in y (perpendicular to the monitor) is equivalent
to depth. This is totally counter-intuitive. The z is z."*

Quite right. The stack is **exploded so that depth is the axis BETWEEN
plates**; a plate is a horizontal plane. Making the screen axis that runs
away from the viewer *also* mean depth overloads it with the one meaning the
explosion exists to remove. Now `PlateDepthM(L) = LAYER_CENTRE_M[L]` — one z
for the whole plane; the clicked plate is the depth, the clicked cell is only
where on that plane the hole comes out.

**Why the centre and not a boundary.** Three reasons, and the third is the
one that settles it:

1. A plate stands for its whole stratum — its cells carry that stratum's
   grade — so the middle is what it represents.
2. A marker parked on a boundary line reads as belonging to either of the two
   bands it separates.
3. **It is the only choice that is level with itself.** The strip gives every
   stratum an equal band and puts each plate's slot at that band's centre
   (`ProsDockFrom`), and `LAYER_CENTRE_M` is the exact midpoint of each layer
   — so a plate drawn at its stratum's centre lands exactly on its own depth
   line in the strip, by construction, at any layout. At an interface the
   plate would float half a band away from the marker that names it. Pinned
   by a test.

Cost: four target depths (6 / 23 / 51 / 94 m) instead of a continuum. That
is the point — depth is now chosen by *which plate*, x and y by *which cell*,
and neither axis pretends to be the other.

Everything downstream followed: `AimAt`, the coring crossing, the hover
cursor. `GetCrossingCell` lost its row-refinement step and got simpler.

## 4b. Retuning for a 94 m hole: pushing must stay a gamble

Fixing the depth law made the deepest hole 94 m instead of ~79. Two things
broke, and only one of them had a test.

The trip is **depth-priced** (`BIT_TRIP_BASE_S + m·BIT_TRIP_S_PER_M`), so at
0.30 s/m a fracture now cost 31 s while driving hard saved about 12. Pushing
the redline became **strictly dominated** — not a gamble, a trap — which is
the exact inversion section 1 had already fixed once at the shallower depth.
Repriced to **0.12 s/m**, with fatigue eased 0.065 → **0.055**:

| clicks/s | column | trips |
|---|---|---|
| 0 (AUTO) | 207 s | 0 |
| 1 | 107 s | 0 |
| 2 | 75 s | 0 |
| 4 | 75 s | 1 |
| 6 | 69 s | 1 |
| 8 | 68 s | 1 |
| 12 | 69 s | 1 |

Hands-on is 3.0x AUTO, harder is never slower, and a fracture is still a real
event that shows in the core log — it just no longer eats more than it costs.

**What the search taught, worth more than the numbers.** Past ~6 clicks/s the
string is heat-capped (auto-peck), so 8/s and 20/s accrue fatigue at nearly
the same rate: there is **no window** where one survives and the other
fractures, and hunting for one produced knife-edge tunings (at 0.045: 4/s
survived, 6 and 8 fractured, 12 survived). Fatigue low enough to make every
rate survive deletes the mechanic outright. The lever was never the fatigue
rate — it was the *price of the outcome*.

**And the property that broke had no test.** Section 1 fixed this inversion
once; nothing then guarded it, so it came back silently the moment a depth
changed. The campaign instrument would have shown it, but nobody runs an
instrument by accident. `driving the string harder is never slower` is a test
now.

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

The lane's sticks are counted PER STRATUM (`PROS_LOG_PER_LAYER` = 6, so 24
in the column, 2.0/3.7/5.7/8.7 m by unit) and drawn through the borehole
strip's own mapping -- equal height on screen AND level with the bit, which
a fixed metre length could not be at once. They are graded by the thermal
DOSE they were cut under -- the mean
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

## 8. The end of a hole: the string comes out

Playtest ask: *"When a drill is finished, the drill should be pulled back up
and the drill line on the 4-layered panel should vanish."*

Before this, a finished hole left its string parked at the bottom forever and
its line drawn over the block model forever. Two holes in a session left two
lines on the plates with no way to tell which one the rig was actually on,
and the machine never visibly finished anything.

**The state machine grew one state.**
`NONE -> AIMING -> DRILLING -> RETRACTING -> DONE`
(`LineHoleState`, `prospecting_system.h`). RETRACTING is the hoist: nothing
advances, clicks do nothing, the bit cools in the open as it does on a trip.
DONE now means something it did not mean before — **the string is out of the
ground** — and that is the single fact the block model reads.

| | drawn over the plates | in the borehole strip |
|---|---|---|
| DRILLING | line + string + twin cursor | string at depth |
| RETRACTING | line + string, retreating up it | string rising, hole stays cut |
| DONE | **nothing** | hole, core log, assay ticks, rig parked |

What the hole *produced* outlives it: cored cells, flipped classes, the core
log lane, the specimen, the borehole itself. What vanishes is the *live
operation*. That split is the whole design — see
[dark-plating §9.2](../graphics/dark-plating.md).

**The hoist is a beat, not a price.**

```
DRILL_PULL_BASE_S  = 1.2       DrillPullSeconds(m) = BASE + m * PER_M
DRILL_PULL_S_PER_M = 0.045     79 m column -> 4.8 s
```

Same winch as a trip, one direction and nothing to re-seat, so it is ~5x
cheaper than the fracture trip's out-and-back (`BIT_TRIP_*`: 26.7 s at the
same depth). The **payout fires at the bottom, not at the top** — the
specimen is shelved and `UpdateLineHole` returns true the instant the bit
reaches `endM`, while the machine spends the next few seconds hoisting.
Delaying knowledge behind the animation would turn a flourish into a tax.

Drawn depth runs `depthM -> PROS_IDLE_DEPTH_M` on a smoothstep
(`ProsShownDepthM`) — a winch takes up, runs, and eases the last rods in.
It lands exactly on the pose the rig rests at with no hole, so the handover
to DONE cannot jump. The trip's motion stays what it was, a half-sine out
**and back**, because a trip resumes the same hole.

**Rejected, for the record:**

- **Vanish on completion, no animation.** One frame: line, then no line.
  Cheapest, and it fails the same way the old behaviour did in reverse —
  the player never sees the machine finish, only the result blink away.
- **Vanish the core log with it.** Simplest gate (`state == NONE` everywhere)
  and it throws away the record the player just paid 79 m of heat for. The
  log is the product; the line is the process.
- **Reuse the trip's out-and-back motion.** Free code, wrong fiction: the
  string would come back down into a finished hole.
- **Trip-priced hoist** (26.7 s at 79 m). Physically consistent, and it is a
  26-second lockout with no decision in it after every hole. Rule 1 says
  costs buy *time when something went wrong*; nothing went wrong here.
- **Let a new hole be aimed during the hoist.** Cuts the wait to zero, and
  the aim would be drawn through a string still coming out of the last hole.
  `StartAim` refuses while DRILLING or RETRACTING; 4.8 s is a beat, not a
  queue.

Covered by `tests/test_line_hole.cpp`, *"a finished hole hoists its string
out before it reads DONE"*; preview states `line-pull` (mid-hoist) and
`line-done` (racked, line gone).
