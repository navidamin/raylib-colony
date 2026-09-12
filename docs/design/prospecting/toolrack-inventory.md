# ToolRack — gap inventory

Required by `docs/CANVAS2D_PORT_SPEC.md` before any code is written.
Source: `js/dashboard.html` lines 21–712. Design space 1536×1024
(`ToolRack.SCENE`).

## Scope: variant B only

ToolRack ships **two racks**. Variant A (`drawRack`/`drawScene`, the
`drawSpine`/`drawTopBracket`/`drawGusset` chassis, `drawHeader`/`drawFooter`,
`drawPill`, `drawRow`, `drawCaption`, and the `drill`/`seismic`/`rover`
icons) is not reachable from the dashboard. The only entry points the
dashboard calls are:

- `ToolRack.drawRackB(ctx, cfg.rack, 0, 0, {...})` — `dashboard.html:1396`
- `ToolRack.hitTestB(...)` — `dashboard.html:1619`
- `ToolRack.makeGrain(...)` — `dashboard.html:1652`
- `ToolRack.util` destructured for `rpoly, rrect, text, condensed, glowOn,
  glowOff` — `dashboard.html:1295`

So this port covers **variant B plus the shared primitives**, and leaves
variant A unported. That is a scope decision, not a skipped row: porting a
second rack the game never draws would be a few hundred lines of dead code.
If the dashboard is ever pointed at `drawRack`, variant A owes its own
inventory. Variant A's `screw` (line 205) *is* in scope — variant B calls it.

## Shared primitives (lines 64–157)

| Line | Canvas call | Spec § | Shim function | Notes |
|---|---|---|---|---|
| 68 `rpoly` | `quadraticCurveTo` corner rounding | **gap, see below** | `c2d_rpoly_pts` (new) | per-vertex radius + `(dx,dy)` offset. The single most-used primitive in the module |
| 79 | `quadraticCurveTo` | **gap** | flattened in `c2d_rpoly_pts` | one quadratic per rounded corner |
| 83 `rrect` | — | — | `c2d_rpoly_pts` with 4 equal radii | the JS defines it in terms of `rpoly`; so does the port |
| 91 `line` | `moveTo/lineTo/stroke` | 2.8 | `c2d_polyline` | |
| 95 `crossfade` | `globalAlpha = tool.fade` | 2.5 | `c2d_push_alpha` | draws prev state, then current at `fade` |
| 100 `glowOn` / 101 `glowOff` | `shadowBlur` | 2.3 | `c2d_glow_stroke` | **a state toggle, not a draw call.** See "the glow problem" below |
| 104 `slab` | `createLinearGradient` | 2.2 | `c2d_gradient_linear` | vertical, over the polygon's bounds |
| 111, 113 | `ctx.clip()` ×2, **nested** | 2.4 ③ | **gap** | clip inside clip — the shim refuses to nest |
| 115 | `globalAlpha = o.grainA ?? 0.09` | 2.5 | `c2d_push_alpha` | |
| 120 `hole` | `ctx.clip()` | 2.4 ③ | clip stack | not called by variant B; ported with the primitives for completeness |
| 128 `makeGrain` | `createPattern` | 2.12 | `c2d_grain_init` / `c2d_grain_draw` | **`Math.random()`** — see "grain and the diff" |
| 139 `text` | `measureText` | 2.9 | `c2d_text_tracked` | per-character advance for letter spacing |
| 145 | `measureText` | 2.9 | `c2d_measure` | |
| 149 `condensed` | `translate` + `scale(sx,1)` | **gap** | `c2d_text_scaled` (new) | horizontal squeeze; **every string in the module goes through it** |
| 152 | `createLinearGradient` as `fillStyle` for text | **gap** | `c2d_text_gradient` (new) | vertical gradient over the cap height |

## Variant B (lines 449–712)

| Line | Canvas call | Spec § | Shim function | Notes |
|---|---|---|---|---|
| 476 `railBlock` | `createLinearGradient(0,0,w,0)` | 2.2 | **gap** | **horizontal.** `C2DGradient` is y-only |
| 480 | `globalAlpha = 0.08` | 2.5 | `c2d_push_alpha` | grain over the rail |
| 498, 503, 504 `hexNut` | `ctx.arc` ×3 | §3 | `c2d_disc` | screen space, small — antialiased disc |
| 506 `drawChassisB` | — | — | — | body slab, rails, top and bottom brackets |
| 532 `drawBarsB` | — | — | — | header and footer bars, 3 screws, gradient title, underline |
| 549 `drawPillB` | `createRadialGradient(cx,cy,8, cx,cy,64)` | 2.10 | `c2d_fill_radial` | `r_in > 0`, as the spec notes for all six sites |
| 556 | `glowOn(PB.glow, 16)` | 2.3 | `c2d_glow_stroke` | active pill core — **fills, not strokes** (see below) |
| 572–575 `screwB` | `ctx.arc` ×4 | §3 | `c2d_disc`, `c2d_ring` | 4.5px heads; antialiasing matters |
| 585 | `glowOn(PB.sel, 14)` | 2.3 | `c2d_glow_stroke` | selected box outline — a genuine stroke |
| 592 | `ctx.clip()` (rrect) | 2.4 ③ | clip stack | frame highlight, clipped to the rounded box |
| 607 | `ctx.clip()` (rpoly `lab`) | 2.4 ③ | clip stack | label plate edge + pip spill |
| 610 | `createRadialGradient(...,2,...,64)` | 2.10 | `c2d_fill_radial` | pip light spilling onto the plate |
| 615 | `ctx.arc` | §3 | `c2d_disc`, `c2d_ring` | 6px hole in the label plate |
| 624 | `glowOn(PB.glow, 14)` | 2.3 | `c2d_glow_stroke` | status pip — **a fill** |
| 638 `drillStriped` | — | — | — | |
| 640 | `glowOn(PB.glow, 6)` | 2.3 | glow | drill head — **a fill** |
| 645 | `ctx.clip()` (rpoly `shaft`) | 2.4 ③ | clip stack | stripes clipped to the shaft |
| 646 | `translate` + `rotate(-PI*0.28)` | **gap** | `c2d_transform` (new) | 13 rotated bars; the only `rotate` in the module |
| 649 | `glowOn(PB.glow, 5)` | 2.3 | glow | the dashed ellipse |
| 650 | `setLineDash([7,4])`, `lineDashOffset = 3` | 2.7 | `c2d_dashed_polyline` | **`lineDashOffset` — gap**, no phase argument |
| 651 | `ctx.ellipse(cx, cy+21.5, 40.5, 16, 0, 0, TAU)` | 2.11 | `c2d_ellipse_pts` | full sweep |
| 652 | `lineDashOffset = 0` | — | — | reset |
| 654 `seismicWide` | — | — | — | |
| 658 | `glowOn(PB.glow, 5)` | 2.3 | `c2d_glow_stroke` | the trace; `lineJoin='miter'` here, not round |
| 661 `sonar` | — | — | — | |
| 663 | `glowOn(PB.glow, 6)` | 2.3 | glow | |
| 665 | `ctx.arc(ox,oy,r, -PI*0.44, PI*0.1)` ×3 | 2.11 | `c2d_ellipse_pts` | **partial arcs** — `rx == ry`; a full-sweep implementation silently closes them, exactly the 2.11 trap |
| 666 | `ctx.arc(ox,oy,5,0,TAU)` | §3 | `c2d_disc` | emitter dot |
| 667 | `setLineDash([3,4])` | 2.7 | `c2d_dashed_polyline` | the bearing line |
| 691 `drawRackB` | `translate(x,y)` | **gap** | `c2d_transform` | the rack's own origin |
| 701 `drawSceneB` | `scale(s,s)` | **gap** | `c2d_transform` | 512×768 units × 2 |

## Counts, against spec §3.5

Spec §3.5 gives ToolRack: linear 5 · radial 4 · glow 17 · clip 8 · dash 3 ·
alpha 3 · ellipse 4 · measureText 1 · drawImage 0 · pattern 1.

Whole-module actuals (A + B + shared), which is what that column counts:
linear 5 ✓ · radial 4 ✓ · clip 8 ✓ · dash 3 ✓ · measureText 1 ✓ ·
drawImage 0 ✓ · pattern 1 ✓. Three rows differ and both discrepancies are
in the table, not the greps — see CLAUDE.md:

- **ellipse** — the row says 4 for ToolRack, actual is 2 (lines 369, 651).
  The other two (`dashboard.html:1425, 1427`) belong to Dashboard, whose row
  says 0. The total of 5 is right.
- **glow and globalAlpha** count *code sites*, not textual occurrences.

Variant B's own share: linear 1 (+2 shared) · radial 2 · glow 7 · clip 3
(+3 shared) · dash 2 · alpha 1 (+2 shared) · ellipse 1 · arc 10.

## Six things the shim cannot do yet

The spec says to say so explicitly rather than approximate. None of these is
one of the twelve gaps; all six are real and all six block the port.

1. **`c2d_rpoly_pts`** — polygon with per-vertex corner radius, corners as
   quadratic Béziers, plus the `(dx,dy)` offset `slab` relies on. Used ~20
   times in variant B and it is what `rrect` is built from. Without it every
   rounded plate, box, pill and pip is the wrong shape.
2. **Nested clip.** `c2d_clip_poly_begin` early-outs when a clip is already
   open. `slab` (line 104) nests two deep on every bevelled part, which is
   most of the chassis. Needs a clip stack that intersects.
3. **Horizontal linear gradients.** `C2DGradient` is `c2d_gradient_linear
   (float y0, float y1)` — y only. `railBlock` (476) and variant A's label
   plate are `createLinearGradient(x0,0,x1,0)`. Needs an axis, and
   `c2d_gradient_at` needs the coordinate along it.
4. **A transform stack** — `translate`, `scale`, `rotate`. `condensed` uses
   translate+scale for *every string in the module*; `drillStriped` uses
   translate+rotate; `drawRackB` and `drawSceneB` use translate and scale.
   Not reducible to baking coordinates: the stripes rotate about a point
   inside a clip.
5. **Gradient-filled text** (`condensed` with `o.gradient`) — the header
   title and variant A's caption. A vertical gradient over the cap height,
   used as the glyph fill.
6. **`lineDashOffset`** — `drillStriped` starts its dash pattern at phase 3,
   variant A's drill at −3. `c2d_dashed_polyline` always starts at 0, so the
   dashes land in the wrong places around the ellipse.

## The glow problem, which is bigger than it looks

`c2d_glow_stroke` glows a **polyline**. Five of variant B's seven glow sites
glow a **fill**: the pill core (556), the status pip (624), the drill head
(640), the sonar emitter (663). Canvas's `shadowBlur` applies to whatever is
painted next — `fill()` as readily as `stroke()` — and a filled shape's
shadow is the shape's own silhouette blurred, not its outline stroked.

Approximating it by stroking the outline would put a halo ring around a
solid shape and leave its interior unlit. So the shim needs
`c2d_glow_fill(pts, n, colour, blur)` alongside the stroke version, built
the same way: N source-over passes of the polygon dilated outward, summing
to the ink-conserving total from the 2.3 amendment. For a fill the
conserved-ink peak is not `0.8·w/blur` — that formula is for a stroke of
width `w`. A large filled area's shadow saturates near its own alpha, so the
total is `min(1, …)` against area, and the honest thing is to derive it the
same way it was derived for the stroke: measure one isolated case against
the reference and fit.

## Grain and the diff

`makeGrain` (128) fills a 128×128 tile from `Math.random()` and returns a
repeating pattern. It is **not seeded**, so the reference's grain differs on
every run and no diff over a grained region can ever be stable.

The harness will therefore render both halves with `grain: null` — which is
a supported path, not a hack: `drawRackB` already passes `grain = level >= 6
? opts.grain : null`, so `level: 5` disables it through the module's own
API. Grain gets its own check: `c2d_grain_init(seed)` is deterministic, so
the port is verified for *coverage and alpha* (mean and variance of the
grained region against the reference's) rather than pixel identity.

## Plan

The shim work in "Six things" comes first, with a `c2dtest` assertion for
each before it is used — the spec's rule is not to start a step before the
previous one has a passing test. Then the module, then the diff at `SS=2`.
