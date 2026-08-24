# Excavation — The Design

> Status: DRAFT — the agreed frame, firmed up
> Last Updated: 2026-08-24 (added §2 access, and the three confidence classes in §3)
> Parent: [README.md](README.md)
>
> Supersedes [design-options.md](design-options.md) and
> [design-options-v2.md](design-options-v2.md). Those were exploration; this is the design.

---

## 1. The Frame

> **You point a machine at a spot in the ground and tune how hard it works.**
> **Prospecting tells you which spot is worth it — if you paid for prospecting.**

Five pillars, and each one is real rather than invented:

| Pillar | What it is | Where it comes from |
|--------|-----------|--------------------|
| **The panel** | Target material, pace, power cap. What you read is how much of what you dug is actually the target | The friendliest of the four earlier options, and a natural fit for AI |
| **Place** | Which spot in the grid, and how deep. No reach, no names | Prospecting's grid — already built |
| **The gamble** | Digging without having paid to survey first | Prospecting costs energy and time; skipping it is a real choice |
| **Machinery** | A stable of machines with genuinely different jobs | The `Excavator` struct and the four `method` strings already in the code |
| **Access** | Deep ground has to be connected to the surface — stripped, or reached by a built shaft | The one thing a working face needs that a drill hole doesn't |

### What was wrong before, and is now fixed

An earlier draft claimed there was no variation inside a sect's cell, so "place" had to mean
depth only. **That was wrong** — it looked at `ResourceManager` alone and missed that
prospecting builds its own finer grid on top. Prospecting already provides everything
excavation needs:

| Already implemented | Where |
|---------------------|-------|
| A **fixed 8×8** sub-cell lattice per extraction unit — sub-cell size never changes | `prospecting_constants.h:14` |
| Tier extends **reach**, as concentric rings: **T0 2×2 · T1 4×4 · T2 6×6 · T3 8×8** | `prospecting_constants.h:18` |
| Reach helpers: `GetReachForTier`, `IsSubCellInReach`, `TierRequiredForSubCell` | `prospecting_types.h:135-144` |
| Real spatial variation — each sub-cell runs **0.3× to 2.0×** the cell average | `prospecting_constants.h:79-80` |
| Per-sub-cell, per-depth truth: `GetGroundTruth(subX, subY, depth)` | `prospecting_grid.h:22` |
| Per-sub-cell confidence: `SubCell::aggregateConfidence` | `prospecting_types.h:106` |
| Depth access gated by prospecting tier: **1 → 2 → 3 → 4 layers** | `prospecting_constants.h:15` |
| Grid reachable from the unit: `prospectingSystem->GetGrid()` | `prospecting_system.h:24` |

So excavation invents no geography. It reads prospecting's grid and digs in it.

---

## 2. Place

**A spot is one sub-cell at one depth.** That's the whole of place.

- The lattice is a **fixed 8×8** — 64 spots per depth, each always the same size
- **Reach grows with tier**, as rings out from the sect: 2×2 → 4×4 → 6×6 → 8×8
- Four depths, gated by what your machine can reach
- You point at a spot; the excavator works it until it's exhausted or you move it
- Spots deplete individually, so the worked-out area spreads across the grid over time

No reach *slider* — reach is a tier unlock, not a dial. And no names: a name would be
invention over a 12 m square.

### Excavation has its own reach

Prospecting's reach comes from its own tier. **Excavation should read `IsSubCellInReach`
with the *excavation* module's tier**, not prospecting's — the helper already takes tier as
a parameter, so this costs nothing to implement and produces the module's best structural
idea:

| If… | Then… |
|-----|-------|
| Excavation reach **>** prospecting reach | You can dig ground you have no way to survey. **This is the gamble, made structural** |
| Prospecting reach **>** excavation reach | You know about good ground you can't yet touch — a concrete reason to upgrade excavation |
| Equal | The normal case: survey what you can dig |

So the two tiers become a real build decision — *upgrade prospecting to know more, or
excavation to reach further?* — and the gamble stops depending only on a statistical blur.
It becomes a place on the map you can see and can't yet learn about.

### Depth costs access

Reach is lateral. **Access is vertical, and it is not free.**

A drill hole is a needle: it goes wherever you point it and needs nothing but a rig on the
surface. A working face is a volume, and a volume has to connect to the surface. That single
asymmetry is the main thing keeping excavation from being prospecting with a different verb
— without it, both modules reduce to *click the spot with the best number*.

**The rule: a spot at depth `d` is workable only if it is connected.** There are two ways to
connect it, and they are a real choice rather than a progression.

| Way in | What it costs | When it's right |
|--------|--------------|-----------------|
| **Strip** — work out every spot above it in the same column first | Time and machine-hours spent on low-grade overburden. You keep everything you dig | One good spot; shallow ground; or the layers above are worth something on their own |
| **Sink an access** — a built shaft at one spot, down to a depth you choose | CONSTRUCTION_MATERIALS, a long build, and a standing power draw for as long as it exists | A whole ore body at depth, worked over a long time |

**Strip needs no new state.** A spot is stripped when every spot above it in its column is
worked out — which is exactly the `worked` flag Rule 5 already requires. So the cheap half of
access costs nothing to implement; it reads state the module has to keep anyway.

**A shaft opens a neighbourhood, not a column.** Sinking one at (x, y) down to depth `d`
makes the **3×3 block centred on it** workable at every depth down to `d`.

**3×3 is measured, not guessed.** `colony_measure_clusters` reads the real generator across
all 400 planet cells × 4 depths × every resource — 8,948 fields, the full population at the
standard seed:

| Footprint | Share of the lattice | Share of a field's yield, best-placed | Concentration |
|-----------|---------------------|--------------------------------------|--------------|
| 2×2 | 6% | 14.3% | 2.3× |
| **3×3** | **14%** | **30.8%** | **2.2×** |
| 4×4 | 25% | 47.2% | 1.9× |
| 5×5 | 39% | 61.3% | 1.6× |

A perfectly-sited 3×3 opens **under a third** of what is there, at more than double the
average yield density. It is the last footprint on the good part of that curve — 4×4 starts
to feel like it solves the ore body rather than opening a door into it.

**A shaft cannot swallow a body — and that is the good news.** Rich ground (sub-cells at
≥ 1.5× the field's mean) averages **15 of 64 sub-cells in a 4.8 × 4.8 bounding box**, and
fits inside a 3×3 only **7%** of the time. So one shaft is a way *in*, never a way to take
the whole thing: working a body out means a second shaft, or a shaft plus stripping, or
accepting the third you can reach. **Shafts compose rather than solve**, which keeps siting a
recurring decision instead of a single puzzle solved once per body.

> An earlier draft of this section claimed *"one well-sited shaft serves a whole blob."*
> The measurement says otherwise and the text above is the corrected version.

Siting still matters: a *randomly* placed 3×3 gets its 14% area share, a well-sited one gets
30.8%, so choosing well is worth **~2.2×**. Enough to reward surveying at depth, not so much
that a mediocre site is unrecoverable.

| Shaft depth | Opens | Roughly |
|-------------|-------|---------|
| To layer 2 | 9 spots × 2 depths | Cheap, quick, usually strip is better |
| To layer 3 | 9 spots × 3 depths | The normal mid-game shaft |
| To layer 4 | 9 spots × 4 depths | Expensive, slow, and the deepest ground is also the least surveyed |

Depth re-rolls the clusters (§3), so the 3×3 that is rich at layer 2 is **not** the 3×3 that
is rich at layer 4. A shaft sunk to layer 4 opens four independently-placed ore bodies' worth
of ground, most of which will be ordinary. That is the deep shaft's real cost, and it is
structural rather than a price tag.

> Balance note: cost and build time should scale with depth **faster than linearly**, so that
> a layer-4 shaft is a campaign decision and not a default. Exact figures belong to the
> balance pass; the shape is what matters here.

### Why access makes the gamble bigger, not just slower

Sinking a shaft is a **commitment made before the payoff is known**, which is a different
kind of bet from digging one blind spot:

- Digging blind risks **one spot's worth** of machine time. You find out quickly and you
  keep whatever came up.
- Sinking blind risks **the whole build** — materials, days, and a standing power draw — on
  ground you may not have surveyed at that depth. And per §3, *depth re-rolls the clusters*,
  so a rich layer 1 tells you nothing about layer 3.

That gives surveying a second, much sharper reason to exist. Surveying to pick a spot is
worth +33% to +130% (the table below). Surveying **to site a shaft** is worth the entire
build. It is the first point in the module where the player has a strong reason to survey
ground they cannot yet dig.

**It also makes the two modules' upgrades stop competing on one axis.** Prospecting tier buys
knowledge, excavation tier buys lateral reach, and access buys depth — three purchases, three
different reasons.

### Keeping access friendly

Access must not complicate the opening. It doesn't:

| Tier | Depths available | Shafts | What access feels like |
|------|-----------------|--------|----------------------|
| **T0** | 1 (surface only) | — | Invisible. There is nothing below to reach |
| **T1** | 2 | 1 | "I can strip down to layer 2, or build one way in" |
| **T2** | 3 | 2 | Siting starts to matter |
| **T3** | 4 | 4 | Planning an operation around where the ways in are |

At tier 0 the whole system is dormant, because `MAX_DEPTH_PER_TIER[0]` is 1 — there is no
depth to be locked out of. The first shaft arrives exactly when the second depth does.

AUTO handles it the same way it handles machines: it strips when stripping is cheaper and
proposes a shaft when it isn't, so a player who never thinks about access still plays a
complete game.

---

## 3. The Gamble

**The gamble is choosing to dig without paying to survey.**

It is not random noise added to output. It is the difference between what you *think* is in
a spot and what's *actually* there — and that difference is entirely under the player's
control, because surveying shrinks it.

### The five rules that make this sturdy

These matter more than the numbers. Break any one and the gamble becomes a slot machine.

**Rule 1 — Truth is fixed. Only knowledge changes.**
`GetGroundTruth()` already returns a fixed value per spot. What the player sees is that
truth blurred by how little they know. The blur is a **stable offset seeded from the spot's
coordinates** — the same spot always shows the same wrong number until you survey it more.
Never re-roll per tick. A player who digs the same spot twice must get the same answer.

**Rule 2 — Confidence 1.0 means no gamble, and that's the point.**
```
shown estimate = truth ± spread
spread         = maxSpread × (1 − confidence(spot, depth))
```
At full confidence the estimate *is* the truth, the gamble disappears, and excavation
becomes a pure optimization. That's not a failure of the design — it's the reward for
surveying, and the module's arc: **early game it's a bet, late game it's a craft.**

**Confidence is per spot *and* per depth**, so a spot can be well known at the surface and
guesswork below. Combined with `MAX_DEPTH_PER_TIER` (a tier-1 rig sees only two layers) and
machines that reach deeper than the rig can see, the lower layers stay a genuine bet long
after the surface is mapped.

**Rule 3 — Digging is the expensive way to prospect.**
Once you've dug a spot, you know it for real — permanently. So there are two roads to
knowledge:

| Road | Cost | Speed | Gets you material? |
|------|------|-------|-------------------|
| Survey it | Cheap energy, ties up the prospecting module | Fast | No |
| Dig it | Expensive energy, ties up a machine | Slow | Yes |

This is what makes skipping prospecting a legitimate strategy rather than a mistake, and it
means the grid fills in either way — the player is never stuck in the dark.

**Rule 4 — Never nag. Let hindsight do the teaching.**
No warning popup for digging blind. But because dug spots become known, the player later
sees the spot next door was twice as good. The *"I should have surveyed"* moment arrives
as a discovery, not a scolding — and it lands harder for it.

**Rule 5 — Digging writes back into prospecting's grid.**
When excavation works a spot at a depth, it sets that spot's confidence to **1.0** and marks
it as *known by digging* — visually distinct from *known by surveying*, because the two mean
different things:

| State | Confidence | Class shown | What it tells the player |
|-------|-----------|-------------|-------------------------|
| Unsurveyed | low | Unclassified → Inferred | A guess, with a wide range |
| Surveyed | rising | Indicated → Measured | What's there, and it's still there |
| **Dug** | **1.0, marked** | **Worked** | What was there — and how much you've taken out of it |

A spot that is 100% known *and emptied* is very different information from one that is 100%
known *and full*, so the mark has to carry that, not just say "known".

This has a useful side effect: since `surveyProgress` already gates extraction efficiency,
**digging blind slowly bootstraps your own efficiency.** A player who never surveys still
improves, just the expensive way — which is exactly the shape Rule 3 describes.

### The three classes, and one colour key

Rule 2 makes confidence a number. The player does not act on a number — they act on
*"can I commit to this?"* So confidence is presented in **three named bands**, borrowed
straight from how real resource statements are written (JORC, NI 43-101):

| Class | `aggregateConfidence` | Existing `ConfidenceLevel` | Colour | What the player does |
|-------|----------------------|---------------------------|--------|---------------------|
| **Measured** | > 0.80 | `CERTAIN` | `EXT_ACCENT_GREEN` {80,230,150} | Commit. Point the precise machine here |
| **Indicated** | 0.40 – 0.80 | `MODERATE` + `HIGH` | `EXT_ACCENT_GOLD` {255,200,80} | Probably. Worth digging, not worth a shaft |
| **Inferred** | 0.20 – 0.40 | `LOW` | `EXT_CLASS_INFERRED` {124,143,214} | A bet. Dig it with something wide and cheap |
| **Unclassified** | ≤ 0.20 | `VERY_LOW` | `EXT_DIM_TEXT` {120,138,165} | Blind. You know only the cell average |
| **Worked** | 1.0, and emptied | — | distinct hatch, per Rule 5 | Nothing left to decide |

> **The boundaries are the ones already in the code.** `prospecting_constants.h:47-50`
> defines `CONFIDENCE_THRESHOLD_LOW/MODERATE/HIGH/CERTAIN` at 0.20 / 0.40 / 0.60 / 0.80, and
> `GetConfidenceLevel()` already bands the same field five ways for crystal glow. The three
> classes are a **grouping of those bands, not a second opinion on them** — so the two
> readings cannot contradict each other and there are no new constants to keep in sync. An
> earlier draft of this section proposed 0.75 / 0.45 / 0.15; that was written before reading
> the existing enum and is superseded.

> `EXT_CLASS_INFERRED` is a new token. `EXT_ACCENT_VIOLET` {170,110,255} cannot be reused —
> it is within a few units of `EXT_HEADER_COLOR` {168,130,255}, so section headings and
> Inferred ground would read as the same thing. The muted violet is deliberate: Inferred is
> the class you are meant to notice *least*.

At each band's floor, Rule 2's `spread = maxSpread × (1 − confidence)` gives **≤ 0.20 ×
maxSpread** for Measured, **≤ 0.60 ×** for Indicated and **≤ 0.80 ×** for Inferred — so the
names describe an actual range the player will see, not a mood.

**The bands are presentation, not new state.** They are a grouping of thresholds the grid
already applies. Nothing in Rules 1–5 changes; this only gives the numbers a name and a
colour.

#### Why the bands earn their place

**They put a visual on the gamble.** The gamble is currently the module's central pillar and
has no key — the player is asked to weigh certainty with nothing on screen that names it.
Green / amber / violet says *commit / probably / betting* at a glance, on a 12 m square, with
no text.

**They give the shaft decision a vocabulary.** "Don't sink a shaft into violet ground" is a
rule a player can learn in one bad build and then apply forever. Without named bands the same
lesson is a vague feeling about a shading gradient.

**One key, three surfaces.** This is the real reason. The same three colours carry the same
meaning at three completely different scales:

| Surface | What the colour is doing |
|---------|-------------------------|
| The **resource icon** in the element switcher | A three-segment ring: how much of this element is Measured / Indicated / Inferred. Readable at 40 px |
| The **depth map** (four stacked layers) | Per-spot class, so the shape of what you know is visible at a glance |
| The **excavation grid** in the panel | Per-spot class, so target selection and survey planning use one key |

Because the ring is a summary of exactly the field the map shows in detail, a player can
glance at the icon bar and know which elements are drilled out and which are guesses —
**without opening anything.** That is only possible because colour was spent on class rather
than on element. If colour identified the element instead, the ring and the map would be
saying different things and the summary would be worthless.

The element identifies itself where it costs no per-cell channel: the icon, the tint on the
rock wall, the panel heading, and the shape of the relief — which is different for every
element anyway, because per §3 every resource has its hot spot somewhere else.

> Note on terminology: real codes convert *Measured resource* + economics into a *Proven
> reserve*. The game has a cutoff, so that mapping exists — but it adds a word without adding
> a decision, so the design stops at three classes.

### Why anyone would dig blind

Because surveying costs energy and time you may not have, and its payoff scales with how
long you'll stay.

```
value of surveying ≈ (best spot − average spot) × how long you'll work here
```

**These numbers are measured from the actual generator**, not assumed. The grid is built
from 1–2 Gaussian hot spots per resource per depth over the fixed 8×8 lattice, normalised
to mean 1.0 and clamped to 0.3–2.0. Simulating it 200,000 times
(`subcell_distribution_sim.py`) gives:

| Tier | Reach | Spots | Mean spot | Best reachable | **Surveying is worth** | **Best cell reachable** |
|------|-------|-------|-----------|----------------|----------------------|------------------------|
| T0 | 2×2 | 4 | 1.03 | 1.34 | **+33%** | **7%** |
| T1 | 4×4 | 16 | 1.01 | 1.86 | **+97%** | 25% |
| T2 | 6×6 | 36 | 0.96 | 2.00 | **+114%** | 50% |
| T3 | 8×8 | 64 | 0.87 | 2.00 | **+130%** | 100% |

Three things fall out of this, and they matter more than the individual figures.

**1. At tier 0 you are not choosing — you are stuck.** The grid's best spot is inside your
reach only **7%** of the time. Four spots, barely any spread, +33% for surveying them all.
Early game isn't about whether to survey; it's about being pinned to whatever lies directly
under the sect. That is a far better early-game story than "you should have surveyed", and
it makes the first reach upgrade land hard: T0 → T1 raises the best spot you can touch by
**1.40×** on its own, before any surveying.

**2. Expanding reach widens the spread at both ends.** The best reachable spot climbs
1.34 → 2.00, while the *mean* reachable spot **falls** 1.03 → 0.87. More ground is not
better ground; it's more varied ground. So choosing well matters more at every tier, and
choosing badly costs more — which is exactly the pressure targeted digging should be under.

**3. Surveying and reaching are different purchases.** Reach raises your ceiling; surveying
lets you find it. Reach alone tops out at T2 (the best spot is pinned to the 2.0 clamp from
there on), so past that point every further gain comes from knowing where to point. The two
upgrades stop being interchangeable.

> Balance note: the top end is limited by the **clamp**, not the grid — at T2 and T3 the best
> spot sits at 2.0. If late game needs more headroom, raise `SUBCELL_VARIATION_MAX`.

### The ground is clustered, and that matters

The hot spots are Gaussian blobs, not independent random cells. Four consequences worth
designing around:

1. **Surveying one spot tells you about its neighbours.** Rich ground is contiguous, so a
   partial survey is worth more than its share. Players can sample sparsely and interpolate
   — which is what real prospecting does.
2. **The good stuff looks like an ore body.** A blob rather than scattered noise. It reads
   correctly on screen and rewards working outward from a found peak.
3. **Every resource has its hot spot in a different place** — the seed includes the resource
   index (`HashSeed(px, py, depth, resourceIdx)`). So the best spot for iron is not the best
   spot for silicon. **This is what gives the panel's target selector real weight**: change
   what you're after and the right place to stand changes with it.
4. **Depth re-rolls the clusters too** — the seed includes depth. The best spot at layer 1
   is *not* the best spot at layer 3, so you cannot extrapolate downward. Surveying at depth
   is genuinely necessary rather than merely thorough.
5. **Hot spots don't care about your reach.** Cluster centres are placed across the whole
   8×8, so the good ground is wherever it is — which is why T0 sees the best spot only 7% of
   the time. Reach upgrades aren't a bigger number, they're access to ground that was always
   there.

There may also be a second, smaller hot spot (`numClusters` is 1 or 2) — a reason to keep
looking after you've found one.

### The deepest bet

Your machine can reach deeper than prospecting can see — machine depth and
`MAX_DEPTH_PER_TIER` are separate limits. Digging below your survey depth is the biggest
gamble available, and it needs no extra system: it falls straight out of two constants that
already exist.

---

## 4. Machinery

Machines are the module's substance, not a dropdown. You **build** them (they cost
MACHINERY), you **run** several at once, and they **wear out**.

### What separates them

| Stat | What it means | Why it's a real choice |
|------|--------------|----------------------|
| **Depth** | Deepest layer it can work (*not* the spatial reach rings — that's tier) | Hard gate. Some material is only reachable one way |
| **Precision** | How tightly it stays on the spot you aimed at | See below — this is the important one |
| **Pace ceiling** | How far you can push the pace slider | Some machines simply can't be hurried |
| **Power floor** | Minimum draw even when idling | Cheap machines are cheap to *own* |
| **Selectivity** | How much waste it brings up alongside the target | Shows up in the composition handed to beneficiation |
| **Wear** | How fast it eats itself | Fast machines are expensive to keep |

### Precision is where all three pillars meet

A **sloppy machine digs wider than you aimed**, so it averages your chosen spot together
with its neighbours. That has a sharp consequence:

> **A sloppy machine throws away the survey you paid for.** You spent energy learning that
> spot B2 is the best one, then dug it with something that brought up B1, B2 and B3 mixed
> together — and got the average anyway.

So the three pillars lock into one decision instead of sitting side by side:
- **Surveyed carefully?** A precise machine collects the full payoff
- **Digging blind?** A sloppy wide machine is *better* — if you don't know where the good
  spot is, covering more ground is exactly right
- **Precision costs speed**, so it's never free

That single stat makes "should I survey?" and "which machine?" the same question, which is
what the design was missing before.

### The stable

Six machines, drawn from the real technologies in
[excavation-mechanics.md](excavation-mechanics.md) Part 1. Names are plain; the science is
in the stats.

| Machine | Depth | Precision | Pace | Power | Select. | Wear | Its job |
|---------|-------|-----------|------|-------|--------|------|---------|
| **Scoop** | Surface | Low | Low | Very low | Low | Low | The starter. Cheap to own, never great |
| **Bucket Wheel** | Shallow | Low | **High** | Medium | Low | High | Volume. Perfect for digging blind |
| **Drum** | Shallow | **High** | Medium | Medium | High | **Very low** | The workhorse. Collects the payoff on surveyed ground |
| **Hammer** | **Deep** | Medium | Medium | **High** | Medium | **Very high** | The only way into hard deep ground |
| **Auger** | Mid | **Very high** | Low | Low | **Very high** | Medium | Surgical. For one known-rich spot |
| **Blower** | Surface | High | Medium | High + gas | High | Very low | Late game. Ignores hardness entirely, costs consumables |

Wheel and Drum both arrive early and are opposites — that's the first real machinery
decision, and which one is right depends entirely on whether you surveyed.

### Keeping it friendly

Machinery is **optional depth, not required upkeep**:
- **AUTO** picks a sensible machine for the spot and swaps when conditions change
- Repairs happen in the background once the workshop exists; manual repair only at tier 0
- A player who never opens the machine bay still plays a complete game, just ~15% behind

---

## 5. The Panel

One screen, three regions.

```
┌─────────────────────────┬────────────────────────────────┐
│  THE GRID               │  MACHINE BAY            [AUTO] │
│  ┌─┬─┬─┬─┬─┐            │  ┌────────┐ ┌────────┐         │
│  │·│▒│·│▓│·│  ← spots   │  │ Drum   │ │ Wheel  │  ...    │
│  ├─┼─┼─┼─┼─┤    in the  │  │ ●●●○○  │ │ ●●●●○  │         │
│  │▓│█│▒│·│·│    class   │  └────────┘ └────────┘         │
│  ├─┼─┼─┼─┼─┤    colour  ├────────────────────────────────┤
│  │·│⌷│▓│▒│·│            │  TARGET   [ Iron ▾ ]           │
│  └─┴─┴─┴─┴─┘            │  PACE     ──────●────          │
│  DEPTH  [1][2][3][4]    │  POWER    ────●──────          │
│  ■ Meas ■ Ind ■ Inf ⌷shaft                               │
├─────────────────────────┴────────────────────────────────┤
│  B2 · Iron 40–70%  · INFERRED      →  getting 31%        │
│  0.42 useful per power   · reached by strip (2 above)    │
└──────────────────────────────────────────────────────────┘
```

- **Grid** — every spot carries its **class colour**, the same green/amber/violet used by the
  depth map and by the ring on the resource icon. Unsurveyed spots show a wide range;
  Measured spots show a number. Dug-out spots are visibly worked
- **Access marks** — a shaft is drawn on the grid at its spot, with the 3×3 it opens; spots
  reachable only by stripping say how many layers stand above them
- **Machine bay** — cards, AUTO by default
- **Panel** — target, pace, power cap
- **The bottom line is the whole module**: what you thought was there, against what you're
  actually getting — with the class named, so *"I knew this was a bet"* is legible after the
  fact

---

## 6. Tier 0 → 3

| Tier | Grid | Machines | Panel | The feel |
|------|------|----------|-------|----------|
| **T0** | Pick a spot, surface only. No access system | 1, no choice, manual repair | Pace only | Point and dig |
| **T1** | 2 depths, 1 shaft | 2 running, pick types, AUTO available | + power cap | The first real machinery choice, and the first way in |
| **T2** | 3 depths, 2 shafts | 4 running, background repairs | + target selector | Optimizing, and siting |
| **T3** | All 4 depths, 4 shafts | 8 running, auto-rotation | + saved presets | Running an operation |

---

## 7. AI Automation

Everything the player does here is **setting a value**: which spot, which machine, pace,
power cap. So the AI needs no parallel system — it uses the same four inputs through the
same interface, and research improves *how well it chooses*, not what it's allowed to touch.

| AI level | What it does |
|----------|-------------|
| Off | Player sets everything |
| Basic | Picks the best **known** spot. Never gambles. Safe, slow |
| Trained | Weighs unknown spots by their potential — will gamble when the odds are good |
| Expert | Also schedules surveys, so it decides *when knowing is worth the delay* — and sites shafts, which is the same judgement with a much bigger stake |

That top level is the interesting one, because it's the same judgement the player makes.
The AI's quality is legible: watch whether it gambles when you would have.

---

## 8. What Excavation Sends Onward

Unchanged in shape from today:

```
Stage 1 (excavation)  → what came out of the ground   ← this design
Stage 2 (beneficiation separation chain)  — untouched
Stage 3 (sect storage)                     — untouched
```

The existing modifiers — Operations efficiency, Directives, module tier, machine count —
all still multiply in and keep their current meaning, so those modules keep working.

---

## 9. Open Questions

**Needs deciding before building**
- `[?]` Does the power cap draw on the sect's shared pool, or is it a local budget?
- `[?]` Access: is a shaft **per unit** or **per sect**? Per unit is simpler and keeps each
  extraction unit self-contained; per sect makes siting a settlement-level decision and lets
  two units share one way in. Per unit unless a reason appears
- `[?]` Can a shaft be abandoned or moved? If not, a bad siting is permanent — which is
  strong medicine. Probably: demolish for a partial materials refund, on a long timer
- `[?]` Does the material stripped on the way down get **kept**? The table above assumes yes.
  If it didn't, stripping would be pure cost and shafts would always win
- `[?]` When a spot's result disappoints, is it revealed gradually as you dig, or at the end? Slow dread vs sharp surprise — changes how the gamble feels.
- `[?]` Can several machines work different spots at once, or do they stack on one spot? Stacking is simpler; splitting is more interesting and needs more UI.

**Resolved**
- ✅ The distribution is **clustered Gaussian, not uniform** — measured against the fixed 8×8 lattice: survey payoff runs +33% at T0 to +130% at T3
- ✅ **No purity value in excavation.** Beneficiation's separation chain already is the purity system, and "dug dirty" is already carried by the composition map Stage 1 hands over. Pace changes that composition; it does not set a separate number
- ✅ Reach is a **tier-gated ring**, not a slider — and excavation reads it with its *own* tier, which makes the gamble structural
- ✅ Confidence varies **per spot and per depth**, so Rule 2 blurs per depth
- ✅ Dug spots write confidence 1.0 back into prospecting's grid, shown with a distinct mark (Rule 5)
- ✅ Confidence is shown as **three named classes**, not a raw number — Measured / Indicated /
  Inferred, on one colour key shared by the resource icon ring, the depth map and the
  excavation grid. Presentation only; no new state
- ✅ **Depth is gated by access, not by tier alone.** Strip through the column, or sink a
  shaft that opens a 3×3 block. Strip reads the `worked` flag Rule 5 already needs

**Needs building on the prospecting side**
- `[?]` `ResourceClass` (the 3-band grouping) belongs in `prospecting_types.h` beside
  `ConfidenceLevel`, deriving from the same `CONFIDENCE_THRESHOLD_*` constants — both modules
  classify one field and a second set of thresholds would let them drift apart
- `[?]` `EXT_CLASS_INFERRED` {124,143,214} is a new theme token in `rendermanager.cpp`; the
  other three classes reuse existing ones
- `[?]` `SubCell` needs a "worked" state — how much has been taken out, at which depths — for Rule 5's mark to render
- `[?]` Who owns the writeback: does excavation call into `ProspectingGrid`, or does prospecting poll excavation? A single setter on the grid is probably cleanest
- `[?]` Can excavation dig outside *prospecting's* reach if its own tier allows? The design above says yes — that's where the gamble lives — but prospecting currently refuses to sweep or drill out-of-reach cells, so the two modules need to agree that reach is per-module
- `[?]` `prospecting-master-design.md:90` still describes the old resizing grid ("previously surveyed sub-cells are re-divided"). Stale since the reach change — worth fixing on the prospecting side

**Can wait**
- `[?]` The Blower's gas consumable needs a gas production chain first
- `[?]` Machine wear feeding a spare-parts economy needs the Manufacturing unit
- `[?]` Day/night power duty cycle needs the Energy unit redesign
