# Survey Dashboard — implementation plan

Companion to [`survey-dashboard-design.md`](survey-dashboard-design.md).
**Nothing here has been built.** This is the order of work, what each step
touches, and how each one is proved before the next begins.

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

### Stage 0 — the page, the frame, nothing else

**Goal:** an empty console: ground, five bracket panels, two connectors, five
titles and underlines, at 1536 × 1024.

**Work.** New page. Lift `Dashboard`'s primitives verbatim: `rpoly`, `rrect`,
`font`, `label`, `title`, `panel`, `connector`, `segBar`, and the `C` palette.
Lift `ToolRack.util` for `condensed`. Fonts: add **Inter** alongside JetBrains
Mono. Set up the two-canvas pattern the dashboard uses — a cached static frame,
redrawn only on state change, with the live layer composited over it each
frame.

**Verify.** Screenshot against `dashboard.png`; the five rects and the two
connectors should land within a pixel or two.

**Risk:** none. This is the cheapest stage and it establishes the harness.

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

**Work.** Decision 2 in the design doc has to be settled *before* this stage.
The proposal to test first: **the console draws only what it has measured** —
the known volume gets its beds, colours and fill; the unknown volume gets bare
wireframe on the ground colour, with a slow noise crawl, and no bed boundaries
at all. That inverts today's paint-a-haze-over-it approach and suits both the
idiom and the fiction.

Mechanically nothing changes: `KnowAt` still drives it, and fog is still
exactly `1 − known`.

The existing fog raster (coarse/fine split, `fogGrit`, `fogRough`,
`destination-in` masking) is a *painting* technique and probably does not
survive. Whatever replaces it must still be cheap: fog is the largest area on
screen.

**Verify.** Zero holes (all wireframe) → three holes → nine holes (nearly
solid), at two yaws. Frame time at zero holes, which is the worst case.

**Risk:** high, and it is a *design* risk rather than a technical one. Budget
a render-and-look loop, not an afternoon.

---

### Stage 4 — the rack

**Goal:** SURVEY TOOLS, live, replacing the tool rail built on this branch.

**Work.** Lift `ToolRack` whole — it is self-contained, has its own palette and
its own `G` layout table, and it is the most finished piece of the mock. Wire
`hitRack` and the 260 ms `crossfade` toggle. Feed it from our tool table.

Our DRILL icon is the pixel glyph and is drawn by our own code inside the
rack's icon box; the other three are the rack's own line art.

**Depends on decision 1** (which five tools). Until it is settled, build with
`DASH.rack`'s four plus the empty bay.

**Retires:** `DrawToolRail`, `StepToolRail`, `TOOLS`, `ToolLabelW`, the
opening-frame animation, and the four non-drill sprite tables
(`SEIS_ROWS`, `ROVER_ROWS`, `PEN_ROWS`, `GPR_ROWS`). If the rack's line-art
icons win, those four sprites owe a graveyard record.

**Verify.** All three states per bay, the empty bay, a toggle mid-crossfade,
and the hit boxes.

---

### Stage 5 — the ruler, the confidence bar, and peel

**Goal:** the depth ruler right of the block with its leader dots; CONFIDENCE
under it; peel and isolate both working, both gated at 95%.

**Work.**
- Ruler: five boundaries, label + name, dashed leader to a dot on the block's
  right edge. The dot's position is projected, so it follows the rotation.
- CONFIDENCE: `segBar` over `Delineation()`, on the health ramp.
- **Peel** is new to `Holo3D`: hide beds above the chosen one and drop the
  chosen one's top surface to translucent. `render`'s paint order already
  takes an explicit bed list, so peel is a filter on that list plus an alpha,
  not a new pass.
- Isolate is `Holo3D`'s existing explode.
- Both refuse below 95% with the existing `lockFlash`.

**Verify.** Peel each of the five beds; isolate each; the gate before and after
95%.

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
- Telemetry (decision 5): derive the five bars from what exists — rate of
  penetration against the bed's hardness, depth against the string, the bed
  being cut. Five one-line functions returning 0–1, mapped to the `g/y/o/r`
  letter code. **No new mechanic.**
- Log: an append-only ring of entries generated by real events — a hole
  completed, a bed's confidence crossing a tier, a crater found, the gate
  opening. Layer tags and resource chips as the mock draws them.
- TOOL STATS: three bars off the selected tool's `stats` triple.

**Verify.** Drill three holes and read the log back; each entry must name a
thing that actually happened.

---

### Stage 8 — the bench, and the phone

**Goal:** the development instruments survive, and the console is usable on a
phone.

**Work.**
- Bench (decision 6): the ~30 levers, Copy values and the real-ground site card
  behind `?bench=1`, drawn as an overlay. Off by default.
- Phone (decision 4): the recommended shape is block full-bleed, rack and bar
  in edge drawers. This is its own design pass and it should not be squeezed
  into the tail of this one.

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

**Fog in a line-art world** — the only piece with no obvious answer. Settle the
look on a throwaway page before touching the console.

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
| `peel.js` | 5 | five peels, five isolates, the gate |
| `bar.js` | 6 | four rig states |
| `gate.js` | every stage | 9 holes → 95% → gate opens → peel works |
| `perf.js` | every stage | frame time median / p90 |

All of them: `NODE_PATH=/opt/node22/lib/node_modules` and
`executablePath: '/opt/pw-browsers/chromium'`.

---

## 5. Risk register

| Risk | Likelihood | If it happens |
|---|---|---|
| the rig looks wrong at extreme pitch | medium | clamp pitch while armed |
| 28 × 28 too slow through per-cell shading | medium-high | decimate the drawn surface; widen `fast` |
| fog has no good line-art form | medium | fall back to a translucent unknown body over the beds, as today |
| the console is unusable on a phone | high if unaddressed | stage 8 is a real design pass, not a media query |
| the drill's palette fights the navy ground | low | render and look before retinting anything |
| two palettes fork the project's art direction | certain unless decided | decision 7, before stage 4 |

---

## 6. Not in this plan

- The C++ port. After the prototype settles.
- New mechanics of any kind. The telemetry is derived; the log reports; the
  rack's fifth bay stays empty.
- Any change to the drill.
