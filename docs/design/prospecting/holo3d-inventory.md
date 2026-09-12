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

## Result: where the diff actually stands

The spec's gate is a visual diff under 2%. **It is at 2.60%** — 54,177 of
2,080,000 pixels on the default `home` state, measured by
`tools/visdiff/visdiff.sh` (Chromium renders `tools/visdiff/ref.html`,
`holo3d_visdiff` renders the port, `diff.py` compares them at a tolerance
of 12/255 per channel and writes a heatmap).

Not under the gate, so: not done. But every row above is ported — the
residual is not a skipped gap. Broken down by region:

| Region | Differing px | Cause |
|---|---:|---|
| block cap (top surface + mottle + cell edges) | 23,234 | 1px hairlines: Canvas antialiases them, `DrawLineEx` does not |
| walls (mesh + scatter + speckle) | 17,089 | same |
| callouts (leader, anchor, box, text) | 10,509 | the whole callout sits 1px off — projection rounding, not text |
| base ring | 2,720 | dash phase lands one step out at two of the clip boundaries |
| left margin | 490 | letterbox edge |

Two findings worth keeping, because both cost a cycle:

**The callouts are not a text bug.** Reference text occupies rows 380–425
and the port 381–425; the box's left edge is the same 380 vs 381. Text ink
width matched to within 1px (262 vs 263), so glyph advances and the exact
ascender are right. The entire element is translated by one pixel, which
is `(int)` rounding in the projection, not the shim.

**Faking antialiasing on hairlines makes it worse.** Drawing each thin
segment as a 0.34-alpha skirt at `w + 1.1` plus a 0.80-alpha core, with the
ink conserved, took the diff from 2.60% to **2.89%**. Canvas's coverage
antialiasing is per-pixel-area; a fixed two-pass approximation is wrong in
a different direction on most pixels and wrong in the same direction on
none. Reverted. Closing this properly means an MSAA target or a coverage
line shader, which is a change to the shim's rasterisation model and needs
the spec's author to weigh in first.

## Amendment to spec 2.3, adopted here

The spec's glow prescribes a flat `0.32` total alpha. Measured against the
reference at the block's surface top edge (`blur = 16`, `w = 2.2`), that
is roughly 10× too strong: the reference adds ~6/255 above the background
where the port added 73/255. Canvas's `shadowBlur` is a Gaussian of
σ = blur/2 applied to the stroke's own alpha, so it **conserves ink** —
peak halo is about `0.8 · w / blur`, not a constant. The shim implements
that, clamped at 0.5, split over five additive passes
(`C2D_GLOW_TOTAL` in `src/ui/c2d.c`). This took the diff from 4.80% to
2.62% and is the single largest correction in the port. It is an amendment
to the spec, not an implementation detail — flagged for acknowledgement.
