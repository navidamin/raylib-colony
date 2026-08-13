# Excavation — The Design

> Status: DRAFT — the agreed frame, firmed up
> Last Updated: 2026-08-13
> Parent: [README.md](README.md)
>
> Supersedes [design-options.md](design-options.md) and
> [design-options-v2.md](design-options-v2.md). Those were exploration; this is the design.

---

## 1. The Frame

> **You point a machine at a spot in the ground and tune how hard it works.**
> **Prospecting tells you which spot is worth it — if you paid for prospecting.**

Three pillars, and each one is real rather than invented:

| Pillar | What it is | Where it comes from |
|--------|-----------|--------------------|
| **The panel** | Target material, pace, power cap. Purity is what you read, not what you set | The friendliest of the four earlier options, and a natural fit for AI |
| **Place** | Which spot in the grid, and how deep. No reach, no names | Prospecting's grid — already built |
| **The gamble** | Digging without having paid to survey first | Prospecting costs energy and time; skipping it is a real choice |
| **Machinery** | A stable of machines with genuinely different jobs | The `Excavator` struct and the four `method` strings already in the code |

### What was wrong before, and is now fixed

An earlier draft claimed there was no variation inside a sect's cell, so "place" had to mean
depth only. **That was wrong** — it looked at `ResourceManager` alone and missed that
prospecting builds its own finer grid on top. Prospecting already provides everything
excavation needs:

| Already implemented | Where |
|---------------------|-------|
| A sub-cell grid per extraction unit, sized by prospecting tier: **3×3 → 4×4 → 5×5 → 6×6** | `prospecting_constants.h:4` |
| Real spatial variation — each sub-cell runs **0.3× to 2.0×** the cell average | `prospecting_constants.h:66-67` |
| Per-sub-cell, per-depth truth: `GetGroundTruth(subX, subY, depth)` | `prospecting_grid.h:22` |
| Per-sub-cell confidence: `SubCell::aggregateConfidence` | `prospecting_types.h:106` |
| Depth access gated by prospecting tier: **1 → 2 → 3 → 4 layers** | `prospecting_constants.h:15` |
| Grid reachable from the unit: `prospectingSystem->GetGrid()` | `prospecting_system.h:24` |

So excavation invents no geography. It reads prospecting's grid and digs in it.

---

## 2. Place

**A spot is one sub-cell at one depth.** That's the whole of place.

- The grid is 3×3 at prospecting tier 0, up to 6×6 at tier 3 — so 9 to 36 spots per depth
- Four depths, gated by what your machine can reach
- You point at a spot; the excavator works it until it's exhausted or you move it
- Spots deplete individually, so the worked-out area spreads across the grid over time

No reach slider — the sect's cell was chosen when the player placed the sect, and that
decision is already made. No names — a name would be invention over a 100 m square.

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

| State | Confidence | What it tells the player |
|-------|-----------|-------------------------|
| Unsurveyed | low | A guess, with a wide range |
| Surveyed | rising | What's there, and it's still there |
| **Dug** | **1.0, marked** | What was there — and how much you've taken out of it |

A spot that is 100% known *and emptied* is very different information from one that is 100%
known *and full*, so the mark has to carry that, not just say "known".

This has a useful side effect: since `surveyProgress` already gates extraction efficiency,
**digging blind slowly bootstraps your own efficiency.** A player who never surveys still
improves, just the expensive way — which is exactly the shape Rule 3 describes.

### Why anyone would dig blind

Because surveying costs energy and time you may not have, and its payoff scales with how
long you'll stay.

```
value of surveying ≈ (best spot − average spot) × how long you'll work here
```

**These numbers are measured from the actual generator**, not assumed. The grid is built
from 1–2 Gaussian hot spots per resource per depth, normalised to mean 1.0 and clamped to
0.3–2.0 (`prospecting_grid.cpp:120-181`). Simulating that generator 200,000 times per grid
size gives:

| Prospecting tier | Grid | Median spot | Best spot | **Surveying is worth** |
|------------------|------|-------------|-----------|----------------------|
| T0 | 3×3 (9) | 0.96 | 1.48 | **+50%** |
| T1 | 4×4 (16) | 0.89 | 1.72 | **+77%** |
| T2 | 5×5 (25) | 0.80 | 1.88 | **+99%** |
| T3 | 6×6 (36) | 0.71 | 1.96 | **+114%** |

The mean stays near 1.0 by construction — the generator normalises to it. What changes with
tier is **resolution**: a coarse grid smears the hot spot across big cells, while a fine one
lets you point at the peak itself. Notice the median *falls* as the best *rises* — finer
grids spread the ground out at both ends.

**So the value of a survey more than doubles from T0 to T3, purely from grid resolution.**
Prospecting tier doesn't just reveal more, it makes the ground more worth knowing. That arc
needs no special-casing — it falls out of the generator that already exists.

The other half of the arc is cost: early on you're power-starved and every tick spent
surveying is a tick not producing, so a +50% payoff you have to wait for is a genuinely
hard sell. By T3 you have power to spare and a +114% payoff. **Blind digging is right early
and wrong late, on its own.**

> Balance note: at 6×6 the best spot is pinned to the 2.0 ceiling 82% of the time (at 3×3,
> only 14%). The clamp — not the grid — is what limits the top end at high tier. If T3 ever
> needs more headroom, raise `SUBCELL_VARIATION_MAX`; making the grid finer would add almost
> nothing.

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
| **Reach** | Deepest layer it can work | Hard gate. Some material is only reachable one way |
| **Precision** | How tightly it stays on the spot you aimed at | See below — this is the important one |
| **Pace ceiling** | How far you can push the pace slider | Some machines simply can't be hurried |
| **Power floor** | Minimum draw even when idling | Cheap machines are cheap to *own* |
| **Purity** | How clean its output is at a given pace | Directly sets what beneficiation receives |
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

| Machine | Reach | Precision | Pace | Power | Purity | Wear | Its job |
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
│  │ │▓│ │░│ │  ← spots   │  │ Drum   │ │ Wheel  │  ...    │
│  ├─┼─┼─┼─┼─┤    shaded  │  │ ●●●○○  │ │ ●●●●○  │         │
│  │░│█│▓│ │ │    by how  │  └────────┘ └────────┘         │
│  ├─┼─┼─┼─┼─┤    well    ├────────────────────────────────┤
│  │ │ │░│▓│ │    known   │  TARGET   [ Iron ▾ ]           │
│  └─┴─┴─┴─┴─┘            │  PACE     ──────●────          │
│  DEPTH  [1][2][3][4]    │  POWER    ────●──────          │
├─────────────────────────┴────────────────────────────────┤
│  B2 · Iron 40–70%  (guessing)      →  getting 31%        │
│  0.42 useful per power                                   │
└──────────────────────────────────────────────────────────┘
```

- **Grid** — spots shaded by how well they're known. Unsurveyed spots show a wide range;
  surveyed ones show a number. Dug-out spots are visibly worked
- **Machine bay** — cards, AUTO by default
- **Panel** — target, pace, power cap
- **The bottom line is the whole module**: what you thought was there, against what you're
  actually getting

---

## 6. Tier 0 → 3

| Tier | Grid | Machines | Panel | The feel |
|------|------|----------|-------|----------|
| **T0** | Pick a spot, surface only | 1, no choice, manual repair | Pace only | Point and dig |
| **T1** | 2 depths | 2 running, pick types, AUTO available | + power cap | The first real machinery choice |
| **T2** | 3 depths | 4 running, background repairs | + target selector | Optimizing |
| **T3** | All 4 depths | 8 running, auto-rotation | + saved presets | Running an operation |

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
| Expert | Also schedules surveys, so it decides *when knowing is worth the delay* |

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
- `[?]` Does purity change *what* beneficiation receives, or *how much power* it burns to separate it? The second is more interesting but reaches into beneficiation's design.
- `[?]` Does the power cap draw on the sect's shared pool, or is it a local budget?
- `[?]` When a spot's result disappoints, is it revealed gradually as you dig, or at the end? Slow dread vs sharp surprise — changes how the gamble feels.
- `[?]` Can several machines work different spots at once, or do they stack on one spot? Stacking is simpler; splitting is more interesting and needs more UI.

**Resolved**
- ✅ The distribution is **clustered Gaussian, not uniform** — measured, and the survey payoff runs +50% at T0 to +114% at T3
- ✅ Confidence varies **per spot and per depth**, so Rule 2 blurs per depth
- ✅ Dug spots write confidence 1.0 back into prospecting's grid, shown with a distinct mark (Rule 5)

**Needs building on the prospecting side**
- `[?]` `SubCell` needs a "worked" state — how much has been taken out, at which depths — for Rule 5's mark to render
- `[?]` Who owns the writeback: does excavation call into `ProspectingGrid`, or does prospecting poll excavation? A single setter on the grid is probably cleanest

**Can wait**
- `[?]` The Blower's gas consumable needs a gas production chain first
- `[?]` Machine wear feeding a spare-parts economy needs the Manufacturing unit
- `[?]` Day/night power duty cycle needs the Energy unit redesign
