# Site Selection — Master Design

**Status: DRAFT**
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
| 1 | Orbital | whole disc | ~500 km | ~15% | Pick a region of the Moon |
| 2 | Regional | 500 km | 100 km | 20% | Pick a district |
| 3 | Planet / district | 100 km | 25 km | 25% | Pick a locality |
| 4 | Locality | 25 km | 5 km (one sect cell) | 20% | Pick the cell |
| 5 | **Site** | **5 km** | **1.5 km build footprint** | **30%** | **Commit the base** |

**Cursor sizing rule.** The cursor stays between **15% and 30% of the
window's smaller dimension**. That is the band where it is large enough
to read a label inside and small enough that the choice is meaningful —
below ~12% it becomes a dot and the aggregate readout is unreadable,
above ~40% there is nothing left to choose between. The ratio widens
deliberately at level 5 because the footprint is now a *physical object*
being sited, not a navigation target.

Level 4 → 5 is the existing 5 km sect cell (`TERRAIN_CELL_KM`), so the
ladder lands exactly on the game's established grid.

`[?]` **Snap or free?** Levels 1–3 are naturally free-moving. Level 4
should probably snap to the 20×20 sect grid, since the cell is a real
game entity. Level 5 must be free — the whole point is choosing *where
in the cell* the base sits.

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

`[?]` **Committed or reversible descent?** Reversible is friendlier;
committed adds weight to the choice. Suggest reversible during
exploration and committed only at level 5 (with a confirm step).

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

Sample the underlying survey grid over the cursor footprint and reduce:

- **Resources:** area-weighted **mean** — they are abundances, and a
  mean is what "how much is in this region" means.
- **Slope:** report **mean and max**. Max matters because one cliff can
  disqualify a site that averages flat.
- **Illumination / Earth visibility:** **mean**, but flag the *best* cell
  too — at level 5 the player wants the sunlit ridge, not the average of
  ridge and crater floor.
- **Archetype:** **modal** (most common), with a count so "mixed" reads
  as mixed rather than as a weak single answer.

At coarse levels the footprint covers many grid cells; at level 5 it
covers a fraction of one, so the readout falls through to a direct
`EvaluateSite` call at the cursor's own footprint size — exactly what
the `lunar_map --place` prototype already does.

### 4.3 Confidence — the reason to descend

Aggregates should be shown with **decreasing uncertainty** as the player
descends. Without this, descending is a chore; with it, descending buys
information.

| Level | Presented as | Uncertainty |
|-------|--------------|-------------|
| 1–2 | Coarse bars, no numbers | ±40% band |
| 3 | Bars + rounded numbers | ±20% |
| 4 | Numbers | ±10% |
| 5 | Exact values | none (terrain is measured) |

Implement as a display-time transformation of the true value, seeded
deterministically per cursor cell so it does not shimmer as the mouse
moves. **Do not** perturb the stored data — only the presentation.

`[?]` Should high-altitude readouts be *biased* (systematically wrong)
or merely *imprecise* (noisy but unbiased)? Bias creates genuine
surprises on arrival; imprecision is fairer. Suggest imprecise, with
rare biased outliers as a later gameplay hook.

---

## 5. Implementation plan

Ordered so each step is independently visible and testable.

### Step 1 — Cursor infrastructure (no gameplay change)
- `SurveyCursor` struct: footprint km, centre lat/lon, screen rect.
- Sizing helper enforcing the 15–30% rule from the window span.
- Screen ↔ lat/lon mapping for the top-down views (already derived in
  `DrawPlacementCursor`; promote it to a shared helper).
- **Verify:** cursor renders and tracks the mouse at every level.

### Step 2 — Aggregation
- `SurveyAggregate AggregateOver(latMin, latMax, lonMin, lonMax)` in
  `ResourceManager`, implementing §4.2.
- At level 5, bypass the grid and call `LolaDem::EvaluateSite` directly.
- **Verify:** printed aggregates for a known cell match a manual mean.

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

### Step 6 — Confidence layer
- Deterministic per-cell imprecision, applied at display time only.
- **Verify:** value stabilises as the player descends onto one spot.

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
