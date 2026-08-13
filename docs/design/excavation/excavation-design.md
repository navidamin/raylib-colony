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

### The four rules that make this sturdy

These matter more than the numbers. Break any one and the gamble becomes a slot machine.

**Rule 1 — Truth is fixed. Only knowledge changes.**
`GetGroundTruth()` already returns a fixed value per spot. What the player sees is that
truth blurred by how little they know. The blur is a **stable offset seeded from the spot's
coordinates** — the same spot always shows the same wrong number until you survey it more.
Never re-roll per tick. A player who digs the same spot twice must get the same answer.

**Rule 2 — Confidence 1.0 means no gamble, and that's the point.**
```
shown estimate = truth ± spread
spread         = maxSpread × (1 − confidence)
```
At full confidence the estimate *is* the truth, the gamble disappears, and excavation
becomes a pure optimization. That's not a failure of the design — it's the reward for
surveying, and the module's arc: **early game it's a bet, late game it's a craft.**

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

### Why anyone would dig blind

Because surveying costs energy and time you may not have, and its payoff scales with how
long you'll stay.

```
value of surveying ≈ (best spot − average spot) × how long you'll work here
```

With the current 0.3×–2.0× spread, an average spot is worth about 1.15× baseline, while the
best of 9 is around 1.8×. **So surveying before you dig is worth roughly +60% output** — if
you're going to be there long enough to collect it.

That gives a clean arc without any special-casing:

| | Grid | Best-of | Surveying worth | Because |
|---|---|---|---|---|
| **Early** | 3×3 = 9 spots | ~1.8× | ~+60% | But you're power-starved and every tick surveying is a tick not producing |
| **Late** | 6×6 = 36 spots | ~1.95× | ~+70% | More spots means more spread to exploit, and you have power to spare |

So blind digging is *right* early and *wrong* late. The player's instinct changes over the
campaign on its own.

> These figures assume the sub-cell variation is roughly uniform across 0.3–2.0. Worth
> checking against the actual generator before balancing on them.

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

**Needs checking against the code**
- `[?]` Is sub-cell variation actually uniform across 0.3–2.0? The +60% figure for surveying depends on it.
- `[?]` Does `SubCell::aggregateConfidence` vary by depth, or is it one value per sub-cell? Rule 2's blur needs per-depth confidence to work properly at depth.
- `[?]` What does prospecting currently do when a spot is dug out — does it know, or does it keep reporting the original truth?

**Can wait**
- `[?]` The Blower's gas consumable needs a gas production chain first
- `[?]` Machine wear feeding a spare-parts economy needs the Manufacturing unit
- `[?]` Day/night power duty cycle needs the Energy unit redesign
