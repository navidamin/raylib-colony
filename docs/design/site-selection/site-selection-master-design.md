# Site Selection — Master Design

**Status: SETTLED** — simplified 2026-08-25. See Appendix A for the
model this replaced and why.
**Scope:** orbital view → the build footprint, the survey cursor, and
where resource information lives.

---

## 0. The whole design in two sentences

> **Resources belong to the region. Terrain belongs to the spot.**
>
> You pick a region for what it has. You pick a spot for whether you can
> build on it.

Everything below is consequence. If a rule here cannot be traced back to
those two sentences, it should not exist.

---

## 1. Design intent

The player should feel like a mission planner narrowing down a landing
site, not like someone clicking a tile on a grid. Two properties carry
that:

1. **Two decisions, not one.** *Which region* and *which ground* are
   different questions with different answers, decided at different
   zooms. Collapsing them into a single "pick a tile" is what makes grid
   games feel like spreadsheets.
2. **The last step is different in kind.** Everything above it is
   navigation. Placing the base is commitment, and it must *look*
   different, not merely behave differently.

An earlier draft had a third property — *information sharpens as you
descend* — and it was wrong. See Appendix A.

---

## 2. Two decisions, and the camera between them

| | Decision 1 | Decision 2 |
|---|---|---|
| **Question** | Which region? | Which ground? |
| **Scale** | 100 km — one playfield | 1.5 km — the base footprint |
| **Decided by** | resource holdings | slope, relief, illumination, PSR |
| **Data** | orbital survey, averaged over tens of km | LOLA/SLDEM, 59 m, measured |
| **Where** | orbital view (`OrbitalPickToLatLon`, `SetTerrainAnchor`) | inside a 5 km sect cell |

**Everything between the two is camera movement.** The game's existing
Planet (100 km) → Colony (25 km) → Sect (5 km) views are how the player
travels between the two decisions; they are not decisions themselves and
carry no readout of their own beyond what is already on screen.

This is deliberately *not* a new ladder. One region is one playfield —
the 20×20 grid of 5 km cells the game already has — so decision 1 is the
terrain anchor the orbital view already sets, and decision 2 is placement
inside a cell. No new levels are introduced.

**Cursor sizing rule.** Wherever a cursor is shown it stays between
**15% and 30%** of the window's smaller dimension, and where it snaps,
the window must be a whole number of cursors — otherwise the grid leaves
ground the player can see but cannot select. Below ~12% the cursor is a
dot with an unreadable label; above ~40% there is nothing left to choose
between.

**Snapping.** The cursor snaps to the 5 km cell grid while navigating and
is **free-moving** at the placement step — the whole point of the final
step is choosing *where within* the cell the base sits, and the buildable
ground may be a corner of it.

---

## 3. The survey cursor

### 3.1 Appearance (navigating)

- Translucent fill, ~15% alpha, neutral — no green/red, nothing is being
  judged yet.
- 2 px outline, corner ticks at 35% of the half-width.
- A compact readout panel anchored to the **opposite side of the window
  from the cursor**, so it never covers the ground it describes. (Found
  and fixed in the `lunar_map` prototype.)
- Panel contents: mean and max slope, relief, illumination. Terrain only
  — resource figures live on the region panel and do not follow the
  cursor (§4).

### 3.2 Appearance (placing the base)

The register change is the point. Everything here should say *building*,
not *browsing*:

- **Colour becomes a verdict.** Green = buildable, red = rejected, using
  the existing `JudgeSite` thresholds (mean slope 8°, peak 25°,
  roughness 40 m, relief 400 m, no PSR).
- **Engineering overlay instead of a plain rectangle:** corner survey
  markers, a centre stake, a dimension label ("1.5 km"), and a dashed
  outer ring showing the full 5 km sect extent the base will eventually
  occupy.
- **A ghosted footprint of the sect layout** — core dome plus the unit
  ring — drawn faintly inside the cursor, so the player sees the shape
  of what they are placing. Geometry already exists in
  `TerrainSiteDisturbance` (`coreRadiusKm`, `ringRadiusKm`, `domeCount`).
- **The readout names the blocking limit**, not just "invalid" — already
  prototyped: failing rows highlight, passing rows stay neutral.
- Cursor lag/damping: a slight smoothing on cursor motion makes it feel
  like positioning equipment rather than moving a mouse.

### 3.3 Behaviour

- Cursor follows the mouse; on touch, follows the drag and commits on
  release.
- Descend: click / tap inside the cursor. Ascend: Esc / right-click /
  back button — matching the existing `colony_viewtest` ladder controls.
- Descending re-centres the next view on the cursor centre, so the
  transition is continuous: the ground under the cursor expands to fill
  the frame. **This is what makes the zoom feel physical**, and the
  terrain chain already supports it — the same lat/lon regenerates the
  same ground at any span.

**DECIDED — reversible.** Backing out is always allowed and costs
nothing; the player can wander freely. Only the build itself is a
commitment, and it takes an explicit confirm.

Consequence for implementation: the path must be a stack, not a single
current position. Backing out restores the parent view *with its cursor
where the player left it*, so trying a neighbouring cell does not reset
everything.

---

## 4. Where resource information lives

### 4.1 The rule

Resource holdings are a property of the **region**, not of any spot
inside it. They are shown once, when the region is chosen, and they do
not change again.

This is not a simplification of the physics — it *is* the physics. A
neutron spectrometer averages hydrogen over roughly 45 km; a gamma-ray
spectrometer is not much better. There is no finer answer to give, so
the game does not pretend to have one by drawing a number that follows
the cursor.

### 4.2 How it is presented

- **Region panel.** Resource bars with a single value each. Tagged with
  the region's name. No error bars, no instrument names, no footprint
  figures. One line of copy explains why it does not move: *orbital
  surveys average over tens of km.*
- **Site panel.** Terrain only, and it updates live with the cursor.
- Both panels are on screen together during placement.

**The mechanic teaches itself in one movement: the terrain panel follows
the cursor and the region panel does not.** Nobody has to be told why.
That single observation replaces the entire apparatus described in
Appendix A.

### 4.3 What this preserves

- **The gamble at commit is intact.** You know exactly what the ground
  is and only roughly what is in it. That tension never came from the
  confidence machinery — it comes from resources being regional.
- **Prospecting is the payoff.** Landing and drilling is still the only
  way to learn local truth, which gives `docs/design/prospecting/` a
  single obvious purpose.
- **Reading terrain still informs resources.** A permanently shadowed
  crater floor still hints at volatiles — it just informs *which region*
  you pick.

### 4.4 What it gives up

- **No sense of information earned by zooming.** But that feeling was
  never real: if descending is free and always sharpens the number, it is
  a toll, not a decision. This trades an illusion for a rule learned in
  one movement.
- **All resources behave alike.** Surface mineralogy (M3, ~140 m/px)
  really is measured far finer than hydrogen, so bundling it into one
  regional figure loses something. But M3 reads only the top few microns
  and is confounded by space weathering — high spatial resolution, low
  depth reliability — so it is a poor candidate for an exception and the
  loss is small.

  **There is no exception worth making.** An earlier draft named *rock
  abundance* as the quantity to promote to the site panel. That was
  wrong: rock abundance is not a resource at all (§4.5). With it
  correctly filed as terrain, no resource needs promoting and the rule
  holds without a special case.

### 4.5 Rock abundance is terrain, not a resource

Worth stating explicitly because it is easy to get wrong. **Rock
abundance** is a real Diviner product: the areal fraction of ground
covered by rocks roughly a metre across and larger, mapped globally at
~237 m/px. It is derived thermally — rock has far higher thermal inertia
than regolith fines, so through the lunar night boulders stay warm while
dust cools fast, and multi-wavelength night temperatures separate the
two.

It says **nothing** about composition. A boulder field and a smooth plain
can be chemically identical. It answers "how bouldery is this ground?",
which is an engineering question, exactly like slope.

So it belongs on the **site** panel if it belongs anywhere — and it
probably does not need to, because `TerrainBuildability::roughnessM`
(RMS residual after a least-squares plane, from real LOLA/SLDEM at 59 m)
already drives the same decision from measured data. Adding a synthetic
rock layer would buy a second name for a veto the game already has.

### 4.6 What the region panel actually shows

The **natural, extractable** resources — the ones the planet grid already
stores: `H2, O2, C, Fe, Si, Ti, Al, Ca`. `OrbitalSurveyData` already
carries the matching fields (`fePercent`, `tiPercent`, `siPercent`,
`alPercent`, `caPercent`, `thPpm`, `kPpm`, `hydrogenSignal`).

Everything else in `ResourceType` — `WATER`, `FOOD`, `BIOFUEL`,
`ALLOYS`, `MACHINERY`, `ELECTRONICS`, `CONSTRUCTION_MATERIALS`,
`SCIENCE`, `MANPOWER` — is **produced, not found**, so none of it appears
in site selection. Water in particular is made from hydrogen and oxygen
by the colony; what the survey sees is the hydrogen signal, not water.

`[?]` Eight bars may be too many to compare regions at a glance.
Grouping into three — volatiles (H2, O2, C), metals (Fe, Ti, Al),
silicates (Si, Ca) — would read faster, at the cost of hiding which
metal. Decide by playtest, not in advance.

`[?]` Region size. 100 km = one playfield is the natural choice and needs
no new machinery. If regions turn out to feel too coarse to choose
between, the alternative is several regions per playfield — but that
weakens "one region, one playfield", so try the simple version first.

---

## 5. Implementation plan

### Step 1 — Cursor infrastructure — **DONE**
`src/TerrainGen/survey_cursor.{h,cpp}`: pure geometry, no game or render
code, shared by the game and the `lunar_map` instrument. Screen ↔ km ↔
lat/lon, grid snapping, clamping, and the descent stack. Verified by
`survey_cursor_test` (headless, 30 checks) and visually by
`lunar_map --ladder`.

**Simplification note:** the ladder table is now longer than this design
needs. Leave it — it costs nothing, the tests cover it, and the game
simply uses the two entries it cares about.

### Step 2 — Region resource panel
- Aggregate the survey grid over the whole 100 km playfield, once, when
  the anchor is set.
- Render as the region card: bars, values, region name, the one-line
  explanation.
- **Verify:** the values do not change while the cursor moves.

### Step 3 — Site terrain panel
- Live `LolaDem::EvaluateSite` at the cursor's footprint.
- Green/red verdict using the existing `JudgeSite` thresholds.
- **Verify:** every row tracks the cursor; the blocking limit is named,
  not just "invalid".

### Step 4 — Placement and commit
- Port `DrawPlacementCursor` into the game's render path with the §3.2
  register change (survey markers, dimension label, ghosted sect layout).
- Explicit confirm; a pre-build summary split into **measured** (terrain)
  and **unknown until prospected** (resources).
- **Verify:** a player who has never seen the game can say what each
  panel is for after one minute.

---

## 6. Reuse — what already exists

Most of the hard parts are done and should not be rebuilt:

| Need | Already available |
|------|-------------------|
| Cursor rect + verdict colouring + readout panel | `lunar_map --place` (`DrawPlacementCursor`, `JudgeSite`) |
| Real terrain gating | `LolaDem::EvaluateSite`, `TerrainBuildability` |
| Cell → lat/lon | `TerrainGridCellToLatLon` |
| Orbital click → lat/lon | `OrbitalPickToLatLon` |
| Continuous zoom on the same ground | `GenerateTerrainChain`, deterministic per lat/lon |
| Ladder controls + view walking | `colony_viewtest` |
| Existing site-selection screen | `View::SITE_SELECTION`, `DrawSiteSelectionView` |

The `lunar_map` prototype is effectively a working level 5. Step 5 is
largely a port of it into the game's render path.

---

## 7. Deliberate placeholders

- **Resource distribution is synthetic.** Real elemental abundance needs
  Clementine/M3 spectral data and Lunar Prospector gamma-ray; hydrogen
  needs neutron spectrometer (LEND/LPNS). None are derivable from a DEM.
  The aggregation model is written so swapping in real data later
  changes only the source, not the interaction.
- **Terrain is already real** — slope, illumination and Earth visibility
  come from LOLA. Any mismatch between "looks flat" and "reads steep" is
  now a bug, not a design compromise.
- **PSR detection needs polar data.** The global 1.9 km DEM cannot
  resolve cold traps; a polar SLDEM crop via the `fetch-dem` workflow
  would fix it.

---

## 8. Risks

| Risk | Mitigation |
|------|------------|
| Region choice feels arbitrary — all regions look alike | The region panel must show enough spread to matter; this is a requirement on `ResourceManager`'s generation, not on the UI |
| The frozen region panel reads as broken | The one-line explanation plus the contrast with the live terrain panel; playtest whether anyone actually asks |
| Placement doesn't feel different enough from navigating | Register change is explicit in §3.2; playtest specifically |
| Cursor sizing breaks at extreme aspect ratios | Size from the *smaller* window dimension |

---

## Appendix A — the model this replaced

**Kept as reasoning, not as work.** Nothing in this appendix should be
built. It is here so the argument is not re-derived from scratch, and so
the reason for the cut is on record.

### What it was

A five-level descent ladder in which every quantity carried its own
resolution floor, set by the instrument that measured it. Terrain
resolved all the way down; elemental abundance froze around 25 km;
hydrogen froze at its ~45 km neutron footprint. Below a floor the value
held and its **uncertainty band widened**, because descending changes the
question asked of a measurement without changing the measurement. Three
non-modal cues carried the limit: each layer drawn blocky on its own
measurement grid, the instrument footprint drawn as a ring around the
cursor, and the widening band.

### Why it was cut

It was *correct* and it was *unteachable*. Five levels, three
instruments, three footprints, bands moving in opposite directions on the
same panel, rings that turned into dashed frames when they outgrew the
view, rows that greyed out mid-descent, and an archetype rule that had to
inherit all of it — all of that machinery existed to communicate one
sentence: **resource data does not sharpen when you zoom.**

Attaching resources to the region says the same thing by construction, in
one observation, with nothing to read.

### What was right in it, and survives

- Descending is a camera move, not an instrument change. *(Now
  structural: there is nothing to descend *for*, resource-wise.)*
- Confidence is a property of the quantity, not of the zoom level.
  *(Now: resources have one confidence, terrain has another, and they sit
  in separate panels.)*
- Nothing is ever injected — no seeded noise, no per-level fudge. *(Now
  trivially true.)*
- A cost on descent is rejected: the survey is a pre-existing dataset the
  player inherits, with instruments they did not choose. There is nothing
  legitimate to charge for.

### The one thing worth remembering

Whatever the resource generator does, **the regions have to differ enough
to make choosing between them matter.** The old model needed structure
*below* the instrument floor; this one needs spread *between* regions.
Either way it is a requirement on generation, and no UI work substitutes
for it.

### The full argument, as written

### A.1 The model

Every level shows **the same quantities**, aggregated over the cursor
footprint. Only the sharpness changes. The quantities:

| Quantity | Source | Notes |
|----------|--------|-------|
| Fe / Ti / Si / Al / Ca | `OrbitalSurveyData` | elemental, 0–1 |
| Hydrogen (water proxy) | `OrbitalSurveyData.hydrogenSignal` | synthetic — needs neutron data to be real |
| Th / K (KREEP) | `OrbitalSurveyData` | |
| Mean slope | **`TerrainBuildability`** | real, from LOLA |
| Illumination | **`TerrainBuildability`** | real, ray-marched horizon |
| Earth visibility | **`TerrainBuildability`** | real |
| Site archetype | `GetSiteArchetype` | derived classification |

Terrain quantities are **already real**. Resource quantities stay
synthetic for now — that is an explicit, acceptable placeholder for
visualising the interaction (§7).

### A.2 Aggregation rule

Sample the underlying survey grid over the **aggregation window** (§4.3)
and reduce:

- **Resources:** area-weighted **mean**, plus the **standard deviation**
  over the same window. They are abundances; a mean is what "how much is
  in this region" means, and the spread is what makes the mean honest.
- **Slope:** report **mean and max**. Max matters because one cliff can
  disqualify a site that averages flat.
- **Illumination / Earth visibility:** **mean**, but flag the *best* cell
  too — at level 5 the player wants the sunlit ridge, not the average of
  ridge and crater floor.
- **Archetype:** **modal** (most common), with a count so "mixed" reads
  as mixed rather than as a weak single answer.

For terrain the window shrinks with the cursor, so at level 5 the readout
falls through to a direct `EvaluateSite` call at the cursor's own
footprint — exactly what the `lunar_map --place` prototype does. For
resources it does **not**; see below.

### A.3 Confidence — what descending actually buys

An earlier draft made confidence a function of zoom level: the deeper you
go, the sharper the number. That model does not survive contact with the
rest of the design. **If uncertainty falls monotonically with an action
that is free and reversible, it is not uncertainty — it is a toll.** The
optimal play is to descend on everything, back out, and descend again;
the coarse readouts never inform a decision, they just stand between the
player and a number they are always allowed to have. Adding *bias* to the
coarse readout does not fix this. It makes the toll feel unfair as well
as tedious, and the player's counter-strategy is unchanged: zoom in and
ignore what the high levels said.

The premise is what is wrong. **Descending is a camera move, not an
instrument change.** Looking harder at a map does not give you a better
neutron spectrometer. What actually sets the sharpness of a number is
*which instrument measured it*, and that does not care where the camera
is.

So confidence is **per quantity, not per level**. Each has its own
resolution floor, fixed by its instrument, and the descent walks past
those floors one at a time.

| Quantity | Source | Approx. footprint | Sharpens until |
|----------|--------|-------------------|----------------|
| Elevation, slope, relief, roughness | LOLA / SLDEM2015 | ~59 m | **level 5 — fully resolved** |
| Illumination, PSR, longest night, Earth visibility | ray-marched from LOLA | ~59 m (needs ~60 km of horizon context) | **level 5 — fully resolved** |
| Surface mineralogy | M3 (Chandrayaan-1) | ~140 m | level 5 — but surface only, weathering-confounded |
| Thermal inertia, rock abundance | Diviner | ~200 m | level 5 |
| Elemental abundance (Fe, Ti, Th, K) | gamma-ray spectrometer | tens of km | **level 3 — frozen below** |
| Hydrogen / volatiles | neutron spectrometer | ~45 km | **level 2–3 — frozen below** |
| Subsurface structure, regolith depth, ice at depth | nothing from orbit | — | **never — prospecting's job** |

*(Footprints are order-of-magnitude and must be checked against the real
instrument papers before they are hard-coded.)*

**The aggregation window is therefore per quantity:**

```
window = max(cursor footprint, instrument footprint)
```

Terrain rows use the cursor. Resource rows stop shrinking once the cursor
is smaller than the instrument, and from there the number simply stops
changing as the player descends.

#### Why this is better than either "imprecise" or "biased"

- **Nothing is injected.** Every number shown is a true mean of a real
  area. The game never lies, and never needs a seeded noise function.
- **Dilution falls out for free, in the physically correct direction.** A
  1.5 km rich deposit averaged over a 45 km neutron footprint really does
  read low. That is the memorable-failure appeal of the old model (3)
  without its unfairness, because the cause is visible: draw the
  instrument footprint on the map and the player can see it is thirty
  times the size of the base.
- **It is learnable.** "The neutron footprint is 45 km; that number will
  never get sharper from up here" is a rule a player can hold, act on,
  and eventually exploit.
- **Descending stays worth doing** — it resolves terrain completely, and
  terrain is what gates the build.
- **Descending stops being exhaustible.** No amount of zooming resolves
  the chemistry, so brute-forcing the ladder buys nothing beyond what the
  coarse resource readout already said.

#### Direct answer to "biased no matter how far I zoom?"

Neither, and the split is the point:

- **Terrain: fully resolvable.** Zoom to level 5 and the slope, relief,
  illumination and PSR status are measured truth. Ascending loses nothing
  — that knowledge is not taken away.
- **Chemistry: permanently blurred**, at a floor well above the site
  scale. Not biased, not noisy — *averaged*, honestly, over an area much
  larger than the base. It will read low over a small rich deposit and
  high over a small poor one, and it will do that for ever.
- **Subsurface: not observable at all.**

Which makes the commit at level 5 a real decision under real uncertainty:
**you know exactly what the ground is, and only roughly what is in it.**

#### The band widens as the value freezes

The freeze alone is not enough — a number that simply stops changing
looks like a bug. What actually happens below an instrument's floor is
sharper than that, and it is the honest answer to the player's real
question.

Descending does not change the measurement, but it **changes the question
being asked of it**. At level 3 the player is asking "what is in this
25 km cell?", and a 45 km mean is a decent answer. At level 5 they are
asking "what is under this 1.5 km base?", and the same 45 km mean is a
much worse answer to that question — not because the data degraded, but
because the question got thirty times sharper while the data did not.

So below the floor:

- the **value stays fixed** — it is the same measurement, and the game
  must not pretend otherwise;
- the **uncertainty band widens** — because the spread of cursor-sized
  patches inside that footprint is what the player is now exposed to.

Terrain rows and resource rows therefore move in **opposite directions**
on the same panel as the player descends: terrain bands close, resource
bands open. That contrast is the clearest possible statement of what the
descent does and does not buy, and it needs no text at all.

Nothing is invented here either. The band is:

```
band = standard deviation of cursor-sized patches within the
       instrument footprint
```

which the game can compute directly, because it generates the field. It
is exactly the quantity a geologist would quote, it widens on its own as
the cursor shrinks, and it saturates at the field's own variance. No
seeded noise, no per-level fudge factor.

#### Making the limit visible without a popup

Three redundant cues, none of them modal:

1. **The data layer goes blocky.** Render each quantity on *its own
   measurement grid*. Terrain keeps resolving all the way down; the
   hydrogen layer turns into 45 km blocks and then into one flat block
   filling the whole screen. Every player has seen a raster hit its
   resolution and understands instantly that there is no more detail
   there. This is the strongest device and it costs no UI space.
2. **The instrument footprint drawn on the map** — a faint ring around
   the cursor wherever the footprint is larger than the cursor. Seeing a
   45 km ring around a 1.5 km base explains the whole mechanic in one
   glance.
3. **The widening band** on the bar, opposite in motion to the terrain
   rows beside it.

A row label carrying the instrument and its footprint (`NEUTRON · 45 km
avg`) is the fourth, weakest cue — worth having as the precise statement,
but it should never be the *only* one.

#### What this does NOT solve

Terrain can still be brute-forced: descend on everything and you will map
every buildable cell. The coarse terrain readout is what keeps that
unattractive — mean **and max** slope over the footprint tells you a
region hides a cliff without descending, so you descend to find *where*,
not *whether*.

A cost on descent is **rejected**. The survey is a pre-existing dataset
the player inherits — measured before they arrived, with instruments they
did not choose. Coverage is not a resource and instruments are not a
decision, so there is nothing legitimate to charge for. Descending stays
free, which §3.3's reversibility already requires.

**STATUS: proposed, supersedes the earlier "resolution-limited" decision.
Awaiting confirmation before step 2 is built against it.**

---

### A.4 What the instrument floors forced next

The floors are not just a display rule — they reach into the resource
generator, the archetype classifier and the prospecting hand-off. In
rough order of how badly each one can sink the idea:

#### 1. Each level now answers a DIFFERENT question

This is the biggest consequence, and it is a gain. A 45 km neutron
footprint over the game's 100 km playfield means hydrogen has perhaps
four or five independent values across the *entire* 20x20 grid. At sect
scale the hydrogen map is essentially flat.

That is not a problem to engineer around — it is the ladder telling us
what each level is for:

| Levels | The question | Decided by |
|--------|--------------|------------|
| 1–3 | *Which region?* volatiles, bulk composition, KREEP | instrument-limited data |
| 4–5 | *Which ground?* slope, relief, illumination, PSR | fully resolved data |

Choose your **region** for what is in it; choose your **site** for what
you can build on. §8 listed "aggregate readouts look identical across
levels" as a risk — this removes it far better than a confidence layer
would have.

#### 2. The generator must put structure BELOW the floor

If composition varies only smoothly over hundreds of km, a 45 km
footprint captures it almost perfectly and the entire mechanic is inert —
the band would be narrow everywhere and nothing would ever surprise
anyone. **The mechanic only bites if the resource field has real
structure at scales the instruments cannot see.**

This is a requirement on `ResourceManager`'s cluster generation, not on
the UI, and it is the single most important implementation consequence
here. Concretely: hydrogen wants small, high-contrast concentrations
(cold traps are metres to km across, not tens of km), so the field needs
power at 1–10 km. Iron and titanium genuinely do vary at basin scale, so
they can stay smooth.

`[?]` How much sub-footprint contrast is enough to be interesting without
being arbitrary? Needs playtesting against a real distribution.

#### 3. Terrain becomes a proxy for chemistry — the actual skill

You cannot measure hydrogen at 1.5 km. You *can* see, at 59 m, that a
crater floor is permanently shadowed. Cold traps are where volatiles
survive, so **the sharp data predicts the blurry data**.

That is the deepest thing this model creates: the player learns to read
terrain as evidence about composition, which makes descending genuinely
informative about chemistry *indirectly*, without ever faking a
measurement. Worth designing for on purpose — the archetype hints, the
PSR flag and the thermal rows should all be legible as chemistry clues,
not just as buildability rows.

#### 4. Not everything should be blurry

If every number is uncertain the player has nothing to plan with. A
gradient of trustworthiness across the panel is what makes the panel
readable:

- **Trustworthy:** slope, relief, illumination, PSR (measured, sharp).
- **Fairly trustworthy:** Fe / Ti — they track mare vs highland, which is
  *visible in the imagery*. A player who learns to read dark mare as
  iron-rich is using a real skill on real data.
- **Barely trustworthy:** hydrogen. Highest stakes, worst resolution.
  This is where the gamble lives, and it should be the only place.

#### 5. The archetype must inherit the floor

`SiteArchetype` is derived from composition, so it cannot be sharper than
the composition it is derived from. If the archetype label sharpens as
the player descends it leaks information the instruments do not have.
Classify on the instrument-footprint aggregate, not the cursor.

#### 6. The uncertainty has to bite, and prospecting is what resolves it

If a wrong guess never costs anything, the band is decoration. The chain
should be: the orbital number was never wrong, only coarse → the base is
built → **prospecting measures the local truth** → it may be well below
what the region average implied.

This is a clean hand-off to `docs/design/prospecting/`, whose survey
progress mechanic already exists and whose entire purpose becomes
*breaking the instrument floor*. It also sets a hard UI rule: the site
readout must never present a footprint average as if it were a local
measurement, or the reveal reads as the game cheating rather than as the
player learning what "45 km average" meant.

`[?]` Is the gamble avoidable? If the player can always found a cheap
scout base, prospect, and only then commit, the uncertainty is a slower
toll again. Suggested shape: the **first** colony is a real commitment;
later ones can be scouted first. That is also a natural difficulty curve.

#### 7. Implementation note — cost of a large aggregation window

A 45 km footprint re-aggregated on every mouse move is a lot of grid
samples per frame. A summed-area table over the survey grid makes it
O(1) per quantity; variance needs a second table of squares. Cheap,
standard, and worth doing in step 2 rather than retrofitting.

---

