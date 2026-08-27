# Dark Plating — the style, as this engine implements it

**This document is self-contained.** It is the reference for everything in
`darkplate/` and depends on nothing outside this directory.

*Provenance: sections 1–4 were first worked out while drawing a drill rig in
another project, and are restated here in compact form so this engine stands on
its own. Sections 5 onward are new — they are the part that engine could not
express, and they were written against the renders in this directory.*

The name is the thesis: a **dark** world, and everything in it built from
**plates** — hard-edged bands of tone with a heavy line around them, like
enamel plating on machinery. Quantized reads as *drawn*; smooth reads as CG.

| Layer | Sections | A new graphic... |
|---|---|---|
| **The world** | §1 palette, §2 the line, §3 tone | inherits all of it, always |
| **Materials** | §4 metal — ramps and ladders | inherits the material it is made of |
| **Component families** | §5 plated volumes | a new box-shaped machine inherits §5 wholesale; a new *kind* of thing starts its own section |
| **Craft** | §6 capture, §7 extending | inherits whatever its context needs |

---

## 1. The World

### 1.1 The ground is near-black, and committed

Single-theme by choice. Every colour is painted explicitly — nothing inherits
from a host theme, the page background is always set. Light comes from **tone
structure**, not from simulated light sources: nothing casts a computed shadow;
things carry their shading in their bands.

### 1.2 Palette

```
--ground:#070b11  --panel:#0d151e  --panel2:#111c27  --rule:#1c2a39
--text:#c9d8e8    --dim:#61768a    --dimmer:#3d4e5e
--am:#d9962f  --am-lit:#f4c66a     amber   — machinery, attention, action
--cy:#50e1ff                       cyan    — instruments, information, idle
--hot:#ff5a28                      hot     — heat, damage, loss
--good:#5fd39a                     green   — health, success
--ice:#7fd8ee                      ice     — volatiles

OUT  #0a0e14   the chunky outline (front work)
OUTB #101820   softer outline for back-facing work
```

Semantics are load-bearing: amber always means *machine/attention*, cyan always
means *instrument/info*, hot always means *damage/heat*. Never reuse a semantic
colour decoratively. Ice is `--ice`, never cyan — cyan is information.

Implemented in `engine/palette.js`.

### 1.3 Type

**Chakra Petch** for display/UI, **JetBrains Mono** for labels and numerals —
always `tabular-nums`, uppercase labels letter-spaced `.14em`–`.2em`.

---

## 2. The Line

The strongest style marker: **every silhouette sits on a heavy near-black
line** (`OUT`, ~2.2 px at 2× canvas scale; `OUTB` behind back-facing work).

- **One flood pass, then faces.** Fill the *entire* silhouette in `OUT` first,
  slightly inflated, then paint the faces over it. Stroking each face
  individually puts black ribbing *across* the interior.
- Inflation is stroke-then-fill with a round join, so no boolean union is
  needed: pass every polygon of an assembly through the flood pass first and
  the seams between them get painted over by their own faces. Only the outer
  boundary survives, which is the line you wanted.
- **Under-edge lines ground a part.** A ledge gets a dark line under its bottom
  edge before its body is filled, so it sits *on* something.
- Outline widths are **style constants, not per-shape choices** — that is what
  makes separate parts read as one machine. (But see §5.6: they are constants
  *per scale*.)

Implemented in `engine/line.js`.

---

## 3. The Tone

### 3.1 Stepped, never smooth

All shading is quantized. Two mechanisms:

- **Hard-stop gradients** — a `LinearGradient` whose stops come in *pairs*, so
  each band is flat: a plate of tone, not a ramp.
- **Tone quantization** — computed shades snap to a ladder before use:
  7 steps for surfaces, 3 for glints.

### 3.2 Back-facing work is remapped, not darkened

```js
const dim = (v, front) => front ? v : 0.15 + v*0.44;
```

Compressing the *range* (not multiplying) keeps the far side legible but
unmistakably behind — and stops it glowing white-hot when a state tint pushes
every shade up.

### 3.3 State tints the material; it is never an overlay

Heat, charge, corrosion, power — all enter through the material function
itself, so one scalar re-colours every band, glint and outline consistently.
Painting a translucent state layer *over* finished art is the CG look this
style exists to avoid. The only overlays allowed are **atmospherics**: a radial
glow at a hotspot, a sub-15%-alpha full-frame wash past a threshold.

### 3.4 Determinism

Speckle and grain use a seeded LCG with a fixed seed per drawing pass; ground
must not shimmer between frames. `Math.random()` is only for genuinely
transient particles. A `Rng(seed)` closure owns its stream, so one pass cannot
disturb another's.

Implemented in `engine/core.js`.

---

## 4. Metal — the material

### 4.1 The ramp, for turned parts

```js
function steel(shade, heat){
  const base = [lerp(96,238,shade), lerp(104,244,shade), lerp(118,252,shade)];
  const glow = [255, lerp(55,165,shade), 25];
  return mix(base, glow, clamp(heat*1.15, 0, 1));
}
```

The base ramp is a **cool blue-biased grey** (b runs 118→252 while r runs
96→238) — that bias is the plating's colour identity. The glow ramp runs
black-red → orange → near-white as `shade` rises, so hot *bright* metal whitens
while hot *dark* metal stays ember-red.

A cylinder is one banded fill across its width. The canonical five-band
profiles (span, shade):

```
rod   [[0.15,0.11],[0.17,0.98],[0.21,0.58],[0.27,0.30],[0.20,0.07]]
joint [[0.15,0.16],[0.18,0.94],[0.22,0.56],[0.26,0.28],[0.19,0.10]]
chuck [[0.17,0.06],[0.16,0.62],[0.22,0.34],[0.26,0.18],[0.19,0.04]]
```

Read the structure: dark edge → **bright hot-spot band off-centre left** → mid
→ darker → dark edge. The off-centre specular is the implied upper-left light.

### 4.2 Ladders, for flat volumes

`steel()` interpolates two RGB endpoints. That is right for a turned part,
where quantization happens downstream in the banded fill's paired stops. It
fails for a **saturated** material: lerping a dark amber `#241704` to a pale
highlight `#f8d78c` passes through a desaturated tan, so an amber housing
shaded that way goes muddy in exactly the midtones you use most.

The fix is to make quantization the data structure instead of a step applied to
it. A **ladder** is an ordered list of plate tones, dark to bright; a shade
picks a rung and there is deliberately no interpolation between rungs.

```js
register("plating", ["#1b2128","#28313b","#37424e","#495764","#5e6d7c",
                     "#778796","#93a1ae","#b3bfc9","#d9e0e7"]);
tone("plating", 0.74, heat)      // -> rung 6, then heat-mixed as §4.1
```

Use a ladder for a **flat volume**, `steel()` for a **turned** one. Both take
heat through the same glow ramp, so a rod and a housing warm together.

`plating`'s floor is darker than `steel()`'s `(96,104,118)` on purpose: a rod
never shows a face fully turned from the light, a box does, and without a real
shadow rung every volume reads flat. That is a range change, not a new alloy —
the blue bias is unchanged, because that bias is the identity.

Registered: `plating`, `amber`, `dark` (plinths, caps, recessed panels),
`ember` (lit throats, worked metal), `ice`.

### 4.3 The joint grammar

Machines are **assemblies**, and the joints say so:

- **Collar** — a band slightly wider than the part, white glint on top
  (`rgba(255,255,255,.34)`), hard shadow underneath (`rgba(0,0,0,.45)`).
- **Grip parts are always darker than the thing they grip** (a chuck's max
  shade is 0.62, and heat reaches it at 0.6×).
- **Housing box** — outline, flat fill, then a bevel: white top (0.30), white
  left (0.14), black bottom (0.30), black right (0.22). Vents are dark slots
  with a 1.6 px inner shadow line.

### 4.4 Varying the metal — tints, never structure

A new metallic thing keeps the band *structure*, the outline weight, the
glint/shadow grammar, the quantization. It varies the base ramp's colour bias,
brightness ceiling, and heat response.

| Variant | How |
|---|---|
| Plated steel | `steel()` as-is |
| Grip steel | same, capped shades (≤0.62) + reduced heat coupling |
| Amber housing | flat fill + bevel; bands are for *turned* parts |
| Carbide | flat facets — 3 triangles at fixed shades (0.90/0.52/0.22), heat ×1.35 |

For a genuinely new alloy, clone the ramp with a new `base`/`glow` pair, or
register a new ladder, and add it to this table.

### 4.5 Heat, as a field

Heat is Gaussian around the working point, fed to every material call, so the
glow *spreads through the machine* from where the work happens. Feeding one
flat scalar to every material instead turns the whole machine tan — CG lighting
wearing a stepped-tone costume. Atmosphere on top: a radial gradient at the
hotspot, and a full-frame `rgba(255,60,20, ≤.10)` wash only past a danger
threshold.

Implemented in `engine/material.js`.

---

## 5. Plated Volumes — the iso-solid family

Everything a box-shaped machine seen from above needs: a factory building, a
tank, a cabinet, a crate. §4's grammar assumes screen-space rects and swept
cylinders — there was no way to say "a plated volume seen from above". This
section is that way.

Reference implementation: `engine/iso.js`, `sprites/assembler.js`
(stage `assembler.html`, capture `tools/shoot.js`).

### 5.1 The projection is a preset, and it is not symmetric

Axis vectors are screen displacement per **one world unit**, so foreshortening
lives in the preset rather than being applied afterwards.

```
studio34  x:[ 1.000, 0.315]  y:[-0.556, 0.426]  z:[0,-1]   the default
iso21     x:[ 1.000, 0.500]  y:[-1.000, 0.500]  z:[0,-1]   corner-on 2:1
shallow   x:[ 1.000, 0.280]  y:[-1.000, 0.280]  z:[0,-1]   shallow dimetric
```

`studio34` was measured off a reference sprite whose top face is a
parallelogram with edge vectors `(54,17)` and `(-30,23)` px. **The two ground
axes are deliberately unequal.** A corner-on isometric splits the front of a
box evenly between two faces of the same width; a rotated three-quarter gives
one wide face to put a door in and one narrow face beside it. Every readable
building icon in this genre does the second thing.

`+x` runs right-and-down, `+y` runs left-and-down, `+z` is up — so the visible
faces of a box are always **east (x1), south (y1), top (z1)**, and depth
increases with `x+y`.

`fit(preset, bounds, rect, pad)` solves for the scale and origin that centre a
world box in a screen rectangle. Sprite work is "put this volume in that
rectangle", not "pick a scale and hope".

### 5.2 The chamfered volume is the primitive

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

§2's flood rule generalises without any boolean geometry: run the outline pass
over **every polygon of the assembly** first, then paint all the faces. A solid
stacked under another one must skip the top face in *both* passes, or its line
prints through the piece above it.

### 5.3 Face-local coordinates

The feature that makes the primitive usable. `V.on(face, box)` returns a mapper
whose `(u,v)` are world units measured from the face's top-left **as it appears
on screen**: `u` runs right along the face, `v` runs down it. Lay a shape out
like a UI rectangle and it lands the right way up, on the right face, in the
right place, with no per-face special-casing.

Without it every greeble is hand-projected, which is how iso art turns into a
pile of magic numbers that cannot be moved.

- `m(u, v, lift)` offsets along the face's outward normal — positive stands a
  plate proud, negative sinks it.
- `m.at(u, v, lift)` is the depth-compensated twin. A point pushed into a face
  does not stay where you put it: on a south face it slides right and up by the
  projection of the normal. `m.at` solves the 2×2 system that cancels that
  displacement, so `(u,v)` means the same screen place at any depth.
- `m.vShift(lift)` is the vertical half of it, for when you want real parallax
  but need to know where the usable band of an opening starts.

The furniture built on this — `panel`, `studs`, `louvres`, `rail`, `lamp` — is
§4.3's joint grammar restated for faces. Same rules: grip and recess parts
darker than what they grip, bright rule on a top edge, dark rule under a bottom
edge.

### 5.4 The recess — a hole with thickness, and a room behind it

A doorway drawn as a black shape on a face is a sticker. Three surfaces make it
a hole:

- **reveal** — the plate's own cut edge, banded bright across the top-left
  shoulder and falling to nothing on the right. Painting it evenly all the way
  round is the tell that it was drawn as an outline instead of a surface: the
  far side of a cut edge does not face the light. Generate the opening and its
  reveal from the *same* path generator at two insets, rather than offsetting a
  polygon.
- **tube** — the camera looks down and from the left, so of the six inner faces
  exactly three face it: the **back**, the **floor**, and the inner side wall on
  the **left**. Take the sill from the path's first and last points and the left
  jamb from its first two, and the floor and wall fall out of the geometry
  instead of being drawn as art. (Both path generators here start bottom-left
  and end bottom-right, which is what makes that work.)
- **back plane** — the path again at `-depth`. Because it lands up-and-right of
  the mouth on a south face, the sliver of tube left visible along the bottom
  and left *is* the floor and the near wall.

Bands that trail into the depth must be **relative** to the mouth shade. Hard
values there made a dark floor get *lighter* the deeper it went — a lit back
wall in a machine with no light in it, and the tell was a pale wedge in an idle
throat.

An interior light is the one overlay allowed (§3.3, atmospherics): a radial
gradient placed at the work, inside the clip.

### 5.5 Painter order for an overhanging base

Bottom-to-top is not the order. A plinth that overhangs the body has front
faces **nearer the camera than the body's**, and a doorway is a hole, so
anything painted before it shows through. Draw the deck first, then the body
and its cuts, then repaint the base as a **near lip**: its front faces and only
its front chamfers, plus the strip of deck outside the body's footprint.
Repainting the back chamfers too puts dark wedges across the machine's front —
they belong to a rim that is now occluded.

### 5.6 Scale is a material property

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

---

## 6. Capture — making stills of it

- **Deterministic stepping**: expose `__setState(o)`, `__manual(on)`,
  `__step(dt)`; harnesses drive the frame at fixed dt so output is reproducible
  regardless of headless timing.
- **Zoom to judge**: at full frame a doorway reveal is four pixels. Screenshot
  crops at `deviceScaleFactor` 2–3 and *look at them* before claiming anything.
- **Poses for stills**: a geometry pose is motion stopped and particles
  cleared — debris on top of the part being judged hides exactly what you are
  judging.
- Pick pose timestamps that land the mechanism where you want it. A press
  photographed mid-return tells you nothing about the press.

`tools/shoot.js` does all of this.

---

## 7. Extending This Document

**Every new graphical component is a change to this document too.**

1. **Before drawing** — read §1–§3 (the world), §4 for what the thing is made
   of (and §4.2 on ladder vs ramp), and the family section if one exists.
2. **While drawing** — reuse the named helpers. Change tints, never structure
   (§4.4). Render previews and look at them. Never claim a visual result
   without rendering it.
3. **In the same commit** — if the component needed a technique this document
   does not have, add it: a subsection under the right material, or a new
   §5-style family section. If it needed nothing new, it should look like it
   belongs; if it doesn't, one of you is wrong.

- New **material** → new §4-style subsection: its function or ladder, its
  recipe profiles, its variant table.
- New **component family** → new §5-style section: the core geometric trick,
  the proportions that make it read, its grammar of parts, its particles.
- New **technique inside an existing family** → a subsection there, **with the
  failed pass that taught it** if there was one. The failures are half the
  value of this document.
- Every entry names its reference implementation (file + function). If the
  reference moves, move the pointer in the same commit.
