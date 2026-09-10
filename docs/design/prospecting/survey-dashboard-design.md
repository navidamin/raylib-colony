# Survey Dashboard — the prospecting module's new look

**Status:** DESIGN — plan of record for the module's *presentation*.
**Sources:** [`prototypes/dashboard/dashboard.html`](prototypes/dashboard/dashboard.html),
[`prototypes/dashboard/holo3d.js`](prototypes/dashboard/holo3d.js),
[`prototypes/dashboard/layers-block-3d.html`](prototypes/dashboard/layers-block-3d.html)
(vendored verbatim as received) · reference render:
[`prototypes/dashboard/dashboard.png`](prototypes/dashboard/dashboard.png)

**Supersedes** the panel chrome, layout and interaction surface of
[#9 block-model-design](block-model-design.md) and of
[#13 prototypes/layer-block.html](prototypes/layer-block.html). It supersedes
**nothing about the mechanics** — the knowledge field, the confidence model,
the re-fitting estimate, drilling, delineation gating and the resource model
are unchanged, and §6 below is the map from each of them to its new home.

**Does not touch the drill.** The drill glyph, the cursor, the planted rig and
the borehole-bar rig stay exactly as they are (§5). Everything else moves.

---

## 0. What changed, in one paragraph

The module was a **single tall diagram** — a block on the left, a borehole bar
on the right, a lever rail beside it. It becomes a **console**: five bracketed
panels on a 1536 × 1024 field, in three columns, holding a machined tool rack,
a live rotatable holographic block, a scrolling survey log, and the drill bar
with its telemetry. The block stops being a fixed isometric drawing and becomes
**software 3D** — yaw *and* pitch, beds that explode apart and isolate. The
palette moves from near-black-and-amber to **navy-and-cyan hologram**. The
prospecting loop underneath is the same loop.

---

## 1. The three source files

| File | What it is | Role |
|---|---|---|
| `dashboard.html` | The whole console. Four self-contained UMD modules in one page: `ToolRack`, `HoloBlock` (the static iso block, now unused), `Holo3D`, `Dashboard` — plus a `DASH` config object and a driver | **The design of record.** Layout, palette, panel chrome, every widget |
| `holo3d.js` | `Holo3D` on its own | The block renderer we adopt |
| `layers-block-3d.html` | `Holo3D` plus a bare driver | The block in isolation, for reading and for tuning |

`HoloBlock` (the second module in `dashboard.html`) is a **static isometric**
version that the live `Holo3D` replaced; the dashboard still ships it but only
calls `Holo3D`. It is not part of this design.

The `DASH` object at the foot of `dashboard.html` is the whole configuration —
panel rects, rack contents, layer names, log entries, drill telemetry — and it
is the shape our own state should present to the renderer.

---

## 2. The frame

**1536 × 1024 design units**, three columns, five panels. Exact rects, from
`DASH.layout`:

| Panel | x | y | w | h | Notes |
|---|---|---|---|---|---|
| `survey` — SURVEY TOOLS | 40 | 22 | 338 | 676 | rack drawn at (48, 32), scale 0.845 × 0.875 |
| `stats` — TOOL STATS | 40 | 724 | 338 | 242 | branch connector at x 207 |
| `layers` — LAYERS | 400 | 22 | 740 | 944 | the live block; log box occupies its lower third |
| `drill` — DRILL BAR | 1164 | 22 | 328 | 668 | |
| `dstats` — DRILL STATS | 1164 | 706 | 328 | 260 | branch connector at x 1328 |

Two **connectors** — a short vertical cyan branch with a pip at each end — tie
each column's lower panel to the one above it, so TOOL STATS reads as belonging
to the rack and DRILL STATS to the bar. That is the only hierarchy the frame
expresses, and it is worth keeping: it is what stops five panels reading as
five unrelated windows.

### The 3:2 problem

The current prototype is 760 × 880 — nearly square, and it fits a phone held
upright. The dashboard is **3:2 landscape**. On a phone in portrait it scales
to roughly a third of the height it needs, and every 13 px label becomes
unreadable. This design does not solve that; **§8 decision 4** is where it has
to be solved, and no implementation should start the left column until it is.

---

## 3. The visual language

### Palette

| Role | Value | Where |
|---|---|---|
| page ground | `#020b13` | outside the panels |
| panel fill | `#03121d` | inside |
| panel rule | `#1a4a5c` | the dim rounded outline |
| accent | `#35d8ee` | corner brackets, connectors, active rules |
| accent, dim | `#1c7f95` | connector stems |
| hot cyan | `#00fbfe` / `#6ffefe` core / `#00bdd3` edge | powered rack slots, the block's lit edges |
| title | `#62b3f5` with a `#21e3f0` underline | panel titles |
| body prose | `#6f8fb0` | the blurb, in **Inter**, not mono |
| label / bright / dim | `#a3b8cc` / `#bcd2e6` / `#5f7a96` | mono labels |
| meter, on/off | `#24dcf2` / `#0a2230` on `#153747` | tool stats |
| health ramp | `#3fe36e` `#e9e34b` `#ffa441` `#ff5a5a` | drill telemetry, confidence |
| resource chips | power `#f2c94c` · water `#58a8ff` · propellant `#a98cff` · farming `#4fe57a` · life `#ff6262` · construction `#9fb6cc` | the log |
| strata | `#7a5230` `#9fb8cf` `#365269` `#2c5f9e` `#1d2731` | the drill bar's beds |

The **rack has its own palette** (`ToolRack.P`, ~60 entries) because it is
machined metal rather than hologram — steel bodies, brackets, studs, screw
heads, and the cyan slot lights. Do not merge the two: the rack is a physical
object sitting on a holographic console, and that contrast is the point.

### Type

- **JetBrains Mono** 500/600/700 for every label, number, title and tag.
- **Inter** 400/500/600 for prose only — the two blurb lines under LAYERS.
  This is the first sans-serif in the project and it is deliberate: it marks
  the one place the console speaks in sentences.
- Titles are drawn **condensed** (`sx: 0.9`), 26 px, weight 700, with a 40 px
  underline bar 3.5 px tall, 10 px below the baseline.

### The four chrome idioms

1. **Bracket panel.** A 12 px-radius rounded rect in `C.line` at 1.5 px, then
   four bright `C.accent` corner brackets at 2 px with a 26 px leg — and a
   5 px gap punched in the dim outline just past each leg, so the bracket
   reads as *on top of* the frame rather than as part of it.
2. **Branch connector.** Vertical 2 px stem in `accentDim`, a 6 × 3 pip at
   each end in `accent`, with the panel ground punched through behind both
   ends.
3. **Segment bar.** `n` rounded 15 × 15 cells on a 3 px gap; filled cells get
   a dark stroke and a 2 px white top highlight, empty cells `barOff` with a
   `barEdge` stroke. Every meter in the console is this one widget.
4. **Hex vignette.** The block sits inside a large flattened hexagon in
   `C.hex` (`#1f5a6e`) that frames it without boxing it.

---

## 4. Panel by panel

### 4.1 SURVEY TOOLS — the rack

Not a list of buttons. A **machined rack**: a steel spine down the left with
hex-bolted end caps, six studs, and a row of slot lights; five bays to its
right, each a square **icon box** and a **name tag** shaped like a luggage
label (notched right edge, a hole punched in it).

Geometry, from `ToolRack.G` (rack-local units, rack ≈ 370 × 842):

| | |
|---|---|
| spine body | x 20, w 38, bottom at 750 |
| rows | first top at 140, pitch 122, row height 108 |
| icon box | x 77, w 109, h 103 |
| name tag | x 194, w 161 |
| slot light | centred at x 38 |
| studs | y 137, 240, 356, 477, 595, 714 |

**Three states per bay**, and they are what carry the module's honesty:

| State | Icon box | Tag | Spine light |
|---|---|---|---|
| powered + selected | cyan outline, glow, `boxIn` near-black, bright icon | cyan name, cyan edge, glow | tall cyan pill, lit |
| powered | steel edge, dark fill, bright icon | cyan name, grey type | tall cyan pill, lit |
| unpowered | `boxInOff` steel, dim icon | `nameOff` grey | short grey pill |
| **empty bay** | dark box with a blank plate | blank tag | short grey pill |

The fifth bay is **empty**, not "coming soon" — a rack with a visible hole in
it says *there is a slot here and nothing in it* far better than a greyed
tile does.

**Contents** (`DASH.rack.tools`): DRILL (Point), SEISMIC (Line), ROVER (Line),
SONAR (Area), empty. Each carries a `stats: [power, time, crew]` triple that
feeds TOOL STATS.

**Interaction.** Tap a bay to power it; the last bay switched on becomes the
selected one; keys 1–5 do the same. State changes **crossfade over 260 ms** —
the previous state is drawn underneath and the new one faded in on top
(`ToolRack.crossfade`), so a slot lighting up is a slot lighting up rather
than a repaint.

### 4.2 TOOL STATS

Titled `TOOL STATS — <NAME> (<Type>)`. Three rows — **Power**, **Time**,
**Crew** — each a glyph, a label, and an 8-cell segment bar filled from the
selected tool's `stats` triple. It reads the rack; it has no state of its own.

### 4.3 LAYERS — the holographic block

The centre of the console and the largest single piece of new code.

**Model** (`Holo3D.build`). Six boundary surfaces (top + four bed floors +
base) as height fields on a 16 × 13 grid, 740 wide × 700 deep. Each surface
comes from a **measured edge profile** — 20 samples along `u ∈ [0, 2]`, where
`u = 0` is the left-back corner, `u = 1` the front corner and `u = 2` the
right-back — made into a surface by the separable trick
`f(x, z) = p(x) + p(1 + z) − p(1)`. Cheap, and it guarantees the two visible
walls carry exactly the profile that was measured off the reference art.

**Camera** (`Holo3D.camera`). Yaw about the vertical, pitch 0.18–1.25 rad,
zoom. `yaw = 0` is the reference three-quarter view. Two properties matter
downstream and are worth stating plainly:

- **World-vertical projects to screen-vertical.** For two points differing
  only in world *y*, the projected *x* is identical and the projected *y*
  differs by `−Δy·cos(pitch)·zoom`. So anything drawn upright — the drill,
  the string, the borehole — stays upright at every camera angle. It only
  **foreshortens**, by `cos(pitch)`.
- Consequently the panel's "pixels per metre of depth" is no longer a
  constant. It is `D · cos(pitch) · zoom / column_metres`.

**Render passes**, per bed, painted bottom-to-top with the selected bed last:

1. **Walls** — only the two facing the camera. A four-stop vertical gradient
   `neon → mid → deep → deep`, shaded by a fixed light `[−0.55, 0.65, −0.5]`.
2. **Wall mesh** — verticals at every grid column, horizontals every ~26 px,
   plus a sparse scatter of darker and lighter cells (18% of them) that reads
   as rock rather than as graph paper. Skipped while dragging.
3. **Top surface** — per-cell slope shading from the true facet normal, mixed
   between the bed's `deep` and `neon`; a grid over it; a 12% scatter of dark
   cells.
4. **Edges** — the top bed's rim in white at 2.2 px with a 16 px cyan bloom,
   other beds in their own `line` colour at 1.5 px; hidden edges as a 1 px
   ghost; vertical corner edges bright only where two visible walls meet.

**Explode and isolate.** `offY(k) = (2 − k) · gap` — the stack opens
symmetrically about the middle bed rather than lifting off the top. Everything
but the selected bed drops to alpha 0.28. Tap a bed to isolate it, tap it
again or the background to collapse; keys 1–5.

> **The blurb promises two verbs and `Holo3D` implements one.** "Take off
> everything above a layer and see through what is left" is *peel*; "One layer
> only lifts the chosen bed out on its own" is *isolate*. `Holo3D` has isolate.
> Peel — which the current prototype does have — must be added. See §8
> decision 3.

**HUD** (`Holo3D.drawHud`), all of it *projected*, so it turns with the block:

- **Scan wavefront.** A glowing line that crosses the top surface from the far
  corner to the near one over ~4 s, then spills over the two near edges and
  slides down the walls at constant depth, breaking at explode gaps. Three
  passes with decaying tails give it a comet trail. Prints
  `SCAN nnn%  <depth>`.
- **Brackets.** Four corner brackets around the block's screen bounds.
- **Base ring.** A dashed circle with 36 ticks on the ground plane beneath the
  stack, clipped to *outside* the block's convex hull so it never draws over it.
- **Reticle.** True circles on the top surface at the drill site — two rings,
  24 ticks, two spinning arcs, crosshairs, a pulsing centre, and a leader line
  to a `DRILL SITE · LOCK` label with live θ and yaw.
- **Callouts.** One card per bed, anchored to that bed's screen-rightmost
  corner at mid-depth, with an elbow leader: `L<n> · NAME`, the depth range, a
  hex tag, and an 8-segment value bar.

In the dashboard these callouts are **not** drawn — the depth ruler to the
right of the block does that job instead, with leader dots and dashes at each
boundary. Both exist; the ruler is what the reference render shows.

**CONFIDENCE.** A 28-cell segment bar under the block running the health ramp
red → orange → yellow → green, with the percentage at its right.

**Interaction** (`Holo3D.attach`). Drag to rotate (yaw `dx × 0.012`, pitch
`dy × 0.009`, clamped) — measured in **CSS pixels**, so the feel is identical
on a phone and a desktop. A press that moves under 10 px and lasts under
600 ms is a tap, and a tap selects. Wheel zooms. `state.fast` is set while
dragging and drops the expensive scatter passes and every `shadowBlur`.
Auto-rotation runs until the first interaction, then stops for good.

### 4.4 The survey log

Inside the LAYERS panel, below the confidence bar. Timestamped entries in
prose, with two inline objects:

- **layer tags** — `layer 3` in a rounded box, cyan on `#0a2431` with a
  `#2c7d95` edge, set inline in the sentence;
- **resource chips** — a row of pills under an entry, each an icon and a name
  in that resource's own colour.

A scrollbar with stepper buttons sits at the right. Entries are the module
*talking*: "Discovered a mature pocket in `layer 1` and tagged the resources."

### 4.5 DRILL BAR

A vertical strata column — five bands in `C.strata` — with the drill inside it
and a depth ruler with tick marks down the right at 0 m SURFACE, 200 m,
600 m, 1.15 km, 2.00 km BOREHOLE.

**The drill inside it in `dashboard.html` is a placeholder** (`roughDrill`,
commented as such). Ours replaces it unchanged. See §5.

### 4.6 DRILL STATS

Five telemetry rows — Rotary Speed, Load, Temperature, Bit Wear, Vibration —
each a glyph, a label and a 10-cell bar whose fill is given as a **letter
code**: `g` green, `y` yellow, `o` orange, `r` red. `'ggyor'` is five lit
cells ramping to red. Then a full-width `DRILL RUNNING` status button.

This panel is **new information** — the module has never shown rig telemetry.
§8 decision 5 is where it comes from.

---

## 5. What is kept: the drill

Explicitly out of scope for the restyle, and to be moved across unchanged:

| Piece | Where it is now | New home |
|---|---|---|
| the pixel drill glyph (`BIT_ROWS`, `DrawBitSprite`, `DrawGlyphRing`, `DrawGlyphTrack`, `DrawDrillGlyph`) | `layer-block.html` | the rack's DRILL icon, and the cursor |
| the armed cursor | the aim state | unchanged, over the block |
| the planted rig (bit, three-cell string, marker box, ring, tail with lead-dash collars, green/cyan state) | `DrawDrill` | unchanged, standing on the block's top surface |
| the borehole-bar rig (schematic, guides, louvred casing, dual handles, black clamp, ball bearing, neon spiral, capped chrome jacket) | `drawHead`, `drawThread`, `drawShaft` | unchanged, inside DRILL BAR |

The rack's DRILL icon in the reference render **is already our glyph** — a
marker square over a striped bit inside a dashed ellipse. The other three rack
icons are the dashboard's own thin neon line art, and those we adopt.

The drill's palette is the one thing that needs a look: the rig is white/steel
with cyan and green accents on a near-black ground, and the console is navy.
It should sit on the new ground without retint — but that is a thing to
**render and look at**, not to assert.

---

## 6. Where every existing mechanic lands

This is the table the implementation is written against. Nothing in the left
column changes; only its address does.

| Mechanic (today) | New home |
|---|---|
| four strata as one cut body | the `Holo3D` block, unchanged in meaning |
| procedural interfaces (fbm, conformity, dip, dip fan, 28 × 28 lattice) | **stays ours.** `Holo3D`'s six measured profiles are a *look*, not a model — our height fields feed `model.pts` instead |
| truth + error, the estimate that re-fits with each hole | unchanged; the block simply redraws |
| fog of war (unknown body, haze, `destination-in` mask) | **needs re-expression.** §8 decision 2 |
| knowledge field `KnowAt(i,j,m)`, per-hole near term + floor | unchanged, no UI |
| delineation (mean confidence, INFERRED/INDICATED/MEASURED) | the **CONFIDENCE** bar under the block |
| the 95% gate on layer controls | gates *peel and isolate*; the rack and the drill stay live |
| peel (click a layer, everything above comes off) | LAYERS, blurb line 1 — **to be added to `Holo3D`** |
| solo / one-layer-only | LAYERS, blurb line 2 — `Holo3D`'s isolate |
| the borehole bar as depth readout **and** depth control | DRILL BAR, unchanged |
| arm → spud → set depth → cut → trip out → bore marker | unchanged; the cursor and rig live over the block |
| scour bowl, spoil rim, ejecta | unchanged, on the block's top surface |
| craters, the sect dome, built ground | unchanged, on the top surface |
| real LOLA ground + the site card | **no home in the console.** §8 decision 6 |
| the ~30-lever bench + Copy values | **no home in the console.** §8 decision 6 |
| the tool rail (5 buttons, frames that open left) | **replaced** by the rack. Its four non-drill sprites may not survive — §8 decision 1 |
| depth scale down the left of the block | the ruler right of the block |
| the readout line (block size, column, lattice, V.E.) | **needs a home.** Suggest: a footer line under the LAYERS panel, small and dim |

---

## 7. What this costs

Said out loud, because these are the things a future session will otherwise
rediscover the hard way.

1. **The rock textures go.** `layer-block.html` ports `rock_texture.cpp`
   verbatim — regolith gardening, breccia Voronoi, ice-filled fractures,
   basalt vesicles — so the block wears *the game's own rock*. The holographic
   block is line art and slope shading; there is no place in it for a 128 px
   modulation tile. This is the single biggest loss in the change and it owes
   a graveyard record when it happens.
2. **The block stops being a specimen and becomes a display.** Today the
   prototype claims the block *is* the game's ground. Afterwards it is a
   readout of the ground. That is a defensible thing for an in-fiction console
   to be, but it is a different claim and the copy should stop making the old
   one.
3. **The 3:2 field costs the phone.** §2.
4. **Two palettes now live in the project.** Dark Plating (near-black, amber,
   the rest of the game) and this (navy, cyan). Either the console is an
   exception with a stated boundary, or Dark Plating gains a chapter and the
   game follows. §8 decision 7.

---

## 8. Decisions still open

Each of these blocks a specific implementation stage; none blocks starting.

1. **Which five tools?** The mind-map named Drill · Active seismic · Rover
   traverse · Penetrometer · GPR line. `DASH.rack` holds Drill · Seismic ·
   Rover · Sonar · empty. *Recommendation:* take the rack's four plus the
   empty bay — the empty bay is a better statement than a fifth greyed tile,
   and "Sonar (Area)" introduces the third geometry class the tag system
   already has room for.
2. **What does fog look like in a line-art block?** The current fog is a
   painted haze over an unknown body. In a hologram the natural reading is
   the opposite: **the known part is drawn and the unknown is a bare
   wireframe** — no fill, no bed colours, just the grid and a noise crawl.
   That is a better fit for the idiom *and* for the fiction (a console draws
   what it has measured), but it is a redesign of the module's most-worked
   visual and needs its own render-and-look pass before it is committed to.
3. **Peel, isolate, or both?** The blurb promises both. Recommendation: both,
   with peel on the depth ruler (click a depth → everything above comes off)
   and isolate on the block (tap a bed), so the two verbs have two different
   controls and cannot be confused.
4. **What happens on a phone?** Options: (a) reflow to one column and scroll;
   (b) a tab bar with one panel at a time; (c) keep the block full-bleed and
   put the rack and bar in drawers. Recommendation: (c) — the block is the
   game, the rest is chrome.
5. **Where does the drill telemetry come from?** Rotary speed, load,
   temperature, bit wear, vibration are five values the model does not have.
   Either invent them as real state (they would give the rig a maintenance
   loop it does not have) or drive them off what exists — depth, rate,
   the bed being cut. Recommendation: derive them; do not add a mechanic to
   fill a panel.
6. **Where do the bench and the real-ground card go?** They are development
   instruments, not game UI. Recommendation: keep both, behind one `?bench=1`
   toggle, off by default, drawn over the console.
7. **Is this the game's new look, or this module's?** Everything outside
   prospecting is Dark Plating. Recommendation: treat the console as
   *in-fiction glass* — a holographic instrument the crew looks at — which is
   a legitimate sub-idiom of a near-black art direction, and give Dark Plating
   a chapter for it rather than replacing it. That keeps the sect view, the
   terrain and the other 39 module panels where they are.

---

## 9. Related

- [`survey-dashboard-implementation.md`](survey-dashboard-implementation.md) — the build plan
- [`block-model-design.md`](block-model-design.md) — the loop this presents
- [`confidence-system.md`](confidence-system.md) — what CONFIDENCE means
- [`../graphics/dark-plating.md`](../graphics/dark-plating.md) — the style this diverges from
