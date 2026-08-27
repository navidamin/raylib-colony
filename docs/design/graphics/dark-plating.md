# Dark Plating — The Coded-Art Style Guide

**Status: LIVING** — grows with every component. See the Rule in
[README.md](README.md): read before drawing, extend in the same commit.

Everything here was learned building the drill rig
(`../subsurface/prototypes/drill-rig.html` and `redline.html` — the reference
implementations; every helper named below exists in them verbatim). The guide
is layered so a new graphic knows exactly how much it inherits:

| Layer | Sections | A new graphic... |
|---|---|---|
| **The world** | §1 ground & palette, §2 the line, §3 tone | inherits all of it, always |
| **Materials** | §4 metal, §5 rock & ground | inherits the material it is made of; new materials get new sections |
| **Component families** | §6 machines that turn, §7 plated volumes | a new drill inherits §6 wholesale, a new building §7; a new *kind* of thing starts its own family section |
| **Stagecraft** | §8 camera & motion, §9 console chrome, §10 linked views, §11 capture | inherits whatever its context needs |

The name is the thesis: a **dark** world, and everything in it built from
**plates** — hard-edged bands of tone with a heavy line around them, like
enamel plating on machinery. Quantized reads as *drawn*; smooth reads as CG.

---

## 1. The World

### 1.1 The ground is near-black, and committed

The style is single-theme by choice. Every colour is painted explicitly —
nothing inherits from a host theme, the page/panel background is always set.
Light in this world comes from **tone structure**, not from simulated light
sources: nothing casts a computed shadow; things carry their shading in their
bands.

### 1.2 Palette tokens

The chrome palette (CSS custom properties in the prototypes; mirror these as
constants when porting to raylib):

```
--ground:#070b11  --panel:#0d151e  --panel2:#111c27  --rule:#1c2a39
--text:#c9d8e8    --dim:#61768a    --dimmer:#3d4e5e
--am:#d9962f  --am-lit:#f4c66a     amber   — machinery, attention, action
--cy:#50e1ff                       cyan    — instruments, information, idle
--hot:#ff5a28                      hot     — heat, damage, loss
--good:#5fd39a                     green   — health, success
--ice:#7fd8ee                      ice     — volatiles
```

Canvas-side fixed colours:

```
OUT  #0a0e14   the chunky outline (front work)
OUTB #101820   softer outline for back-facing work
sky  #0a1018   above the surface        borehole fill  #0e0b08
surface rule  #4a5560                   casing steel   #1c2530
spoil #463e31
```

Semantics are load-bearing: amber always means *machine/attention*, cyan
always means *instrument/info*, hot always means *damage/heat*. Do not reuse
a semantic colour decoratively — that is how the sweep-heat ramp once
collided with Measured-green and had to be rebuilt.

### 1.3 Type

Prototypes pair **Chakra Petch** (display/UI) with **JetBrains Mono**
(labels, numerals — always `tabular-nums`, uppercase labels always
letter-spaced `.14em`–`.2em`). The in-game extraction UI uses **Exo 2** via
`FS()` scaling (see CLAUDE.md); keep mono-style tabular numerals for gauges
either way.

---

## 2. The Line

The single strongest style marker: **every silhouette sits on a heavy
near-black line** (`OUT`, ~2–2.5 px at 2× scale; `OUTB` behind back-facing
work).

Rules learned the hard way:

- **One flood pass, then faces.** Outline a complex sweep (like a thread) by
  filling the *entire* silhouette in `OUT` first, slightly inflated, then
  painting the faces over it. Stroking each segment individually puts black
  ribbing *across* the surface. (`drawThread` pass 1 vs. its faces.)
- **Under-edge lines ground a part.** A blade or ledge gets a dark line under
  its bottom edge before its body is filled, so it sits *on* something.
- Outline widths are style constants, not per-shape choices. Front `OUT`,
  back `OUTB`, everything at the same weight — that is what makes separate
  parts read as one machine.

## 3. The Tone

### 3.1 Stepped, never smooth

All shading is **quantized**. Two mechanisms:

- **Hard-stop gradients** — `steelBands()` (§4.1) emits a `LinearGradient`
  whose stops come in *pairs*, so each band is flat: a plate of tone, not a
  ramp.
- **Tone quantization** — computed shades snap to a ladder before use:

  ```js
  const band = v => Math.round(clamp(v,0,1)*7)/7;   // surfaces: 7 steps
  // highlights/glints are even coarser: Math.round(v*3)/3
  ```

### 3.2 Back-facing work is remapped, not just darkened

```js
const dim = v => front ? v : 0.15 + v*0.44;
```

Compressing the *range* (not multiplying) keeps the far side legible but
unmistakably behind — and stops it glowing white-hot when a heat tint pushes
everything up.

### 3.3 State tints the material, it is never an overlay

Heat (and any future state: charge, corrosion, power) enters through the
material function itself — `steel(shade, heat)` — so one scalar re-colours
every band, glint and outline consistently. Painting a translucent state
layer *over* finished art is the CG look this style exists to avoid. The only
overlays allowed are atmospherics: the radial glow at a hotspot and the
sub-15%-alpha full-frame wash past a threshold.

---

## 4. Metal — the material

### 4.1 The two functions

Everything metallic is built from exactly two helpers:

```js
function steel(shade, heat){          // shade 0..1 dark->bright, heat 0..1
  const base=[lerp(96,238,shade),lerp(104,244,shade),lerp(118,252,shade)];
  const glow=[255,lerp(55,165,shade),25], t=clamp(heat*1.15,0,1);
  return `rgb(${...})`;               // lerp base->glow per channel by t
}
function steelBands(x0,x1,heat,tones){ // tones = [[span,shade],...] summing ~1
  // LinearGradient with PAIRED stops -> flat bands
}
```

`steel`'s base ramp is a **cool blue-biased grey** (b runs 118→252 while r
runs 96→238) — that bias is the plating's colour identity. The glow ramp runs
black-red→orange→near-white as `shade` rises, so hot *bright* metal whitens
while hot *dark* metal stays ember-red.

### 4.2 The cylinder recipe

A rod/cylinder is one `steelBands` fill across its width. The canonical
five-band profiles (span, shade):

```
rod   [[0.15,0.11],[0.17,0.98],[0.21,0.58],[0.27,0.30],[0.20,0.07]]
joint [[0.15,0.16],[0.18,0.94],[0.22,0.56],[0.26,0.28],[0.19,0.10]]
chuck [[0.17,0.06],[0.16,0.62],[0.22,0.34],[0.26,0.18],[0.19,0.04]]  (+heat*0.6)
```

Read the structure: dark edge → **bright hot-spot band off-centre left** →
mid → darker → dark edge. The off-centre specular is the implied
upper-left light. Draw the body in thin horizontal slices (~1.4–2 px) so the
profile can vary with y (taper, cone) while staying banded across x.

### 4.3 The joint grammar

Machines are **assemblies**, and the joints are what say so:

- **Joint/collar** (`drawJoint`): a band slightly wider than the rod, white
  glint on top (`rgba(255,255,255,.34)`, ~1.7 px), hard shadow underneath
  (`rgba(0,0,0,.45)`). A rod *steps thinner* across each joint going down —
  telescoping sections, not one pipe.
- **Chuck/clamp** (`drawChuck`): shorter, wider, *darker* (see profile —
  max shade 0.62, and heat reaches it at 0.6×), with vertical slot shadows
  and small bright bolts. Grip parts are always darker than the thing they
  grip.
- **Housing box** (`box(x,y,w,h,fill,bevel)` in `drawPowerhead`): `OUT`
  outline, flat fill, then a 3.5 px bevel — white top (0.30), white left
  (0.14), black bottom (0.30), black right (0.22). This is the recipe for
  *any* boxy machine body. The signature housing colour is amber `#d9962f`
  with `#f4c66a` bolts; vents are dark slots with a 1.6 px inner shadow line.

### 4.4 Varying the metal — tints, never structure

A new metallic thing keeps: the band *structure* (counts, spans, off-centre
specular), the outline weight, the glint/shadow grammar, the quantization.
It varies: the base ramp's colour bias, brightness ceiling, and heat
response. Precedents:

| Variant | How |
|---|---|
| Plated steel (default) | `steel()` as-is |
| Clamp/grip steel | same, capped shades (≤0.62) + reduced heat coupling |
| Amber housing | flat fill + bevel instead of bands; bands are for *turned* parts |
| Carbide (tips) | flat facets — 3 triangles at fixed shades (0.90/0.52/0.22), heat ×1.35 |

For a genuinely new alloy (brass, blued steel...), clone `steel()` with a new
`base`/`glow` pair, name it (`brass()`), and add it to this table.

### 4.5 Heat, damage, wear on metal

- **Heat field**: Gaussian in screen space around the hotspot —
  `heatAt(y) = heat * exp(-(d²)/(2·110²))` — fed to every `steel()` call, so
  the glow *spreads up the machine* from the working point.
- **Atmosphere**: radial gradient at the hotspot
  (`rgba(255,110,30, .45*heat)` → transparent over ~95 px), plus a full-frame
  `rgba(255,60,20, ≤.10)` wash only past the danger threshold.
- **Cracks** (`drawCracks`): three strokes over the same jagged polyline —
  dark under-stroke 3 px, heat-modulated orange 1.2 px
  (`rgba(255,130,35, .25+heat)`), then a white 1 px highlight offset +1.6 px x.
  Cracks belong to the *steel*, so they ride the part (offset from the bit),
  not the world.
- **Sparks**: only where the work is hard (`hard > 0.5`), 2.4 px squares in
  `rgba(255,170–240,60)`, scattered in a half-disc around the contact point.

---

### 4.6 Ladders — materials a linear ramp cannot express

`steel()` interpolates two RGB endpoints. That is right for a turned part,
where the quantization happens downstream in `steelBands`' paired stops. It
fails for a **saturated** material: lerping a dark amber `#241704` to a pale
highlight `#f8d78c` passes through a desaturated tan, so an amber housing
shaded that way goes muddy in exactly the midtones you use most.

The fix is to make quantization the data structure instead of a step applied to
it. A **ladder** is an ordered list of plate tones, dark to bright; a shade
picks a rung and there is deliberately no interpolation between rungs.

```js
register("plating", ["#1b2128","#28313b","#37424e","#495764","#5e6d7c",
                     "#778796","#93a1ae","#b3bfc9","#d9e0e7"]);   // 9 rungs
tone("plating", 0.74, heat)      // -> rung 6, then heat-mixed as §4.1
```

Use a ladder for a **flat volume**, `steel()` for a **turned** one. Both take
heat through the same glow ramp, so a rod and a housing warm together.

`plating`'s floor is darker than `steel()`'s `(96,104,118)` on purpose: a rod
never shows a face fully turned from the light, a box does, and without a real
shadow rung every volume reads flat. That is a range change, not a new alloy —
the blue bias (b ahead of r at every rung) is unchanged, because that bias is
the plating's colour identity.

Registered ladders: `plating`, `amber`, `dark` (plinths, caps, recessed
panels), `ember` (lit throats, worked metal), `ice`.


## 5. Rock & Ground — the material

- **Strata are flat slabs**: one flat colour per layer, a crisp 2.5 px
  darker `edge` rule at each boundary, mono uppercase label + depth figure at
  the boundary. No vertical gradients inside a layer.
- **Grain speckle**: sparse chunky rects (3–6 × 2.5 px) in a per-layer
  `grain` colour, density ~1 per 9 px of layer height.
- **Determinism**: all speckle uses the seeded LCG
  (`grainSeed = (grainSeed*16807) % 2147483647`) with a *fixed seed per
  drawing pass* (sky 3, strata 7, borehole 29). Ground must not shimmer
  between frames; `Math.random()` is only for genuinely transient particles
  (sparks).
- **Ice / volatiles**: bright `rgba(160,225,245)` flecks + thin dark fracture
  polylines wandering horizontally. Ice colour is `--ice`, never cyan (cyan
  is information).
- **The borehole**: fill `#0e0b08`, *ragged* walls (seeded black rects
  jittering the edge every ~7 px), then a horizontal darkening gradient
  (0.7 alpha at both walls → clear at centre) to make it a hole and not a
  stripe.
- **Surface furniture**: casing block with top glint; spoil piles as
  half-ellipses in `#463e31` with faint highlight ellipses offset up-wind.

---

## 6. Machines That Turn — the drill family

Everything a rotating, helical, boring machine needs. Another drill (hand
auger, wireline rig, excavator screw...) starts from this section and varies
proportions, tip, and head.

### 6.1 The helicoid — how a thread is actually drawn

The thread is a **real helicoid surface**, not a ribbon following the crest.
For each small step in angle θ, project the radial segment running from root
radius to crest radius:

```
x(θ) = CX + r·sin(θ)·HAND        y(θ) = threadTop + PITCH·θ/2π + TILT·cos(θ)·(r/R)
```

Fill the quad between consecutive θ (root/crest × θ/θ+dθ). Its projected
width is `(crest−root)·sin θ`, so the surface **pinches to nothing edge-on
(θ = 0, π) and is widest at the silhouettes** — the sawtooth teeth *emerge
from the projection*; they are never drawn as shapes. (`threadSegs` /
`drawThread`.)

The craft around it, each item bought with a failed pass:

- **Front/back split** by sign of cos θ; back drawn first, through `dim()`
  (§3.2), under the rod. Painter-sort segments by cos θ, farthest first.
- **V cross-section**: axial thickness tapers root→crest (`TH_T 5.6` →
  `TH_C 2.1`). Constant thickness reads as *stacked rings* — this taper is
  what makes teeth come to points.
- **Four faces per step**: body slab (edge-on), underside (in shadow), crest
  rim, ramp face — shades computed from cos θ + a left-bias term, each
  through `band()`; then a coarse-quantized glint along the crest, front only.
- **One flood outline pass** for the whole sweep before any face (§2).
- **Known limitation**: the `TILT` ellipse term makes dy/dθ unequal at the
  two silhouettes, so left teeth read slightly broader than right. Reducing
  TILT narrows the gap but flattens the from-above read. Accepted at
  `TILT 1.9`.

### 6.2 Proportions that read as a thread

```
crest R ≈ 1.8 × root RS          (17.0 / 9.6)
pitch  ≈ 1.0–1.3 × crest diameter (21.5 vs 34)
threaded stem = FIXED length      (THREAD_LEN = PITCH·6.2)
taper to tip over last 1.5 turns  (TAPER_PX), then a faceted carbide cone
```

The fixed stem matters: a stem defined as a *fraction* of the visible rod
grows as the hole deepens — no real tool does that. The **rod above**
lengthens instead (§4.3 sections), which is also true.

### 6.3 The shaft grammar, top to bottom

powerhead (amber housing + side pod with status lamp) → chuck → 1–3 plain rod
sections, each ending in a joint, each a step thinner → **transition collar**
(a `big` joint) → threaded stem → carbide facets. Count of sections scales
with depth. The lamp on the side pod reads machine state semantically
(cyan idle / amber driven / hot over-driven).

### 6.4 Rotation & particles

- Spin is one phase scalar: `phase -= rpm * 9 * dt`, consumed only inside the
  θ offset. Nothing else "rotates".
- **Chips** ride the *outer envelope* — position from `radAt(y)+2.5`, the
  function that returns crest radius on the stem and rod radius above it.
  Never place particles from a stale constant; when the geometry changed,
  chips clumping mid-rod was the tell.
- Front/back chip alpha 0.95/0.45 by cos of their own angle; they climb at
  a rate scaled by rpm (the flights carry them).
- **World consistency rule**: debris never below the bit — *there is no hole
  down there yet.* Annulus debris tumbles only in the cut section above it.

---

## 7. Plated Volumes — the iso-solid family

Everything a box-shaped machine seen from above needs: a factory building, a
tank, a cabinet, a crate. Reference implementation `gfx/engine/iso.js` and
`gfx/sprites/assembler.js` (stage: `gfx/assembler.html`, capture:
`gfx/tools/shoot.js`).

§4's grammar assumes screen-space rects and swept cylinders — there was no way
to say "a plated volume seen from above". This section is that way.

### 7.1 The projection is a preset, and it is not symmetric

Axis vectors are screen displacement per **one world unit**, so foreshortening
lives in the preset rather than being applied afterwards.

```
studio34  x:[ 1.000, 0.315]  y:[-0.556, 0.426]  z:[0,-1]   the default
iso21     x:[ 1.000, 0.500]  y:[-1.000, 0.500]  z:[0,-1]   corner-on 2:1
shallow   x:[ 1.000, 0.280]  y:[-1.000, 0.280]  z:[0,-1]   = ProsBlockGeom
```

`studio34` was measured off a reference sprite whose top face is a
parallelogram with edge vectors `(54,17)` and `(-30,23)` px. **The two ground
axes are deliberately unequal.** A corner-on isometric splits the front of a
box evenly between two faces of the same width; a rotated three-quarter gives
one wide face to put a door in and one narrow face beside it. Every readable
building icon in this genre does the second thing.

`+x` runs right-and-down, `+y` runs left-and-down, `+z` is up — so the visible
faces of a box are always **east (x1), south (y1), top (z1)**, and depth
increases with `x+y`. `fit(preset, bounds, rect, pad)` solves for the scale and
origin that centre a world box in a screen rectangle; sprite work is "put this
volume in that rectangle", not "pick a scale and hope".

### 7.2 The chamfered volume is the primitive

A raw box reads as a texture-mapped cube. The chamfer is what makes it cast
metal:

- the top face is **inset by `c`** and the four top edges become a ring of
  quads running out to the full footprint at `z1 - c`;
- each chamfer quad spans corner-to-corner (`[inset_i, inset_j, box_j, box_i]`),
  so adjacent quads share the box corner and the ring **tiles with no gaps** —
  no separate corner triangles;
- all four are visible: north and west form the bright back rim *above* the top
  face, south and east the lit band *between* the top face and the front faces.

Chamfer rims are always brighter than the face they crown
(`west .96 · north .92 · south .84 · east .44`), against face shades
`top .74 · south .60 · east .26`.

**Faces get sub-bands along their own axis.** A flat quad has no curvature to
band, but a big face painted in one tone reads as dead vinyl. Three bands at a
spread of ~0.03–0.06 is enough; the eye reads plate, not gradient. One
exception, learned by rendering it: a band boundary is a straight line *on
screen*, so on a **top** face it cuts across the parallelogram at an angle and
reads as a fold in the plate. Big top faces take one flat tone and let the
chamfer rim do the tonal work.

The §2 flood rule generalises without any boolean geometry: run the outline
pass over **every polygon of the assembly** first, then paint all the faces.
Interior seams get painted over by their own faces; only the outer boundary
survives. A solid stacked under another one must skip the top face in *both*
passes, or its line prints through the piece above it.

### 7.3 Face-local coordinates

The feature that makes the primitive usable. `V.on(face, box)` returns a mapper
whose `(u,v)` are world units measured from the face's top-left **as it appears
on screen**: `u` runs right along the face, `v` runs down it. Lay a shape out
like a UI rectangle and it lands the right way up, on the right face, in the
right place, with no per-face special-casing.

Without it every greeble is hand-projected, which is how iso art turns into a
pile of magic numbers that cannot be moved.

`m(u, v, lift)` offsets along the face's outward normal — positive stands a
plate proud, negative sinks it. `m.at(u, v, lift)` is the depth-compensated
twin: a point pushed into a face does not stay where you put it (on a south
face it slides right and up by the projection of the normal), so `m.at` solves
the 2×2 system that cancels that displacement and `(u,v)` means the same screen
place at any depth. `m.vShift(lift)` is the vertical half of it, for when you
want real parallax but need to know where the usable band of an opening starts.

The furniture built on this — `panel`, `studs`, `louvres`, `rail`, `lamp` — is
§4.3's joint grammar restated for faces. Same rule as ever: grip and recess
parts darker than what they grip, bright rule on a top edge, dark rule under a
bottom edge.

### 7.4 The recess — a hole with thickness, and a room behind it

A doorway drawn as a black shape on a face is a sticker. Three surfaces make it
a hole:

- **reveal** — the plate's own cut edge, banded bright across the top-left
  shoulder and falling to nothing on the right. Painting it evenly all the way
  round is the tell that it was drawn as an outline instead of a surface: the
  far side of a cut edge does not face the light. Generate the opening and its
  reveal from the *same* path generator at two insets rather than offsetting a
  polygon.
- **tube** — the camera looks down and from the left, so of the six inner faces
  exactly three face it: the **back**, the **floor**, and the inner side wall on
  the **left**. Take the sill from the path's first and last points and the left
  jamb from its first two, and the floor and wall fall out of the geometry
  instead of being drawn as art. (Both generators here start bottom-left and end
  bottom-right, which is what makes that work.)
- **back plane** — the path again at `-depth`. Because it lands up-and-right of
  the mouth on a south face, the sliver of tube left visible along the bottom
  and left *is* the floor and the near wall.

Bands that trail into the depth must be **relative** to the mouth shade. Hard
values there made a dark floor get *lighter* the deeper it went — a lit back
wall in a machine with no light in it, and the tell was a pale wedge in an idle
throat.

An interior light is the one overlay allowed (§3.3, atmospherics): a radial
gradient placed at the work, inside the clip.

### 7.5 Painter order for an overhanging base

Bottom-to-top is not the order. A plinth that overhangs the body has front
faces **nearer the camera than the body's**, and a doorway is a hole, so
anything painted before it shows through. Draw the deck first, then the body
and its cuts, then repaint the base as a **near lip**: its front faces and only
its front chamfers, plus the strip of deck outside the body's footprint.
Repainting the back chamfers too puts dark wedges across the machine's front —
they belong to a rim that is now occluded.

### 7.6 Scale is a material property

Outline weight, bevel rules and stud sizes are px constants tuned against a
hero-sized sprite. Drawn unchanged at 32 px they *are* the sprite. The view
carries the correction:

```js
V.lw(base)   // line weight for this scale, clamped to a hairline floor
V.px(len)    // screen px for a world length — ask before drawing a detail
V.lod        // 1 at hero scale (300 px/unit), falling with the sprite
```

A component consults `V.lod` and **drops greebles** rather than rendering them
into mud. Two thresholds are enough: one for fine work (studs, louvres,
interior mechanism), one for mid (rails, recessed panels). An icon is not a
small picture of a hero asset; it is a different drawing of the same object.

Render the size ladder — 128 / 96 / 64 / 48 / 32 — every time. It is the
cheapest test in this document and it caught both the line-weight problem and
the detail problem in one pass.

## 8. The Stage — camera & motion

- **The canvas is a window, not the world.** World coordinates stay fixed
  (640×880 here); the view is a crop (268×560) translated by a camera. Draw
  ground against the *view* rect, machines in world space.
- **Follow with ease**: target keeps the working point ~62% down the view;
  `cam += (want − cam) · min(1, dt·4.5)`, clamped to world bounds.
- **Anchor to the world, not the frame**: the powerhead sits relative to the
  surface collar, so it scrolls away as the string descends. Anything
  anchored to the canvas top is UI, not scene.
- **Shake is an impulse**: set `shake = 1` on a hit, decay `−dt·7`, apply as
  ±1.5 px random offset to the whole scene transform. Gate behind
  `prefers-reduced-motion` along with sparks and idle particle churn.
- **Clamp dt at both ends**: `clamp((now−last)/1000, 0, 0.05)`. The first
  rAF timestamp can *precede* the `performance.now()` captured before it —
  a negative dt once ran depth below zero and indexed an array at −1.

## 9. Console Chrome

The instrument-panel language around a stage (in-game panels: defer to
`docs/guides/ui-panels.md`; this is the prototype/artifact dialect):

- Cards on `--panel` with 1 px `--rule` borders, 6 px radius; mono uppercase
  card titles in `--dim`.
- **Segmented gauges**: a row of flat cells, lit count = value; semantic
  classes recolour lit cells (warm/crit/ok). A *range* shown on a gauge
  (like the pressure band) is a static `band`-class cell underlay — the
  target is visible before the needle reaches it.
- Big numerals in mono with `tabular-nums`; unit suffix small and dim.
- Overlays (report/trip) live *inside* the stage, `rgba(7,11,17,.94)` +
  blur, never a browser modal.
- One structural HTML lesson: **never nest a button in a button** — the
  parser hoists the inner one out and silently breaks the layout. A
  clickable stage is a `div role="button" tabindex="0"` with key handlers.

## 10. Linked Views — Instrument Pairs

When one subject appears in two projections at once — the borehole cross-section
beside the 4-layer block model — the pair must read as **one world, two
instruments**, never two apps sharing a screen. Reference:
`../prospecting/prototypes/drill-dock.html` (three placements, `?v=a/b/c`).

### 10.1 The ground is the join

Both panels are drawn on **one canvas**, and the *material* is what crosses the
seam — never the chrome. Three proven joins:

- **Wedge leaders** (`v=a`): a tapered quad of the stratum's own colour and
  grain runs from the depth band in one panel to its counterpart's vertex in
  the other, ~0.30 alpha fill with firmer edge lines. Reads as "this band IS
  that plate".
- **Continuous bands** (`v=b`): the strata bands span the full canvas — full
  strength inside the working panel, ~0.20 alpha as a backdrop under the
  instrument — and the boundary rules run unbroken across both, passing
  through the instrument's explosion gaps. Requires mapping the working
  panel's depth axis to the instrument's slots (a piecewise `yOf`), trading
  true thickness for perfect alignment.
- **The material slab** (`v=c`): the working panel is itself drawn as a body
  in the instrument's projection — an extruded column with a top cap and a
  darkened side face at the same iso slope (`dy/dx = 0.28`), so the panel and
  the plates visibly share a geometry. Thin leaders with diamond beads tie
  layer centres to plates.

Distinction, meanwhile, is cheap and must stay cheap: the working panel keeps
a full border **except on the facing edge, which is dashed** — a cut mark the
rock passes under — plus full-strength vs dimmed material. Never separate
palettes, never a second style.

### 10.2 One line, two projections

A path the player draws exists **once**, rendered into each view with the
same recipe, consumed by the same advance:

- **Planned** — marching cyan dashes (`[7,5]`, `lineDashOffset = -t·26`),
  drawn twice: wide `rgba(80,225,255,.22)` under narrow `#50e1ff`. Cyan
  because a plan is information.
- **Done** — solid: chunky `OUT` under-stroke, then `--text` steel. What is
  drilled is fact, and fact is drawn like metal.
- Progress markers accrue along the done part (assay ticks in Measured
  green), identical currency in both views.
- Through an exploded stack, build the path as per-layer segments whose gap
  connectors drop straight across the explosion — the line respects the
  instrument's own geometry rather than cutting through it.

### 10.3 Twin cursors

The moving point is the **same glyph in both views, on the same clock**: an
amber diamond (the instrument's cell shape — the motif carries the block
model's geometry into the mud view) pulsing at ~2 Hz off the shared `t`.
Synchronised pulse is what makes the eye accept the two dots as one object;
a second style or an unsynced phase breaks it instantly.

### 10.4 Sympathetic state

State changes land in both views in the same tick: the stratum being cut
rim-lights its plate (amber, pulsing alpha) while the bit is inside it; a
cell the trace passes flips class with a brief white overlay flash and keeps
a bore-ring; leaders to the active layer warm from dim steel to amber. Every
correlation cue is *event-driven and reversible* — nothing permanent joins
the panels except the ground itself.

## 11. Capture — making stills and GIFs of it

- **Deterministic stepping**: expose `__setState(o)`, `__manual(on)`,
  `__step(dt)`; capture harnesses drive `frame(dt)` at fixed dt so output is
  reproducible regardless of headless timing.
- **Zoom to judge**: at full frame a 40 px-wide machine is unjudgeable;
  screenshot crops at deviceScaleFactor 2–3 (`zoom.js`) and *look at them*
  before claiming anything.
- **Pixel-snap the camera for GIFs**: sub-pixel scroll redraws every
  background pixel per frame and defeats delta compression (~35% size win
  from snapping to 2 px).
- **One shared palette for GIFs**: quantize all frames against a mid-action
  frame's palette (MEDIANCUT, ≤96 colours, no dither) or the heat ramp
  shimmers between frames.
- Poses for stills: a geometry pose is rpm 0 with particles cleared — chips
  on top of the part being judged hide exactly what you are judging.

---

## 12. Extending This Document

- New **material** → new §4/§5-style section: its two functions, its recipe
  profiles, its variant table.
- New **component family** → new §6/§7-style section: the core geometric trick,
  the proportions that make it read, its grammar of parts, its particles.
- New **technique inside an existing family** → subsection there, with the
  failed pass that taught it if there was one. The failures are half the
  value of this document.
- Every entry names its reference implementation (file + function). If the
  reference moves, move the pointer in the same commit.
