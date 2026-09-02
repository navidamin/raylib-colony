# Dark Plating — The Coded-Art Style Guide

**Status: LIVING** — grows with every component. See the Rule in
[README.md](README.md): read before drawing, extend in the same commit.

Everything here was learned building the drill rigs
(`../subsurface/prototypes/drill-rig.html` and `redline.html` for the auger,
`../excavation/prototypes/diamond-drill.html` for the coring crown — the
reference implementations; every helper named below exists in them verbatim).
The guide is layered so a new graphic knows exactly how much it inherits:

| Layer | Sections | A new graphic... |
|---|---|---|
| **The world** | §1 ground & palette, §2 the line, §3 tone | inherits all of it, always |
| **Materials** | §4 metal, §5 rock & ground | inherits the material it is made of; new materials get new sections |
| **Component families** | §6 machines that turn | a new drill inherits §6 wholesale; a new *kind* of thing starts its own family section |
| **Stagecraft** | §7 camera & motion, §8 console chrome, §9 linked views, §10 capture | inherits whatever its context needs |

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
| Diamond grit | bright specks at `band(0.88+)` on a 2 px dark seat, seeded per part and placed in **part-local** coordinates (§6.5) |

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
- **The wear ladder** (game dock): cracks appear past **0.55 wear**, one
  more per ~0.09 after, capped at 6 — each with a FIXED seed so it is a
  stable feature of the part, not per-frame noise. At wear 1.0 the bit
  fractures: a shake jolt, then the **trip** — the string runs out of the
  hole and back on a half-sine of the trip clock
  (`shown = depth · (1 − sin(π·f))`), spin in slow REVERSE (backing rods
  off), particles stopped, while the borehole itself stays cut to the
  deepest point reached. The hole is a fact; only the tool leaves.

---

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
- **The block model's plates are the same ground** (`ProsDrawBlockLayer`).
  Each plate wears the moon tile the colony and sect views tile the world
  with, stretched once over the plate so a crater spans a few cells, and
  modulated by the cell's fill so class colour and stratum tone survive
  underneath. The tile is a mid-grey with ±9 levels of crater in it — fine
  tiled at full size under the sect view, invisible once stretched over a
  dark plate — so the plates get their own contrast-pushed copy
  (`plateTiles`: `ImageColorContrast +55`, bilinear) and a gain that undoes
  the tile's mean (1.85), so a textured plate averages the tone its flat
  fill would have had and no palette was re-tuned. One quad per cell on the
  one texture: a plate is a single batch. `--bench` at 32x32 reads
  17 ms/frame, the software-raster floor.
- **Relief is read by slope, not height.** A flat-lit iso plate does not
  show its shape at any relief (three rounds of "the curvature is not
  visible enough" were spent raising it: 0.30 → 0.45 → 0.60 of the plate's
  diamond height). What made it legible was hill-shading: light implied from
  the upper-left of the screen,
  `shade = 1 + a · tanh(0.45·toward + 0.25·left)`, with the slope measured in
  *relief per plate width* (per-cell rise × N / relief) so a mound keeps its
  light whether the lattice is 8 or 32 across — a denser lattice halves the
  per-cell rise, and must not halve the light. `a` is 0.70 on the lit side
  and 0.50 in shadow: a shadowed face still has to show its craters and its
  class. tanh, not a clamp, so steep flanks grade off instead of going
  two-tone. The plate gap is derived from the relief, so plates never
  overlap whatever it is set to.

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

### 6.5 The crown — a rotary bit as an elliptical annulus

A diamond core bit is not a helix. It is a **ring of segmented pads split by
waterways**, and the trick that draws it is the annulus counterpart of §6.1:
the camera sits a little above, so the ring projects to an ellipse and every
pad owns an arc of it.

```
x(θ) = CX + r·sin θ        y(θ) = crownTop − SQ·r·cos θ        SQ = 0.30
front = cos θ < 0          (nearer the eye, so lower on screen)
```

Each pad is the quad swept between root radius `crownRi` and crest radius
`crownR` across its arc, subdivided ~4 ways so the ring does not read as a
polygon. **Only front pads show their outer wall** — the vertical extrusion by
`crownH` — so the segmentation *emerges from the projection* exactly as the
auger's teeth do; pads are never drawn as shapes. Painter-sort by `−cos θ`,
back first, back faces through `dim()` (§3.2).

The waterway slots are **not drawn**. One flood pass in `OUT` under the whole
crown (§2), then pad faces painted over it; the gaps are that flood showing
through. Each front pad's kerf lip sits ~1 px proud of the flood, which
scallops the crown's foot instead of ending it on one flat line.

**Grit is the crown's identity, and it lives in pad-local coordinates.** Specks
are placed as *(u along the arc, v down the wall)* from a per-pad seeded LCG and
projected every frame, so grit turns *with* its own pad. Placed in screen space
it swims across the bit — the same class of error as chips positioned from a
stale constant (§6.4).

```
pads or teeth per ring: 8 (compact) – 10 (heavy)
waterway / gap ≈ 0.30 of each segment's arc
```

**The toothed cone is the same projection.** The concept sheet's shipped bit
is not a flat crown but a **cone of conical cutters** — and nothing changes
underneath. Each ring of teeth is an elliptical annulus at its own height and
radius; every tooth owns an angle on it; back teeth ride the far side of the
ellipse smaller and remapped bright enough to survive against the dark cone
body (a plain `dim()` sinks them into it — that was the failed pass). The cone
is a *consequence*: the outer ring's tips pull toward the axis only ~0.20 of
the ring radius (they splay), the inner ring ~0.36, the centre point sits on
the axis and is painted last. Gather every ring's teeth into ONE painter sort
— sorting per ring lets a front tooth of the outer ring vanish under a back
tooth of the inner. Rotation is still one phase scalar marching the teeth
around their rings.

**The lower works vary §6.3 at the bottom end only** — and they are data, not
drawing code: each variant declares a `stack` of segments (step collars, a
two-tone barrel at `P_BARREL`, a vented box, the amber **stabiliser** with its
blade slots, the heavy's bundled-column section) rendered bottom-up from the
bit by one segment painter, so a variant is a list, never a branch. The top
works are the concept sheet's: bevelled-lid motor box, amber collar (one dark
mouth slot on the compact, three lit vents on the heavy), slotted neck. Rod
joints ride at fixed offsets above the lower works — rods are added at the
top, so what is bolted together stays bolted together as the hole deepens.

**Two failed passes worth keeping.** The bore, and the core standing inside it,
were drawn *under* the barrel — which is wider than the bore, so none of it ever
appeared. An outside view of a coring tool cannot show its core: the recovered
column has to be a second instrument (§9), never a detail on the machine. And
the heat Gaussian was inherited at σ = 110 from a *scrolling* stage; on a
full-column view that tinted the entire string mauve. At σ = 34, with the
cutting heat target dropped so only *failure* runs hot, the read becomes a
glowing crown on a cold string — which is the thing worth showing.

**What the crown does NOT invent.** Its wear ladder is §4.5's verbatim (cracks
past 0.55 consumed, one more per ~0.09, capped at 6, fixed seeds), and the core
strip beside its stratigraphy column is §9.45's lane palette verbatim. A second
language for a variable that already has one is the failure this guide exists
to prevent — the crown is new geometry, not a new dialect.

Reference: `../excavation/prototypes/diamond-drill.html` — `drawBit`,
`toothPath`, `drawStackSeg`, `drawPowerhead`, `crackList`. (The flat pad-crown
described above was the first pass; it lives on in this section because the
projection, the flood pass and the grit rules are identical — only the
segment shape changed when the concept sheet's toothed cone replaced it.)

---

## 7. The Stage — camera & motion

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
- **Shake, console form** (game dock): the whole stage translates as ONE
  rigid body (`rlPushMatrix` inside the scissor) while the scissor and the
  panel frame stay fixed — the console rattles *in its mount*. The rumble
  scales with the GROUND as well as the drive:
  `rpmN² · (0.25 + 1.2·hardness)` px — regolith hums, basalt rattles, an
  idle crawl is near still (no base term; drive earns all of it) — plus a
  5 px jolt decaying over 0.6 s when the bit fractures. Never shake the
  frame or the text panel; a readout that rattles is a bug, a stage that
  rattles is work.
- **Clamp dt at both ends**: `clamp((now−last)/1000, 0, 0.05)`. The first
  rAF timestamp can *precede* the `performance.now()` captured before it —
  a negative dt once ran depth below zero and indexed an array at −1.
- **A comparison sheet takes no camera at all.** When two machines have to be
  judged against each other, the whole world goes on screen at one shared scale
  and nothing scrolls — a shorter reach becomes something you *see* rather than
  a figure you read. Everything else in this section still applies to the
  machine; only the follow is dropped. Note the knock-on: distances that a
  cropped view kept off-screen are now all visible at once, so any
  screen-space falloff (§4.5 heat) has to be retuned, not inherited.

## 8. Console Chrome

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

## 9. Linked Views — Instrument Pairs

When one subject appears in two projections at once — the borehole cross-section
beside the 4-layer block model — the pair must read as **one world, two
instruments**, never two apps sharing a screen. References:
`../prospecting/prototypes/drill-dock.html` (three placements, `?v=a/b/c`) and
the raylib port in `src/Engine/rendermanager.cpp` (`ProsDrawBoreholeDock`,
`ProsDrawTraceBlock`, `ProsDockGeom` — the in-game prospecting panel).

### 9.1 The ground is the join

Both panels are drawn on **one canvas**, and the *material* is what crosses the
seam — never the chrome. Three proven joins:

- **Wedge leaders** (`v=a`): a tapered quad of the stratum's own colour and
  grain runs from the depth band in one panel to its counterpart's vertex in
  the other, ~0.30 alpha fill with firmer edge lines. Reads as "this band IS
  that plate".
- **Continuous bands** (`v=b`) — **CHOSEN for prospecting's panel**: the
  strata bands span the full canvas — full strength inside the working
  panel, ~0.20 alpha as a backdrop under the instrument — and the boundary
  rules run unbroken across both, passing through the instrument's explosion
  gaps, with **no leader lines at all**: alignment is the correlation.
  Requires mapping the working panel's depth axis to the instrument's slots
  (a piecewise `yOf`), trading true thickness for perfect alignment. Place
  the plate slots at the band centres and the two panels share **one depth
  mapping** — which is what makes §9.2's oblique line and level twin
  cursors possible. The wedge and slab joins below remain available
  patterns for pairs that cannot share an axis.
- **The material slab** (`v=c`): the working panel is itself drawn as a body
  in the instrument's projection — an extruded column with a top cap and a
  darkened side face at the same iso slope (`dy/dx = 0.28`), so the panel and
  the plates visibly share a geometry. Thin leaders with diamond beads tie
  layer centres to plates.

Distinction, meanwhile, is cheap and must stay cheap: the working panel keeps
a full border **except on the facing edge, which is dashed** — a cut mark the
rock passes under — plus full-strength vs dimmed material. Never separate
palettes, never a second style.

### 9.2 One line, two projections

A path the player draws exists **once**, rendered into each view with the
same recipe, consumed by the same advance:

The prescribed line is **dead straight** — one segment from collar point to
target point, exactly as the player drew it. Never re-project it into flat
per-plate pieces joined by drops across the explosion gaps (plumbing, not a
hole), and do not bend it to the exploded geometry either: instead let depth
travel the straight line on the shared axis — `u(m) = yShared(m)`
normalized — so every point of the line is a depth, and the cursor stays
level with its twin across the seam. The cells it classifies are wherever
the line **actually crosses** each plate, snapped from the crossing point,
so the bore-rings sit on the line by construction.

Its dress is three layers on one path:

- **The shadow** — the full prescribed path, always visible, quiet: a wide
  `rgba(80,225,255,.10)` halo under a 1 px `rgba(160,190,215,.30)`
  hairline. The plan's ghost; everything else moves on it.
- **The string** — the drilled portion is the drill string, not a line:
  `OUT` 5 px, mid-steel `#77879a` 3 px, then bright `#e8f0f8` dashes
  (`[6,10]`) whose `lineDashOffset = phase·6` — the **drill's own spin
  phase**, so the barber-pole banding turns with the machine and stops when
  it stops. Rotation mimicked axially, exactly as a turning rod reads
  side-on.
- **The advance** — the remaining path carries a fine cyan dash
  (`[4,8]`, 1.3 px, `.55` alpha, offset `-t·26`) progressing along the
  shadow toward the target.

Progress markers accrue along the done part (assay ticks in Measured
green), identical currency in both views.

### 9.3 Twin cursors

The moving point is the **same glyph in both views, on the same clock**: an
amber diamond (the instrument's cell shape — the motif carries the block
model's geometry into the mud view) pulsing at ~2 Hz off the shared `t`.
Synchronised pulse is what makes the eye accept the two dots as one object;
a second style or an unsynced phase breaks it instantly.

### 9.4 Sympathetic state

State changes land in both views in the same tick: the stratum being cut
rim-lights its plate (amber, pulsing alpha) — and that rim must be drawn
**where the per-corner lifts are known**, tracing the plate's real
silhouette through the same `cornerLift` the surface quads use. A rim
computed at `lift = 0` sits on the plate's BASE plane while the surface
floats up to `relief` above it, so it drifts off the shape it is meant to
mark, by more the higher the relief runs (a x1.5 relief pass made it
plain). Tracing the boundary cannot drift by construction while the bit is inside it; a
cell the trace passes flips class with a brief white overlay flash and keeps
a bore-ring; leaders to the active layer warm from dim steel to amber. Every
correlation cue is *event-driven and reversible* — nothing permanent joins
the panels except the ground itself.

**Picking a lifted surface.** The same relief that the rim has to follow
also breaks naive picking: inverting the iso transform at `lift = 0` answers
for the base plane, not the surface under the cursor, and the error is
`lift / tileY` lattice rows — tens of pixels against a ~4 px tile at 16x16.
Lift only shifts Y, so a cell's own lift decides whether the point lands in
it; scan the plate and take the FRONT-MOST cell that covers the point, which
is also the correct occlusion answer (a block hidden behind a higher one
cannot be clicked). Iterating from a flat guess does not work — for a lifted
cell near the back edge the flat solve lands off the lattice entirely.
`src/Prospecting/block_pick.h`, round-trip under test.

**One lift law.** The plate's corners (`ProsCornerLift`: the mean grade of
the blocks touching the corner, to the 0.8 power, times relief), the hover
outline and the pick (`ProsCellLift`: the mean of a cell's four drawn
corners), and both ends of the prescribed line (`ProsPlateLift`, handed to
the trace by the pass that drew the plates) all go through the same two
functions. The line's collar used to be placed at lift 0 — up to a full
relief *under* the mound the player had just clicked on, and the x2 lattice
would have moved it again. Anything that has to sit ON a plate asks the
plate; nothing re-derives the height.

### 9.45 The core log lane

The score sheet (redline's third panel), folded into the borehole strip as
one narrow lane (7 px + OUT frame) down its left edge.

**The lane reads depth through the strip's own mapping — never its own.**
Two mappings side by side is the trap this lane fell into twice. Sticks of
a fixed metre length drawn through the strip's per-band scale swing 4x in
height with depth (the bands are equal pixels, the strata are 12/22/34/52
m), which reads as a glitch; a lane given its own linear scale to fix that
then ran up to **78 px behind the bit** mid-column, because the two
projections no longer shared a ground.

The resolution is to count sticks **per stratum**, not per metre:
`PROS_LOG_PER_LAYER` equal sticks in every band. Within a band the strip is
linear, so all sticks come out the same height; and because the lane is
drawn with `YOf` like everything else, the record stands level with the
string. A stick's metre-length then varies by unit (2.0 / 3.7 / 5.7 / 8.7
m), which is how a log is cut anyway — the sample interval belongs to the
unit, not to the tape. Layer boundaries are stick boundaries by
construction, so no stick straddles a seam. Every stick edge is
pixel-snapped: fractional edges rasterise to uneven 0/1/2 px gaps that read
as breaks in the record. Each stick is graded by the thermal DOSE it was cut under
(mean squared heat-excess per metre — a sustained level, never a single
instant: worst-instant grading made the auto-peck sawtooth alternate
stick by stick, which read as random breaks), in redline's own log
legend: **dark** `#0f1821` = uncut, **intact** `#93a7b8`, **partial**
(smoked) `#5c6675`, **lost** — the stick the bit fractured in, rubble where the core was —
a hot `#3a1e16` fill with a `#963e22` inner border (the hot semantic, so
it can never be mistaken for uncut ground); cut sticks of the icy stratum carry a 2 px ice tick `#7fd8ee`.
A landing core still flashes its whole stratum white for 0.5 s (same
clock as the plate cell), and the bit's position rides the lane as an
amber tick — a third cursor on the shared clock. The lane records how
the hole was drilled; it never animates on its own. Its **legend** is a
small backed console chip at the strip's bottom-right (swatch + name per
grade, the LOST swatch with its inner border) — a legend belongs on the
instrument that uses it, not in the page chrome.

The plates carry the same stratum identity: a plate's resting fill is
`PROS_ROCK_COL[layer]` pulled toward the panel ground (0.80 mix, ~0.65
lightness floor) — the dock paints the rock full-strength, the model
paints its quieter twin. Colour-is-class still rules the relief, but the
class **fades toward the rock as certainty falls** (MEASURED 0.90 →
UNCLASSIFIED 0.12 class weight): unknown ground looks like its stratum,
and knowledge reads as colour rising out of the rock.

### 9.5 The animation recipes

Approved in the drill-dock prototype; reuse verbatim:

| Motion | Recipe |
|---|---|
| Shadow path | static full-path ghost: 6 px `rgba(80,225,255,.10)` + 1 px `rgba(160,190,215,.30)` — never animated, it is what everything moves on |
| String rotation | bright dashes `[6,10]` over mid-steel, `lineDashOffset = phase·6` where `phase` is the drill's spin accumulator — banding turns with the rpm and stops with it |
| Marching dashes | `setLineDash([4,8])`, 1.3 px, `lineDashOffset = -t·26` — the advance, flowing toward the target on the shadow; same offset in both views so the two lines progress in step |
| Twin cursor pulse | size `4.6 + 1.2·sin(t·12.6)` (~2 Hz) over an `OUT` under-diamond +1.6 px; both views read the same `t` |
| Class-flip flash | fill the cell top with its new class colour, then `rgba(255,255,255, 0.55·pop)` where `pop = 1-(t-flipT)/0.5` clamped — a white overlay decaying over 0.5 s, never a colour swap |
| Flip pop | prism height +`4·pop` px during the flash, so the cell physically jumps |
| Active-plate rim | bounding-diamond stroke in amber, alpha `0.45 + 0.25·sin(t·12.6)` — same clock as the cursor pulse |
| Bore-ring | a cored cell keeps a 2.6 px `OUT` dot with a 1 px `--text` ring on its top face, permanently |

One clock rule: every correlated motion samples the same `t`. Two animations
on different clocks read as two apps; on one clock they read as one machine.

## 10. Capture — making stills and GIFs of it

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

## 11. Extending This Document

- New **material** → new §4/§5-style section: its two functions, its recipe
  profiles, its variant table.
- New **component family** → new §6-style section: the core geometric trick,
  the proportions that make it read, its grammar of parts, its particles.
- New **technique inside an existing family** → subsection there, with the
  failed pass that taught it if there was one. The failures are half the
  value of this document.
- Every entry names its reference implementation (file + function). If the
  reference moves, move the pointer in the same commit.
