# Survey Dashboard — the prospecting module's new look

**Status:** DESIGN — plan of record for the module's *presentation*.
**All seven open decisions are settled** (§8). Nothing here is waiting on an
answer; §8 is now a record of what was decided and what each decision commits
the implementation to.
**Sources:** [`prototypes/dashboard/dashboard.html`](prototypes/dashboard/dashboard.html),
[`prototypes/dashboard/holo3d.js`](prototypes/dashboard/holo3d.js),
[`prototypes/dashboard/layers-block-3d.html`](prototypes/dashboard/layers-block-3d.html)
(vendored verbatim as received, **rev. 2026-09-11**) · reference render:
[`prototypes/dashboard/dashboard.png`](prototypes/dashboard/dashboard.png)

> **Revision 2026-09-11** changed two things in `Holo3D`, and nothing else —
> the same two edits appear in `dashboard.html`'s embedded copy. The **scan
> wavefront is removed** (§4.3), and **bed isolation is fixed** (§4.3, and the
> technique is worth reading before Stage 1). Both are carried below.

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

The empty-bay dress stays in the renderer even though nothing uses it now: the
rack is five bays and the tool list is data, so the day a tool is removed or a
sixth is planned, the hole draws itself.

**Contents.** All five bays are filled, and the tools are the survey set, not
`DASH.rack`'s placeholder four:

| Bay | Tool | Geometry |
|---|---|---|
| 1 | DRILL | Point |
| 2 | ACTIVE SEISMIC | Line |
| 3 | ROVER TRAVERSE | Line |
| 4 | PENETROMETER | Point |
| 5 | GPR | Line |

Each carries a `stats: [power, time, crew]` triple that feeds TOOL STATS.

`DASH.rack`'s SONAR (Area) is **not** in the set, so **Area is an unused
geometry class**. The renderer should still understand it — it is one string in
a data table, and an area tool is an obvious thing for this module to grow.

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
symmetrically about the middle bed rather than lifting off the top. Tap a bed
to isolate it, tap it again or the background to collapse; keys 1–5.

**How the ghosting is done, and why it has to be done that way** (fixed in
rev. 2026-09-11). The obvious implementation — paint every bed in depth order
with `globalAlpha = 0.3` on the unselected ones — is wrong in two compounding
ways: stacked ghosts **accumulate**, so four translucent beds behind each other
come out nearly opaque and the "ghost" is only really transparent at the edges;
and painting the focus bed last to keep it solid breaks the 3D order, so beds
that are genuinely in front of it appear behind.

The fix, which the implementation must carry across:

1. `paintLayer(ctx, k)` paints one bed **fully opaque** into *any* context.
2. The beds split into two groups: **below** the focus (painted first) and
   **above** it (painted last). Each group is rendered opaque into an
   **offscreen buffer** and composited **once** at `GHOST = 0.3`.
3. The focus bed is painted opaque straight to the target, between the two
   composites.

So ghosts never accumulate — a group is one flat 30% image however many beds
are in it — and the true paint order survives: beds above the focus really are
above it, and you see the focus *through* them.

Two details that matter downstream:

- **`paintLayer` binds its `path` helper to the context it was handed.** The
  pre-revision code closed over the outer context, which is exactly why
  drawing into a buffer was not possible before. Anything we add to this
  renderer — fog above all — must take its context as an argument and never
  close over one.
- **`setCanvasFactory(f)`** is exported so a host without `document` can supply
  the buffers. That is the seam the eventual C++ port uses for render targets,
  and the one a headless harness uses.

> **Isolate is the only verb.** The dashboard's blurb offers two — "take off
> everything above a layer" (*peel*) and "one layer only lifts the chosen bed
> out on its own" (*isolate*). Peel is **cut** (§8 decision 3). `Holo3D`'s
> explode-and-isolate is exactly right and needs nothing added; the current
> prototype's peel is retired, and the blurb becomes one line about isolating.

**HUD** (`Holo3D.drawHud`) — four elements, all *projected*, so they turn with
the block:

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

> **The scan wavefront is gone** (rev. 2026-09-11). It was a glowing line that
> crossed the top surface from the far corner, spilled over the near edges and
> slid down the walls — handsome, and constantly moving whether or not anything
> was happening, which is the problem with idle animation on an instrument: it
> spends the eye's attention on nothing. Do not port it back as decoration. A
> travelling wavefront is, however, exactly the right shape for a *survey event*
> — a shot fired, a pass completed — and if the module ever wants one, it comes
> back attached to something that happened.

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

### 4.7 The module bar — horizontal, along the top

The unit's module selector (Prospecting · Excavation · Beneficiation ·
Operations · Directives) **moves out of the left column and becomes a
horizontal bar across the top of the console**. Prospecting is one of five
modules and the console is what one of them looks like; the bar is how you
leave it.

It sits above the five panels, spanning the full 1536, in the console's own
chrome — a row of tabs in the bracket idiom, the active one carrying the
accent rule. It is the only element that belongs to the *unit* rather than to
prospecting, and it is the same bar every other module will eventually hang
under, which is why it runs the full width rather than sitting inside a panel.

The five panel rects in §2 shift down by the bar's height; nothing else moves.

### 4.8 The phone

The console is 3:2 and a phone is not. The answer is **not** a reflow of five
panels into one column — it is that on a phone **the block is the app** and
everything else is summoned.

| | Phone |
|---|---|
| **block** | full-bleed, the whole first screen. Drag to turn, tap a bed to isolate, tap the ground to drill. Confidence bar under it |
| **module bar** | stays at the top, horizontal, scrolled with the page |
| **survey tools rack** | **closed by default.** A labelled button opens it as a drawer over the block; picking a tool closes it |
| **drill bar** | **appears on top when there is drilling to do** — it opens over the block the moment the rig is collared, because that is when it stops being a readout and becomes the depth control, and it closes when the hole is finished |
| **survey log** | **below the block**, reached by scrolling down. Not an overlay: it is the thing you read between holes |
| **tool stats / drill stats** | fold into the drawer and the drill-bar overlay respectively |

Two consequences for the implementation, and they are why this decision had to
be made before any code:

1. **Panel rects are a function, not constants.** A `Layout(width)` returns the
   five regions for the current breakpoint. Nothing may hard-code 1536 × 1024
   geometry, including the rack's internal `G` table, which is already scaled
   in `DASH.layout` and must stay that way.
2. **Two panels become transient.** The rack and the drill bar need an
   open/closed state with an animation, on desktop as well as phone, or the
   phone build forks from the desktop one. One codebase, one state, two
   layouts.

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

**The drill keeps its own design inside the new one.** It is not restyled to
the console, not redrawn in line art, not re-proportioned to the hologram. It
is a machine, drawn as a machine, standing in a holographic readout — and that
contrast is the same one the rack makes against the panels. Its geometry, its
pixel grid, its two steels and its green/cyan state colours all come across
unchanged.

The only thing to check is that the rig reads against navy as well as it read
against near-black, and that is a thing to **render and look at**, not to
assert. If it does not, the fix is the console's ground behind it, never the
rig.

---

## 6. Where every existing mechanic lands

This is the table the implementation is written against. Nothing in the left
column changes; only its address does.

| Mechanic (today) | New home |
|---|---|
| four strata as one cut body | the `Holo3D` block, unchanged in meaning |
| procedural interfaces (fbm, conformity, dip, dip fan, 28 × 28 lattice) | **stays ours.** `Holo3D`'s six measured profiles are a *look*, not a model — our height fields feed `model.pts` instead |
| truth + error, the estimate that re-fits with each hole | unchanged; the block simply redraws |
| fog of war (unknown body, haze, `destination-in` mask) | **re-expressed as unresolved wireframe** — §8 decision 2. The mechanic is unchanged; the painting technique is retired |
| knowledge field `KnowAt(i,j,m)`, per-hole near term + floor | unchanged, no UI |
| delineation (mean confidence, INFERRED/INDICATED/MEASURED) | the **CONFIDENCE** bar under the block |
| the 95% gate on layer controls | gates **isolate**; the rack and the drill stay live |
| peel (click a layer, everything above comes off) | **CUT** — §8 decision 3. Owes a graveyard record when it goes |
| solo / one-layer-only | LAYERS — `Holo3D`'s explode-and-isolate, the module's only take-it-apart verb |
| the borehole bar as depth readout **and** depth control | DRILL BAR, unchanged |
| arm → spud → set depth → cut → trip out → bore marker | unchanged; the cursor and rig live over the block |
| scour bowl, spoil rim, ejecta | unchanged, on the block's top surface |
| craters, the sect dome, built ground | unchanged, on the top surface |
| real LOLA ground + the site card | the bench overlay, `?bench=1` — §8 decision 6 |
| the 37-lever board + Copy values / Roll ground / Reset | the bench overlay, `?bench=1` — §8 decision 6 |
| the tool rail (5 buttons, frames that open left) | **replaced** by the rack, which holds the same five tools. Its four non-drill pixel sprites are replaced by the rack's line art and owe a graveyard record |
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
3. **Peel is gone.** Looking *down into* the block to a chosen depth was a
   real reading of the ground and it is being traded for one clean verb. If
   the module later wants a depth-slice view, it comes back as its own thing,
   not as a second meaning for tapping a bed.
4. **The phone build is a second layout, not a narrower one.** §4.8 — two
   panels become transient, and that has to be in the code from the first
   commit rather than retrofitted.
5. **Isolation costs two full-size offscreen canvases.** They are allocated
   lazily on the first isolate, cached on the model, and rebuilt whenever the
   target canvas resizes. At 1536 x 1024 and dpr 1.5 that is about 28 MB of
   backing store, which is nothing on a desktop and not nothing on a phone.
   Two mitigations are available and neither is in the source yet: size the
   buffers to the **block's region** rather than the whole canvas, and reuse
   **one** buffer for both groups by compositing it twice. Measure before
   choosing.
6. **Two palettes now live in the project,** with a boundary: prospecting and
   excavation are the console; everything else stays Dark Plating. §8
   decision 7. The boundary has to be *held* — each new panel is a chance to
   leak cyan into the rest of the game.

---

## 8. Decisions — settled

All seven, with what each one commits the implementation to. **Decided
2026-09-11.**

### 1. Which five tools → all five, the survey set

DRILL (Point) · ACTIVE SEISMIC (Line) · ROVER TRAVERSE (Line) ·
PENETROMETER (Point) · GPR (Line). Five bays, five tools, no empty bay.

*Entails:* four icons beyond the drill's, in the rack's thin line-art style,
not the pixel sprites built for the old rail — those four are replaced and owe
a graveyard record. **Area** is left as an understood-but-unused geometry
class; SONAR is not in the set.

### 2. Fog → the console draws only what it has measured

**Known volume** gets its bed colours, fills and mesh. **Unknown volume** gets
no fill and no boundaries at all — bare grid on the panel ground, carrying a
faint noise crawl so it reads as *live but unresolved* rather than *not
loaded*. Boundaries appear where knowledge crosses the threshold, and a bed's
colour arrives with them.

*Entails:* the mechanic is untouched — fog is still exactly `1 − KnowAt`, and
under it the ground still has no type and no boundaries. What is retired is the
*painting*: the haze raster, the coarse/fine split, `destination-in` masking,
and the `fogGrit` / `fogRough` levers. It should come out **cheaper** than
today, because the unknown case draws less rather than more. The one thing to
watch is the first impression at zero holes, which is now a wire box.

*Later experiment, not in scope:* drawing beds everywhere with their boundaries
as **error envelopes** — thin where confident, a wide band where not — which
would render the truth+error model directly. It contradicts "no layers under
fog", so it is a separate decision if it is ever wanted.

### 3. Peel or isolate → isolate only

Tapping a bed explodes the stack and leaves that bed solid with the others
ghosted. Tapping again or the background collapses it. No peel.

*Entails:* `Holo3D` needs nothing added. The current prototype's peel
(`st.sel` with `solo` off, drawing beds L…4) is **removed** and owes a
graveyard record. The 95% delineation gate now gates one verb. The LAYERS
blurb becomes one line.

### 4. The phone → the block is the app

Full-bleed block; module bar at the top; the rack behind a labelled button;
the drill bar opening over the block when there is drilling to do; the log
below, on scroll. Spelled out in §4.8.

*Entails:* panel rects become a `Layout(width)` function from the first
commit, and the rack and drill bar get an open/closed state with an animation
on **both** layouts so the builds do not fork.

### 5. Drill telemetry → derived now, wired properly as the rig grows

Rotary speed, load, temperature, bit wear and vibration are **derived from
what already exists** — rate of penetration against the bed being cut, string
length against depth, time under load — and nothing is invented to fill the
panel. They are wired to real rig state as that state comes into being; the
panel is built so that swapping a derived value for a real one is one line.

*Entails:* no new mechanic now, and no fake numbers either. Five small
functions returning 0–1, mapped to the `g/y/o/r` letter code, each with a
comment naming what it is standing in for.

*And, restated because it governs everything:* **the drill's own geometry is
the design already built on this branch** — the pixel glyph, the cursor, the
planted rig and the borehole-bar rig. It is *implemented and integrated into*
the console while keeping its own design. See §5.

### 6. The bench → kept, behind `?bench=1`, off by default

This page is now **the game view**, not a tuning bench, so the instruments go
behind a flag rather than into a panel. Glossary, since these are prototype
artefacts rather than design:

| Instrument | What it is |
|---|---|
| **the 37 levers** | The slider rail in `layer-block.html`, in 8 groups — *Interface shape* (relief, feature size, detail, conformity, depth damping, dip, dip direction, dip fan, grain), *Impact craters* (how many, size, depth, rim, reaches up), *Fog* (opacity, granularity, roughness, detail, spread across, spread down), *Strata* (each bed's thickness), *Body* (rotate, tilt, lattice, depth scale), *Light & line*, *Real ground*, *Layer*. They drive the **generator** — they are how the block's look was tuned. A player never sets "Dip fan" |
| **Copy values** | A button that dumps the whole lever set as text, so a setting worth keeping can be pasted back into the code as the new defaults. Siblings: *Roll ground* (re-seed) and *Reset* |
| **the LOLA site card** | The *Real ground* card. Reads NASA's LOLA LDEM_16 — the elevation model the game already ships, 1.9 km/px — from two baked 182 km patches (Tycho and Mare Serenitatis), shows a hillshaded locator you can click to move the block, and drives the block's large-scale shape from **measured** elevation instead of the generator. Off by default, because at 6 km across the DEM is only about three samples wide |

*Entails:* one overlay, one query flag, no panel space, and the generator keeps
its lever-driven seam so the bench still works.

### 7. Scope of the look → prospecting and excavation, and no further yet

The console is the look for **the block and the modules that read it** —
prospecting and excavation. The **rack's style is shared** wherever a rack
appears, though its contents change per module. Nothing else adopts it yet:
the sect view, the terrain, and the other module panels stay Dark Plating.

*Entails:* Dark Plating gains a scoped chapter rather than being replaced, and
the boundary is a rule someone has to hold — see
[`../graphics/dark-plating.md` §10b](../graphics/dark-plating.md).

---

## 9. Related

- [`survey-dashboard-implementation.md`](survey-dashboard-implementation.md) — the build plan
- [`block-model-design.md`](block-model-design.md) — the loop this presents
- [`confidence-system.md`](confidence-system.md) — what CONFIDENCE means
- [`../graphics/dark-plating.md`](../graphics/dark-plating.md) — the style this diverges from
