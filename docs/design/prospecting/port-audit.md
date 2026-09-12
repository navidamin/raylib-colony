# Survey console port — audit against `docs/CANVAS2D_PORT_SPEC.md`

Written after the spec arrived, against commits `e1b88af` … `22fa4be`
(C1–C4 of the survey console port). The short version: **the port was a
reinterpretation, not a translation**, and the spec names almost every way
it went wrong before I made the mistakes.

The spec's own diagnostic line applies:

> If a port looks flatter, dimmer or more geometric than the JS reference,
> the cause is almost always a rule below that was skipped.

It looks flatter and more geometric. Here is which rules were skipped.

## The two structural violations

**§1 — fixed design space.** The spec says render the module into a
`RenderTexture2D` at the design size and blit it letterboxed, so every JS
coordinate ports over verbatim. `ComputeSurveyLayout(region)` does the exact
thing the spec forbids: it re-derives every rectangle from the panel's size
using ratios I extracted by eye. Everything downstream inherits that — the
block is a different size, the panels are different proportions, the rack was
squeezed into a column that does not exist in the design. And hit tests run in
screen space, which the spec forbids in the same paragraph.

**§2 — the shim.** There is no `c2d`. `src/Survey/*.cpp` makes ~70 direct
raylib draw calls. Every gap below is a consequence: with no shim there was
nowhere for a gradient, a glow or an alpha stack to live, so each call site
approximated one and the approximations do not compose.

## The twelve gaps

| § | Feature | Status | What is actually in the code |
|---|---|---|---|
| 2.1 | concave fill, ear clipping | **sidestepped** | walls decomposed into per-column quads instead of filling the polygon; no triangulator exists |
| 2.2 | multi-stop linear gradient | **partial** | per-vertex interpolation is there, but hand-rolled in `WallTone`, not a stop table, and used nowhere else |
| 2.3 | `shadowBlur` → additive glow | **wrong** | `GlowLine` does two **normal-blend** passes at fixed alpha and fixed widths. No `BLEND_ADDITIVE` anywhere in `src/Survey/`. The spec calls this "most of the visual difference between the mockup and the current build" |
| 2.4 | polygon clip | **acceptable** | analytic clamping, which is the spec's own first preference |
| 2.5 | `globalAlpha` stack | **absent** | alpha threaded by hand through every signature |
| 2.6 | ghost compositing via offscreen buffers | **wrong** | I shipped the JS **fallback** path (per-bed alpha) and wrote a comment admitting it. The spec: "This is load-bearing… per-bed alpha looks visibly wrong" |
| 2.7 | `setLineDash` | **absent** | dashes hand-rolled at one site |
| 2.8 | round joins and caps | **absent** | `DrawLineEx` per segment, no joint discs — notches on every layer edge |
| 2.9 | text | **wrong** | Exo 2 at two weights, not JetBrains Mono at three; raylib's top-anchored `y` with hand-tuned offsets, not baseline conversion |
| 2.10 | radial gradients | **absent** | **this is the reported bug.** The rack slot lights are flat rounded rects. The spec predicted the symptom exactly: "Dropped, the rack slots become flat rectangles" |
| 2.11 | ellipses | **wrong** | six `DrawEllipse` / `DrawEllipseLines`, which the spec forbids: axis-aligned, full-sweep, unantialiased |
| 2.12 | grain / noise pattern | **absent** | |

## One finding the spec slightly overstates

`hash(a,b,c)` cannot be reproduced **bit for bit** with `uint32_t`
arithmetic, because the JS loses precision: `(h ^ (h >>> 13)) * 1274126177`
is a **float64** multiply whose product reaches ~2.7e18, well past 2^53, so
the low ~11 bits are gone before `ToInt32` truncates it.

Measured over ten triples, uint32 arithmetic agrees with the JS to about
**6e-8** — the corrupted bits only ever reach the bottom of the result,
because the final `h ^ (h >>> 16)` sources its low bits from the intact high
half. Every threshold the JS tests against (`> 0.18`, `< 0.14`, `< 0.12`)
is orders of magnitude away from that, so the speckle pattern is identical
in practice. The port uses uint32 as the spec directs, and the test asserts
agreement to 1e-6 **and** that no sample in the used domain straddles a
threshold — which is the property that actually matters.

## What follows

Phase 1 of `docs/PORT_PROMPTS.md`: build the shim, test it, port nothing.
Then re-port the block through it against `js/dashboard.html`, with the
inventory table first.

### One thing the spec does not settle, and my proposal

The design space is 1536 × 1024 (3:2). The game renders 1280 × 720 (16:9)
between a top bar and a bottom bar, leaving 1280 × 604 — fit-contain gives
906 × 604 and 187 px of black either side.

**Proposal: a console module takes the whole window.** The console already
has its own module bar along the top, so the game's top bar is redundant
while it is open, and the bottom status strip can move inside the design
space later. Fit-contain into the full 1280 × 720 gives 1080 × 720 with
100 px bars either side, which is an acceptable price for every coordinate
porting verbatim. Flagged rather than decided.
