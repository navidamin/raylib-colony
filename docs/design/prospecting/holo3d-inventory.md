# Holo3D — gap inventory

Required by `docs/CANVAS2D_PORT_SPEC.md` before any code is written.
Source: `js/dashboard.html` lines 961–1287. Verified byte-identical to
`js/holo3d.js`, so it is ported **once**.

Counts checked against the spec's §3.5 column for Holo3D — all ten rows
agree: linear 1, radial 0, glow 4, clip 2, dash 2, alpha 4, ellipse 0,
measureText 0, drawImage 1, pattern 0. `glowOn` is 0 here: that helper is
ToolRack's, and grepping only `shadowBlur` would have been correct for
this module and badly wrong for the next one.

| Line | Canvas call | Spec § | Shim function | Notes |
|---|---|---|---|---|
| 1055 | concave `path(poly); fill()` | 2.1 | `c2d_fill_poly_gradient` | `[...top, ...bot.reverse()]` over a height field — reliably concave |
| 1057 | `createLinearGradient` | 2.2 | `c2d_fill_poly_gradient` | 4 stops, `neon/mid/deep/deep` at 0 / 0.2 / 0.75 / 1, over the polygon's screen-y extent |
| 1062 | `ctx.clip()` | 2.4 ① | *analytic* | mesh lines lerp between `top[i]` and `bot[i]` and are inside the wall by construction |
| 1069 | scatter quads | 2.1 | `c2d_fill_poly` | 4-gons, convex, but routed through the same fill |
| 1082 | top-surface cells | 2.1 | `c2d_fill_poly` | slope-shaded, one quad per cell |
| 1083 | mottle quads | 2.1 | `c2d_fill_poly` | gated on `hash(i+3, j+5, 99+k) < 0.12` |
| 1096 | `shadowBlur` via the `stroke()` helper | 2.3 | `c2d_glow_stroke` | **one code site, five blur values**: 16 surface top edge · 10 other top edges · 0 hidden edges · 3 base edge · 14 bright corner / 2 silhouette corner. Honours `fast` |
| 1125 | `globalAlpha = GHOST` | 2.6 | — | the JS **fallback**. Deliberately not ported: it is the path that looks wrong |
| 1129 | `drawImage` + `globalAlpha` | 2.6 | `c2d_group_*` | ghost group drawn opaque, composited **once** at 0.3 |
| 1135 | `globalAlpha = 1` | — | — | reset |
| 1181 | `textBaseline = 'alphabetic'` | 2.9 | `C2D_BASELINE_ALPHABETIC` | applies to every HUD string |
| 1184 | `shadowBlur = 8` | 2.3 | `c2d_glow_stroke` | HUD corner brackets |
| 1186 | `shadowBlur = 0` | — | — | reset |
| 1191 | `ctx.clip('evenodd')` | 2.4 ② | `c2d_clip_segment_outside` | base ring against `model.hull`, so the ring hides behind the block |
| 1192 | `setLineDash([8s, 6s])` | 2.7 | `c2d_dashed_polyline` | base ring |
| 1193 | `setLineDash([])` | — | — | reset |
| 1202 | `globalAlpha = alphaOf(0)` | 2.5 | `c2d_push_alpha` | the whole reticle |
| 1204 | `setLineDash([5s, 5s])` | 2.7 | `c2d_dashed_polyline` | reticle inner ring |
| 1205 | `setLineDash([])` | — | — | reset |
| 1207 | `shadowBlur = 12` | 2.3 | `c2d_glow_stroke` | the two reticle sweep arcs |
| 1212 | `ctx.arc` | §3 | `c2d_disc` | pulsing centre dot, 3.5·s — screen space, and ≤4px so it needs the antialiased disc |
| 1214 | `shadowBlur = 0` | — | — | reset |
| 1217 | `textAlign = 'left'` | 2.9 | `C2D_ALIGN_LEFT` | `DRILL SITE · LOCK` and the θ/YAW line |
| 1220 | `globalAlpha = 1` | — | — | reset |
| 1228 | `globalAlpha` 0.35 / 1 | 2.5 | `c2d_push_alpha` | callout dimming for unselected beds |
| 1229 | `shadowBlur = 6` | 2.3 | `c2d_glow_stroke` | callout leader line |
| 1231 | `ctx.arc` + `shadowBlur = 0` | §3 | `c2d_disc` | callout anchor dot, 3.2·s |
| 1237 | `textAlign = 'left'` | 2.9 | `C2D_ALIGN_LEFT` | `L{k+1} · NAME` |
| 1239 | `textAlign = 'right'` | 2.9 | `C2D_ALIGN_RIGHT` | the bed's tag, right-aligned to `bx + bw - 10s` |
| 1242 | `textAlign = 'left'` | — | — | reset |
| 1244 | `globalAlpha = 1` | — | — | reset |

Not a Canvas gap, but load-bearing and easy to lose:

- `lineJoin`/`lineCap = 'round'`, set at 1046 (block) and 1181 (HUD) — spec
  2.8. Every stroke goes through `c2d_polyline`, which adds the joint discs.
- `hash(a,b,c)` seeds the wall speckle and the top mottle. `c2d_hash`, with
  the precision caveat in the port audit.
- The monotone-chain convex hull, which the base ring's clip needs.
- Six boundaries × 17 × 14 points: the JS reallocates freely, the port
  preallocates on the model (spec §3).

---

## Result: where the diff stands

Measured by `tools/visdiff/visdiff.sh` on the default `home` state
(Chromium renders `tools/visdiff/ref.html`, `holo3d_visdiff` renders the
port, `diff.py` compares at a tolerance of 24/255 per channel and writes a
heatmap).

| Supersampling | Differing px | % |
|---|---:|---:|
| off (`SS=1`) | 42,509 | 2.04 |
| **`SS=2` (the default the console should ship)** | **23,927** | **1.15** |
| `SS=3` | 27,863 | 1.34 |

**Under the spec's 2% gate at SS=2.** It was 2.60% when the port was first
written; four findings account for the difference, and three of them were
defects rather than approximations.

### 1. The reference was rendering in the wrong font (harness bug)

`ref.html` pulled JetBrains Mono from `fonts.googleapis.com`, which the
agent proxy blocks. A blocked webfont does not fail loudly: `document.fonts
.ready` resolves anyway, Canvas walks down the family stack, and Chromium
rendered the whole reference in DejaVu Sans Mono. Every text comparison up
to this point was measuring one typeface against another — and it presented
as a *port* bug, because the reference's text looked a weight heavier.

`ref.html` now declares `@font-face` against the same three TTFs the port
loads, and `shoot.js` exits non-zero if any of the three fails to load, so
a silent fallback can never quietly invalidate a run again.

### 2. The base ring was clipped the wrong way round (port bug)

The JS reads:

```js
ctx.beginPath(); ctx.rect(-1e5, -1e5, 2e5, 2e5);
path(model.hull); ctx.closePath(); ctx.clip('evenodd');
```

which looks like "everywhere except the hull" — the ring passing behind the
block. It is not. `path` is `pts => { ctx.beginPath(); ... }`
(`js/holo3d.js:219`), so it **discards the rect**, and the clip is the bare
hull. Even-odd on a simple hull is just its interior, so the reference draws
the base ring *only where the block covers it*: a ghost arc showing through
the strata, never a ring around the base.

The port had implemented the evident intent. The spec says translate what
the code does, and the reference render is the ground truth, so it now
clips inside (`c2d_clip_segment_inside`, added for this). **This is almost
certainly a bug in the original `holo3d.js`** — flagged for upstream. If it
is fixed there, this is a one-line change back.

### 3. Glow composited additively instead of source-over (shim bug)

`c2d_glow_stroke` built its halo with `BLEND_ADDITIVE`. Canvas composites
each `shadowBlur` with the normal operator, so two overlapping glows give
`1-(1-a)(1-b)` and saturate slowly; additive gives `a+b`. Indistinguishable
for one isolated stroke on a dark ground — which is exactly the case the
five-pass profile had been tuned against — and catastrophic on the block's
cap, where cell edges, outline and top edge all overlap. Measured across the
cap's back edge the reference ramps +30 over 15px; additive ramped +100 and
clipped to white.

Now source-over, with the per-pass alpha inverted out of the accumulation
(`u = 1-(1-total)^(1/passes)`) so the five passes still sum to the intended
total and the isolated-stroke profile does not move.

### 4. Supersampling, which is worth less than it looks

Canvas antialiases strokes by pixel-area coverage; raylib's rasteriser is a
binary inside/outside test at the pixel centre, so 1px lines come out hard.
`c2d_set_supersample(n)` allocates every offscreen target n times larger and
scales every draw to match; drawing stays in design units and callers see
nothing.

It converges fast and then stops: 2.60 → 2.04 → 1.97 → 1.95 at 1/2/3/4×
against the pre-fix reference. Since 4× is already a 16-sample coverage
estimate, that plateau was the evidence that antialiasing was worth only
~0.65pp and something else dominated — which is what turned up findings 1-3.
**SS=2 is the sweet spot**; 3 and 4 buy 0.09pp and 0.02pp for 2.25× and 4×
the fill rate.

### What is left, and why

Residual at SS=2 is 29,658 px. The largest single group is the five callout
boxes at ~5,700 px combined. That is **not** a position error — a ±3px
cross-correlation search puts the best alignment at exactly (0,0) for all
five. It was ink: at matched size and matched typeface the port laid down about
20% less coverage per glyph (title row, ref 494 lit px vs port 391).

**That diagnosis was wrong, and the ToolRack port found out why.** It is not
a rasteriser difference at all: raylib's `fontSize` goes to
`stbtt_ScaleForPixelHeight`, which makes ASCENT-DESCENT that many pixels,
while Canvas's `24px` sets the EM SQUARE to 24. For JetBrains Mono the two
differ by 1320/1000, so **every string the port drew was 24% too small**.
`c2d.c` now reads `unitsPerEm` from `head` and the ascender/descender from
`hhea` and scales accordingly. A gamma correction on the atlas alpha was
tried first, on the old theory, and does nothing measurable once the size is
right. Holo3D's diff went 1.32% -> **1.15%** on that one fix.

Two earlier claims in this file were wrong and are withdrawn: the callouts
are not offset by a pixel of projection rounding, and the cap/wall error was
not mostly unantialiased hairlines.

## Amendment to spec 2.3, adopted here

The spec's glow prescribes a flat `0.32` total alpha. Measured against the
reference at the block's surface top edge (`blur = 16`, `w = 2.2`), that is
roughly 10x too strong: the reference adds ~6/255 above the background where
the port added 73/255. Canvas's `shadowBlur` is a Gaussian of sigma = blur/2
applied to the stroke's own alpha, so it **conserves ink** — peak halo is
about `0.8 * w / blur`, not a constant. The shim implements that, clamped at
0.5, split over five source-over passes (`C2D_GLOW_TOTAL` in
`src/ui/c2d.c`). It is an amendment to the spec, not an implementation
detail — flagged for acknowledgement.
