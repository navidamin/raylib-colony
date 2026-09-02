# Excavation — Mining the Block Model

> Status: **SKETCH** — deliberately less settled than its prospecting counterpart
> Last Updated: 2026-08-25
> Parent: [README.md](README.md)
> Requires: [prospecting/block-model-design.md](../prospecting/block-model-design.md),
> [subsurface/module-interplay.md](../subsurface/module-interplay.md)
>
> 2026-08-26 note: prospecting has dropped depth gating, its reach ring and
> its tier entirely (rigs + techniques —
> [prospecting/progression-design.md](../prospecting/progression-design.md)).
> Two consequences for this document: (1) excavation **keeps its own tier and
> reach for now** — its reach is about hauling and machines, not instruments —
> but its machines already *are* the instrument frame, so when its tier
> dissolves it dissolves the same way; (2) the lattice has moved from 8 × 8
> through 16 × 16 to 32 × 32, and **every footprint number below was measured on 8 × 8**
> (the 3 × 3 shaft = 37.5 m; the same metres are 12 × 12 at 32 × 32). Re-run
> `colony_measure_clusters` before trusting any of them on the new lattice.
>
> **Relationship to [`excavation-design.md`](excavation-design.md):** that
> document is built and shipped (Phases 1–6). Its four pillars — the panel, the
> place, the gamble, the machinery — all survive. What changes is the *thing
> being pointed at*: a spot on a flat lattice becomes a block in a model with a
> shape. Read that document first; this one records only the deltas.

**Why this is a sketch and not a design.** Prospecting has to be built first —
there is no block model to mine against yet — and several of the decisions here
depend on how the model actually feels to read. Committing them now means
committing them blind. What follows is the shape of the work and an honest list
of what is not decided.

---

## 1. What Survives Unchanged

| Pillar | Why it still holds |
|--------|-------------------|
| **Precision as the linking stat** | A sloppy machine digs wider than you aimed and averages your block with its neighbours, throwing away the survey you paid for. This gets *stronger* with a block model, not weaker — now there is a real grade gradient to average across |
| **The gamble** | Digging ground you did not pay to survey. Unchanged, and now it has a name the player can see: Inferred |
| **The machines** | Six machines, different depth / precision / pace / wear. Untouched |
| **No purity value** | Beneficiation's separation chain *is* the purity system. Stage 1 hands over a composition; a scalar would say the same thing twice |
| **AUTO by default** | Machinery is optional. A player who never opens the machine bay still plays |

---

## 2. What Changes

### From a spot to a block

Today excavation picks one of 64 lattice spots and a depth. That stays, but the
spot now has properties it did not have: an estimated grade, a confidence, a
class, and a **worked fraction**. The panel's grid becomes a slice through the
same block model prospecting draws — the same colours, the same class ring,
the same element switcher.

The player should be able to move between the two panels without relearning
anything. Prospecting draws the model to decide where to *look*; excavation
draws it to decide where to *dig*.

### Access: the constraint prospecting cannot have

A block at depth `d` is workable only if it is connected to the surface.

**Strip** — every layer above it in its column is worked out. Free to check;
`SubCell::workedFraction[4]` already exists, so this is a predicate over state
the game already keeps:

```cpp
bool IsStripped(const SubCell& c, int depth)
{
    for (int d = 0; d < depth; ++d)
        if (c.workedFraction[d] < STRIP_COMPLETE_FRACTION) return false;
    return true;
}
```

**Shaft** — sunk at a spot, down to a chosen depth, opening the 3 × 3 block
centred on it at every depth it passes.

3 × 3 is **measured, not guessed** — `colony_measure_clusters`, all 400 planet
cells × 4 depths × every resource:

| Footprint | Of the lattice | Yield opened, best-placed | Concentration |
|-----------|---------------|--------------------------|--------------|
| 2 × 2 | 6% | 14.3% | 2.3× |
| **3 × 3** | **14%** | **30.8%** | **2.2×** |
| 4 × 4 | 25% | 47.2% | 1.9× |
| 5 × 5 | 39% | 61.3% | 1.6× |

A shaft opens under a third of a field: a way *in*, never a way to take the
whole thing. Rich ground averages 15 of 64 sub-cells in a 4.8 × 4.8 box and
fits inside a 3 × 3 only 7% of the time, so **shafts compose rather than
solve**, and siting stays a recurring decision. Siting well is worth ~2.2× over
siting at random.

> One thing the 3D subsurface model changes here, and it is significant. Under
> the current generator each depth re-rolls its clusters, so a shaft's four
> layers are four unrelated lotteries. With a dipping shoot, a shaft sunk down
> a **plunging** body follows the ore instead of leaving it — and a shaft sunk
> straight down through a steeply dipping one exits the side of it almost
> immediately. Shaft siting becomes a question about geometry rather than a
> weighted coin. That is a much better mechanic, and it does not exist until
> prospecting does.

### Selectivity and dilution — the bridge to beneficiation

A machine cannot cut a 12.5 m block exactly. Its **selective mining unit** is
how finely it can carve, and anything smaller than that gets mixed:

```
   precise machine              blunt machine
   ┌───┬───┬───┐                ┌───────────┐
   │   │███│   │  takes ore     │▒▒▒███▒▒▒▒▒│  takes ore AND the waste
   └───┴───┴───┘  only          └───────────┘  either side of it
     higher grade out             more tonnes out, lower grade
     slower, more energy          faster, cheaper, dirtier
```

Dilution is not a penalty number. It is more of everything-else in the
composition map Stage 1 already hands to beneficiation, which then has more to
separate. The three modules chain honestly: **prospecting says what is there,
excavation decides how precisely to cut it, beneficiation cleans up the mess.**

### The empty-out

Covered in [`module-interplay.md`](../subsurface/module-interplay.md) §3. From
excavation's side it is three obligations:

1. Call `RecordExcavation` with the fraction actually taken
2. Refuse a target that is not connected, with a reason the panel can show —
   *"2 layers above"* and *"no shaft in range"* are different problems
3. Draw the void, so the player is not planning against ore they already took

---

## 3. What Is Not Decided

Everything here changes the data model, not the tuning. **Do not build against
any of it until prospecting exists and the model has been played with.**

**Shape**

- `[?]` Does a shaft open its footprint at every depth it passes, or only at its
  terminal depth? Every depth is more forgiving and probably right.
- `[?]` Can a shaft be sunk at an angle, following a plunging shoot? Deeply
  attractive, and it doubles the module's UI.
- `[?]` Is the block the mining unit, or is there a bench that spans several?

**Ownership**

- `[?]` Shaft per unit, or per sect? Per unit is simpler and self-contained;
  per sect makes siting a settlement decision and lets two units share a way
  in. Leaning per unit until a reason appears.
- `[?]` Does the shaft live on the excavation facade or on `ProspectingGrid`?
  The facade is cleaner; the grid is what persists.

**Economy**

- `[?]` Is stripped material kept? The design assumes yes. If not, stripping is
  pure cost and shafts always win, which collapses the choice.
- `[?]` `STRIP_COMPLETE_FRACTION` — is a 90%-worked layer stripped, or must it
  be 100%? 100% explains better; 90% avoids a frustrating last sliver.
- `[?]` Does a partly-worked block lose grade as it empties (best rock first)?
- `[?]` Every shaft cost figure — calibrate against dumped data in the balance
  pass, never guess now.

**Interface**

- `[?]` Does the module need a build queue, or is a shaft an instant spend plus
  a timer? Nothing in excavation has a queue today, and adding one is a bigger
  change than it looks.
- `[?]` How much of the block model does the excavation panel show — the full
  four-layer view, or one working level at a time? The full view risks two
  panels that look identical; one level risks losing the shape entirely.

---

## 4. Why Access Is Worth The Trouble

Without it, excavation and prospecting make the same decision with a different
verb: *click the spot with the best number.* Access is the one mechanic
excavation has that prospecting structurally cannot, because a drill hole is a
needle and a working face is a volume.

It also gives surveying a second and far sharper reason to exist. Surveying to
pick a spot is worth +33% at T0 to +130% at T3. Surveying to **site a shaft**
is worth the whole build — and once the ground has a 3D shape, it is worth
knowing which way that shape goes before committing to a hole in it.
