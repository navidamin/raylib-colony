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
> build on it. Every level in between still decides something — but it
> decides *position*, never chemistry.

Everything below is consequence. If a rule here cannot be traced back to
those two sentences, it should not exist.

---

## 1. Design intent

The player should feel like a mission planner narrowing down a landing
site, not like someone clicking a tile on a grid. Three properties carry
that:

1. **Every level asks one question, and it is a different question.**
   Not the same question at five sharpnesses — that was the rejected
   model (Appendix A). Each level's question is the one that naturally
   lives at that scale, answered by data that is real at that scale.
2. **One verb.** The decision at every level is where you put the cursor
   before you click. Click descends, Esc backs out. No level introduces
   a new interaction; only the question changes.
3. **Five questions, one commitment.** Only founding the base (level 5)
   binds. Everything above it is freely revisable — the
   reversible-descent rule (§3.3) is what makes asking five questions
   cheap enough to be pleasant, and the player should never be asked to
   commit to an economy before seeing the ground under it.

---

## 2. Four levels, four questions

| # | Level | Span | Cursor | The question | Answered by |
|---|-------|------|--------|--------------|-------------|
| 1 | Orbital | disc | — | **Which economy?** | terrane + named feature + latitude: Fe/Ti/Th, rock type, day/night regime |
| 2 | Playfield | 100 km | 25 km | **Which mix?** | position against boundaries: mare/highland shore, large craters, PSR craters (polar) |
| 3 | Colony | 25 km | 5 km | **Which neighbourhood?** | buildable fraction, mean slope, distance to PSR / shore / landmark |
| 4 | Site | 5 km | 1.5 km | **Which ground?** | live `EvaluateSite` at 59 m: slope, relief, illumination, PSR |

These are the game's own scales: PLANET is 100 km, COLONY 25, SECT 5.
The ladder has no rung that is not a view the game already has.

> **As built (2026-09-02).** The ladder in code is three rungs, not four:
> ORBITAL (3000 km usable disc, 200 km cursor) → DISTRICT (200 km window,
> 25 km cursor) → SITE (25 km window, cursor refining from the 5 km cell
> to the 1.5 km footprint as the view zooms to 5 km). Colony and Site
> merged on 2026-08-29 — one decision was wearing two framings — and the
> district widened from 100 to 200 km on 2026-09-02 so the three zooms
> read 15× / 8× / 5×. The district therefore no longer coincides with the
> 100 km PLANET playfield, and the cursor floor is 12% (the 8× rung sits
> at 12.5%). `src/TerrainGen/survey_cursor.cpp` is the authority.

The code carried a fifth, 500 km "REGIONAL" rung between the disc and
the playfield, which was never in this table. It answered no question of
its own — the region card freezes at level 1 and never refines (§4.6),
so there was no new mix to read at 500 km — and it matched no game view.
Removed. The one thing it bought was geometric: a 100 km cursor is 1/55
of the disc rather than the 15–30% band the ladder holds elsewhere. That
band exists so a cursor *rectangle* stays legible, and level 1 draws no
rectangle — it is picked by hovering named regions — so the constraint
was never binding there. `survey_cursor_test` exempts level 1 explicitly.

**Nothing locks until the base is founded.** Levels 1–3 are all free
navigation; level 4 is the only commitment, and founding fixes
everything at once — the terrain anchor, the 20×20 grid, the region.

This is a correction to an earlier draft that locked the anchor at
level 2. Committing to an economy *before* seeing whether the ground
can carry a base is exactly backwards, and it is unnecessary: terrain
generates deterministically from lat/lon
(`GenerateTerrainChain`), so there is no boundary at the playfield edge
to enforce — only a viewport. The 20×20 grid matters once a colony
exists, not before.

**The player may pan at any level, including past the first 100 km.**
If panning crosses into a different named region the region card
re-labels, with new numbers. That does **not** contradict §4.6: the
numbers never *refine*, they belong to the region — so different ground
means different numbers, and the same ground always means the same
ones. Crossing a boundary and watching the card flip from mare identity
to highland identity is the clearest possible teaching of what a region
*is*: ground, not a menu item.

Panning needs no rule limiting it by scale. At 100 km the view is a map
and panning is natural; at 5 km and below the frame is following a
cursor that is placing something, and the player is no longer browsing.

**Chemistry decides once; geometry decides at every level after.** The
region panel freezes at level 1 (§4.6) and is never contradicted below.
What levels 2–4 present instead is **measured geometry** — distances and
fractions — and those sharpen honestly with zoom, because the *candidate
position* is sharpening, not the instrument. Distance-to-PSR reads
differently at level 3 than at level 2 because the player moved, not
because anything was re-measured.

**The same ladder asks different questions in different geography.** At
a polar region levels 2–4 are about PSR-and-sunlit-ridge geometry — get
near the ice without falling into the dark. At a mid-latitude mare they
are about the mare/highland shore and crater access. Same mechanics,
same panels, different terrain answering — which is what makes region
choice at level 1 replayable rather than cosmetic.

### 2.1 Level 1 is a strategy menu, and the menu already exists

`SiteArchetype` (game_enums.h) — currently a dead label — becomes the
tag shown with the region's name at level 1. Each archetype is a
strategy with a visible cost, and the trade-offs are real geochemistry
(§4.6), not invented balancing:

| Archetype | Where | You get | You give up |
|-----------|-------|---------|-------------|
| MARE_INDUSTRIAL | PKT mare interior | Fe/Ti-rich ground, flat, strong Earth comms | aluminium (import it), 14-day nights |
| HIGHLAND_CONSTRUCTION | feldspathic highlands | Al/Ca — cheap structures | metal (import it), rough ground |
| POLAR_VOLATILE | polar crater rim | PSR ice next door, near-constant sun on the crest | low metals, brutal terrain, marginal comms |
| KREEP_SCIENTIFIC | thorium anomaly | science | mediocre everything else |
| MIXED | the mare **shore** | both Fe and Al at moderate grade, no imports | master of none |

MIXED is worth noticing: it is not chosen at level 1 at all — it
*emerges at level 2*, by anchoring the playfield on the mare/highland
boundary so both rock types are inside trucking distance. That is the
clearest example of a level-2 decision being real: same region, and the
shore playfield plays differently from the interior one.

### 2.2 What each level shows

One card per level, few rows, all measured. The frozen region card
(chemistry + name) stays on screen from level 1 down, unchanged.

- **L1** — region name, terrane, archetype tag, rock, Fe/Ti/Th,
  latitude and its meaning ("14-day nights" / "polar: ridge sun, PSRs").
- **L2** — what the playfield touches: shore yes/no, named landmarks
  inside, PSR count (polar), buildable fraction of the whole field.
- **L3** — neighbourhood: buildable fraction, mean slope, distance to
  PSR / shore / landmark, room for how many flat sect cells.
- **L4** — the cell and its neighbours: cell aggregate, and a small
  3×3 buildability glyph so expansion room is visible before committing.
- **L5** — the live site panel and verdict, as already designed (§3.2),
  then the measured / unknown-until-prospected commit split.

This is deliberately still *not* a new view stack. Levels 2–4 are the
game's existing Planet → Colony → Sect views; the ladder adds one card
per level and the cursor, nothing else.

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

- **Region panel** — two groups, both frozen:
  - *Resources.* One value each, no error bars, no instrument names. One
    line of copy explains why they do not move: *orbital surveys average
    over tens of km.*
  - *Terrain character.* What kind of ground this region is overall —
    mean slope, and **rock abundance** (§4.7). This is what stops a
    player anchoring a whole playfield on uniformly unbuildable
    highland, and it is a genuinely different question from "is this
    exact spot flat".
- **Site panel.** Exact terrain at the cursor, updating live: mean and
  peak slope, relief, illumination, PSR.
- Both panels are on screen together during placement.

**No rings, no footprint indicators, anywhere.** An earlier draft drew
the instrument footprint as a ring around the cursor, and a dashed frame
when the footprint outgrew the view. Cut — see Appendix A. Nothing on
screen should require the player to know what an instrument footprint
is; the frozen panel says everything that machinery was trying to say.

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
  wrong: rock abundance is not a resource at all (§4.7). With it
  correctly filed as terrain, no resource needs promoting and the rule
  holds without a special case.

### 4.5 Named regions — the region panel's identity

**DECIDED.** The region is not "the 100 km box you claimed". It is a
**named, real lunar region**, drawn in colour on the orbital disc and
outlined on the ground below it.

Two tiers, both real, no invented nesting:

| Tier | Where | Source |
|------|-------|--------|
| **Terrane** | orbital disc | Jolliff, Gillis & Haskin (2000): PKT, FHT-An, FHT-O, SPAT-inner, SPAT-outer, with published FeO / Th figures |
| **Named feature** | the 100 km playfield | `src/assets/planet/zones.json` — 73 real features, nearest-feature-within-radius lookup |

`src/assets/planet/zones.json` **already ships and no C++ file reads
it**: 21 maria, 37 craters, 14 landing sites, one basin, each with name,
feature type, lat/lon, diameter, terrain description, dominant rock,
iron / titanium / thorium, PSR flag, Earth visibility, age and missions.
Generated by `prototypes/planet_visuals/zones_db.py`. So the region layer
needs no invented data — only a loader and a lookup.

`docs/design/prospecting/resource-distribution-model.md` already
specifies Voronoi geological provinces with per-province composition and
is marked implementation-ready with zero implementation. This section is
that document surfaced at the site-selection step.

**What a player remembers is the name, never the number.** "I settled the
high-Ti flows in southwest Imbrium" is the sentence the design should
produce. Lead every panel with the name.

**Presentation rules:**

- **No colour fill anywhere, including the orbital disc.** An earlier
  draft filled the disc by terrane, on the grounds that there was
  nothing else to look at there. There is: the real LOLA relief, where
  dark mare and bright highland already separate the provinces better
  than a tint can. Boundaries and names only, at every level — one rule
  instead of two.
- **The terrane is a label, not a layer.** It never provides a
  selection: 87% of the PKT's area already sits inside a named feature,
  and PKT ground with no feature is 3% of the near side. Its boundary is
  also 19 hand-placed vertices traced off a figure, so drawing it as a
  filled province claims a precision the data does not have. It survives
  as the second line of the hover chip and the region card, and as the
  fallback name for unnamed ground. The one thing it uniquely carries —
  thorium, the only genuinely terrane-scale quantity — is a number on
  the card with a hint.
- **Only the thing under the cursor highlights.** It is the single shape
  on screen that is both real data and pickable, so it is the only one
  allowed to lift off the imagery (soft fill + ring + name chip).
  Nothing terrane-sized ever responds to hover: a continent lighting up
  because the cursor is somewhere inside it is not an affordance.
- **From 100 km down, boundaries only** — a tinted outline plus the
  region's name over *unmodified* imagery. This project's distinguishing
  asset is that the ground is real: dark mare already *is* the
  iron-rich unit. Tinting hides the best evidence on screen.
- **No cursor rectangle at orbital or planet.** The lit region *is* the
  selection; a rectangle would be a second selection mechanism doing the
  same job.
- **The named regions are labels, not a menu.** The player clicks
  anywhere on the disc and the playfield anchors at that exact lat/lon;
  the feature name appears when the click happens to land inside one and
  falls back to the terrane otherwise ("Feldspathic Highlands (polar)").
  There is no snapping to a region and no list to choose from — which is
  what keeps the ~54% of the near side with no feature name fully
  playable.
- **Keep the previously viewed region's panel on screen, ghosted**, beside
  the current one. Otherwise the player can only visit regions one at a
  time and cannot actually compare them. One extra draw of values already
  computed — no shortlist, no bookmarks, no new state.

### 4.6 Why the numbers do not refine as you descend

**DECIDED — they do not refine at all.** A region's numbers appear once,
where the region is claimed, and never change. A refining variant was
proposed and rejected; the reasoning is worth keeping because it is
counter-intuitive.

**The information runs the opposite way to intuition.** It is natural to
assume knowledge degrades as you approach the ground. On the real Moon it
is the reverse:

| Quantity | Instrument | Footprint |
|----------|-----------|-----------|
| Fe, Ti, Al, Ca | Clementine UVVIS / M3 / Kaguya MI multispectral | **20–200 m** |
| Th, K | gamma-ray spectrometer | **45–200 km** |
| H | neutron spectrometer | 45+ km |

A 1.5 km footprint is *thousands* of multispectral pixels — the richest
compositional data anywhere in the descent. Meanwhile **one gamma-ray
pixel is the entire 100 km playfield or larger.** So thorium is the one
quantity that genuinely cannot improve below the top screen.

That gives two independent reasons the region panel is frozen, and
neither is "the instrument cannot see it":

1. **Th and K are terrane-scale by instrument.** Unarguable.
2. **Fe, Ti, Al and Ca are near-constant inside a mapped unit** — because
   spectral homogeneity is the *mapping criterion* that defined the unit.
   Showing them per cell would display noise, not signal.

**Three further corrections that shape the panel:**

- **Fe, Al and Ca are one number, not three.** All are carried by the
  plagioclase-to-mafic ratio; plagioclase is CaAl₂Si₂O₈, so Ca tracks Al
  near-stoichiometrically and both anticorrelate with Fe. Three gauges
  moving together would overstate what the player learned.
- **Si should never be a gauge.** SiO₂ spans roughly 40–48 wt% across
  every lunar lithology — under 1.25× dynamic range. It would only ever
  read "about 45%".
- **Ti is the discriminating axis.** "Mare" spans under 1 to about 14 wt%
  TiO₂, over 20×. In the shipped region data Mare Imbrium reads 2.5 wt%
  and Mare Tranquillitatis 8.0 — the real spread is already there.

**Volatiles move to the terrain panel.** Hydrogen is governed by shadow
geometry and maximum temperature, not by rock, and `EvaluateSite` already
returns `illumination`, `isPsr` and `longestNightDays` from measured
elevation. A composition row that never narrows reads as unfinished; a
PSR flag reads as a fact. (Note the cold-trap reasoning is *polar*
physics — at a mid-latitude mare playfield the honest answer is simply
that there is no trapped water, which the PSR flag says directly.)

`[?]` **Regions must be trade-offs, not tiers.** On the published figures
the PKT leads on iron *and* thorium *and* holds >60% of mare basalt by
area — a dominant answer would kill the region choice. Frame regions by
*what you will have to import*: iron-rich ground that starves you of
aluminium is a real decision. Needs the generator to produce genuine
complementarity, not a quality ranking.

`[?]` **Composition currently does nothing at all.**
`Colony::GetArchetypeBonus` is declared, defined and **never called
anywhere in the repository**. Until the generator is inverted so the
region decides abundance — rather than a label being derived from
abundance scattered with no reference to where on the Moon the playfield
sits — the region panel reports flavour, not fact. Also note thorium is
currently synthesised as `(Fe + Ca) × 0.5`, making KREEP an artifact of
iron.

### 4.7 Rock abundance is terrain, not a resource

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

**DECIDED — it goes in the terrain group on the region panel.** Not as a
resource, and not as a site row. At region scale it answers a question
the player actually has when choosing where to settle: *is this rough,
bouldery ground or open plain?* That is terrain character, and it sits
next to mean slope.

It is deliberately **not** repeated on the site panel. `roughnessM` (RMS
residual after a least-squares plane, from real LOLA/SLDEM at 59 m)
already drives the site-level veto from measured data, so a second
bouldery-ground row there would be two names for one decision.

The two are the same physical idea at two scales serving two different
questions — *what kind of region is this* against *can I build on this
exact spot* — which is why both earn their place and neither duplicates
the other.

*Honest note:* rock abundance really does resolve at ~237 m, finer than
the 1.5 km build footprint, so confining it to the region panel discards
precision the data has. That is a deliberate trade for one rule with no
exceptions: everything on the region panel is frozen, everything on the
site panel is live. If playtesting shows boulders need to veto individual
spots, `roughnessM` is already there to do it.

### 4.8 What the region panel actually shows

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

### 4.9 Hints — the teaching layer

**DECIDED.** Every number and label on the panels carries a short,
hover-only hint saying what a value of that size *means* for the
player's plans: hover "titanium 2.5 wt%" and the game explains what
low titanium is good for; hover the rock name or the PSR note and it
explains those. Never permanently visible — the panels stay numbers,
the hint appears while the cursor rests on a row and vanishes when it
leaves. A dotted underline is the affordance marking a row as
hoverable.

**It is data, not logic** — `src/survey_hints.h`, following the
`ResourceDescriptor` table pattern:

- **Banded quantities** (`SurveyHintBand`): each key carries its own
  low/high thresholds and three texts, so "high titanium" is defined
  exactly once, in the one place the whole game reads. Thresholds
  follow the real classifications where one exists: the TiO₂ bands are
  the low/high-Ti basalt scheme (low < 3, high > 6 wt%), the thorium
  high band is the PKT contour (3.5 ppm), iron splits feldspathic from
  mafic ground at 8–10 wt%.
- **Categorical hints**: rock types matched against the region's
  dominant-rock string (mixed/shore forms first), and PSR proximity
  banded by haul distance (≤ 15 km = in rover reach).
- `GetSurveyHint(key, value)`, `GetRockHint(rock)`, `GetPsrHint(km)`
  return a title ("LOW TITANIUM") plus one plain-language paragraph.

**Wording rule** (inherits §5.0): a hint may name what a resource is
*for* in this game's design — "feedstock for alloys" — but must not
claim a live consequence from a system that does not exist yet.
Wording that implies simulation ("production has stalled") lands with
the system that simulates it.

Why this earns its place: the hint is where the teaching lives, so the
panels never need explanatory clutter, and the surprising cases land —
the industrial mare hovering "titanium" and learning it is *low*-Ti
plain-metal country is exactly the nuance the descent should teach.

Verified in `lunar_map --demo`: each demo site's L1 frame renders one
row hovered with its hint open (imbrium → low titanium, apennine → the
shore rock, shackleton → PSR in haul range).

---

## 5. Implementation plan

### 5.0 The coherency contract — what the game must make true

A level's question is only a decision if some game system consumes the
answer. Audit of where each dependency stands today:

| Decision | Is real only if | Status today |
|----------|-----------------|--------------|
| L1 chemistry mix | construction consumes Al/Ca; alloys/machinery consume Fe/Ti | `CONSTRUCTION_MATERIALS` exists as a type with **zero producers or consumers** — MISSING |
| L1 latitude | energy scales with sun; the 14-day night forces storage or shutdown | no lunar night in `TimeManager`; `solarIllumination` has **zero consumers** — MISSING |
| L2/L3 PSR distance | water is extractable from PSR ice, hauling cost grows with distance | WATER is consumed (farming) but **nothing produces it**; no ice extraction — MISSING |
| L2–L4 distances | transport between sects is priced by distance | transport system exists (auto-balance, deficit) but is **not distance-priced** — PARTIAL |
| L4 adjacency | expansion onto neighbouring cells | `BuildNewSect` on the grid — PARTIAL |
| L5 ground | slope/relief/PSR gate the build | `EvaluateSite` + `JudgeSite` — **DONE** |
| the commit gamble | prospecting reveals local truth after founding | prospecting module implemented — LARGELY DONE |

Two rules follow:

1. **No level may lie while its system is missing.** The panels show
   only measured terrain and real region identity, so every row is true
   today; the strategic *consequences* arrive as each chain is built.
   Never show a consequence (e.g. "night shuts down production") before
   the system exists.
2. **Build the cheapest reality first.** Priority order:
   **C1** construction consumes Al/Ca, alloys consume Fe/Ti — **done**.
   Not the pure-data change this list first assumed: the tables existed
   but Manufacture produced nothing, and *nothing in the game had a
   build cost at all*, so there was no consumer for either branch. Tier
   upgrades became that consumer — they were live, reachable and free.
   `tools/c1test/` asserts the economics behaviourally (see §5.1);
   **C2** lunar night + illumination-scaled energy — makes latitude and
   level-5 illumination matter;
   **C3** water chain with PSR ice extraction — makes the polar strategy
   exist at all;
   **C4** distance-priced transport — makes levels 2–4's geometry bite.

### 5.1 C1 — the materials split (done)

The first coherency chain, and the one that makes the level-1 region
choice mean something the first time it is played.

**Two branches on different elements**, in `game_constants.h`:

```
metals        Fe 2.0 + Ti 0.5 + ENERGY 3.0  ->  ALLOYS
construction  Al 1.5 + Ca 1.0 + ENERGY 2.0  ->  CONSTRUCTION_MATERIALS
```

The split is real geochemistry rather than invented balance: rock is
plagioclase (Al, Ca) plus mafic minerals (Fe, Ti), and more of one is
less of the other. A mare region leads on metals and starves for
structural stock; a highland region the reverse.

**Both are required by the consumer.** `MODULE_TIER_UPGRADE_COSTS` gives
every module tier a price in *both* ALLOYS and CONSTRUCTION_MATERIALS. A
module that declares its own `upgradeCosts` keeps them; everything else
falls back to the shared table, so the split has teeth everywhere
without a per-module cost list. Neither region can supply itself alone.

**Three things this turned up that the plan had wrong:**

- *Nothing in the game had a build cost.* `Sect::BuildUnit` is a
  `TODO` stub with no callers, and unit construction is a bare timer. So
  "add build costs" would have built an unreachable feature. Module tier
  upgrades were the right consumer instead: live, reachable, and free
  until now.
- *Consumption is derived, not declared.* `CalculateConsumption()`
  clears each module's `consumptionRates` every tick and rebuilds them
  from the unit's `productionCosts` table times its production rate.
  Setting consumption per module — the obvious first move — is silently
  overwritten. The fix is one line: give the Manufacture unit its cost
  table, as Extraction and Farming already do.
- *A compile is not a test.* The first implementation built cleanly and
  produced both outputs at full rate while consuming nothing, because of
  the point above. Only running it caught that.

**Verified behaviourally** by `tools/c1test/`, which drives the real
`Unit` production loop against a real `Sect`'s shared storage:

| Fed | ALLOYS | CONSTRUCTION_MATERIALS |
|-----|--------|------------------------|
| everything | 48.0 | 48.0 |
| mare (no Al/Ca) | 48.0 | **0.0** |
| highland (no Fe/Ti) | **0.0** | 48.0 |

and asserts that a tier upgrade is refused with either branch alone,
allowed with both, and actually spends both. The tech gate is unlocked
first, so a refusal proves the *cost* gate and not the tech gate.

---

### Step 1 — Cursor infrastructure — **DONE**
`src/TerrainGen/survey_cursor.{h,cpp}`: pure geometry, no game or render
code, shared by the game and the `lunar_map` instrument. Screen ↔ km ↔
lat/lon, grid snapping, clamping, and the descent stack. Verified by
`survey_cursor_test` (headless, 30 checks) and visually by
`lunar_map --ladder`.

**Simplification note:** the ladder table is now longer than this design
needs. Leave it — it costs nothing, the tests cover it, and the game
simply uses the two entries it cares about.

### Step 2 — Region identity and panel
- **JSON loader for `src/assets/planet/zones.json`** (73 real features,
  already shipping, currently unread) plus a nearest-feature-within-radius
  lookup by lat/lon. The game has no JSON parser today: one dependency, or
  a short reader for this one flat schema.
- A five-entry terrane table (Jolliff 2000) for the orbital tier.
- Render the region card: **name first**, then metals / building /
  science, one value each. No bounds.
- Ghosted previous-region panel alongside, for comparison.
- **Verify:** the values do not change while the cursor moves, or at any
  level below 100 km.

### Step 3 — Site terrain panel
- Live `LolaDem::EvaluateSite` at the cursor's footprint.
- Green/red verdict using the existing `JudgeSite` thresholds.
- **Volatiles row lives here** (§4.6): illumination and PSR from measured
  elevation, not a composition bar.
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
| One region dominates (PKT leads on Fe, Th and mare coverage) | Frame regions as trade-offs — what you must import — not as a quality ranking (§4.6) |
| The region panel is flavour because composition has no effect | `Colony::GetArchetypeBonus` is dead code. Invert the generator so the region decides abundance (§4.6) — do not bolt on multipliers |
| Colour fill hides the real imagery, which carries the same information | Fill only the orbital disc; outline only below it (§4.5) |
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

