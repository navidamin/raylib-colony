# Survey Dashboard — implementation plan

Companion to [`survey-dashboard-design.md`](survey-dashboard-design.md).
**Nothing here has been built.** This is the order of work, what each step
touches, and how each one is proved before the next begins.

**All seven design decisions are settled** (design doc §8), so no stage is
waiting on an answer. Three of them changed this plan: the phone layout moved
work *into* Stage 0, fog is now specified rather than open, and peel is cut.

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

The game port (`DrawProspectingPanel` in `src/Engine/rendermanager.cpp`) is
**not in this plan**. It comes after the prototype is settled, as its own pass.

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
- Isolate is `Holo3D`'s existing explode-and-ghost, wired to our `st.sel` and
  refusing below 95% with the existing `lockFlash`.
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
| `isolate.js` | 5 | five isolates, collapse, the gate |
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
