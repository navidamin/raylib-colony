# Planet surface generation — design notes

Living document describing the procedural moon-surface pipeline in
`generate.py`, what each parameter does, and what's still on the
table for future iteration.

Use this to pick up where we left off without re-deriving everything.

---

## Pipeline overview

Six stages, each emits a debug PNG to `output/stage*.png`:

1. **Heightmap (FBM)** — multi-octave value-noise FBM, compressed to
   ~30% amplitude. Provides very gentle regional relief; deliberately
   small so it doesn't read as basins.
2. **Mare basins** — *currently disabled*. Mask is still computed for
   the pipeline diagram but contributes nothing to height or albedo.
3. **Crater field** — Bridson-ish placement + secondary clusters,
   profile = flat floor + power-curve wall + tiny rim (no ejecta).
   Plus a fine pink-noise (1/f) regolith texture added on top.
4. **Hillshade + cast shadows** — Lambertian shading from a NW sun
   (azimuth 315°, altitude 35°, z_factor 75) plus a horizon ray-march
   for cast shadows.
5. **Archetype tinting** — per-cell `SiteArchetype` picks the base
   colour. All archetype base colours kept in a tight 138..168
   brightness range so no biome reads as a shadow patch.
6. **Decals + dust** — biome-specific decals (lava-tube voids, polar
   frost glints, KREEP warm bloom) + regolith dust grain.

---

## Parameter reference

### Heightmap (`build_planet`)

| Parameter | Value | Why |
|---|---|---|
| `base = fbm(.., 5, 384, 0.5)` | 5 octaves, base scale 384 | Large-scale relief |
| `detail = fbm(.., 3, 128, 0.5)` | 3 octaves, base scale 128 | Mid-scale variation |
| `height = 0.92*base + 0.08*detail` | mostly base | Detail almost zero |
| `(height - mean) * 0.30` | 30% amplitude compression | Otherwise FBM dips read as basins |

### Crater placement (`sample_craters`)

| Parameter | Value | Why |
|---|---|---|
| `count_small/med/big` | 126 / 49 / 3 | Highland-density preset, picked from the v3 dispersion sweep at density_mult=0.9 |
| Radii ranges | 8-18 / 18-42 / 42-110 px | Power-law size mix |
| `min_separation` | 1.35 × (r1+r2) | Looser gives a less Bridson-clustered look at this density |
| Bridson growth ratio | 15% near, 85% global | Mostly uniform, Bridson rescue only |
| Secondary threshold | r > 55 | Only the very biggest spawn rays |
| Secondaries per primary | 1..3 | Rare visible chains, not clusters |
| Secondary radii | 4..9 px | Smaller than smallest primary |
| Secondary placement | annulus 1.8..3.5 r, 55% along 1 of 2 ray angles | Ejecta-chain look |

### Crater profile (`apply_craters`)

| Zone | d range | Shape |
|---|---|---|
| Floor | 0..0.70 | Flat at `depth_amp` |
| Wall | 0.70..0.95 | `depth_amp * (1 - u^p)`, p = 3..5 (deeper = steeper top) |
| Rim | 0.95..1.05 | Gaussian peak at d=1.00, σ=0.05, height = 5% of depth |
| Ejecta | — | **Disabled** (was reading as crater overlap) |

| Per-crater random | Distribution | Range |
|---|---|---|
| `depth_jitter` | Beta(2.2, 2.2) × 1.6 + 0.15 | 0.15..1.75 |
| `age` | Beta(4.0, 1.5) (heavily skewed older) | 0..1 — calibrated for highland realism |
| `depth_amp` | -0.42 × sharp × jitter | -0.05 to -0.66 |

`size_factor` was removed deliberately — every radius now draws from
the same depth distribution so any crater can be deep or shallow.

### Surface texture

```python
height_with_craters += 0.03 * gaussian_blur(pink_noise(shape, rng), 1.5)
```

Pink noise (1/f spectrum) at amplitude 0.03 with sigma=1.5 post-blur. Picked
from the v3 sweep as the softest visible setting that still gives
the densely-jagged regolith look from LRO photos. Comparison sheets
in `output/texture_comparison{,_v2,_v3}.png`.

### Lighting

| Parameter | Value | Notes |
|---|---|---|
| `azimuth` | 315° (NW) | Standard hillshade convention |
| `altitude` | 35° | Low enough that small craters cast shadow |
| `z_factor` | 75 | Vertical exaggeration |
| `smooth_px` | 1.0 | Pre-blur to suppress single-pixel noise |
| Cast-shadow ray-march | max 70 px, step 1.5, soft edge band 35% of tan(alt) | Continuous shadow gradient |
| Floor AO | `1 - clip(depth_below × 0.85, 0, 0.45)` | Crater bottoms ~70% as bright as ground |

### Archetype palette

All `base` colours in 138..168 brightness range:

| Archetype | Base RGB | Tint |
|---|---|---|
| MARE_INDUSTRIAL | (138,134,128) | Neutral grey |
| HIGHLAND_CONSTRUCTION | (162,158,150) | Slightly warm |
| POLAR_VOLATILE | (168,174,184) | Cool blue |
| KREEP_SCIENTIFIC | (158,144,130) | Warm tan |
| LAVA_TUBE | (148,142,134) | Neutral darker grey |
| MIXED | (152,146,138) | Neutral |

Height-based shadow/high blending was **removed** — it was producing
soft circular dark patches anywhere the FBM dipped. Albedo is now
purely the archetype base colour, modulated only by hillshade and
cast shadows.

---

## Sanity test

`sanity_crater.py` produces `output/sanity_crater.png` — three rows
of trivial test surfaces (NW ramp, hill, crater) plus a crater-size
sweep. The hill and crater rows should look like opposites; the
size sweep should grade from fully-shadowed small craters to lit
big bowls. Run after any change to lighting math.

---

## Texture comparison sheets

Three iteration rounds, each produced six side-by-side variants:

- `output/texture_comparison.png` (v1) — six fundamentally different
  approaches: high-freq FBM, micro-crater swarm, Worley, pink (1/f),
  Gaussian bumps, domain-warped FBM.
- `output/texture_comparison_v2.png` — softer variants of pink + Worley
  family: pink soft, redshift, brown, pink+blur, Worley smoothed,
  Worley+pink blend.
- `output/texture_comparison_v3.png` — pink-noise softness sweep:
  amplitude {0.015, 0.022, 0.030} × blur sigma {0, 1.5}.

The current pipeline uses pink, amp=0.03, blur sigma=1.5 (the
"pink + blur" cell — softer than raw pink at the same amplitude).

---

## Future work (in rough priority)

### 1. Real-data calibration
Find LRO close-up patches at the same scale as our render and place
side by side. We've been calibrating against my mental model of
LRO photos; a literal A/B comparison would surface what's still off.

### 2. Texture follow-ups
- Try other amp/blur cells from v3 in-game and see what reads best at
  game zoom (the 600-px comparison panels may not be the right scale).
- Worley + pink blend (v2 #6) was my second pick; could be revisited.
- Texture currently constant across the whole planet — could vary by
  archetype (e.g., polar = smoother / icier, KREEP = grainier).

### 3. Crater morphology gaps
- Complex craters (large primaries) should have terraced walls and
  central peaks. The Gaussian-bump central peak we deleted needs to
  come back with proper morphology — talus around the peak, not a
  bare dome.
- Old / eroded craters could have visibly degraded rims and partially
  filled floors. Currently `age` only damps depth and rim height.
- Crater-on-crater overlap with the older one partially destroyed
  ("saturation equilibrium" look) — ruled out for this round but
  noted as real lunar morphology.

### 4. Distribution refinements
- Archetype-aware density: mare regions should be sparser (younger),
  highlands denser (older / saturated).
- Better secondary chains: currently 55% biased to one of two random
  ray directions; real chains follow more discrete radial rays from
  the primary impact.

### 5. Lighting + shading
- Sun angle is fixed at NW altitude 35°. Could vary per planet seed
  for different "times of day" looks.
- Could add a subtle limb-darkening / fall-off near the planet edges
  so the rectangular grid doesn't read as such.
- Earthshine: a faint blue-tinted fill light from the opposite
  direction would soften deep cast shadows.

### 6. Mare basin re-introduction
We disabled mare because the soft circular dark patches read as ugly
shadows. They were the only physically-meaningful regional dark
feature, though. Future attempts: make them less circular (heavily
distorted boundaries), apply only as a subtle albedo shift not a
height delta, and limit to one per planet maximum.

### 7. Decals revisit
Lava-tube void rosettes, polar frost glints, KREEP warm bloom are
present but very subtle. With the surface texture now in place they
might need recalibration to remain visible.

### 8. Archetype tile preview
The archetype-tile strip (`output/archetype_tiles.png`) is useful for
tuning per-biome looks in isolation but doesn't reflect any
biome-specific decals or future-work biome textures.

### 9. Performance
Current full-pipeline runtime at 1600×1600: a few seconds. Acceptable
for a prototype. For run-time bake at game start, the heaviest costs
are the cast-shadow ray-march and pink-noise FFT.

### 10. Game integration
Not started. Two paths laid out in the README: bake-time (port to C++
once, save a per-seed PNG, load via existing `LoadMoonTiles()`) or
run-time (port noise + hillshade to a fragment shader). Bake-time is
the cheaper integration path.
