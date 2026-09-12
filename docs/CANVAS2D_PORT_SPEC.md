# Canvas 2D → raylib porting contract

Standing rules for porting Lunar Prospect UI modules from their JS
reference implementation into the raylib codebase.

This file is the source of truth. If a port looks flatter, dimmer or
more geometric than the JS reference, the cause is almost always a rule
below that was skipped.

---

## 0. The core problem

The JS modules (`Holo3D`, `ToolRack`, `HoloBlock`, `Dash`) are **entirely
procedural Canvas 2D**. There are no images, no CSS, no DOM. Every pixel
comes from a draw call.

That means the port is a **1:1 API translation**, not a redesign. Nothing
should be "reinterpreted for raylib." If the JS draws a 4-stop gradient on
a concave polygon, the raylib version draws a 4-stop gradient on a concave
polygon.

Canvas 2D has twelve features raylib lacks. Each has exactly one approved
implementation, listed in §2. **Do not substitute a flat approximation for
any of them.** A flat fill where the reference has a gradient is a port
failure, not a simplification.

### Canonical source

`dashboard.html` is the superset and the source of truth. It contains
four modules:

| Lines | Module | Role |
|---|---|---|
| 25–716 | `ToolRack` | survey tool rack chassis, plates, slot lights |
| 717–964 | `HoloBlock` | static drawn layer block |
| 965–1290 | `Holo3D` | interactive rotatable / explodable block |
| 1291–1698 | `Dashboard` | panels, connectors, seg bars, composition |

**`holo3d.js` is byte-identical to lines 965–1290 of `dashboard.html`.**
It is the same module extracted for standalone work, not a second
implementation. Port it once. Any inventory that counts both files has
double-counted every Holo3D feature.

`HoloBlock` and `Holo3D` are two different block renderers — one static,
one interactive. Confirm you need both before porting both.

---

## 1. Fixed design space — do not re-layout

The JS renders into a fixed-size canvas and lets CSS scale it:

| Module | Design canvas |
|---|---|
| `dashboard.html` | 1536 × 1024 |
| `layers-block.html` | 1600 × 1300 |

**The raylib port must do the same.** Render the whole module into a
`RenderTexture2D` at the exact design size, then blit that texture scaled
into the window.

```c
// once
RenderTexture2D surface = LoadRenderTexture(1536, 1024);

// per frame
BeginTextureMode(surface);
    ClearBackground((Color){0x02, 0x0b, 0x13, 0xff});
    Dash_Draw(&cfg, &state);
EndTextureMode();

BeginDrawing();
    // NOTE: RenderTexture2D is Y-flipped. Source height MUST be negative.
    Rectangle src = { 0, 0, 1536, -1024 };
    Rectangle dst = FitContain(1536, 1024, GetScreenWidth(), GetScreenHeight());
    DrawTexturePro(surface.texture, src, dst, (Vector2){0,0}, 0.0f, WHITE);
EndDrawing();
```

Consequences, all of them deliberate:

- **Every coordinate in the JS ports over verbatim.** `232,100`,
  `bx + bw - 12 * s`, `c[0] + 70 * s` — all of it. No re-derivation, no
  "responsive" rewrite, no percentage math.
- Input must be transformed **screen → design space** before hit testing.
  Invert the `FitContain` rect. Never hit-test in screen coordinates.
- Set `SetTextureFilter(surface.texture, TEXTURE_FILTER_BILINEAR)` so the
  downscale on tablet doesn't alias the 1px hairlines.

If a port re-computes layout from window size, it is wrong. Revert it.

---

## 2. The twelve gaps and their approved implementations

All of these live in the shim layer (`c2d.h` / `c2d.c`), never at call
sites. Call sites should read like the JS.

### 2.1 Concave polygon fill — `path(poly); ctx.fill()`

**Where:** `Holo3D.paintLayer` wall polygons, top-surface cells, callout
boxes.

**Why it breaks:** `DrawTriangleFan` only handles convex polygons. The
layer walls are `[...topEdge, ...bottomEdge.reverse()]` over a height
field — reliably concave. A fan produces visible wedge artifacts across
the bed.

**Approved:** ear-clipping triangulation, then `rlBegin(RL_TRIANGLES)`
with per-vertex colour. Vendor a single-header earcut (e.g. `earcut.h`)
or write ~80 lines of ear clipping. Do not use a fan. Do not use
`DrawPoly`.

### 2.2 Multi-stop linear gradient on that polygon

**Where:** wall fills use 4 stops (`neon → mid → deep → deep` at
0 / 0.2 / 0.75 / 1) mapped over the polygon's screen-space y extent.

**Why it breaks:** `DrawRectangleGradientV` is rectangles only, two
colours only.

**Approved:** build a gradient LUT, then assign each triangulated vertex
a colour sampled at its own `y`, and let `rlColor4ub` interpolate across
the triangle.

```c
Color c2d_gradient_at(const C2DGradient *g, float y);  // y0..y1 → colour
```

This reproduces the Canvas behaviour exactly, because Canvas gradients are
also linear in screen y. Per-triangle flat colour is **not** acceptable —
it bands.

### 2.3 `shadowBlur` — the neon glow (27 call sites)

**Where:** every cyan edge, bracket, reticle, callout leader, drill-site
lock line, and every lit slot on the tool rack.

> **Grepping `shadowBlur` finds less than half of these.** `ToolRack`
> wraps glow in a helper pair:
>
> ```js
> function glowOn(ctx, color, blur) { ctx.shadowColor = color; ctx.shadowBlur = blur; }
> function glowOff(ctx) { ctx.shadowColor = 'transparent'; ctx.shadowBlur = 0; }
> ```
>
> Real distribution: **16 `glowOn(ctx, …)` call sites** plus **11 raw
> `shadowBlur =` assignments** (excluding the 16 `= 0` resets and the one
> inside the helper definition). Every inventory must grep for `glowOn`
> as well as `shadowBlur`, or it will miss the entire rack — which is
> the part of the UI that currently looks least like the mockup.

**Why it matters most:** this single feature is most of the visual
difference between the mockup and the current build. Without it the UI
reads as flat line art.

**Approved:** additive multi-pass stroke.

```c
void c2d_glow_stroke(Vector2 *pts, int n, Color c, float w, float blur);
// blur <= 0  → single DrawLineEx pass
// blur  > 0  → BeginBlendMode(BLEND_ADDITIVE)
//              pass 1: width w + blur*1.6, alpha 0.10
//              pass 2: width w + blur*0.8, alpha 0.22
//              EndBlendMode()
//              pass 3 (core): width w, full alpha, normal blend
```

Pass the JS `ctx.shadowBlur` value straight through as `blur`. `shadowBlur
= 16` on the top edge means `blur = 16.0f`, not "some glow."

Honour `state.fast` exactly as the JS does: when `fast` is true, blur is
skipped (`ctx.shadowBlur = fast ? 0 : blur`). This is the drag-rotate
performance path — keep it.

### 2.4 `ctx.clip()` to an arbitrary polygon (19 call sites)

**Where:** mesh lines clipped to the wall polygon; base ring clipped
even-odd against the block's convex hull.

**Why it breaks:** `BeginScissorMode` is axis-aligned rectangles only.

**Approved, in order of preference:**

1. **Clamp analytically.** The wall mesh lines are generated by lerping
   between `top[i]` and `bot[i]` — they are already inside the polygon by
   construction. Just don't draw outside the parameter range. This
   removes most clip sites at zero cost. Prefer this.
2. **CPU polygon clip.** For the base ring (`clip('evenodd')` against
   `model.hull`), clip the ring's line segments against the hull on the
   CPU and draw only the outside portions.
3. **Stencil via rlgl.** Only if 1 and 2 genuinely don't apply. Enable
   the stencil test, draw the mask polygon, draw content masked, clear.

Never approximate a polygon clip with a bounding-box scissor. It cuts
strokes that should be visible.

### 2.5 `globalAlpha` (17 call sites)

**Where:** `alphaOf(k)` ghosting for unselected beds, callout dimming.

**Why it breaks:** raylib has no global alpha state.

**Approved:** the shim carries an alpha stack and multiplies it into
every colour it emits.

```c
void  c2d_push_alpha(float a);   // multiplies onto current
void  c2d_pop_alpha(void);
Color c2d_tint(Color c);         // every shim draw call runs colours through this
```

No shim draw call may use a raw `Color` without passing it through
`c2d_tint`.

### 2.6 Offscreen buffers — the ghost compositing trick

**Where:** `Holo3D.render` when a bed is selected. Ghost groups are drawn
**opaque** into an offscreen canvas and composited **once** at
`GHOST = 0.3`, so stacked ghosts never accumulate alpha.

**This is load-bearing.** Per-bed alpha is the JS fallback path and looks
visibly wrong — overlapping beds darken. Reproduce the buffer path.

**Approved:** two `RenderTexture2D` at design size, matching `_buf[0]`
and `_buf[1]`.

```c
BeginTextureMode(ghostBelow);
    ClearBackground(BLANK);
    for (int k = ...) PaintLayer(k);       // full alpha
EndTextureMode();
// composite once, remembering the Y flip
DrawTexturePro(ghostBelow.texture, (Rectangle){0,0,W,-H}, full,
               (Vector2){0,0}, 0, Fade(WHITE, 0.30f));
```

Draw order stays: ghost-below → selected bed → ghost-above.

### 2.7 `setLineDash` (20 call sites)

**Approved:** a shim polyline walker that emits on/off runs.

```c
void c2d_dashed_polyline(Vector2 *pts, int n, float on, float off,
                         Color c, float w);
```

Dash lengths in the JS are scale-multiplied (`[8 * s, 6 * s]`). Keep the
`s` factor — it's `min(1.4, zoom / 0.94)`.

### 2.8 Line joins and caps

The JS sets `lineJoin = 'round'; lineCap = 'round'` globally on the block
renderer. `DrawLineEx` has neither — a 2.2px polyline drawn segment by
segment shows notches at every joint on the layer edges.

**Approved:** `c2d_polyline()` draws each segment with `DrawLineEx`, then
`DrawCircleV(joint, w * 0.5f)` at every interior vertex and at both ends.

### 2.9 Text — the highest-frequency silent bug

The JS uses JetBrains Mono at weights **500, 600, 700**, sizes derived
from `fs = max(9, 15 * s)`.

Rules:

- Load **three separate TTFs**, one per weight. Do not fake weight by
  drawing text twice offset — it smears at small sizes.
- Load with `LoadFontEx(path, 64, NULL, 0)` and enable
  `TEXTURE_FILTER_BILINEAR`, or use SDF. Sizes are continuous
  (`15 * s`), so a single small bitmap size will alias badly.
- **Baseline**: Canvas `textBaseline = 'alphabetic'` means `y` is the
  baseline. raylib `DrawTextEx` treats `y` as the **top** of the line.
  Every ported call must subtract the ascender:
  `DrawTextEx(f, s, (Vector2){x, y - c2d_ascender(f, size)}, ...)`.
  Where the JS sets `textBaseline = 'middle'`, subtract half the
  cap height instead. Getting this wrong shifts all text by ~4-11px and
  is the most common cause of "close but subtly off."
- **Alignment**: `textAlign = 'right'` → subtract
  `MeasureTextEx(...).x`. `'center'` → subtract half. There are 17
  alignment sites; each needs the offset applied.
- `ctx.measureText` → `MeasureTextEx`. Six sites depend on measured
  width for layout; don't hardcode the measured result.

### 2.10 Radial gradients (6 sites)

**Where:** `ToolRack` slot lights (`createRadialGradient(cx, cy, 6, cx,
cy, 50)` and the pill bloom at `lx + lw`), `HoloBlock`'s backglow behind
the block face (`radius 10 → depth * 0.9`), and the `Dashboard`
full-screen vignette (`W/2, H/2, 200 → 1000`).

**Why it matters:** these are the soft blooms. Every one of them is a
"this looks lit" cue. Dropped, the rack slots become flat rectangles.

**Why it breaks:** raylib has no radial gradient of any kind.

**Approved:** a triangle fan emitted through `rlgl` — centre vertex at
the inner colour, rim vertices at the outer colour, so the GPU
interpolates the falloff.

```c
void c2d_fill_radial(Vector2 c, float r_in, float r_out,
                     Color inner, Color outer, int segments);
```

Use ≥48 segments; below ~32 the rim polygonises visibly on the large
vignette. Where the JS gradient has an inner radius > 0 (all six do),
emit a ring of inner-colour vertices at `r_in` rather than a single
centre point, or the core reads too hot.

For the full-screen vignette specifically, a single large fan is fine —
do not reach for a shader.

### 2.11 Ellipses and elliptical arcs (5 sites)

**Where:** rack base plates and the pill highlight
(`ctx.ellipse(cx, cy + 23, 36, 14, 0, 0, TAU)`), plus one **partial**
elliptical arc (`ctx.ellipse(cx, cy, rx, ry, 0, a0, a1)`) and one
half-ellipse (`0, Math.PI`).

**Why it breaks:** `DrawEllipse` is axis-aligned, full-sweep only, has
no arc form, and is not antialiased.

**Approved:** sample the ellipse into a polyline and route it through the
existing fill/stroke functions.

```c
void c2d_ellipse_pts(Vector2 c, float rx, float ry, float rot,
                     float a0, float a1, Vector2 *out, int n);
```

Then `c2d_fill_poly` or `c2d_polyline`. This also gives arcs and
rotation for free, and antialiases consistently with everything else.

### 2.12 Grain / noise pattern

`makeGrain` builds a small noise tile and uses `createPattern`.

**Approved:** generate the same noise into an `Image` once at startup,
`LoadTextureFromImage`, set `TEXTURE_WRAP_REPEAT`, and draw with a source
rect larger than the texture to tile. Same seed, same value distribution
(`100 + (rand + rand) * 40`) so the metal reads identically.

---

## 3. Also worth knowing

- **Projected circles are not circles.** `Holo3D` already samples rings as
  polylines through `cam.project` (see `ring()` and `onSurf()`). Port
  those as polylines. Only use `DrawCircleV` where the JS uses
  `ctx.arc` in *screen* space — e.g. the pulsing centre dot.
- **`DrawCircleV` is not antialiased.** For HUD dots ≤4px, use
  `DrawCircleSector` with 24+ segments, or a small pre-rendered disc
  texture.
- **`hash(a,b,c)` must match bit for bit.** It seeds the wall speckle and
  top-surface mottling. In C, use `uint32_t` throughout and mirror the
  `>>> 16` as an unsigned shift. If the noise differs, the beds look
  like different rock.
- **Per-frame allocation.** The JS reallocates point arrays freely. In C,
  preallocate scratch buffers on the model struct — `NX=16, NZ=13`, six
  boundaries, so the ceiling is known at build time.

---

## 3.5 Verified gap inventory

Counted from `dashboard.html` (the canonical superset), with reset calls
excluded. Use this to sanity-check any inventory Claude Code produces —
if a module's row comes back much shorter, it skimmed instead of grepping.

| Feature | ToolRack | HoloBlock | Holo3D | Dashboard | Total |
|---|---:|---:|---:|---:|---:|
| `createLinearGradient` | 5 | 3 | 1 | 2 | 11 |
| `createRadialGradient` | 4 | 1 | 0 | 1 | 6 |
| glow (`glowOn` + raw `shadowBlur`) | 17 | 7 | 4 | 2 | 27 |
| `ctx.clip()` | 8 | 4 | 2 | 3 | 17 |
| `setLineDash` (non-reset) | 3 | 2 | 2 | 1 | 8 |
| `globalAlpha` (non-reset) | 3 | 0 | 4 | 0 | 7 |
| `ellipse` | 4 | 1 | 0 | 0 | 5 |
| `measureText` | 1 | 0 | 0 | 5 | 6 |
| `drawImage` | 0 | 0 | 1 | 1 | 2 |
| `createPattern` | 1 | 0 | 0 | 0 | 1 |

Two `drawImage` sites, and they are different things:

- **line 1129** (`Holo3D`) — the ghost-group composite. Covered by spec
  2.6 / `c2d_group_composite`.
- **line 1666** (`Dashboard`) — a full-frame cache blit wrapped in
  `setTransform(1,0,0,1,0,0)` … `setTransform(dpr,0,0,dpr,0,0)`. This is
  a DPR-aware repaint cache: static chrome is drawn once and blitted,
  and only animated elements redraw per frame. **Keep this optimisation.**
  In raylib it becomes a third `RenderTexture2D` holding static chrome,
  redrawn only when `cfg` changes. Dropping it will cost you frame rate
  on tablet, where it matters most.

## 4. Definition of done for any module port

A port is complete when all of the following hold:

1. Renders into the design-size `RenderTexture2D`; no coordinate was
   re-derived from window size.
2. All 11 linear and all 6 radial gradients have a corresponding
   gradient in the port — same stops, same offsets, same inner/outer
   radii.
3. All 27 glow sites (`glowOn` **and** raw `shadowBlur`) have a
   corresponding `c2d_glow_stroke` / `c2d_fill_radial` with the same blur
   value, and respect `fast`.
3b. All 5 ellipses are polyline-sampled, including the two partial arcs.
4. Every `globalAlpha` site maps to a push/pop pair.
5. Text baselines and alignment verified against the reference
   screenshot — no vertical drift.
6. Side-by-side screenshot diff against the JS at identical design size
   shows no structural difference (see §5).
7. Input hit-tests in design space and selects the same bed the JS does
   for the same pointer position.

---

## 5. Visual regression harness

Do not eyeball this. Set it up once:

- **Reference:** headless Chrome/Playwright loads the JS module at design
  size with a fixed `state` (fixed `time`, `yaw`, `pitch`, `selected`,
  `explode`) and screenshots the canvas.
- **Port:** a `--screenshot` flag runs one frame with the same fixed state
  and calls `TakeScreenshot`.
- **Diff:** compare the PNGs; report percent differing pixels and write a
  heatmap.

Fixed `time` matters — several elements pulse on `sin(t * 3)`.

Target under 2% differing pixels. Above that, the diff heatmap tells you
which of the twelve gaps was skipped: large soft regions = missing glow,
hard banding = missing gradient, uniform vertical offset = text baseline.
