# The Console on Real Data

**Status:** P1–P4 built, P5 in progress, P6–P7 planned
**Supersedes nothing.** This is the integration plan for #16's port: the
console is finished as a *picture*, and this is how it stops being one.

The ported console (`src/ui/`) draws beautifully and invents every number on
it. Nothing on it has ever touched `ProspectingSystem`. Seven phases replace
the demo data with the game's own ground, ordered so each is separately
provable and none needs the next to be correct.

## The phases

| # | What | Status |
|---|------|--------|
| P1 | The console owns no state — a `SurveyDashState` the caller holds | **BUILT** `6132051` |
| P2 | That state lives on `ProspectingSystem`, one per module | **BUILT** `1c89f7f` |
| P3 | One knowledge model: `SurveyKnowledge` becomes a C++ face on `dash_knowledge.c` | **BUILT** `f0524ed` |
| P4 | One column: 120 m, and every vertical measure a fraction of it | **BUILT** `53910a4` |
| P5 | Real rack, real ruler, real block | **IN PROGRESS** |
| P6 | Close the loop: drilling changes `scanMultiplier` | planned |
| P7 | Retire the old console | planned |

### What P1–P4 established, and what it cost

**The old console is already unreachable.** `DrawModularUnitView` returns at
the survey-console branch for every prospecting module that has a prospecting
system, and `DrawProspectingPanel`'s first act otherwise is to print "No
prospecting system." and return. Found in P2. It makes P7 a pure deletion, and
it means `SurveyConsole::Step` is not being called by anything — which P5c has
to fix, because the ground rebuild hangs off it.

**The delineation cache was keyed on the wrong thing.** It cached against the
hole revision alone, so a second caller asking about a different column was
answered out of the first caller's cache. Latent while only one column is
asked; P5 introduces the second caller. Fixed in P3.

**Seven well-spread full-depth holes clear the gate** on the real 120 m column,
and depth is what buys it: 15 holes at 80% of the column, 28 at half, 35 at a
tenth. Measured in P4 over an R2 low-discrepancy site sequence. Two published
figures were wrong and are corrected in `dark-plating.md`; shallow-only
drilling does **not** plateau below the gate, it is five times the work.

## The boundary

`colony_c2d` is C99 and publicly includes only `ui/`. That is worth keeping —
it is what lets the console be driven by `colony_preview` and the visdiff
harnesses without dragging the game in. So nothing C++ crosses.

Three structs, in `survey_dash.h`:

| Struct | Direction | Holds |
|--------|-----------|-------|
| `SurveyDashState` | owned by the game, mutated by the console | the drill, the knowledge, the block pose, the rack, the log — P1 |
| `SurveyDashFeed` | game → console, refilled each frame | tools, selection, the ground sampler, bed names — P5 |
| `SurveyDashHost` | console → game, function pointers | charge energy, record a core, log an event — P6 |

The state lives on `ProspectingSystem` beside `SurveyConsole`, for the reason
already written into `plateLight`'s comment: the renderer is rebuilt from
nothing each frame and could only ever snap.

## P5 — real rack, real ruler, real block

Three steps, smallest first. Only the third needs the boundary.

### P5a — the ruler reads the real strata

Pure C, no game data required. The drill bar already draws its strata bands
from `DrillSim_Strata()` (`dash_chrome.c:702`); only the **ruler ticks** beside
them are still `SurveyDash_DemoDepths()` — 0/30/60/90/120, a round-number scale
that happens to end in the right place.

Replace it with one tick per stratum base, labelled with the stratum it is the
base of:

```
0 m    SURFACE
12 m   REGOLITH
34 m   MEGAREGOLITH
68 m   FRACTURED
120 m  INTACT BASALT
```

Those are the game's four depth layers (P4), so the ruler, the bands, the
energy price and `RecordCore` all finally read the same ground.

*Risk:* `MEGAREGOLITH` is long and the right pane is 370 design units. Render
and look before keeping it; shorten the label, not the stratum, if it collides.

### P5b — the rack reads the real tools

The game has five survey tools (`SurveyTool` in `survey_sprites.h`) with real
names, blurbs, and a `built` flag saying whether the game actually runs each
one. Two are built (DRILL, SURFACE SWEEP); three are not (ACTIVE SEISMIC,
PENETROMETER, GPR).

The rack shows four invented bays (`ToolRack_Demo()`, `toolrack.c:781`).

**An unbuilt tool becomes an empty bay,** not a greyed one. The reference rack
already draws `null` entries as empty machined bays, and "a bay you have no
tool for yet" is exactly what that reads as. This costs no new art and no new
visual state — which matters, because the alternative was drawing a
penetrometer icon the port does not have.

Icon mapping, from what the ported set already contains:

| Tool | Icon | Built |
|------|------|-------|
| DRILL | `TR_ICON_DRILL_STRIPED` | yes |
| SURFACE SWEEP | `TR_ICON_ROVER` | yes |
| ACTIVE SEISMIC | `TR_ICON_SEISMIC_WIDE` | no — empty bay |
| PENETROMETER | — | no — empty bay |
| GPR | `TR_ICON_SONAR` | no — empty bay |

Selection round-trips: the console reports the picked slot, the game sets
`SurveyConsole::SelectTool`, and the feed brings the selection back. One owner.

**Not wired, and not invented:** tier and tech gating. `built` is a static flag
in `survey_sprites.cpp`; nothing in the game ties a tool to a module tier or an
`UnlockRegistry` tech, and prospecting's reach is deliberately ungated
(#10). Inventing a progression rule here would be designing, not integrating.
Recorded as a gap.

### P5c — the block is a picture of the real ground

The largest step, and the one with the payoff: **holes visibly re-fit the
model**. `SurveyGround` already generates four beds with the error model baked
in — each interface carries a wrongness multiplied by `1 - confidence` — so
feeding its surfaces to the block makes every hole move the beds, with no new
code for the effect itself.

Holo3D builds `pts[k][i][j].y` once at build time from its six measured
profiles and never reads the profiles again. So real ground is an injection,
not a rewrite:

```c
/* holo3d.h */
/* Sample boundary `k` (0 = the ground you stand on, `beds` = the base of the
 * column) at (u, v) in 0..1 across the block; return depth as a FRACTION of
 * the column. NULL restores the reference's own profiles. */
typedef float (*H3DDepthFn)(void *ctx, int boundary, float u, float v);
void Holo3D_SetGround(Holo3DModel *m, int beds, H3DDepthFn fn, void *ctx);
```

`m->layerCount` already gates every render loop, and bed `k` is drawn between
boundaries `k` and `k+1`, so four beds costs one assignment. The visdiff
harness keeps passing the reference's five; the game passes four. **The port is
not re-baselined to suit a caller** — that was the reason this moved out of P4.

Three integration points on the C++ side:

1. **One knowledge model, one owner.** `SurveyConsole` owns the
   `DashKnowledge`; `SurveyDashState` borrows a pointer to it
   (`know` → `ownKnow` when nothing is fed, so standalone use still works).
   Without this the console's drill fills one model and the block reads
   another, which is P3's problem wearing a hat.
2. **`SurveyConsole::Step` needs a caller again.** It is what rebuilds the
   ground on a knowledge revision, and nothing has called it since the old
   panel became unreachable.
3. **`SurveyGround::SampleDepth`** is the sampler — it already takes a
   fractional cell, which is exactly what `H3DDepthFn` hands it.

Bed names and ranges come from `SurveyGround::EdgeM` and the strata table.
They are only drawn behind `hud->callouts`, which the console leaves off, so
they are carried for correctness rather than for pixels.

## What P5 deliberately does not do

Both of these are in #15's settled design and are being deferred with reasons,
not dropped.

**Fog as unresolved wireframe.** #15 settles that "the console draws only what
it has measured" — unknown volume gets no fill and no boundary, leaving a wire
cage that retreats as the block is drilled. The C++ `SurveyBlock` does this;
Holo3D has no concept of it. Feeding real surfaces delivers the *re-fitting*
half of the idea for free and the *fog* half not at all. Porting the cage into
Holo3D is a visual feature with its own diff gate and belongs in its own step.

**Grade tint.** Colouring beds by `ProspectingGrid::GetTotalRichness` would
replace Holo3D's depth-ramped palette, which is part of the port's visual
identity, with a data-driven one. That is a look decision against #15, not an
integration. It also needs a second data path (grades per bed) that nothing
else needs yet.

## Gates

| Step | Gate |
|------|------|
| P5a | Render the console and **look at the ruler** — labels must not collide |
| P5b | Render at each tier; the rack shows two tools and three empty bays |
| P5c | Drill holes in the preview and confirm the beds move; Holo3D visdiff still 1.15% (the harness passes five beds) |
| all | `c2dtest` 69/69, `colony_sim` 18/18, all targets build, `sectwalk` opens all 40 modules |

## Cross-references

| Where | What |
|-------|------|
| [survey-dashboard-design.md](survey-dashboard-design.md) | #15 — the design of record for the look; five bays, fog as wireframe, isolate only |
| [survey-dashboard-implementation.md](survey-dashboard-implementation.md) | #16 — the port's own build plan, C1–C7 |
| [../subsurface/README.md](../subsurface/README.md) | the ground both extraction modules read |
| [../graveyard/survey-knowledge-cpp-twin.md](../graveyard/survey-knowledge-cpp-twin.md) | why there is only one knowledge model |
| `docs/CANVAS2D_PORT_SPEC.md` | the porting contract the block is still held to |
