# Site Selection — Master Design

**Status: SETTLED** — implementation in progress (step 1 of 6 done).
**Scope:** orbital view → 1 km build footprint, the survey cursor, and
resource aggregation across scales.

---

## 1. Design intent

The player should feel like a mission planner narrowing down a landing
site, not like someone clicking a tile on a grid. Three properties carry
that feeling:

1. **One question per level.** At every zoom the cursor shows the *next*
   level's footprint. The player is never asked "where exactly?" while
   looking at a whole hemisphere — only "which of these regions?"
2. **Information sharpens as you descend.** Zoomed out, the readout is a
   coarse average with wide error bars. Zoomed in, it is specific. The
   player learns by descending, which is what makes descending feel like
   an action rather than a camera move.
3. **The last step is different in kind.** Levels 1–4 are navigation.
   Level 5 is commitment. It must *look* different, not just behave
   differently.

---

## 2. The descent ladder

Five levels. Each level's cursor is the footprint of the level below, so
the cursor is always "the thing you are about to enter".

| # | View | Window span | Cursor footprint | Cursor / window | Role |
|---|------|-------------|------------------|-----------------|------|
| 1 | Orbital | whole disc | 500 km | 17% | Pick a region of the Moon |
| 2 | Regional | 500 km | 100 km | 20% | Pick a district |
| 3 | Planet / district | 100 km | 25 km | 25% | Pick a locality |
| 4 | Locality | 25 km | 5 km (one sect cell) | 20% | Pick the cell |
| 5 | **Site** | **5 km** | **1.5 km build footprint** | **30%** | **Commit the base** |

**Cursor sizing rule.** The cursor stays between **15% and 30% of the
window's smaller dimension**, and at every *snapping* level the window
must be a whole number of cursors — otherwise the grid leaves ground the
player can see but cannot select. That is the band where it is large enough
to read a label inside and small enough that the choice is meaningful —
below ~12% it becomes a dot and the aggregate readout is unreadable,
above ~40% there is nothing left to choose between. The ratio widens
deliberately at level 5 because the footprint is now a *physical object*
being sited, not a navigation target.

Level 4 → 5 is the existing 5 km sect cell (`TERRAIN_CELL_KM`), so the
ladder lands exactly on the game's established grid.

**DECIDED — grid-snap.** The cursor snaps to the grid at every
navigation level (1–4): each level's grid is the set of footprints of
the level below, so the cursor always lands on a whole child region
rather than straddling two. This makes the ladder a clean tree, makes
aggregates exact (a footprint is always a whole number of cells), and
makes descent unambiguous.

Level 5 remains **free-moving** — the whole point of the final step is
choosing *where within* the 5 km cell the base sits, and the buildable
ground may be a corner of it.

---

## 3. The survey cursor

### 3.1 Appearance (levels 1–4, navigation)

- Translucent fill, ~15% alpha, neutral (no green/red — nothing is being
  judged yet).
- 2 px outline, corner ticks at 35% of the half-width.
- A compact readout panel anchored to the **opposite side of the window
  from the cursor**, so it never covers the ground it describes. (This
  bug was already found and fixed in the `lunar_map` prototype.)
- Panel contents: aggregate resource bars, mean slope, illumination, and
  a confidence indicator (§4.3).

### 3.2 Appearance (level 5, commitment)

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

**DECIDED — reversible.** Ascending is always allowed and costs
nothing; the player can wander the ladder freely. Only the final build
at level 5 is a commitment, and it takes an explicit confirm.

Consequence for implementation: the descent path must be a stack, not a
single current position. Ascending restores the parent view *with its
cursor where the player left it*, so backing out of one region and
trying its neighbour does not reset the whole descent.

---

## 4. Resource aggregation

### 4.1 The model

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

### 4.2 Aggregation rule

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

### 4.3 Confidence — what descending actually buys

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

#### Presentation consequences

- Terrain rows show a value that visibly sharpens as the player descends.
- Resource rows carry their instrument's name and footprint
  (`NEUTRON · 45 km`) and **grey out further sharpening** once the cursor
  is inside the footprint, so the freeze reads as physics rather than as
  a bug.
- The instrument footprint is drawn on the map as a faint circle around
  the cursor at the levels where it is larger than the cursor. This is
  the single most important piece of the whole idea: it makes the limit
  visible instead of merely stated.
- Spread is shown as an error band on each bar, and it stops narrowing
  when the window stops shrinking.

#### What this does NOT solve

Terrain can still be brute-forced: descend on everything and you will map
every buildable cell. Two things keep that unattractive, and neither is a
charge on the camera:

1. The coarse terrain readout is genuinely informative — mean **and max**
   slope over the footprint tells you a region hides a cliff without
   descending. You descend to find *where*, not *whether*.
2. If a real cost is wanted later, put it on **survey coverage, not on
   zoom**: the orbital data does not exist until a satellite has passed
   over that ground. That is physically true (LRO's maps were built over
   years), it makes the scarce thing *information* rather than camera
   movement, and it leaves the descent free — which the reversibility
   decision in §3.3 already requires.

**STATUS: proposed, supersedes the earlier "resolution-limited" decision.
Awaiting confirmation before step 2 is built against it.**

---

## 5. Implementation plan

Ordered so each step is independently visible and testable.

### Step 1 — Cursor infrastructure (no gameplay change) — **DONE**
Landed as `src/TerrainGen/survey_cursor.{h,cpp}`: pure geometry, no game
or render code, so the game and the `lunar_map` instrument share one
implementation instead of drifting apart.

- `SurveyCursor` — window (span, centre lat/lon) plus the cursor inside
  it. The cursor offset is stored in **km from the window centre**, not
  pixels, so it survives a resize and is meaningful without a viewport.
- `GetSurveyLadder()` — the five levels as data. `SurveyFootprintForSpan`
  applies the 15–30% rule to arbitrary spans (for free zooming),
  preferring the ladder's own footprints so sizes stay familiar.
- Screen ↔ km ↔ lat/lon: `SurveyScreenToOffsetKm`,
  `SurveyOffsetKmToScreen`, `SurveyCursorLatLon`,
  `SurveyLatLonToOffsetKm`, `SurveyCursorRect`.
- `SurveyCursorTrack` — the mouse path: map into the window, snap to the
  level's grid (§2), clamp so the footprint stays wholly inside.
- `SurveyDescent` — the stack §3.3 requires. Ascending restores the
  parent cursor where the player left it, and re-entering the *same*
  region keeps the child's cursor too; a different region starts centred.

Two departures from the design as written, both recorded here:

1. **The orbital level's span is the usable disc width (3000 km, ~86% of
   the diameter), not the diameter.** Level 1 is projected, not a
   top-down km window, and ground within ~15% of the limb is too
   foreshortened to aim at. Measured against the full diameter the 500 km
   cursor is 14.4% — below the band — which is an artefact of measuring
   against ground the player cannot use. The figure is rounded to 3000 so
   the 500 km cursor tiles it exactly: an unroundable span leaves the
   outermost cells unreachable, and the cursor then snaps a whole cell
   away from where the player is pointing.
2. **The viewport is the square the window span maps into**, with the
   span on the smaller dimension. This keeps the whole window visible at
   any aspect ratio; `lunar_map`'s top-down camera fits the span to the
   screen height, so its viewport is the centred square of side
   `screenH`.

**Verified:** `survey_cursor_test` (headless, no GL, no DEM — 30 checks:
ratio band, cursor == child window, screen round trips, odd/even grid
snapping, clamping at every level, lat/lon round trip, and the full
descent-stack behaviour). Visually: `lunar_map --ladder` walks
500 → 100 → 25 → 5 km over real terrain with the cursor aimed at one
fixed target through the same ground → km → screen → track path the
mouse takes.

### Step 2 — Aggregation
- `SurveyAggregate AggregateOver(latMin, latMax, lonMin, lonMax)` in
  `ResourceManager`, implementing §4.2 — mean **and spread** per quantity.
- Per-quantity aggregation windows: `max(cursor, instrument footprint)`
  (§4.3). An instrument table alongside the resource descriptors.
- At level 5, bypass the grid and call `LolaDem::EvaluateSite` directly
  for the terrain rows; resource rows keep their instrument window.
- **Verify:** printed aggregates for a known cell match a manual mean,
  and a resource row's value stops changing once the cursor is inside its
  instrument footprint.

### Step 3 — Readout panel
- Shared panel renderer: resource bars, terrain rows, archetype, and
  confidence band.
- Opposite-side anchoring.
- **Verify:** screenshots at each level; panel never overlaps cursor.

### Step 4 — Ladder wiring
- Extend the existing `View` enum / `ViewManager` transitions to the five
  levels, re-centring on the cursor when descending.
- Reuse `colony_viewtest`'s control scheme (click descend, Esc ascend).
- **Verify:** full descent in `colony_viewtest`, screenshots per level.

### Step 5 — Commit step (level 5)
- Verdict colouring via `JudgeSite`.
- Engineering overlay + ghosted sect layout.
- Confirm dialog → create the colony/sect at the chosen lat/lon.
- **Verify:** green site builds, red site refuses with a named reason.

### Step 6 — Presentation sharpening
No perturbation layer (see §4.3): every number is a true mean, so this
step is purely how the limits are shown.
- Significant figures per level (bars only → rounded → exact).
- Spread rendered as an error band on each bar.
- Instrument name + footprint on every resource row; the row greys out
  once the cursor is inside its footprint.
- The instrument footprint drawn as a faint circle around the cursor
  wherever it is larger than the cursor.
- **Verify:** terrain bands narrow all the way to level 5; resource bands
  stop narrowing at their instrument's floor, and the player can see why
  without reading a tooltip.

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
| Five levels feels like too much clicking | Allow scroll-wheel zoom to skip levels; the ladder is the *structure*, not a forced sequence |
| Aggregate readouts look identical across levels | Confidence layer (§4.3) is what differentiates them — do not cut it |
| Level 5 doesn't feel different enough | Register change is explicit in §3.2; playtest this specifically |
| Cursor sizing breaks at extreme aspect ratios | Size from the *smaller* window dimension |
