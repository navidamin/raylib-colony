# Prospecting ↔ Excavation — The Contract, and the Empty-Out

> Status: **DESIGN** — not built
> Last Updated: 2026-08-25
> Parent: [README.md](README.md)
> Requires: [subsurface-model.md](subsurface-model.md)
> Pairs with: [prospecting/block-model-design.md](../prospecting/block-model-design.md),
> [excavation/block-mining-design.md](../excavation/block-mining-design.md)

---

## 1. The Two Loops

They look similar and are opposites.

| | Prospecting | Excavation |
|---|---|---|
| Spends | energy + time | energy + time |
| On | reducing uncertainty | moving rock |
| Produces | a model | tonnes |
| Effect on the block model | **fills it in** | **empties it out** |
| Its instrument | a needle — goes anywhere | a volume — must connect to the surface |
| Its failure | you drilled a barren hole | you mined ground that was not what you thought |

That last row is the one that keeps them from being the same screen with
different verbs. A drill hole is a line; it can be aimed anywhere within reach
at any angle. A working face is a volume; it has to be *reachable*, which means
through the ground above it or through a shaft. Prospecting has no equivalent
constraint and cannot be given one without becoming excavation.

```
        PROSPECTING                          EXCAVATION
   ┌──────────────────┐               ┌──────────────────┐
   │  spend to LEARN  │               │  spend to TAKE   │
   └────────┬─────────┘               └────────┬─────────┘
            │  raises confidence               │  removes tonnage
            ▼                                  ▼
        ┌───────────────────────────────────────────┐
        │            THE BLOCK MODEL                │
        │   grade · confidence · worked fraction    │
        └───────────────────────────────────────────┘
            ▲                                  │
            │        digging is direct         │
            └────── observation ───────────────┘
                    (§3, the empty-out)
```

The loop closes at the bottom: excavation is not only a consumer of the model,
it is the single most powerful *producer* of certainty in the game. You cannot
know ground better than by having removed it.

---

## 2. The Contract

Narrow on purpose, and one-directional.

### What prospecting provides

| Call | Returns |
|------|---------|
| `GetSubCellYield(grid, x, y, depth, type)` | tonnage of one resource in one block — `quantity × composition`, the product that matters |
| `GetDepthConfidence(grid, tray, x, y, depth)` | how well that block is known, **per depth** |
| `GetResourceClass(confidence)` | Measured / Indicated / Inferred / Unclassified |
| `GetClassSplit(grid, tray, type, tier)` | the whole statement, per element, per class |

All four exist today. Excavation already reads the first three.

### What excavation writes back

Exactly one call:

```cpp
void ProspectingGrid::RecordExcavation(int subX, int subY, DepthLayer depth,
                                       float fraction);
```

**Excavation calls prospecting. Prospecting never reaches into excavation.**
One setter, one direction, no cycle. That rule is already enforced by the code
and should survive every change here.

### The rule that falls out of the classes

Inferred ground cannot be committed to. `IsCommittable()` returns false for it.
Excavation is not *forbidden* from digging Inferred — that is the gamble, and
the gamble is the module's central pillar — but nothing that plans ahead
(auto-pilot, shaft siting, a build queue) may treat Inferred tonnage as real.

---

## 3. The Empty-Out

The part with the most consequence and the least prior art in this codebase.

### Three things happen when a block is mined

```
      before                          after
   ┌───┬───┬───┐                  ┌───┬───┬───┐
   │▒▒▒│███│▒▒▒│  grade           │▒▒▒│   │▒▒▒│  ← 1. tonnage gone
   ├───┼───┼───┤                  ├───┼───┼───┤
   │ ? │ ? │ ? │  confidence      │ ~ │ █ │ ~ │  ← 2. certain here,
   └───┴───┴───┘                  └───┴───┴───┘       better either side
                                    ▲   ▲   ▲
                                    └───┼───┘
                                        └── 3. a way through to what is below
```

**1 · The tonnage is gone.** `workedFraction[depth]` rises; available tonnage
is `original × (1 − workedFraction)`. The block model must show this, or the
player will keep planning against ore they already took.

**2 · The block becomes Measured — and so, partly, do its neighbours.** A dug
block was seen with its own eyes, so its confidence is 1.0. But a working face
is also a *cross-section*: standing in the void, you can see the rock on all
sides of it. So mining a block raises confidence in the blocks adjacent to it,
at reduced weight.

> This is the mechanic that gives a mine a *shape*. Certainty grows outward
> from what you have already taken, so mining next to your existing workings is
> cheaper in knowledge than teleporting to the best-looking spot elsewhere. It
> is also true — an underground mine is the best-mapped rock on a property.

**3 · It opens the way down.** A block cannot be worked unless it is reachable:
either every block above it in its column is worked out (**strip**), or a shaft
serves it. So the void is not just an absence, it is infrastructure. See
[`excavation/block-mining-design.md`](../excavation/block-mining-design.md) for
the access rules.

### What the player sees

The void has to be legible in the block model, and it must not be confusable
with "unknown". They are opposites — a void is the most-known state there is.

| State | Height | Colour |
|-------|--------|--------|
| Unclassified | prior estimate, flat | dim grey, no ring |
| Inferred / Indicated / Measured | estimated grade | class colour |
| **Partly worked** | remaining grade | class colour, **hatched** |
| **Worked out** | **flat to the floor** | dark, with a void outline |

Height already carries grade, so a mined block collapsing to the floor is the
correct and automatic reading: *there is nothing there any more*. A mine
becomes a widening flat scar cut into the relief, which is exactly what a mine
looks like from the air.

### The tension it creates

Mining the best ground first is obviously right, and the empty-out makes it
quietly wrong:

- The **best block** is usually in the middle of the shoot. Mining it strands
  its neighbours behind an access problem, because you mined down rather than
  across.
- Mining **outward from an edge** is less immediately profitable and leaves
  every subsequent block cheap to reach and well understood.

Neither is dominant, both are defensible, and the player will feel the
difference three hours later. That is the shape of a good decision.

### Reconciliation *(later, not v1)*

A hook worth leaving room for: compare what the model *predicted* a block held
against what the mill actually received. If your Inferred blocks consistently
under-deliver, your model is optimistic — and the game could tell you so, with
a running figure. It is how real mines learn to trust their own estimates. It
needs beneficiation throughput to compare against, so it is blocked on that
module rather than this one.

---

## 4. Sequencing

Neither module can be built against the other's future state.

```
   subsurface model (3D shoots)
            │
            ├──────────────▶  prospecting: drill holes + estimate field
            │                          │
            │                          ▼
            │                 block model exists
            │                          │
            └──────────────▶  excavation: access + selectivity
                                       │
                                       ▼
                              empty-out writes back
```

- **The subsurface model comes first.** Both modules read it, and without 3D
  continuity neither's central decision exists.
- **Prospecting before excavation.** Excavation's whole premise is choosing
  against a model; there is no model to choose against yet.
- **The empty-out comes last** and is the smallest of the three. `workedFraction`
  and `RecordExcavation` already exist; what is missing is the neighbour
  exposure, the access rule, and the void rendering.

---

## 5. Open Questions

- `[?]` **How much confidence does an adjacent void give?** It must be less
  than being dug and more than a sweep. Somewhere near a sample's weight, but
  it needs measuring against how fast it makes a mine self-mapping — too high
  and prospecting becomes optional past the first pit.
- `[?]` **Does exposure reach diagonally? Below?** A face exposes its sides;
  a floor exposes what is directly beneath it. Probably: 4-neighbours in plan
  at full weight, the block below at reduced weight, nothing above.
- `[?]` **Is `workedFraction` per resource or per block?** Per block — you
  remove rock, not elements. But then a block mined for iron also consumes its
  titanium, which needs to be visible or it reads as a bug.
- `[?]` **Does a partly-worked block dilute?** Realistically the good part goes
  first and grade falls as it empties. That is a nice touch and a balance risk.
- `[?]` **Who owns the void — the grid or excavation?** It sits on
  `SubCell::workedFraction` today, which is prospecting's structure. That is
  probably right (it is a property of the ground, not of the digger) but it
  makes the "purely generated" subsurface model mutable. Named in
  [subsurface-model.md](subsurface-model.md) §5.
