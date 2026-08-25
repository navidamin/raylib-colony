# The Subsurface Model — What Is Actually Down There

> Status: **PRELIMINARY** — the shape is settled, the constants are not
> Last Updated: 2026-08-25
> Parent: [README.md](README.md)
> Consumed by: [prospecting/block-model-design.md](../prospecting/block-model-design.md),
> [excavation/block-mining-design.md](../excavation/block-mining-design.md)

---

## 1. Why This Document Exists

Prospecting and excavation are both readers of one thing: the ground. Until now
that thing had no design document of its own — it was described twice, in two
places, differently, and the code did a third thing.

The block model design asks the player to **learn the shape of an ore body**.
That is only a question worth asking if the ore body *has* a shape. This
document gives it one.

### The two divergences it resolves

**One.** [`resource-distribution-model.md`](../prospecting/resource-distribution-model.md)
§"Depth Layer Interaction" specifies:

```
subcell_depth[x][y][layer] = subcell_surface[x][y] * depth_bias[resource][layer]
```

That is perfect vertical continuity — every deposit is a vertical pillar.

**Two.** The code does not do that. `ProspectingGrid::GenerateLayerDistribution`
seeds with `HashSeed(parentGridX, parentGridY, d, resourceIdx)` — depth `d` is
*in the seed*, so each layer generates **its own independent cluster centres**.
Four layers is four unrelated 2D maps.

Neither is right for a block model, and they fail in opposite directions:

| | Vertical pillars (the doc) | Independent layers (the code) |
|---|---|---|
| Does the body have a 3D shape? | No — it is extruded | No — it is four unrelated slices |
| Does interpolating between depths mean anything? | Trivially, it is constant | No, there is nothing to interpolate |
| Is an angled hole worth more than a vertical one? | Never — one column tells all | Never — no continuity to cut across |
| Can the player learn where the ore *goes*? | Nothing to learn | Nothing to learn |

Both make the central decision of the new prospecting design — *which way do I
point the drill* — meaningless. That is the case for changing them.

---

## 2. The Shape: A Shoot

The unit of geology is a **shoot**: a three-dimensional ellipsoid of elevated
grade, with a position, three radii, and an **orientation**.

```
struct Shoot
{
    Vec3  centre;        // metres, in parent-cell space
    Vec3  radii;         // metres along the shoot's own axes
    float strikeDeg;     // compass bearing of the long axis
    float dipDeg;        // how steeply that axis tilts from horizontal
    float peakGrade;     // 0-1, multiplier on the parent cell's abundance
};
```

Grade at a point is the strongest influence of any shoot, floored by a
background:

```
g(p) = max( BACKGROUND, max over shoots of  peak * exp(-r²/2) )
       where r = |R(strike,dip)ᵀ (p - centre)| / radii     (elementwise)
```

`R` is the rotation that takes world axes to the shoot's axes. That single term
is the whole change: **a shoot can lie at an angle.**

### Why orientation is the point

A dipping shoot is what makes the drilling decision real.

```
   vertical holes                    one angled hole
   ▼    ▼    ▼    ▼                       ╲
 ──┼────┼────┼────┼──  surface          ───╲──────────────  surface
   │    │    │    │                         ╲
   │  ▒▒│▒▒  │    │                          ╲▒▒▒
   │▒▒▒▒▒▒▒▒▒│    │                           ╲▒▒▒▒▒▒
   │    │  ▒▒│▒▒  │                            ╲▒▒▒▒▒▒▒▒▒
   │    │    │  ▒▒│▒▒                           ╲▒▒▒▒▒▒
   ▼    ▼    ▼    ▼                              ╲

 four holes, four short             one hole, one long intersection
 intersections, and the             that shows the body's true
 shoot's direction is a guess       thickness and direction
```

Four vertical holes each clip the shoot briefly and leave its direction
ambiguous. One angled hole drilled *across* the shoot cuts its full width. This
is why real exploration drills at an angle, and it gives the player a genuine
skill to acquire: read the model, guess the orientation, drill to test it.

### Each element gets its own shoots

Shoots are generated **per resource**, so iron and water lie in different
places and at different angles in the same ground. That is what makes the
element switcher a real control rather than a recolour, and it is already how
the current generator behaves laterally — this keeps that property and extends
it into three dimensions.

---

## 3. Geometry and Units

The unit trap named in
[`module-architecture.md`](../../guides/module-architecture.md) Part II is the
most expensive bug in this codebase. So: every quantity here is named with its
unit, once, and the rest of the design refers back to this table.

| Name | Unit | Value | Note |
|------|------|-------|------|
| Parent cell | m | 100 × 100 | `SECT_CORE_RADIUS * 2`; 1 world unit = 1 m |
| Sub-cell | m | 12.5 × 12.5 | fixed 8 × 8 lattice, `PROSPECTING_GRID_SIZE` |
| Layer 0 · Regolith | m | 0 – 12 | fine impact-gardened soil |
| Layer 1 · Megaregolith | m | 12 – 34 | coarse fragmented rock |
| Layer 2 · Fractured bedrock | m | 34 – 68 | cracked but coherent; ice in fractures |
| Layer 3 · Intact bedrock | m | 68 – 120 | solid, pristine |
| Block | m³ | 12.5 × 12.5 × thickness | **not** a constant volume — see below |

Layers get thicker with depth, which is both geologically honest and useful:
a deep block holds roughly **four times** the volume of a surface block. Deep
ground is worth more in absolute tonnage, and it is also the ground that is
hardest to reach and hardest to know. That is a good trade to hand the player,
and it falls out of the geometry rather than being balanced in.

Two quantities, never to be confused:

- **grade** — a fraction, 0–1, the proportion of a block that is the target
  resource. What an assay reports.
- **tonnage** — absolute, what the block actually holds. `grade × volume ×
  density`.

`GetGroundTruth()` returns grades. `GetQuantity()` returns tonnage. Their
product is neither; the product that matters is `GetQuantity × GetGroundTruth[t]`,
which is the target resource's tonnage in that block.

---

## 4. What Changes in the Generator

Small, and mostly a deletion.

| Change | Why |
|--------|-----|
| Drop `d` from `HashSeed(px, py, d, resourceIdx)` | One seed per (cell, resource) generates **one 3D field**, not four unrelated 2D ones. This is the change that creates continuity |
| Shoot centres gain a `z` in metres | Currently implicit at the layer's centre |
| Shoots gain `strikeDeg` and `dipDeg` | The orientation the whole design rests on |
| Sample the field at an arbitrary point, not per layer | A drill trace passes *through* blocks at angles and must be sampled along its length |

What does **not** change, and should not:

- Determinism per parent cell. The same cell generates the same ground, always.
- The FNV/LCG hash approach. It works and it is cheap.
- The 8 × 8 lattice. Fixed; tier is a reach ring, not a resolution.
- The `SUBCELL_VARIATION_MIN` / `MAX` clamps — though see the measurement below.

### Two facts already measured, which constrain this

`colony_measure_clusters` (see [`excavation/implementation-plan.md`](../excavation/implementation-plan.md) §7)
measured the current generator across all 400 planet cells × 4 depths × every
resource — 8,948 fields, the full population at the standard seed:

- **`SUBCELL_VARIATION_MAX` binds in 98.9% of fields.** The 2.0 ceiling is not
  an outlier guard, it is the shape of essentially every ore body. Any new
  generator that wants richer peaks must raise the clamp; the field itself
  cannot supply them.
- **Rich ground averages 15 of 64 sub-cells in a 4.8 × 4.8 box.** Ore bodies
  are already big and diffuse relative to the lattice. A 3D version should keep
  that scale, not shrink it — a shoot that fits inside two blocks is invisible
  to an 8 × 8 lattice.

Re-run that tool after any change here. It is the check that this document did
not quietly make the ground worse.

---

## 5. Deliberately Not Decided

This is the preliminary version. The shape is settled; the numbers are not, and
should be calibrated against dumped data rather than guessed — the rule this
repo has paid to learn twice.

- `[?]` **How many shoots per element per cell?** Currently 1–2. A 3D field may
  want 1–3, with the third small and deep, so there is something left to find
  after the obvious body is drilled out.
- `[?]` **Dip distribution.** Uniform 0–90° makes most bodies steep. Real ore
  bodies cluster shallow-to-moderate. Probably 15–65°, but measure what that
  does to intersection lengths first.
- `[?]` **Should strike correlate across elements in one cell?** Real geology
  says yes — one structural event tilts everything. It would also let a player
  who has drilled out iron make an *informed* guess about titanium, which is a
  lovely bit of transferable knowledge. Not free: it makes the elements less
  independent, which weakens the element switcher.
- `[?]` **Province and cluster scales.** [`resource-distribution-model.md`](../prospecting/resource-distribution-model.md)
  designs Voronoi provinces and planet-scale deposit clusters. Those sit
  *above* this document and are unaffected by it — a shoot's `peakGrade` scales
  the parent cell's abundance, which is where province and cluster effects
  already land. Confirm rather than assume when that doc is implemented.
- `[?]` **Density per resource**, for turning grade × volume into tonnage.
  Currently absorbed into the abstract "quantity" figure. Needs naming before
  tonnage means anything physical.
- `[?]` **Does the void left by mining belong here or in the modules?** See
  [`module-interplay.md`](module-interplay.md) §3 — currently the modules own
  it, which is probably right, but it is the one piece of mutable state layered
  over this otherwise purely generated field.
