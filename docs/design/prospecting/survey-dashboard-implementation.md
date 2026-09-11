# Survey Dashboard — implementation plan

Companion to [`survey-dashboard-design.md`](survey-dashboard-design.md).
This is the order of work, what each step touches, and how each one is proved
before the next begins.

**Built so far: stages 0, 1, 2 and 3.** Each stage carries an *As built* note
where what happened differed from what was planned; where there is no note,
the stage went as written.

**All seven design decisions are settled** (design doc §8), so no stage is
waiting on an answer. Three of them changed this plan: the phone layout moved
work *into* Stage 0, fog is now specified rather than open, and peel is cut.

**Source revision 2026-09-11.** `Holo3D` changed in two ways and the vendored
copies are updated: the **scan wavefront is removed**, and **bed isolation is
fixed** by compositing ghost groups through offscreen buffers. The second one
reaches into Stages 1, 3 and 5 and into the risk register — see below.

---

## 0. The shape of the job

We are not building the dashboard. We are **moving a working simulation into
it**. `dashboard.html` is a mock: its block is live but has no fog, no holes
and no model; its drill bar holds a placeholder rig; its log and telemetry are
literals in a config object. Everything that makes prospecting *work* lives in
`layer-block.html` (≈3,100 lines) and none of it is in the mock.

So the job is a **transplant, not a rewrite**, and it runs in that direction:
the console becomes the host, and the simulation moves in one organ at a time,
with the patient rendering after every step.

### Target

`docs/design/prospecting/prototypes/survey-dashboard.html` — one new
self-contained page, built alongside `layer-block.html`, which is left intact
until the new page does everything it does. Two prototypes for a while is the
cheap way to do this; deleting the old one early is the expensive way.

The game port has its own plan, in §7 below. The prototype stages 0-3 are
done and **the port has started from them**: the deliverable is raylib C++ in
`src/`, and the HTML is a bench for settling the look cheaply, never the
product. Stages 4-9 of the prototype are now superseded by the port's own
stages -- the remaining panels are built directly in C++.

### The rule for every stage

Render it, look at the PNG, then move on. `tools/preview` does not cover a
design prototype, so the harness is Playwright against the page — the same
scripts this branch has been using (`gate.js`, `rig*.js`, `rail.js`, `bar*.js`
in the session scratchpad), updated per stage. Frame time is checked at every
stage that adds drawing, against the standing budget of **16.7 ms median**.

---

## 1. Ground rules

1. **The drill is untouchable.** `DrawBitSprite`, `DrawShaft`, `DrawGlyphRing`,
   `DrawGlyphTrack`, `DrawDrillGlyph`, `DrawDrill`, `drawHead`, `drawThread`,
   `drawShaft` and their constants move across **byte for byte**. If one has to
   change to fit the new camera, that is a finding to report, not a licence.
2. **Mechanics move whole.** `KnowAt`, `Confidence`, `Delineation`,
   `BuildHeights`, the truth+error model, `StepDrill`, the spatter, the scour,
   the craters, the dome, `OnBuiltGround` — copied, not reinterpreted. Any
   behaviour change is a separate commit with its own reason.
3. **One organ per commit**, each rendering.
4. **The config object is the seam.** Follow `DASH`: the renderer reads a plain
   object and nothing else. Our state fills that object each frame. That is
   what makes the eventual C++ port a translation rather than an
   archaeological dig.
5. **Every removal that was once reachable owes a graveyard record** in the
   same commit — the rock textures above all.

---

## 2. Stages

### Stage 0 — the page, the frame, and the layout function

**Goal:** an empty console: ground, module bar, five bracket panels, two
connectors, five titles — **at two breakpoints**, with two of the panels able
to open and close.

**Work.** New page. Lift `Dashboard`'s primitives verbatim: `rpoly`, `rrect`,
`font`, `label`, `title`, `panel`, `connector`, `segBar`, and the `C` palette.
Lift `ToolRack.util` for `condensed`. Fonts: add **Inter** alongside JetBrains
Mono. Two-canvas pattern as the dashboard has it — a cached static frame,
redrawn on state change, with the live layer composited over it each frame.

Three things that are **not** in the mock and must be here from the first
commit, because retrofitting any of them is a rewrite:

1. **`Layout(width)` returns the five regions**, plus the module bar. Desktop
   returns `DASH.layout`'s rects shifted down by the bar. Phone returns the
   §4.8 arrangement. **No geometry is a constant**, including the rack's
   internal `G` table — which `DASH.layout` already scales, and which must
   stay scaled.
2. **The rack and the drill bar are transient panels** — an `open` flag and an
   eased offset, on desktop as well as phone. On desktop they happen to be
   always-open; the state exists anyway, so the two builds never fork.
3. **The module bar**, horizontal, full width, five tabs, Prospecting active.

**Verify.** Screenshot the desktop breakpoint against `dashboard.png` — the
five rects and the two connectors within a pixel or two. Then the phone
breakpoint at 390 × 844: block full-bleed, bar on top, rack closed, log below
the fold.

**Risk:** low, but this is where the phone decision is either honoured or
quietly lost. If Stage 1 starts against hard-coded rects, Stage 8 becomes a
rewrite.

---

### Stage 1 — the block, geometrically ours

**Goal:** our height fields rendered by `Holo3D`'s camera and passes, in the
LAYERS panel, rotatable by drag.

**Work.**
- Lift `Holo3D` whole (`build`, `camera`, `render`, `hull`, `hit`, `attach`).
- **Replace `build`'s profile-derived height fields with ours.** Our
  `SURF[L][j * STRIDE + i]` is metres of depth on an `N × N` lattice; `Holo3D`
  wants `pts[k][i][j] = [x, y, z]` in model units. One adapter function, and
  the six `PROFILES` are deleted with it.
- Reconcile the lattice: ours is 28 × 28 (`P.lattice`), `Holo3D` renders
  16 × 13. 28 × 28 through `Holo3D`'s per-cell top shading is 784 fills per
  visible bed per frame — measure before assuming it is fine, and if it is
  not, render the *surface* at a decimated grid while the *model* stays at 28.
- Keep our `BuildHeights`, morph blend and `Reproject` intact: they produce
  the field, `Holo3D` consumes it.
- **Honour `paintLayer`'s contract from the first line of this stage.** As of
  the 2026-09-11 revision every bed is painted by `paintLayer(ctx, k)` into a
  context it is *handed* — the on-screen one, or one of two offscreen ghost
  buffers. Anything drawn per bed therefore takes its context as an argument
  and never closes over one. Our own prototype does the opposite: `CTX` is a
  module-level `let` that `OnLayer` swaps around. That pattern is *compatible*
  but it is the single easiest way to break the ghosting without seeing it
  break — a stray draw into the wrong buffer lands off-screen and silently
  vanishes. Port the layered drawing to explicit-context functions here, not
  later.
- Call `Holo3D.setCanvasFactory` so the buffers come from wherever this build
  gets canvases — that seam is also what the C++ port will use for render
  targets.

**Verify.** Drag through a full turn and a full pitch sweep; the beds must stay
registered (no bed crossing another). Frame time at 28 × 28 and at 16 × 13.

**Risks.** (a) grid cost; (b) our lattice is square and `Holo3D`'s is not —
the wall samplers assume `NX` along two edges and `NZ` along the other two,
so `NX = NZ = N` must be checked through, not assumed.

---

### Stage 2 — the drill, upright, on the new camera

**Goal:** arm, spud, set a depth, cut, trip out — over the 3D block, at any
yaw and pitch.

**Work.** This is the stage the whole plan is arranged around, so the analysis
is here rather than in the code.

Our drill is drawn **in screen space, vertically**, on the assumption that
world-vertical is screen-vertical. Under `Holo3D`'s camera that assumption
**still holds**: for two points differing only in world *y*, projected *x* is
identical and projected *y* differs by `−Δy · cos(pitch) · zoom`. So the rig,
the string, the tail and the bore all stay upright. Nothing needs re-drawing.

What does change:

- **Depth is no longer a constant number of pixels per metre.** Today
  `PX_PER_M` is a lever. It becomes `D · cos(pitch) · zoom / COLUMN_M`, and
  every consumer — `YOf`, `DepthAtY`, `SpudM`, `BIT_PX`, the sink travel, the
  scour quantisation — has to read it as a function, not a constant.
- **The rig's own scale should not follow pitch.** The bit sprite is pixel art
  on a whole-pixel grid; scaling it by `cos(pitch)` destroys it. So: the rig
  draws at fixed size, and only its *travel* foreshortens. State this in the
  code, because it is exactly the kind of thing that looks like a bug later.
- **Placing a hole** now needs a ray from the pointer to the top surface
  rather than `PickCell` on a fixed iso lattice. `Holo3D.hit` already does
  polygon hit-testing against the last frame's projected cells — the same
  record gives us `(i, j)` if the per-cell polygons are stored with their grid
  indices.
- **`CanPlace`** currently uses `CTX.isPointInPath` on the surface path. The
  per-cell hit record replaces it, and `OnBuiltGround` still refuses the dome.
- **Spatter** picks velocities in screen px and divides into per-axis units.
  Those units are now camera-dependent; the same fix as `PX_PER_M`.

**Verify.** The full drill cycle rendered at pitch 0.2, 0.45 and 1.2, and at
four yaws — bit lands where the pointer was, string stays vertical, tail
shortens, ejecta arcs outward not upward. Then the standing gate regression.

**Risk:** highest in the plan. If the rig cannot be made to look right at
extreme pitch, the fallback is to **clamp pitch** while the drill is armed —
cheap, defensible, and worth saying now so it is not discovered as a crisis.

---

### Stage 3 — fog, re-expressed

**Goal:** the unknown part of the block reads as unknown, in the console's
idiom.

**Work.** Settled: **the console draws only what it has measured.** Known
volume gets its bed colours, fills and mesh; unknown volume gets **no fill and
no boundaries** — bare grid on the panel ground with a faint noise crawl, so it
reads as live but unresolved rather than not loaded.

Mechanically nothing changes: `KnowAt` still drives it, fog is still exactly
`1 − known`, and under it the ground still has no type and no boundaries.

Shape of the work:

- Knowledge becomes a **per-cell and per-boundary-vertex** quantity the
  renderer reads, rather than a raster it paints. `Holo3D`'s passes already
  walk cells and edge samples, so each one gains one lookup and a branch.
- **All of it lives inside `paintLayer`**, taking its context as an argument.
  Fog drawn outside that function would appear on the on-screen block but not
  inside the ghost buffers, so an isolated view would show unfogged ghosts —
  a bug that looks like a fog bug and is not one.
- A boundary segment draws only where both its endpoints are above threshold,
  and **fades in** across it rather than popping.
- The noise crawl is one cheap scrolling pattern over the unknown grid, not a
  per-cell computation.
- **Retired:** the haze raster, the coarse/fine split, `destination-in`
  masking, and the `fogGrit` / `fogRough` levers.

This should come out *cheaper* than today — the unknown case draws less rather
than more.

**Verify.** Zero holes (all wireframe) → three → nine (nearly solid), at two
yaws and two pitches. Frame time at **nine** holes, which is now the worst case
rather than zero. And look hard at zero holes: the first thing a new player
sees is a wire box, and it has to read as unmeasured ground rather than as a
page that failed to load.

**Risk:** medium. The mechanism is clear; the judgement calls are the threshold
and the fade width, and both are render-and-look, not arithmetic.

**As built.** Three things.

*The bed painter is ours now.* Fog has to live inside the per-bed painter, and
that painter was inside the vendored `Holo3D.render`. Rather than fork a file
whose whole value is that re-vendoring it is a copy, `RenderBlock` was written
in our section, derived from its `render` and stamping the same things onto the
model (`cam`, `hits`, `bounds`, `hull`, `offY`, `alphaOf`) so `hit` and
`drawHud` cannot tell the difference. Holo3D keeps the camera, the hit test and
the HUD, byte for byte.

*A fogged wall is drawn in strips, one per lattice column.* Not by choice —
the per-column alpha ramp cannot be a gradient (the wall's fill gradient is
already spoken for: vertical, neon→mid→deep) and it cannot be an erase either.
`destination-out` on the console canvas would punch through the panel behind
the block, and on a ghost buffer it would erase the beds already painted into
it. A wall that is known the whole way across still takes the single-polygon
path and draws exactly what it drew before. Two translucent quads that abut
exactly leave a hairline, so fogged quads are grown a third of a pixel about
their own centre; rendered at four cameras, no seams.

*The wire cage was the pleasant surprise.* The risk register worried that a
bare wireframe would read as a broken page. It does not, because **the surface
is known for free** — an undrilled block is the real terrain standing on a wire
volume, and it reads immediately as unmeasured ground. No floor was needed.

**Measured.** Median frame interval, 1536 × 1044, dpr 1.5, 28 × 28 lattice:

| | still | dragging |
|---|---|---|
| 0 holes (all cage) | 16.7 | 16.7 |
| 1 hole | — | 21.5 |
| 3 holes | — | 23.9 |
| 9 holes (MEASURED) | 16.6 | 22.5 |

The prediction was half right. The unknown end **is** cheaper — an undrilled
block is the vsync floor whether or not you are dragging it. The worst case is
not nine holes and not zero but **three**, where nothing is uniform enough to
take the single-polygon path and every wall is drawn in strips: 23.9 ms, about
4 ms over stage 2's dragging number. Still is still free at every hole count,
which is what the console mostly is.

**Verified.** 0 / 1 / 2 / 3 / 9 holes at four cameras each; isolate with fog on
it (the focus solid, the other four ghosted uniformly, the cage faded out with
explode); the full drill cycle end to end, pointer to reveal, with delineation
going 0 → 0.396 and the beds visibly re-fitting; the stage-0, 1 and 2
regressions and the phone layout, all clean. Harness:
`prototypes/survey-fog-shots.js`.

---

### Stage 4 — the rack

**Goal:** SURVEY TOOLS, live, replacing the tool rail built on this branch.

**Work.** Lift `ToolRack` whole — it is self-contained, has its own palette and
its own `G` layout table, and it is the most finished piece of the mock. Wire
`hitRack` and the 260 ms `crossfade` toggle. Feed it from our tool table.

Our DRILL icon is the pixel glyph and is drawn by our own code inside the
rack's icon box; the other three are the rack's own line art.

**Five bays, five tools:** DRILL (Point), ACTIVE SEISMIC (Line), ROVER
TRAVERSE (Line), PENETROMETER (Point), GPR (Line). No empty bay — but keep the
empty-bay dress in the renderer, since the tool list is data.

Four new icons in the rack's thin line-art style. The DRILL icon is our pixel
glyph, drawn by our own code inside the rack's icon box.

**Retires:** `DrawToolRail`, `StepToolRail`, `TOOLS`, `ToolLabelW`, the
opening-frame animation, and the four non-drill sprite tables (`SEIS_ROWS`,
`ROVER_ROWS`, `PEN_ROWS`, `GPR_ROWS`) — those four owe a graveyard record.

**Also here:** the rack is a **transient panel** (Stage 0), so this stage wires
its open/close on the phone breakpoint — the labelled button, the drawer, and
closing on pick.

**Verify.** Three states per bay, the empty-bay dress on a stub, a toggle
mid-crossfade, the hit boxes, and the drawer opening and closing on the phone
breakpoint.

---

### Stage 5 — the ruler, the confidence bar, and isolate

**Goal:** the depth ruler right of the block with its leader dots; CONFIDENCE
under it; isolate working and gated at 95%.

**Work.**
- Ruler: five boundaries, label + name, dashed leader to a dot on the block's
  right edge. The dot's position is projected, so it follows the rotation.
- CONFIDENCE: `segBar` over `Delineation()`, on the health ramp.
- Isolate is `Holo3D`'s existing explode-and-ghost — **as revised**: ghost
  groups composited once through offscreen buffers, the focus bed painted
  opaque between them. Wired to our `st.sel`, refusing below 95% with the
  existing `lockFlash`.
- **Size the buffers here.** They arrive full-canvas; measure, and if the
  phone build minds, either cut them to the block's region or reuse one buffer
  twice. This is the stage that first allocates them, so it is the stage that
  owns the decision.
- **Peel is removed**, not ported: the `botB = P.solo ? topB + 1 : 4` branch
  in `layer-block.html` goes, along with the solo lever, since isolate is now
  the only behaviour. Graveyard record in the same commit.
- The LAYERS blurb becomes one line about isolating.

**Verify.** Isolate each of the five beds and collapse; the gate before and
after 95%; and that nothing anywhere still offers to take the lid off.

---

### Stage 6 — the drill bar, ours

**Goal:** DRILL BAR with our rig in it, on the console's strata bands.

**Work.** Drop `roughDrill`. Move our `drawShaft` / `drawThread` / `drawHead`
and the strip rock across, restyled to `C.strata` (five flat bands) rather than
the ported rock tiles. The rig itself does not change. The bar keeps its second
job — it is still the depth **control** once the rig is collared, so the
flashing cadre and `DepthAtY` come with it.

**Verify.** Depth 0, mid-cut, trip-out and the awaiting-depth flash.

---

### Stage 7 — the telemetry and the log

**Goal:** DRILL STATS and the survey log carrying real values.

**Work.**
- Telemetry: derive the five bars from what exists — rate of penetration
  against the bed being cut, string length against depth, time under load.
  Five small functions returning 0–1, mapped to the `g/y/o/r` letter code.
  **No new mechanic, and no fake numbers either**: each function carries a
  comment naming the real rig state it stands in for, and swapping a derived
  value for a real one when the rig grows that state must be a one-line change.
  That is the whole point of the seam.
- Log: an append-only ring of entries generated by real events — a hole
  completed, a bed's confidence crossing a tier, a crater found, the gate
  opening. Layer tags and resource chips as the mock draws them.
- TOOL STATS: three bars off the selected tool's `stats` triple.

**Verify.** Drill three holes and read the log back; each entry must name a
thing that actually happened.

---

### Stage 8 — the bench, and the device pass

**Goal:** the development instruments survive; the phone layout is checked on
a real phone rather than at a breakpoint.

**Work.**
- Bench: the 37 levers, Copy values / Roll ground / Reset, and the real-ground
  site card behind `?bench=1`, drawn as an overlay. Off by default. The
  generator keeps its lever-driven seam so the bench still drives it.
- The phone *layout* was built in Stage 0 and wired in Stages 4 and 6; what
  happens here is the **device pass** — the thing a breakpoint screenshot
  cannot tell you: whether the block is draggable with a thumb, whether the
  drawer button is reachable one-handed, whether the drill bar opening over
  the block is a help or a jump-scare.

**Verify.** On the device, per `docs/web-deploy-mobile.md`.

---

### Stage 9 — retire `layer-block.html`

Only once the new page does everything the old one does. Graveyard records for
the rock textures, the iso projection, the tool rail, the four sprites, and
whatever else did not make the move — one file, five questions each.

---

## 3. The three hard parts

**Camera and the upright drill** — settled above by arithmetic: world-vertical
stays screen-vertical, so the rig survives; what changes is that
metres-per-pixel becomes a function of pitch and zoom. Do this stage second,
not last: if it fails, everything after it is built on sand.

**Fog in a line-art world** — the mechanism is settled (unresolved wireframe,
faint noise crawl, boundaries fading in at threshold); what is not settled is
the threshold and the fade width, and those are render-and-look. The one thing
to get right is the zero-hole read: a wire box must say *unmeasured ground*,
not *page failed to load*.

**Ghosting through buffers** — settled by the revision, and verified by render:
with a bed isolated, the other four ghost at a uniform 0.3 with no
accumulation, and beds above the focus keep their true 3D order while the focus
shows through them. What is left is a memory question, not a correctness one.
The trap is context discipline — anything drawn per bed must take its context,
never close over one.

**Grid cost** — 28 × 28 × 5 beds through per-cell shading, plus walls, plus
fog, plus the HUD, at 60 Hz. The mock gets away with 16 × 13 and no fog. Two
levers if it is too slow: decimate the drawn surface below the model's
lattice, and extend `state.fast` (already dropping scatter and blur during
drags) to drop the top-surface scatter and the wall mesh whenever the block is
moving at all.

---

## 4. Harness

| Script | Stage | What it proves |
|---|---|---|
| `frame.js` | 0 | the five rects against `dashboard.png` |
| `block.js` | 1 | a yaw sweep and a pitch sweep, beds registered |
| `drill3d.js` | 2 | the full cycle at three pitches × four yaws |
| `fog3d.js` | 3 | 0 / 3 / 9 holes at two yaws |
| `rack.js` | 4 | three states, empty bay, mid-crossfade |
| `isolate.js` | 5 | five isolates, collapse, the gate, and **ghost uniformity** — sample the same screen point behind two, three and four stacked ghosts; the values must match, because a group is one flat composite however many beds are in it |
| `phone.js` | 0, 4, 6 | 390 x 844: block full-bleed, rack drawer, drill-bar overlay, log below the fold |
| `bar.js` | 6 | four rig states |
| `gate.js` | every stage | 9 holes → 95% → gate opens → isolate works |
| `perf.js` | every stage | frame time median / p90 |

All of them: `NODE_PATH=/opt/node22/lib/node_modules` and
`executablePath: '/opt/pw-browsers/chromium'`.

---

## 5. Risk register

| Risk | Likelihood | If it happens |
|---|---|---|
| the rig looks wrong at extreme pitch | medium | clamp pitch while armed |
| 28 × 28 too slow through per-cell shading | medium-high | decimate the drawn surface; widen `fast` |
| the ghost buffers cost too much memory on a phone | medium | cut them to the block's region, or reuse one buffer for both groups with two composites |
| a stray per-bed draw closes over the wrong context | medium, and it is invisible | every per-bed function takes its context as an argument; the isolate harness catches it by sampling ghosts |
| a block of bare wireframe reads as a broken page | medium | raise the floor: a trace of bed tint under the unknown, or start the block with a shallow band already known |
| the phone layout is honoured at the breakpoint but not in the hand | medium | stage 8's device pass exists for exactly this; do not skip it |
| the drill's palette fights the navy ground | low | render and look; if it does, change the console's ground behind it, never the rig |
| cyan leaks out of prospecting/excavation into the rest of the game | medium, and it compounds | the boundary is a rule in Dark Plating 10b, and each new panel is a chance to break it |

---

## 6. Not in this plan

- The C++ port. After the prototype settles.
- New mechanics of any kind. The telemetry is derived; the log reports; the
  four tools beyond the drill are icons and names, not behaviours.
- The four non-drill tools actually *doing* anything. The rack holds five;
  only the drill works. That is the same honesty the old rail had, now told by
  a slot that is powered rather than by a dashed tile.
- Propagating the console's look beyond prospecting and excavation.
- Any change to the drill.

---

## 7. The game port — the deliverable

**The prototype is not the product.** The module ships as raylib C++ in `src/`
like the rest of the game; the HTML console exists because a look costs five
seconds to iterate there and a rebuild here. Stages 0-3 of the prototype are
settled, so the port runs from them, and stages 4-9 are built **directly in
C++** rather than prototyped first — the questions they answer are layout and
wiring, which the game can render as fast as a browser can.

| | What lands | State |
|---|---|---|
| **C1** | `src/Survey/` — the ground, the knowledge field, the camera, the bed painter and the wire cage, drawn in `DrawProspectingPanel` in place of the four exploded plates | **done** |
| **C2** | the console frame: `ComputeSurveyLayout`, the five panels, the horizontal module bar, the existing controls rehoused | **done** |
| **C3** | the drill — glyph, cursor, planted rig, cycle, spatter, bore markers — on the block's own camera | |
| **C4** | the tool rack, five bays | |
| **C5** | the ruler, the CONFIDENCE bar, isolate gated at 95%, and the ghost compositing the flat per-bed alpha stands in for | |
| **C6** | the borehole bar, and the depth control it owns | |
| **C7** | derived telemetry and the log; retire the old panel and the prototypes, with their graveyard records | |

### C1 — as built

`src/Survey/` is shared by prospecting and excavation, because the two dig the
same rock and must never disagree about it: `survey_ground` (the height fields
and the error model), `survey_knowledge` (`KnowAt`, confidence, delineation),
`survey_camera` (the projection identity the drill will stand on),
`survey_block` (the painter and the cage) and `survey_console` (the facade that
rebuilds the ground when a hole lands or a scour deepens).

**The translation gained something.** Canvas 2D cannot put an alpha ramp along
a wall that already carries a colour gradient, so the prototype draws a fogged
wall as one quad per lattice column at a flat alpha, grown a third of a pixel
to hide the seams. `rlgl` takes a colour **per vertex**, so here the knowledge
ramp and the neon-to-deep gradient are the same interpolation: no strips, no
seams, and no hack to explain.

**The bug worth writing down.** rlgl *queues* vertices and draws them at the
next flush, but `rlDisableBackfaceCulling` sets GL state immediately. Disabling
culling, queueing the block and re-enabling it therefore draws the whole block
with culling **on** — which silently ate every wall quad whose winding came out
the wrong way round, while the cap, wound the other way, rendered perfectly.
The geometry, the mesh and the boundary lines were all in the right place and
only the fills were missing. Found by rendering and looking, not by reading.
The fix is to flush the batch on both sides of the state change.

**Interaction.** Collar by ray-marching the pointer onto the cap; choose the
depth by tapping a bed; drag to turn. A tap is a release that never became a
drag — acting on the press would collar a hole every time the block was
turned, which is the one input bug this geometry makes easy.

**Not yet, and named so:** the drill rig on the block (C3), the prescribed
line over it (it was projected through the plates' iso frame, which went with
the plates), the console chrome (C2), and ghost compositing through buffers —
isolate currently ghosts with flat per-bed alpha, which is the prototype's own
documented fallback (C5).

**Verified by render:** `tools/preview/preview.sh --module prospecting
--holes 0|1|3|9` — a new flag, because a block drawn only where it has been
measured is a wire cage until something drills it, which is correct and
useless for judging the beds.

### C2 — as built

The console takes **the whole screen** between the game's top and bottom bars.
It is not a centre panel with chrome either side: the module selector moves
into its own bar along the top, the control panel's actions move there with it,
and that is what pays for the block being twice the size it was.
`SurveyConsoleModule(unit)` decides, so every other module still draws the
three-column unit view underneath — the design's decision 7, enforced in one
predicate.

**The module bar carries the unit's actions, and the design did not say to do
that.** The browser prototype had no tier, no upgrade and no power state,
because it was not inside a game; the game's CONTROL PANEL held them, and the
console takes that column's space. They belong on the bar for the same reason
the tabs do: that row is the unit speaking, not the instrument.

**The phone breakpoint is NOT here, and the layout function says why.** In the
browser the page reflows and a breakpoint means something. The game renders one
fixed 1280 × 720 frame and scales the canvas, so `GetScreenWidth()` says 1280
on a phone and on a desktop alike, and a breakpoint would be a lie. The phone
problem is real — a 12 px label scaled to a 400 px screen is unreadable — and
the design's answer (the block is the app, the rack and the bar are summoned)
is still right. It needs the game to learn its **display** size, which is a
web-shell change, not a layout change. `ComputeSurveyLayout` returns one
arrangement and is shaped so the second one is cheap.

**Where the existing controls went:** SURVEY TOOLS takes the whole control rail
(calibration, the resource statement, the surface sweep, the auger and the
readout for the spot under the rig) — extracted into `ProsDrawRail`, which now
takes a *rectangle* instead of deriving one from the dock it used to stand
beside. DRILL BAR takes the borehole dock. LAYERS takes the block, the
CONFIDENCE bar (delineation, live, on the health ramp) and the log box. The two
stats panels name the stage that fills them.

**Two fits worth recording.** The dock's depth axis is derived from plate slots
whose spacing `MakeBlockGeom` decides, so *asking* for a height does not get
you one: the first pass ran a quarter of the column out through the bottom of
the panel and across DRILL STATS. It is fitted in two passes — measure what
pass one produced, scale the request by the ratio, re-derive. And the rail is
scissored to its panel, because it is a running column whose height depends on
what the module has found, and a control drawn outside its panel is still
clickable.

**Retired with the plates:** the dim strata bands that ran under the stack
("one ground, both panels"). The solid block *is* the ground and it is in a
different panel from the dock now, so a band between them would cross the
gutter the frame deliberately puts there.
