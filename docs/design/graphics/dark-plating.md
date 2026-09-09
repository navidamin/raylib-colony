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
- **Rock is GENERATED, one texture per stratum**
  (`src/Prospecting/rock_texture.h`). Reusing the world's moon-surface tile
  failed twice over: three tiles for four strata meant the deepest layer wore
  the surface's rock, and a tile authored as lunar plan-view ground says
  nothing about what basalt looks like in section. Each stratum now gets its
  own 128×128 tile built from a different *structure*, because what separates
  these rocks on screen is how the grain is organised, not its colour:

  | | structure | reads as |
  |---|---|---|
  | Regolith | fine sand + broad mottle + warped bedding laminae + angular grit | deposited soil |
  | Megaregolith | two generations of wrapped Voronoi, dark seams, proud clasts | coarse breccia |
  | Fractured | calm low-frequency slabs cut by 4 master joints + 9 branches, some ice-filled | broken rock |
  | Basalt | near-uniform, vesicles (dark core, lit lower rim), columnar joints, cooling cracks | dense lava |

  **Sample it 1:1, and draw nothing a pixel wide.** The strip mapped 128
  texels onto 110 screen pixels — a 0.92 minification, which sounds like
  nothing and was enough on its own to average the finest grain into a wash.
  Regolith, whose whole character is fine grit, read as mush beside basalt's
  chunkier vesicles. Two halves to the fix: the strip now maps one texel per
  screen pixel, and every grain is a *disc with a lit top and a shadow under
  it* rather than a bright pixel, because a single pixel is the first thing
  any resampling averages away. Keep the sizes honest to the column while
  you do it — the first crisp pass turned regolith into gravel sitting above
  the breccia, which inverts the whole stratigraphy; contrast comes from each
  grain being lit and shadowed, not from being large.

  **Dark rock swallows texture.** The modulation is multiplicative, so the
  same texture step is worth fewer screen levels the darker the stratum under
  it: on the old basalt `{39,42,48}` a step of 20 came out as about 6 levels
  and the band read as dim rather than as dense. The darkest rock in a set
  therefore needs *both* halves of the fix — a lifted base tone and a wider
  swing (basalt clamps to ±88 of 128 where the others take ±68) — and the
  lift has to stay in its own hue, since the first attempt at it walked
  basalt into the fractured layer's blue and the two deepest bands stopped
  telling apart. Playtest report: *"needs more contrast between the scattered
  dots and the background, it's very dim all together."*

  **Alpha is not lightness.** Asked to lift the dim interbands "20%", the
  obvious move — scale the alpha 0.11 → 0.13 — moved the band from 20.5 to
  21.8 mean luminance, **1.3 levels out of 255**, and the playtest reported
  nothing had changed. Blending a dim overlay onto a near-black panel makes
  the alpha a poor proxy for what the eye gets. Solve it against the rendered
  pixels instead: measure the region, set a target (+20% → 24.6), and bracket
  the alpha until it lands — 0.20 gave 25.0, +21.7%. Two renders and a
  measurement, against one guess that shipped invisible.

  Two rules make it drop in without disturbing anything:
  **the output is a modulation map, not a colour** — grey centred on exactly
  128, so a surface drawn `PROS_ROCK_COL[L] * 2 * tex/255` keeps the mean tone
  its flat fill had, and not one palette entry needed re-tuning (under test);
  and **it is power-of-two and wrap-safe**, because the strip tiles a band
  down a column of any height and WebGL repeats POT textures only. The single
  deliberate hue in the set is the ice in the fractured layer, which the core
  log's legend already names.
- **One ground, both projections** (§9.1 in the small): the borehole strip's
  band and the block model's plate at that depth wear the *same image* — tiled
  near 1:1 down the section, repeated ×2 across the plan view so a clast is
  about the same size in both. The strip's hand-scattered speckle this
  replaced could not be the same ground: it was drawn from an LCG only the
  strip ran.

  **The textured path is the FAST one, by a factor of eight.** One quad per
  cell on one bound texture keeps a whole plate in a single batch: measured
  on llvmpipe, 27 ms/frame textured against **216 ms** for the untextured
  `DrawTriangle` fallback, which pays the generic batch per triangle 4096
  times over. Texture was expected to cost something and instead paid for
  itself many times; the fallback is now the thing to avoid, and it says so
  in the code. (Texturing the dim interbands as well costs 0.4 ms; drawing
  the plates larger after flattening them costs ~3 ms.)
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

### 7a. Twin panels drift; check them against each other, not against taste

Two panels built from the same parts diverge quietly, and every drift reads
as a decision to whoever looks. Found by putting prospecting's borehole strip
and excavation's shaft strip side by side — the shaft's own comment said
"as the borehole strip has them", so matching had been the intent:

- it framed itself in the near-black **outline** token instead of the visible
  hairline, so one twin had a frame and the other appeared to have none;
- it dashed the **top** edge, where the rule (§9.1) is that the dash marks
  the edge FACING THE MODEL — the left, for both panels;
- its depth figures stopped at the last boundary instead of naming the floor,
  so the strip never stated its own depth;
- figures and footer used local RGB literals rather than the shared dim-text
  token, at their own alphas.

But **do not homogenise a difference that carries meaning.** The same pass
looked at the two REACH captions — dim grey in one, amber in the other — and
left them alone: excavation's reach really does grow with tier and really
does stop it digging, so amber (a constraint) is right, while prospecting's
ring was deleted and its figure is merely the size of the ground. What
changed there was the *word*: prospecting now says LATTICE, because one term
meaning two things across twin panels is how a real limit gets read as
decoration and a fixed fact gets read as a wall.

### 7a2. A readout is not a sentence

The line a module reports through earns structure, not prose. Excavation's
read "SPOT 16,16  C 2219 (known)" against "GETTING 323 C/day of 431 moved
(75% useful)" -- five numbers and two parenthesised asides in one weight and
one colour, so nothing told the figure that matters from the words carrying
it.

Same information, drawn as **segments**: dim label, bright value, dim
qualifier. "SPOT 16,16" recedes, `C 2219` reads, "known" recedes; the rate
takes the state colour and the share sits against the tonnage it divides
("75% of 237 moved" rather than "(75% useful)" adrift at the end). Words that
only carried grammar -- GETTING, useful -- are gone; the layout says what
they said.

The rule: in a readout, colour and weight ARE the grammar. If every token is
the same weight, the reader parses a sentence to find a number.

**A number nobody can read is not a readout.** The same line then failed the
other way: it moved every frame. Two causes, and only one of them was noise.

The first was arithmetic. The rate was reconstructed at draw time as
`mass / GetFrameTime()`, which assumed the draw frame's length was the
interval the work ran over. It was not, so the panel rendered twice reported
323 and 178 C/day for identical work. **A rate is measured, not
reconstructed** — see `docs/guides/module-architecture.md` §2.

The second was real: taper, depletion and the power cap genuinely move the
output every tick. So the rate is eased before it is shown
(`EXC_RATE_SMOOTH_TAU_S`, two thirds of a second), and eased **on the dig
tick, not the draw** — which is the distinction §9.46's easing rule leaves
open. The plate light is pointer-driven, so it eases on the draw clock; heat
and output are consequences of *work*, so they ease on the work's clock.
Easing them in the renderer meant a headless preview, which draws two frames,
could never show a hot bit or a settled rate however hard the unit was
digging.

Two corollaries, both learned by getting them wrong first:

- **Seed the first sample whole.** A one-dig preview screenshot must show the
  true figure, not one still climbing from zero — a young number is read as a
  wrong number.
- **Gate the line on the eased value, not the raw tick.** Otherwise a single
  tick that moved nothing flips the whole readout to IDLE and back, which is
  precisely the flicker the easing exists to remove.

And derive both halves from the same pair: taking the percentage from the raw
tick and the tonnage from the eased rate lets "74% of 111 moved" stop being
true on screen.

### 7b. One type scale, reachable from everywhere that draws

A view with a font scale has to apply it to *all* of its text, and the way
that quietly fails is scope: `RenderManager::FS` is a member, the block-model
helpers are free functions, so their labels were the only text in the panel
drawn at raw base size. The level name came out at 8 px against everything
else's 10-18 and read as mush -- reported twice in playtest before the cause
was found, because "that text is small" looks like a choice, not a bug.

Put the multiplier at file scope and have the member call *it*, so a helper
that cannot reach the class can still reach the scale. Then a size that
bypasses it is visible as an anomaly rather than hiding among the others.

### 7c. Restyling a view a generation behind

Site selection was written 2026-02-05 and had **one commit in its entire
history**. The language it was written in predates this guide. Bringing such a
view forward is mostly mechanical, and the checklist is the same every time:

1. **Inline colours become tokens.** Every colour in it was a literal
   (`{20,20,40,220}`, `{100,100,200,200}`) or a raylib primary (`GREEN`,
   `YELLOW`, `RED`). Raw `RED` is the loudest thing that can appear on a dark
   panel; `EXT_ACCENT_RED` is the same signal without the shout.
2. **Solid bars become `ExtDrawSegBar`.** Five GRS bars were most of that
   panel's surface area, so this alone changed its character more than
   anything else.
3. **Outlined rectangles become `ExtDrawPanelFrame`.** Corner brackets are
   what make a rectangle read as an instrument.
4. **Uniform text becomes dim label / bright value** (§7a2), and the sizes
   collapse into one named scale (§7b).

Two things that are NOT mechanical, and both were found only by rendering:

- **Check what range your data actually occupies.** The cell tint used
  composition fractions as if they spanned 0–1. Dumped over the grid they
  span **0.000–0.193**, so every cell crushed to within a few levels of black
  and the map stopped distinguishing anything. Normalise against the measured
  span and say where the number came from. (Same rule as
  `module-architecture.md` §2 — calibrate against dumped real data.)
- **A side panel steals width from the map.** Centring the grid on the raw
  screen put its last columns *underneath* the panel, where they could be
  neither read nor hovered, with dead space on the other side. Frame world
  views against the space the player can actually see, not the window.

And the structural one: the view sat **above** the tokens and widgets in the
file, so it could not call them. Seven months of drift was partly just
declaration order. If a view cannot reach the kit, move the view.

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
same recipe, consumed by the same advance.

**The line is the operation, not the record.** It is drawn while there is
something in the ground to draw — being aimed, being cut, being hoisted out
— and it goes when the string clears the collar, leaving the working panel
showing only what the hole taught it. Everything the hole produced belongs
to the instrument view and stays: the borehole, the log lane, the assay
ticks. Draw a finished path forever and a second hole cannot be told from
the first, and the machine is never seen to finish anything. The renderer
asks the state machine one question — *is there a string in the ground* —
and never re-derives it (`ProsDrawTraceBlock`, prospecting's RETRACTING
state, [drill-tuning §8](../prospecting/drill-tuning.md)).

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

### 9.46 One plate at a time

Four planes of data stacked in one panel is more than anyone reads at once,
and dimming them *by depth* — the obvious move, and what this panel did for
months — helps nobody: every plate is a bit quieter than the last, none is
ever fully readable, and the stack competes with itself at every moment.

Give the stack a **focus** instead:

- **One plate is pinned lit** whatever the pointer does. In prospecting that
  is the surface: it is the plate holes are collared on and the one that
  answers *where am I*. A stack with no anchor reads as four dim things.
- **The rest rest dim, and the one under the pointer comes up to full.**
  Dim means *recede*, not hide — at `{0.50, 0.44, 0.38}` of full, class
  colour is still legible, it has just stopped competing. Keep a slight
  gradient across the dim values so depth still reads when nothing is
  hovered; that is the only depth cue an exploded iso stack has left.
- **Ease it, and put the eased value where it survives a frame.** A snap
  flickers as the pointer crosses the stack. ~0.14 s to full (exponential,
  `tau = 0.045`) reads as a light coming up while still feeling like the
  pointer did it; past ~0.15 s a hover response starts to feel laggy. The
  renderer is rebuilt from nothing every frame and can only ever snap, so the
  value lives on the module facade (`ProspectingSystem::plateLight`, under
  test) — the same rule as every other piece of persistent UI state.
- **Decide before you draw.** The hover was computed *after* the plates in
  this panel, which was harmless while nothing in the draw depended on it and
  became a one-frame lag the moment brightness did — a visible smear of the
  wrong plate under a fast pointer. Pick, then light, then draw, then put the
  cursor decoration on top.

Measured on the real panel: a hovered plate's own region rises 32 → 46 mean
luminance while the pinned plate does not move.

**A plane must look like a plane.** Two supports, both learned from a
playtest that read the stack's depth axis wrongly (prospecting's drill-tuning
§4). Draw the plates **flatter** — the more tilt an iso plane carries, the
more the axis running away from the viewer invites being read as depth, which
is the one meaning an exploded stack must reserve for the gaps between plates
(0.28 → 0.22 of tileX here). And **texture the gaps**: the strata between the
plates wear the same generated rock as the instrument beside them, so the
plates visibly float *in* ground rather than in nothing.

**Tuck the plane under its own boundary — and measure against the DRAWN
surface, not the base plane.** An iso diamond's lateral corners sit at its
centre height, so a plate centred on its boundary has its widest points
balanced *on* the line. But a plate is drawn LIFTED: `cornerLift` raises each
corner by `(grade/maxGrade)^0.8 × relief`, and a field with nothing surveyed
holds the same layer mean in every cell — which lifts every corner the *full*
relief. So a layout computed against the base plane put the visible plate
22 px **above** a line that was, on paper, 5 px above its base. The eye sees
the lifted surface; the layout has to be told about it.

Compensating every plate by the same full relief is not enough, and the way
it fails is instructive: lift is normalised against the max grade across the
WHOLE stack, so on an unsurveyed field the richest layer floats to the full
relief while poorer ones sit lower, by more the poorer they are. Measured on
the playtest, the plate-to-line gaps ran 24 / 27 / 35 / 40 px going down and
read as the stack drifting away from its own borders.

So **hang each plate from its own ceiling**: push it down by its tallest
corner (`plateDrop`, applied inside `Iso` so the draw, the pick, the trace
and the hover cursor all inherit it), and the plate's top lands on its slot
whatever its layer holds. The boundary is then a plain half-a-half-height
above that, identical for all four — verified by dumping the geometry: every
lateral corner at `+11.31 px` below its line, spread zero. Amplitude still
comes from the shared scale, so a barren layer is still visibly flatter than
the ore; only the plate's *placement* became its own business. With real data
a plate hangs from its mound, so the richest cells rise toward the boundary
and poorer ground sits further below it — which is the right reading anyway.

Whatever draws at that boundary — the strip's band, the plate, the depth
label and its ruling — must derive it from **one** function. They did not,
and the first time the plates moved the labels stayed anchored to the plate
and drifted off the depth they name.

But dim it against the plates' **resting** state, not their lit one. At a
third strength this backdrop *camouflaged* the very plates it sat behind —
they rest at 0.38–0.50 of full themselves, so a background at 0.34 is nearly
their equal and the panel reads as one sheet of texture with diamonds faintly
in it. At 0.11 the measured gap is 21 against 31–39 mean luminance: ground
you can read as rock, that no resting plate has to compete with. The rule
generalises — a backdrop is dim relative to the quietest state of what sits
on it, never to the loudest.

**The hover twin, and the ground between.** On a plate, a bright dot marks
the cell and its twin marks the same point in the section. But the strata
bands run the **full width** of the panel, so the empty ground between the
plates is a depth too — and reading it out is what makes the two views feel
like one column. Hover bare rock and a pale, part-transparent dot appears
under the pointer with its twin sliding to the same height in the section,
depth in metres beside it. Sweep down the gap and the section's cursor tracks
you.

Two cursors, two jobs, and the styling has to say which is which: the cell
cursor is solid and saturated because it marks something you can act on, the
ground reading is pale and translucent because it marks only a height. Give a
reading aid the same weight as an actionable cursor and the panel starts
lying about what is clickable.

A dot on the plane and a dot in the section, moving
together: the plane's dot at the cell, the section's at that plane's single
depth and at the same left-right position (iso screen x is `gx - gy`, so the
section's horizontal axis is the one the plates already show). The section's
dot travels **sideways only** — the guide line it rides is level, because the
whole plane is one depth. A cursor that walked up and down as the pointer
crossed a plane was the misreading itself, drawn.

### 9.47 The solid body, and peeling it

The exploded stack (§9.46) is one way to show four planes of ground. The
other is to stop exploding them: draw the column as **one body**, cut into
its four strata, and take layers off when the player asks. Reference:
`../prospecting/prototypes/layer-block.html`.

The prototype is now a **shape bench**: its interfaces are generated rather
than measured, and every number that bends them is a lever — relief, feature
size, octaves, conformity between beds, depth damping, dip with a fan into
angular unconformity, grain, a buried impact bowl (depth, radius, rim, which
interface it was cut into, how far up it reaches), the four thicknesses, tilt,
lattice, depth scale, light angle and strength, the two face tones, texture
strength and scale, and the seam and lip of the bedding line. That is the
document's own argument made operable: the constants below are defaults, and
the bench exists to argue with them.

Reach for the body over the stack when **the ground itself is the subject**
— its structure, its thicknesses, where one rock gives way to the next. Keep
the stack when **four planes of data must be readable at once**, because
that is the one thing the body cannot do: solid, only its surface shows, and
everything below is rock until you peel it. That is a real loss and the peel
is what pays it back. What the body buys is worth the trade in a panel about
ground: one object instead of four sheets floating in nothing, thicknesses
to scale, and a depth axis with no gaps in it.

**The axis goes straight, and that is the point.** An exploded stack has to
give every plate an equal slot and interpolate depth across the gaps, buying
alignment with true thickness (§9.1). A body has no gaps, so `yOf` collapses
to `surfY + m · pxPerM` and the trade simply goes away: the bands beside it
are their real thickness *and* still perfectly aligned. Prospecting's column
went from four equal slots to 12 / 22 / 34 / 52 m drawn as 40 / 73 / 112 /
172 px.

**Hang the body from its waist.** Put `i + j = N` — the line through the
left and right corners — exactly on `yOf(m)`:

```
x = cx + (i − j)·tileX
y = yOf(m) + (i + j − N)·tileY
```

Then the left corner lands on the depth label's ruling and the right corner
lands on the instrument's band boundary, at every depth, for free. Both
joins are construction, not tuning, which is what §9.1 asks of a join.

**The flattening rule lapses here.** §9.46 draws plates at `tileY = 0.22 ·
tileX` so the receding axis cannot be misread as depth. In a body depth is
unmistakably the vertical extent of the cut faces, so the misreading has
nowhere to happen and the tilt can come back up — 0.42 reads as a solid
thing without the top ceasing to read as a plane. Note the shape of that
argument: the rule was kept until the reason for it was gone, not because
the new picture looked nicer.

**Two faces, and they are not slivers.** The viewer sees the `j = N` face
(falling away to the lower left) and the `i = N` face (turned toward the key
light). The cell-prism convention of §9.46 puts these at 0.42 and 0.60 of
the top; on a face the size of a whole block that crushes the rock into a
smudge, so they are remapped rather than multiplied down (§3.2) — **0.64 and
0.84**, which keeps the ratio and keeps the grain. The rock is laid on them
the way the instrument lays it, 1:1 and entering the tile at row `L·41`, so
a face and the band beside it are visibly one image.

**A dark seam on dark rock is not a line.** Every stratum boundary gets the
seam *and* a lit lip: `ROCK_EDGE[L]` at 2.2 px with the layer above's rock
at ×1.9 on a 1.1 px line 1.7 px higher — the newly exposed bedding plane
catching the key light. Drawn as the seam alone, four layers of near-black
rock read as one gradient.

**Peeling.** Everything above the chosen layer goes; the layer is left
translucent; what is under it stays solid.

- **Composite the slab once, not fill by fill.** Fading each quad and each
  texture multiply separately is the obvious way and it is a different
  picture: a half-strength multiply barely darkens, so the rock washes out
  to milk, and where the slab's top crosses its own side faces the two
  translucent passes stack into a brighter patch than either. Draw the whole
  body opaque on a scratch canvas, then composite it at `ALPHA_SLAB` once.
The next three were learned on the prototype while it still carried
prospecting's class map, and they govern the **game panel**, which does; the
prototype itself has since had its data plots taken out to become a shape
bench (below), so read them against `DrawBlockLayer`, not against the file.

- **One job per exposed surface.** Peeling exposes two planes at 22 m apart
  and ~70 px apart on screen. If both speak class they superimpose into an
  unreadable double image. So the slab's own top carries **shape** — rock,
  relief, class only whispered at ×0.10 — and the floor beneath it carries
  the **class map**, which is what the floor was exposed for.
- **Never brighten a class key to beat the glass.** Lighting the floor past
  full to buy back what the transparency takes clips the largest channel
  first: MEASURED green went to `(111,255,204)` and read as cyan, a key
  lying about which class it is. Thin the glass instead — that costs no hue
  at all. Lay the grain on the floor lightly too (×0.6); megaregolith's
  seams will eat a class colour whole.
- **UNCLASSIFIED is not promoted.** It is the absence of a class, so on the
  floor it stays its stratum while the three named classes go to 0.92.
  Promoted with them it becomes a pale field louder than the ore.
- **A translucent body has no silhouette**, so the line is what says where it
  starts and stops: `--text` at 0.88 on the top, 0.75 and 0.50 on the two
  boundaries, 0.62 on the corner verticals.

**Shade against the surface's own range, never a nominal amplitude.** The
hill-shading divisor started as the relief constant — correct while relief was
the only thing bending the ground. Give the same surface a regional dip and an
impact bowl and it spans three times that, so every slope arrives at `tanh`
already saturated: the surface goes two-tone and a crater rim reads as the edge
of a mesa. Measure `hi - lo` over the surface being drawn and divide by that.
The rule generalises past this panel — **a shading normaliser has to be
measured from the shape that is there, not from the parameter that used to be
the only one making it.**

**A texture-strength control must not be a brightness control.** Laying rock on
at strength `a` (a multiply blend at `globalAlpha = a`) lands on
`base·(1 − 0.498a)`, because the tile's mean is exactly 128 (§rock_texture.h).
Left alone, turning texture down turns the block *up*, and every palette
judgement made at full texture is wrong at half. Divide the base by that same
factor — `base = rock / (1 − 0.498a)` — and the mean tone is identical at every
strength, so the slider moves material and nothing else. At `a = 1` it reduces
to the familiar gain of 2.

**Fit the frame to the body, and only ever shrink.** A bench whose levers can
triple the column has to re-fit every frame or it clips exactly when a setting
gets interesting. Scale the whole scene — instrument included, or the shared
axis breaks — from the body's measured extent. But clamp the scale at 1:1:
scaling *up* centres the design width on the canvas and pushes the depth gutter
off its left edge, taking the scale the whole panel is read against with it.

**Cache the body; it is not what is moving.** Measured before any caching:
45 ms a frame stacked, 74 ms peeled — 13 fps, and wasm would have been worse
(dev-workflow's 55 ms estimate-field regression was unplayable). Profiling
put it in two places, neither of them the one to guess: the per-cell surface
fills, and the **drill bar's per-row steel gradients** — `drawShaft` walks
the string in 1.4 px rows and was asking for a fresh `createLinearGradient`
on every one, some 400 objects a frame. Both are static, so both are cached
rather than made clever: the body redraws only when a layer is peeled or the
focus eases (light quantised into the cache key, or an ease defeats the
cache exactly when the panel is busiest), the bar's rock is drawn once, and
gradients are memoised on half-width. 45 → 3.5 ms stacked, and the ~6.5 ms
that remains is the rig actually turning. Colour strings are quantised to 3
levels a channel and memoised too — invisible under texture, and it collapses
thousands of `rgb(...)` allocations a frame to a few hundred.

### 9.48 Fog of war — the ground you have not established

A block model draws an interpretation. Fog of war is the panel admitting how
much of that interpretation is invented: which rock a bed is, where one gives
way to the next, how the column is arranged at all. Reference:
`../prospecting/prototypes/layer-block.html`.

**It is two bodies, not a haze.** A haze dims a truth the panel is still
telling you; this withholds it. Draw an **unknown** body — one undifferentiated
mass from the top of the block to its base, one structureless texture, no
bedding line anywhere in it — then the real strata on top, **masked** to the
part that has been established. Where nothing is established the real body is
not dimmed, it is not drawn, so the panel cannot leak a boundary it has no
business knowing.

**Fog has no shape of its own; it IS the uncollapsed ground.** Giving it a
depth ramp and a noise field of its own and then hiding a finished column
behind it is still a picture with a curtain in front, and it reads that way —
the eye follows the veil rather than the ignorance. Make coverage exactly
`1 − collapsed`, where the only thing that collapses ground is the player
looking. Then the appearance controls (opacity, granularity, the two spreads,
thickness) shape how the fog *looks* and never where it is, which is the line
that keeps a fog-of-war honest. The one thing collapsed for free is the
surface, because you can see it.

**A collapse needs an edge.** A long soft falloff at the rim of a revealed
patch reads as fog thinning over a fixed picture; a short one (~75% of the
radius at full, the rest a quick ramp) reads as ground becoming determinate.
The gradient length is the whole difference between "wiping a window" and
"finding out". What shape that patch takes is set by the action that made
it — see 9.49.

Order is the whole argument: unknown mass → haze over it → real strata masked
on top → the block's outline over everything.

**The unknown tile has to have no structure.** Every other tile in the set says
*which* rock it is by its structure — clasts, breccia cells, joints, vesicles —
so the tile for "not established" is the one with nothing to read: a broad,
low-contrast mottle, clamped to a narrow range. Its tone has to be **neutral**
for the same reason. Every stratum colour in a palette leans somewhere (tan,
brown, blue, near-black), so an unknown that leans reads as a guess at which
rock it is rather than as an absence of one.

**The noise must shift the depth ramp, never multiply it.** Multiplying
(`hidden = dep · noise`) means a low noise value scales the whole thing down at
*any* depth, so `hidden` never reaches 1 and the real strata leak through the
deepest ground — the one thing fog of war must not do. Add instead:
`hidden = dep·1.35 + (n − 0.5)·0.85`. The noise then moves where the fog's edge
sits without ever lifting the floor under it, and the bottom of the column
saturates at completely unknown.

**Haze and mask come out of one pass.** They are two readings of the same
number — `a = min(hidden, 0.9)` and `known = 1 − hidden` — and computing them
separately lets them drift, which shows up as rock visible through fog that is
supposedly opaque. One loop, two `ImageData`.

**Clicks are the gameplay, and they live in the fog's own space.** Store each
established patch as a point in (perimeter, depth) or (u, v), never in screen
pixels, and it survives every change of tilt, lattice and depth scale. The hit
test is the exact inverse of the projection the haze is drawn through, so a
patch lands where it was clicked at any geometry.

**Whatever caches the body must know about the clicks.** The body cache was
keyed on the parameter object; reveals are not in it, so clicking rebuilt the
fog raster and then blitted the stale body over it — the click appeared to do
nothing at all. Any state that changes the picture belongs in the cache
signature, including state that is not a setting.

**Fog is DARK, measured against the panel it sits in.** The obvious pale haze
`{138,156,178}` put full fog at luminance 150 against basalt's 52: the unknown
half of the column became the loudest thing on screen. `{66,78,95}` over
`{58,63,72}` ground lands it near 80 — plainly lighter than the rock, plainly
not lit. Cap the alpha short of opaque: a trace under the haze says there *is*
ground there, merely unestablished.

**Rasterise coarse where it is expensive, fine where it shows.** Parameterise by
distance around the block's **visible perimeter** rather than per face, so one
continuous field covers both cut faces and meets itself exactly at the corner
they share, and draw it through the face's own affine with
`imageSmoothingEnabled` — the same transform the rock texture already uses.

Run the raster at roughly **1:1 with the face as drawn**. Half that and every
grain is smoothed into another soft blob, which is the whole problem. It is
affordable because the two costly terms — the knowledge field, which walks
every hole, and the fbm — go on a **coarse grid and get interpolated**, while
only the cheap one, a single hash per texel, runs at full resolution. Four
times the texels for less work than before: detail you can see per pixel,
structure you cannot, per five.

**fbm alone is a wash, and octaves do not fix it.** Every value sits near the
middle of the range and every edge is a long gradient — adding octaves adds
detail to a shape that is smooth by construction. Two separate controls earn
their place:

- **Roughness** folds the field at its mid-line (`1 − |2n − 1|`), which turns
  that mid-line into a *crease*, so the fog gets edges and not only slopes, and
  mixes in a faster second layer to put structure between the big blobs. Raise
  contrast with it: a rough field still living near 0.5 is a rough field nobody
  can see.
- **Granularity** is one hash per texel, applied to the alpha, to the tint, and
  to what the fog lets through — dust in the fog, in its tone, and in its
  edge. Fog with no tooth reads as an airbrush, and an airbrush reads as a
  graphic laid over the picture rather than as air in front of it.

Check the result against the panel with a **measurement, not an impression**:
the mean luminance must not move (fog stays plainly lighter than the rock and
plainly unlit) while the standard deviation goes up. Texture that also
brightens is a tone change wearing a texture's clothes.

**Redraw the body's outline on top.** Where the column *goes* is not in doubt
even where its contents are. Without it the block dissolves into the background
and stops being an object.

**Stroke only the edges that are silhouette.** Stroking a whole base diamond to
get that outline also draws its two BACK edges — which the body hides, and
which come out as a pair of diagonals ruled across both cut faces: the edge of
a shape you cannot see. Trace the two front edges instead. The same care
applies to any capping face on a solid iso body.

### 9.49 Drilling — making the collapse an action, and the depth a decision

Clicking to collapse ground is a placeholder. The moment a rig does it, three
graphics problems appear at once, and they are the whole of the effect:
**the rig**, **the denudation** (what the hole does to the ground) and **the
spatter** (what comes out of it). Reference:
`../prospecting/prototypes/layer-block.html`.

**The tool is the cursor, tip-anchored.** Arm from a dock whose icon is the
rig *at rest* — string drawn up into the head, so idle reads as stowed rather
than as a small drill — then hide the system cursor and draw the rig with the
**bit tip exactly on the pointer**. Anchoring anywhere else (centre of the
body, the head) makes the player aim with a part of the tool that is not
doing the work, and every placement feels a few pixels wrong without their
being able to say why.

**A vertical tool is the one thing an iso projection draws honestly.** World
"down" maps to screen "down" with no foreshortening, so the rig is drawn
straight, at one scale, at any tilt — no per-cell axonometric rig. Everything
else about the hole (mouth, bowl, ejecta) still has to be projected.

**Denudation is not a decal. It is the surface.** Drive the bowl into the
height field the block already draws — a smooth `(1−u²)²` well of a few metres
inside ~3 cells, plus a Gaussian rim of spoil just outside it — and the
hill-shading, the silhouette against the sky, the fog raster and the cut faces
all pick the hole up for free. An overlay ellipse could not have done one of
those. Two consequences worth planning for:

- the *rim* is what actually reads. The bowl is a dark patch among dark
  patches; the bright lip of spoil catching the key light is what says
  "something was dug here". Give it a real height (~40% of the bowl's depth).
- the ground has to be **re-generated**, not merely redrawn, and the height
  field is usually cached. Quantise the scour depth into ~10 steps and rebuild
  only when the step changes; the rig and the ejecta run smooth on top of a
  ground that updates ten times over the cut, and nobody can tell.

**Spatter is a cone, and its speed must be chosen in SCREEN units.** This is
the trap. Ejecta moves in two different coordinate systems at once — laterally
across a lattice scaled by the tile width, vertically through a depth axis
scaled by pixels-per-metre — and picking "1–4 cells/s sideways, 11–33 m/s up"
looks reasonable in code and renders as a **narrow vertical plume**, because
those two numbers came out 20:1 apart in pixels. Choose a launch speed and an
elevation angle in pixels per second, then divide into each axis' own unit at
spawn:

```js
const az = Math.random() * TAU;
const el = 0.72 + Math.random() * 0.46;      // 41 deg .. 68 deg
const S  = 78 + Math.random() * 74;          // px/s along the throw
const up = S * Math.sin(el), out = S * Math.cos(el);
vi = Math.cos(az) * out / TX;                // lattice cells per second
vj = Math.sin(az) * out / TX;
vz = up / PX_PER_M;                          // metres per second
g  = EJECTA_G_PX / PX_PER_M;                 // and gravity likewise
```

Gravity chosen in pixels too (~300 px/s²) keeps the arc short enough to read as
grit rather than as fireworks. Colour each grain by the bed the bit is in *at
the moment it is thrown*, and the first thing the player learns about a column
is something they watched come out of it.

**A borehole constrains a model, it does not light a patch of wall.** The
obvious implementation gives each hole a *band* on the face — measure how far
along the section you are, cut off hard, done. It is wrong, and it is worth
knowing why, because the shape of the mistake recurs: a lit band treats the
picture as a wall and the hole as a lamp, so ground outside the beam stays
exactly as ignorant as before and a hole in the far corner does *nothing at
all*. That is not how a survey behaves. One hole anywhere improves every
section you can see.

So define **one scalar field over the block** — how much of the truth is
settled at (i, j) down to depth m — and let holes raise it:

- a **near term** that dies over roughly a third of the block's width, plus a
  **floor that never dies**. The floor is the whole point: it is what makes a
  hole in the far corner mildly but visibly worth drilling.
- combined as independent evidence, `known = 1 − Π(1 − kₙ)`, so three mediocre
  holes beat one good one and no single hole ever settles the block alone.
- **depth is the one hard edge.** A hole that stopped at 30 m has seen nothing
  at 100 m. Decay its weight to about a fifth below its own bottom — a fifth
  and not zero, because beds continue, and that inference is one the panel is
  entitled to make.

Then `fog = 1 − known` and nothing else. The fog has no shape of its own.

**What the fog hides is the ESTIMATE, not the truth — so the estimate has to
move.** This is the part that turns a reveal mechanic into a survey. Draw the
interfaces as a blend between a **prior** and the truth, weighted by the
confidence at each point, and every new hole visibly re-fits the model: beds
rise and fall, dips shallow out, a bed that looked level turns out to roll.
Nothing else in a panel like this says *this is an estimate* half so plainly —
a static picture behind a thinning veil always reads as a finished answer being
uncovered.

- **Write it as truth plus error, not as a blend toward a prior.**
  `shown = truth + error × (1 − confidence)`. Blending between two fixed
  smooth fields looks like what it is: one nearly straight plane sliding
  toward another, barely moving. An error field is a whole *wrong horizon* in
  its own right and carries all three of the things you do not actually know
  about a bed — its **depth** (a bulk offset: that there is an interface is
  not the same as knowing where it is), its **dip** (wrong way, wrong amount),
  and its **roll** (ups and downs, at two scales, in the wrong places). Scale
  each by the thinner of the two beds it separates, so a thin bed is never
  given an error that would swallow it, and draw all of it from a fixed hash
  per interface so the block is wrong the same way every time it loads.
- **The error being pinned by confidence is the whole trick.** Because
  confidence is high at a hole and low away from it, the error goes to nothing
  where you drilled and stays free between — so the beds bend *toward* the
  boreholes instead of sliding about as one plate. That is how an interpolated
  horizon behaves, and it is what makes the picture legible as a fit rather
  than as an animation.
- **Some surfaces are not in doubt and must not move**: the ground you are
  standing on, and the bottom of the surveyed column, which is a depth somebody
  chose rather than a bed. Moving those wobbles the body's own silhouette, and
  the object stops being an object.
- **Ease it, don't snap it.** Most of a second, and re-derive the heights from
  noise only when something actually changed — through the ease itself, only
  the blend and the projection need to run.
- **Cap the confidence.** Rescale so the last few percent of fog does not
  count: past that the model is called settled and stops moving. Without a
  ceiling the interfaces creep for ever by amounts too small to see and too
  large to ignore.

**Retract, and leave the evidence.** The column is established at the moment the
string clears the collar, not before: you do not know a hole until you have
finished it. What stays behind is the bowl, the spoil, and a churned patch
where the mouth was — the rig leaves, its work does not.

**Coming out is not going in reversed.** The handle stays where it landed while
the string is drawn back up *through* it — the collar does not lift off the
ground just because the tool is finished — so what rises is the tail, back to
its full length. Then give the end three beats instead of one frame: the string
clears the collar and the column resolves; the machine **stands in the hole it
just finished for about a second**; and only then does it cross-fade out and the
marker in. Resolving the column and packing up the rig are two separate events,
and run on the same frame neither of them registers. Fade the rig **composited
as one object**, not fill by fill, or it shows its own tail through its own head
on the way out.

**A finished hole is not a hole.** The dark ellipse the live string stands in is
right while the string is in it and wrong the moment it leaves: nothing else in
a panel like this is a hole *through* the ground, so a black disc left on the
surface reads as a puncture in the picture — a missing pixel rather than a
place. Fill it with the soil that came out of it: a scatter of chips lighter and
darker than the local rock, thinning outward so the edge of the churn is ragged
rather than drawn, over ground that keeps its own tone and shading. Keep the
fill almost transparent — an ellipse of any solidity is just the black hole in
another colour. And keep it **flat**: the raised rim already exists a few cells
out where the spoil is, and a second raised thing at the centre of it turns one
landform into two competing ones. Seed the grain per hole, or it boils.

**Two decisions need two clicks.** *Where* to drill and *how deep* are separate
judgements, and running both off the one click on the ground makes the second
one disappear: the tool goes straight to some depth nobody chose. Split the
cycle at the collar — one click **spuds in** (the bit sinks its own length, the
handle comes to rest on the ground, which is exactly as far as a rig gets
before anyone decides anything), and then the machine waits. Everything below
follows from that pause existing.

**Give the waiting tool a control, and let the control say it is waiting.** The
depth belongs on the instrument already ruled in metres — here the borehole bar,
which had been a readout since it was built. While the rig is planted, frame
that instrument in a **flashing cadre** in the tool's own colour, with static
corner ticks so it still reads at the dim end of the flash, and put a dashed
target line and a metre tag under the pointer inside it. A control that has
never been used has to say so; a readout being promoted to a control has to say
so twice.

**Hide the pointer only while the pointer IS the tool.** An armed tip-anchored
cursor that stays hidden once the rig is planted leaves the player pointing an
invisible mouse at the control you just told them to use. Draw the tool only
where it can actually be placed (here: on the cap, nowhere else) and give the
real pointer back everywhere else — which also stops the armed cursor covering
the very instrument it is waiting for with a picture of a drill standing in a
diagram.

**The tail is the depth, and the handle never leaves the ground.** A bit that
only shows what stands out below the handle can never go deeper than its own
length. Draw the **string above the handle** — the rod every rig has, the one
the bar has always run off the top of its strip — and shorten what hangs below
to about a third.

Then be careful about *what* descends. Sinking the whole assembly, powerhead
included, is the obvious reading and it is wrong twice over: a powerhead is a
collar, it sits ON the ground and the string runs THROUGH it, and a wide amber
box disappearing down a narrow hole is a box being swallowed, not a hole being
drilled. Pin the handle to the ground the moment it lands, feed the string down
through it, and let the **tail be consumed** — it shortens from the top, one
pixel per pixel of bit travel, until a stub is still standing at the bottom of
the run. That makes the tail a gauge you can read without looking away: what is
left above the handle is what is left to drill. Collars marching down the rod
are what make a constant-width bar read as being *fed* rather than merely
translated.

The one time the handle does move is spudding in, where it rides down with the
bit until it lands — which is the whole content of that first click. So the
handle's height is an *argument* to the rig, not something measured up from the
tip; derive it from the tip and it can only ever sink with it.

The travel is a **gesture, not a scale**: map the chosen depth onto the sky you
have (a hundred-odd pixels), and let the bar carry the true metres. Both views
then show one tool at one depth — drive the bar's own rig from the live drill
while a hole is being cut, or the panel is running two clocks and reads as two
machines.

**Reveal only as deep as the bit got.** This is what makes choosing a depth mean
anything at all: cut the established band off just under the hole with a short
skirt, and a field of holes becomes a set of *depths* — readable at a glance,
no labels — instead of a set of marks.

**What is left in a finished hole has to be a wireframe, and it has to turn.**
A solid stub at this size reads as a pebble and a still one reads as a scratch
in the texture. Three dashed generatrices going round a pair of dashed rings,
their brightness split front/back, with the dash offset crawling: it reads as a
small cylinder turning in the ground from the first frame, and it costs nothing.
Carry the hole's depth in its height and the marker doubles as the survey.

**Leave a way out that costs a hole, not the session.** Backing out of a planted
rig should keep the shallow column it did establish — the ground is broken and
the spoil is on it either way. A control that can strand the panel in a state
with no move available is worse than one that costs the player a bad hole.

**Give the survey one number, and let it gate what depends on it.** A panel
whose whole subject is confidence should say what the confidence *is*: mean
confidence over the block's volume, on a coarse grid, computed from the same
field the fog is drawn from so the bar and the picture can never disagree. Then
use it. Controls that are meaningless before the survey is done — here, peeling
the block apart layer by layer, which reads a number off a shape you invented —
stay **locked** until the number clears its gate.

- **Borrow the domain's own vocabulary** where it exists. *Inferred /
  Indicated / Measured* are real resource categories and carry the whole idea
  in one word each; "38%" alone does not tell anyone what kind of thing they
  are looking at.
- **Prove the gate is reachable before you set it, arithmetically.** A
  threshold nobody can hit is a dead end that looks like a bug. Here: ~8
  well-spread full-depth holes clear 95%, a 3×3 grid reaches 99.8%, and
  shallow-only drilling plateaus around 94% for 25 holes — which is the right
  answer, since nothing has been established about the bottom of the column.
  Show the gate on the bar from the first frame, long before it bites.
- **A status row is head-up: pin it, do not lay it out.** Put the bar in the
  panel's own layout and it drifts down the canvas whenever the content is
  short enough to be centred — the one place a status bar must never be.
  Reserve a band at the top of the window, fit the rest of the panel into what
  is left, and draw the row outside the fit transform, taking only its left and
  right edges from the content so it still lines up with what it describes.
- **Lock, do not hide.** A control that vanishes teaches nothing; one plainly
  present and plainly unavailable teaches the rule. Dash its border, drop its
  opacity, say why in a line, and flash the gate on the bar when someone
  reaches for it — that flash is what connects the refusal to its cause. The
  way *back* (here, returning to the stacked view) is never gated; only the
  step that needs the knowledge is.

**One first-run tag, on the one control that cannot be discovered.** Everything
else on a panel like this is either labelled or found by dragging something; a
dock is the only control that does nothing until you know it is a tool you pick
*up*. Give it a tag, make the tag a hit target in its own right (a hint you have
to aim past is a worse hint), and delete it for good the first time the tool is
taken. A tutorial that stays is an admission that the design needed one.

### 9.494 Giving the block a size

A block diagram whose numbers are decorative is a picture; one anchored to real
ground is an instrument, and the difference is a handful of constants. Anchor
it and every lever, every depth read, every borehole acquires a size somebody
could stand next to. Reference:
`../prospecting/prototypes/layer-block.html` (6 km across, 2 km down, centred
on a 1 km sect).

**Put a known object in it, at its real size.** One structure at true diameter
does what no label can: it makes the block's width self-evident and gives the
eye a ruler it already trusts. Take the diameter from the code that owns it,
and when the design and the code disagree, say so rather than silently picking
one.

**A circle on the ground is an ellipse at every yaw.** Rotation is a symmetry
of a circle, so its projection has semi-axes `(R·√2·TX, R·√2·TY)` whatever the
camera is doing — a footprint needs no per-frame fitting, and neither does the
ring that marks it.

**Exaggerate the vertical, compute the factor, print it.** 6 km against 2 km
drawn true is a sheet of paper; every real block diagram stretches the depth
and states the stretch. Derive the number from the geometry (`px per metre
down ÷ px per metre across`) rather than choosing it, so it stays honest when
the levers move — and put it in the readout. An unstated exaggeration is a lie
about the ground.

**Do not exaggerate the buildings.** The ground is stretched because it must be
to be read; a structure is a thing you could walk up to, and stretching it to
match makes the one object with a known real shape the wrong shape. Take its
height from its own screen width instead.

**Re-tune everything measured in metres, and check what stops making sense.**
Scaling the numbers is the easy half. The hard half is that some things were
sized to be *visible* at the old scale and become absurd at the new one: a
borehole scour that had to read as a landform at 120 m becomes, at 6 km, a
crater the size of the settlement, and the scale the known object just
established is the first casualty. Tools and marks stay symbols at fixed pixel
sizes — but a symbol that towers over the real object undoes the anchor, so
size them against it.

**Version the saved settings.** A lever set stored before the block had a size
is measured in the wrong units throughout; loaded silently it looks like a
corrupt panel, not an old one.

### 9.4944 Reproducing a pixel-art reference exactly

When someone hands you a reference and says *this one, precisely*, eyeballing
it is the wrong tool. Pixel art in particular fails in a specific way: get the
unit or the alignment slightly wrong and it stops being pixel art and becomes
vector art wearing a pixel costume, which is worse than an honest redraw.

**Find the unit before you draw anything.** Scan a run of the artwork — a
dashed line is ideal — and read the run lengths. Five dashes measuring
9,8,8,9,9 px with 10,11,10,10 px gaps is a 1-unit dash on a 2-unit pitch, and
the unit is ~9.4 px. Every other measurement then goes in units, and the whole
thing becomes a small integer table instead of a pile of magic numbers.

**Transcribe the irregular parts, don't approximate them.** A tapering
three-band screw bit is not worth re-deriving as a procedural helix — sample
the artwork on the unit grid (half-units if the shape needs it), classify each
cell into the two or three tones, and store the result as a character grid:

```js
const BIT_ROWS = ["...mLm.....", "..LLLmmm...", "mLLLLmmmm..", …];
```

The shape is then *the artwork's* shape rather than your reading of it, it is
legible in the source, and it costs one nested loop to draw.

**Keep the unit a whole number of pixels — and let it set the sizes.** Cells
on fractional boundaries turn the art to mush under rounding. That constraint
propagates: if the icon is 15 units wide and the unit must be integral, the
button is 60 px or 64 px and nothing in between, so size the container from the
glyph rather than negotiating the glyph into a container.

**Sample the palette, don't guess it.** Reading light `#fafafa` and mid
`#8595ac` off the file takes a minute and removes an entire category of "close
but not right".

**Dashes on a curve must stay axis-aligned.** A dashed ellipse stroke gives
segments rotated to the tangent — the one thing pixel art never has. Place each
dash as a rectangle whose width and height come from the local tangent instead:
wide and flat where the curve runs horizontal, tall where it runs vertical.
That is what the artwork does, and it is what makes the ring read as drawn
rather than as stroked.

**Measure the negative space too.** The gap in that ring is 70°, not the 120°
that looked right — the artwork's topmost dashes sit about 31° either side of
the top. Openings, offsets and margins carry as much of a glyph's character as
its marks.

**Animating pixel art: step it, never ease it.** A sprite that eases through
fractional sizes and sub-pixel offsets is a smooth sprite wearing a pixel
texture — the one failure this style cannot survive. Quantise every motion:

- **Pulses grow in whole pixels.** `base + round(swell·amount·base/2)·2` — the
  mark is 4 px or 6 px, never 5.4, and the ×2 keeps it centred on its own axis.
- **Shift the shading, not the shape.** To turn a helix inside a fixed flute,
  cycle the *tones* through the sprite's filled cells while the silhouette
  stays exactly where it is. Collect each row's occupied indices, rotate the
  characters among them, redraw. Shifting the sprite itself just slides the
  tool sideways.
- **Rotate a whole ring, not an arc.** An arc with a gap in it rotates by
  sliding both its ends around, which reads as stretching. Place the dashes
  around the *full* ellipse and skip the ones that fall in the gap — they
  travel round and vanish behind whatever the gap is for, which is what a
  turning ring does. Even angular spacing bunches the dashes toward the left
  and right extremes, and that is correct: it is what the projection of a
  rotating circle does, and it sells the rotation.
- **Give a multi-part pulse a direction.** Running the swell down the lead line
  and finishing on the marker makes the line read as *feeding* the tool.
  The same beats in a random order read as decoration.

**A live cursor's size is set against the control it came out of.** Trim the
part that costs the most and is worth the least — here the lead line, five
dashes on the spec sheet but only two in the reference's own live states, which
takes the cursor from 95 px to 71 px against a 64 px button: near enough the
same object, without touching the unit that keeps it crisp.

**A tool is one object in every state it has.** Restyling the icon and the
cursor and leaving the *placed* machine as it was is the failure this rule
exists to catch: the player picks up a pixel drill, clicks, and a completely
different machine appears in the ground. It reads as the thing in their hand
having been only a picture of a tool. The fix is not to redraw the working
version in the same spirit — it is to build it out of the icon's own parts:

- the same sprite for the business end, on the same shading cycle;
- the sprite's own top row, continued, for anything long — a three-cell
  `mLm` cross-section extended upward *is* the string, so there is no second
  drawing of a rod that can drift from the first;
- the same ring on the ground, turning the same way, which is the single
  clearest tie between the carried tool and the planted one;
- and for the parts the icon has no answer for, the same two steels on the
  same half-unit grid.

Where the old art carried a colour that was doing a *signalling* job — an
amber powerhead, in a panel where amber means "waiting for you" — move the
signal to a lamp and let the body join the rest of the tool. A machine
painted in the attention colour is saying "attend to me" even while it is
busy, which spends the colour that the one control that really is waiting
needs.

### 9.49445 Converting a rendered machine to the neon language

The borehole bar's rig was the honest article: a cut steel helicoid, four
shaded faces per segment, a chrome chuck at the mouth, an amber powerhead —
`ProsRig`'s grammar from §6, at full detail, as a reference specimen. Bringing
it into the same language as the pixel drill was not a repaint. What the pass
taught:

**Keep the geometry, change the paint.** The helix still walks the same pitch
at the same crest radius and still splits front from back, so it climbs at the
true rate and turns on the same phase. Only the fill changed: four shaded
polygons per segment became a stroked line. A flight is a pump — it is there
to lift cuttings — and a band of light is not, so the *look* may change
completely while the *motion* must not.

**Build a glow out of the line, not out of a shadow.** Three round-capped
passes, widest and faintest first, then a bright core, and a dark halo pass
underneath so it reads against chrome as well as against rock. A `shadowBlur`
on three hundred short segments costs the frame; four `stroke()` calls over one
path do not. Measured: unchanged at 16.6 ms.

**A coarser step is affordable once you stroke.** Shaded faces have to be
small enough to hide their facets; a round-capped stroke joins itself. Twice
the helix at three-quarters the segments.

**Deleting a part exposes what it was hiding.** The chrome chuck went because
the new head has a clamp and a bearing standing in the same place — two
machines claiming one joint. Underneath it were a tool-joint collar and a
step in the string that had never been seen, and both then read as debris
poking out of the ground. Whenever a covering part is removed, render the
frame where the thing it covered is at its most exposed — here, the bit still
at the surface, where the spiral does not exist yet and the collar for it was
being drawn anyway.

**The lit part carries the state.** With the amber powerhead gone there was no
lamp. The double-square handles are the only lit thing on the machine, so they
became the running indicator — better than the lamp was, because the state is
now on the part the eye already goes to.

### 9.49446 An icon rail whose frames open

A vertical rack of tool buttons where hovering opens the frame sideways to
show the name. Four things decide whether it reads as a mechanism or as a
tooltip:

**Pin the edge the icon is on.** The button never changes size; the frame's
far edge travels and the near edge stays. The icon does not move, so there is
nothing for the eye to track, and the row does not appear to jump when a
neighbour opens.

**Set the label against the icon and clip it to the frame.** Right-aligned at
the icon's edge, clipped to the opening, the label is *uncovered* by the
travelling edge. Left-align it and it slides in behind the edge instead — a
label that slides is a label arriving; a label that is uncovered was always
there. Only one of those is a drawer.

**The hit target is the button, never the opened frame.** A frame that grows
under the pointer and then stays open *because* it is under the pointer is a
control you cannot leave without crossing it.

**Ease the frame, round the edges.** A drawer is a mechanism, not a sprite, so
it may ease (~55 ms to 1/e); but round the travelling edge to a whole pixel
before drawing or the border breathes between one and two pixels wide.

**Say the cost of the gutter out loud.** The frame opens into empty
background, and that background has to be *reserved* — it cannot overlap
anything, because the label appears exactly where the pointer is not looking.
Here it was 88 px of a 760 px canvas: 80 came from slack at the opposite edge
and the rest off the widest element. Write the trade into the constant's
comment; the next person will otherwise reclaim the gutter and break the one
thing the rail does.

**Encode the type in the glyph, not only in the label.** These five tools are
point tools and line tools; point tools stand in the dashed ring the cursor
already has, line tools stand on a dashed traverse whose dashes travel. The
subtitle then confirms what the icon already said, instead of being the only
place it is said.

### 9.4945 Real ground, and where it runs out

A block diagram can be built on measured ground rather than invented ground —
but only down to the data's resolution, and the interesting design work is all
at that boundary. Reference: `../prospecting/prototypes/layer-block.html`,
built on the LOLA LDEM_16 the game already ships (16 px/°, **1,895 m/px**).

**Measure the resolution against your window before designing anything.** At
1.9 km/px a 5 km block is **2.6 pixels** across and a 10 km block is 5.3.
There is no terrain in that to read, and no interpolation invents any. What
there *is* — and what is worth having — is the real large-scale form: the
site's true elevation, its true regional tilt, and the real curvature of the
ground. On Tycho's ejecta blanket that tilt is 310 m across 6 km; on the
highland plain south of it, 1,138 m; the panel had previously *assumed* 67 m.
Being an order of magnitude wrong about the ground is worth more than any
amount of invented texture.

**Draw the split at the floor and say where it is.** Above the sampling
interval the surface is measured; below it the surface is invented. Feed the
real heights in as the *shared* term the generator already had — the one every
bed follows in proportion to a conformity lever and forgets with depth — and
every control that shaped the invented ground still shapes the real one, while
the per-bed invented term keeps living below the floor where it belongs. Then
put the floor in the readout. A panel that showed synthetic bumps and called
them Tycho would be worse than one that showed no terrain at all.

**Interpolate with a cubic, not bilinear.** Over three samples bilinear is
three flat facets with creases between them, and the creases read as terrain
that is not there. Catmull-Rom gives the smooth curvature a real surface has at
this scale without pretending to detail.

**Topography is a surface fact; the base is not a bed.** Let the beds inherit
some of the terrain near the top and less with depth, and let the bottom
inherit *none* — it is a cut at a chosen depth, and terrain warping it turns
the block into a bent plate with no readable bottom. That is the difference
between a block diagram and a drape.

**Some real ground will not fit, so clamp it and report the clamp.** Tycho's
central peak swings 1.5 km across 6 km against a 2 km column; at full strength
the cap runs off the body. Cap the swing at a fraction of the column and print
the factor. A picture quietly flattened is worse than one that says it had to
flatten.

**Show the footprint on the real map.** A panel that says "Tycho" without
showing *where* on Tycho asks to be taken on trust. Hillshade the patch from
the same numbers the block is built from, mark the block's window on it at true
size, and the resolution problem becomes self-evident: the footprint is three
pixels wide, and you can see that it is.

Two traps, both paid for here:

- **Know what your extractor already corrected.** A window cut *square in
  kilometres* has had the cos(latitude) longitude widening applied for you;
  applying it again downstream double-counts it.
- **Rebuild before you re-sync.** Statistics computed during the geometry
  rebuild are one interaction stale if the UI re-syncs first — which looks
  exactly like a decoding bug, and costs an hour proving the decoder right.
  Verify a port against the source implementation numerically: matching
  Mare Serenitatis at −2,567 m and a 5 m tilt is the check that ends the
  argument.

### 9.495 Turning the block

An iso block that cannot be turned shows you two of its four walls for ever, and
the two it hides are the two the survey never has to answer for. Making it
rotate is a change to the *projection* and to nothing downstream of it, provided
the yaw goes in at the right place. Reference:
`../prospecting/prototypes/layer-block.html`.

**Turn the lattice, not the camera.** Rotate the grid offsets about the block's
centre and then apply the same fixed isometric drop. The vertical axis stays
screen-vertical at every angle, so a metre is the same number of pixels down as
it always was: the depth ruler still rules, the drill is still drawn straight,
and every overlay that was parameterised in depth needs no thought at all.

**One quantity settles everything: screen depth, `a′ + b′`.** Its sign says which
faces are front-facing (normal pointing down-screen), which corner is hidden
round the back (the shallowest — draw verticals on the other three), and which
way to sweep each lattice axis so a heightfield paints back-to-front with no
sorting at all. Write it once and read it four times.

**Face shading has to follow the face.** As the block turns, a wall swings from
the shaded side of the key light to the lit one; interpolate the two face tones
by the normal's screen-x, or the block stops being lit and starts being a
diagram. For the same reason take the light's azimuth *against the lattice*
(`lightAz − yaw`), so the key stays put in the room while the block turns
under it.

**Make the four faces a continuous loop.** Face *f* ends exactly where face
*f + 1* begins, so one perimeter parameter runs all the way round and one fog
field covers every wall and meets itself at every corner. Then a per-face table
of "step *t* → lattice cell" is the only face-specific code in the panel.

Two traps, both paid for here:

- **A face table written as a literal freezes the lattice size.** `c0: [0, N]`
  in a `const` at module scope captures `N` while it is still undefined; the
  paths still come out right (their closures read the live `N`) but every
  *projected* overlay gets a NaN transform, draws nothing, and looks precisely
  like a face nobody remembered to fog. Derive the corners from the stepping
  function.
- **Each face's raster hangs off its own starting corner, at that corner's
  height.** Hanging it off the datum works for whichever face happens to start
  at the datum and puts the other face's fog a hundred pixels too high.

**Drag the block, not a handle** — the block *is* the handle. That means
press-and-release has to be told apart from press-drag-release, so the whole
click path moves from pointerdown to pointerup. Yaw touches no geology, so a
drag re-projects and never re-derives the height field.

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
