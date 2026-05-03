# Planet surface generation — design notes

Living document for the planet visual prototype. Read this to pick up
the work without re-deriving everything from the conversation history.

The prototype lives entirely in `prototypes/planet_visuals/`. It is
**not yet wired into the game** (no C++, no texture loading hook). The
goal is to validate the look first, then port whichever path the user
picks (bake-time PNG vs run-time shader).

---

## Two pipelines, same renderer

| Script | Purpose | Output |
|---|---|---|
| `generate.py` | Procedural moon surface (FBM heightmap + crater generator) | `output/planet_full.png`, `comparison.png`, `seed_variants.png`, `archetype_tiles.png`, etc. |
| `generate_real.py` | Real-data moon surface (LOLA elevation tiles for a named region) | `output/{region}_real.png`, `{region}_compare.png`, `{region}_heightmap.png` |
| `wrap_to_sphere.py` | Project flat planet PNG onto a sphere disc | `output/planet_orbital.png` |

The shading core is shared — both pipelines use the same hillshade,
cast shadows, AO, archetype tinting, and pink-noise texture. The only
difference is the heightmap source.

### Auxiliary scripts

| Script | What it does |
|---|---|
| `sanity_crater.py` | Lighting math sanity sheet (NW ramp / hill / crater + size sweep) |
| `texture_comparison.py` / `_v2.py` / `_v3.py` | A/B sheets that picked the current regolith texture |
| `dispersion_sweep.py` | A/B sheets that picked the current crater dispersion |
| `biome_compare.py` | A/B of old (Voronoi) vs new (noise-field) biome distribution |

---

## Procedural pipeline (`generate.py`)

Six stages, each emits a debug PNG:

1. **Heightmap (FBM)** — multi-octave value-noise FBM, compressed to
   ~30% amplitude. Gentle regional relief; deliberately small so it
   doesn't read as basins.
2. **Mare basins** — *currently disabled*. Mask is computed for the
   pipeline diagram but contributes nothing to height or albedo. The
   biome-tinting layer carries the mare/highland distinction now.
3. **Crater field** — Bridson-ish placement + secondary clusters,
   profile = flat floor + power-curve wall + tiny rim (no ejecta).
   Plus a fine pink-noise (1/f) regolith texture added on top.
4. **Hillshade + cast shadows** — Lambertian shading from a NW sun
   (azimuth 315°, altitude 35°, z_factor 75) plus a horizon ray-march
   for cast shadows.
5. **Archetype tinting** — per-cell `SiteArchetype` picks the base
   colour. All archetype base colours kept in a tight 138..168
   brightness range so no biome reads as a shadow patch.
6. **Decals + dust** — lava-tube voids, polar frost glints, KREEP
   bloom, regolith dust grain.

### Locked parameters (calibrated through ~5 sweep iterations)

#### Heightmap (`build_planet`)

| Parameter | Value |
|---|---|
| `base = fbm(.., 5, 384, 0.5)` | 5 octaves, base scale 384 |
| `detail = fbm(.., 3, 128, 0.5)` | 3 octaves, base scale 128 |
| `height = 0.92*base + 0.08*detail` | mostly base |
| `(height - mean) * 0.30` | 30% amplitude compression |

#### Crater placement (`sample_craters` defaults)

| Parameter | Value | Why |
|---|---|---|
| `count_small/med/big` | 17 / 7 / 1 | Sparse mare-like preset (density 0.12 of original) |
| `size_scale` | 1.5 | Uniform multiplier on all crater radii |
| `size_variance` | 0.5 | Tight buckets — most craters near each bucket centre |
| `min_separation` | 1.35 × (r1+r2) | Looser → less Bridson clustering |
| `age_alpha` | 4.0 | Beta(α, 1.5) — heavily skewed older / eroded |
| Bridson growth ratio | 15% near, 85% global | Mostly uniform, Bridson rescue only |
| Secondary threshold | r > 55 | Only the very biggest spawn rays |
| Secondaries per primary | 1..3 | Rare visible chains |

Native bucket radii: 8-18 / 18-42 / 42-110 px. After `size_scale=1.5`
that becomes 12-27 / 27-63 / 63-165.

#### Crater profile (`apply_craters` defaults)

| Zone | d range | Shape |
|---|---|---|
| Floor | 0..0.70 | Flat at `depth_amp` |
| Wall | 0.70..0.95 | `depth_amp * (1 - u^p)`, p = 3..5 |
| Rim | 0.95..1.05 | Gaussian peak, height = 5% of depth |
| Ejecta | — | **Disabled** (was reading as crater overlap) |

| Per-crater | Distribution |
|---|---|
| `depth_jitter` | `1.0 + (Beta(2.2, 2.2) × 1.6 − 0.8) × depth_variance`, clipped at 0.05 |
| `age` | `Beta(age_alpha, 1.5)` |
| `depth_amp` | `-0.42 × sharp × jitter × depth_scale` |

| Parameter | Locked default |
|---|---|
| `depth_scale` | 1.0 |
| `depth_variance` | 1.0 |

#### Surface texture

```python
height_with_craters += 0.03 * gaussian_blur(pink_noise(shape, rng), 1.5)
```

#### Lighting

| Parameter | Value |
|---|---|
| `azimuth` | 315° (NW) |
| `altitude` | 35° |
| `z_factor` | 75 |
| `smooth_px` | 1.0 |
| Cast-shadow ray-march | max 70 px, step 1.5, soft edge band 0.35 × tan(alt) |
| Floor AO | `1 - clip(depth_below × 0.85, 0, 0.45)` |

#### Archetype palette (all in 138..168 brightness range)

| Archetype | Base RGB |
|---|---|
| MARE_INDUSTRIAL | (138,134,128) |
| HIGHLAND_CONSTRUCTION | (162,158,150) |
| POLAR_VOLATILE | (168,174,184) |
| KREEP_SCIENTIFIC | (158,144,130) |
| LAVA_TUBE | (148,142,134) |
| MIXED | (152,146,138) |

Height-based shadow/high blending was **removed** — produced soft
circular dark patches anywhere the FBM dipped. Albedo is now purely
the archetype base colour, modulated by hillshade + cast shadows + AO.

#### Biome distribution (`assign_archetype_grid`)

Switched from Voronoi-with-random-centroids to **noise-field with
hard constraints** (the "v2" approach validated in `biome_compare.py`).

  * MARE in the top 20% of an FBM noise field. Connected blobs.
  * POLAR forced at top-2 / bottom-2 rows.
  * KREEP at 1-2 random radial hotspots (~5-10 cells each).
  * LAVA TUBE at 1-3 randomly scattered isolated cells.
  * MIXED auto-assigned in ±0.04 band around mare threshold.
  * HIGHLAND default fill.

Resulting proportions per planet roughly: mare 10% / highland 50% /
polar 20% / kreep 2% / lava 0.5% / mixed 15%.

---

## Real-data pipeline (`generate_real.py`)

End-to-end render of a named lunar region using LOLA-derived
elevation. Path is **proven** (Plinius region renders cleanly). Region
swap is a 5-line edit at the top of the script:
`REGION_SLUG`, `REGION_TITLE`, `LAT_RANGE`, `LON_RANGE`, `LANDMARKS`.

### Data source

NASA/USGS hosts (Astrogeology, Moon Trek, PDS) are blocked from the
sandbox. Working source is the GitHub mirror:

  `jaanga/moon-heightmaps-256p-ne` (gh-pages branch)

Encoded as 1°×1° PNG tiles at 256 ppd (~117 m/pixel). Elevation
packed into PNG R/G channels: `height = R + 255 × G`. Repo covers the
NE quadrant (lat 0..60°N, lon 0..180°E). Other quadrants exist as
sibling repos (`-se`, `-sw`, `-nw`).

Cached tiles under `data/{region}/lat{N}_lon{E}.png`.

### Region currently set up

**Plinius** (15.4°N, 23.7°E, 43 km crater on Mare Tranquillitatis /
Mare Serenitatis boundary).

  * 3×3 tile patch at lat 14..16°N, lon 22..24°E (~91×88 km)
  * Plinius is a sharp-rimmed fresh crater, depth/D ≈ 0.047
  * Surrounding mare is flat — many small fresh craters scattered

### Failed / abandoned regions

  * **Apollo 11** (Mare Tranquillitatis) — too flat, geologically real
    but visually featureless at 90×90 km / 117 m/pixel.
  * **Aristoteles** (87 km crater, lat 50.2°N, lon 17.4°E) — too big
    (rim outside even a 5×5 patch in longitude) and depth/D = 0.038
    (visually shallow).

### Tile-seam artefact handling

Per-tile encoding has ~700-unit DC offset jumps at tile boundaries.
Fixed in two passes:

  1. **BFS mean-alignment** from a pinned centre tile.
  2. **Per-column / per-row residual correction** at every seam:
     compute the step at each column, low-pass filter (keep encoding
     artefact, discard real terrain), symmetrically subtract from
     both sides over a 24-px fade band. Step magnitude capped at
     ±80 units to prevent runaway corrections in flat regions.

### Differences vs procedural pipeline

| | Procedural | Real-data |
|---|---|---|
| Heightmap | FBM | Sampled LOLA |
| Crater generator | sample_craters() | Skipped — real craters in elevation |
| Albedo | Archetype palette | Synthetic mare-vs-highland gradient from low-pass elevation |
| Texture amp | 0.03 | 0.005 (real data already has detail) |
| Boundary | Square | Soft circular disc on deep-space background |
| Markers | None | Annotated landmark crosshairs |

Real WAC mosaic for true albedo is **TODO** — sandbox doesn't allow
USGS/NASA hosts and we haven't found a github mirror. The synthetic
gradient is a stand-in.

---

## Sphere-wrap (`wrap_to_sphere.py`)

Projects `planet_full.png` onto a sphere viewed head-on. Output:
`planet_orbital.png` — moon disc with limb darkening on a starfield.

Math:
  * Treats input as equirectangular projection of one hemisphere
  * For each pixel `(xs, ys)` in the disc: `z = √(1 − xs² − ys²)`
  * `lat = asin(ys)`, `lon = atan2(xs, z)`
  * Bilinear sample texture at `(lat, lon) → (tx, ty)`
  * Brightness × `z^0.6` for limb darkening
  * Soft disc edge blended over the starfield

Limitations: the input texture's hillshade is baked for top-down NW
sun, so crater shadows near the disc edge won't match the globe's
true normals. Acceptable for a thumbnail / planet-select UI.

---

## Decision log

Major calls made through the iteration. Reverse chronological.

| Date / sweep | Decision | Reason |
|---|---|---|
| Sphere wrap | Build option B (projection + limb darkening) over A (disc crop) | Looks like a real moon thumbnail; cheap |
| Biome v2 | Wire noise-field as default `assign_archetype_grid` | Voronoi was geologically chaotic |
| Per-biome density | **Skip for now** (bullet on future-work list) | Locked sparse preset reads as believable terrain regardless of biome tint |
| Game integration approach | **Hybrid**: real data for orbital/region-selection; procedural for surface gameplay; biome category links them | Real mare regions are visually flat at gameplay scale; procedural is replayable |
| Texture amp/blur | 0.03 / σ=1.5 | "Pink + blur" cell from v3 sweep — softer pink |
| Dispersion v4 (locked) | density 0.12 / size_scale 1.5 / size_variance 0.5 / depth_scale 1.0 / depth_variance 1.0 | Sparse mare-like, picked by user from v4 sweep |
| `age_alpha=4.0`, `min_sep=1.35` | Locked from v1 sweep | Heavily eroded + breathing room reads as natural |
| Real-data region: Plinius | Pivoted from Apollo 11 (too flat) → Aristoteles (too big/shallow) → Plinius | Right scale + sharp rim for the data resolution |
| Crater profile | Flat floor + power-curve wall + tiny rim, **no ejecta** | Real LRO morphology; ejecta was reading as crater overlap |
| Mare basin layer | **Disabled** | Soft circular dark patches read as ugly shadows |
| Archetype palette | Tight 138..168 brightness range | Bigger spread caused biome blocks to read as shadows |

---

## Game integration plan (proposed, not implemented)

The conversation landed on a **hybrid layered** approach:

| UI layer | Source | Purpose |
|---|---|---|
| Orbital / planet-select | `planet_orbital.png` (sphere-wrapped) | "This is the Moon" recognition |
| Region selection within planet | Procedural biome map (current `assign_archetype_grid`) overlaid on the orbital image | Player picks a landing biome |
| Close-up gameplay (sect / unit) | Procedural surface generation, parameters keyed by the cell's `SiteArchetype` | Each landing unique, biome-appropriate |

**Don't** generate real-data PNGs for actual gameplay surfaces — real
mare regions are visually flat at gameplay scale. Real data is for
the strategic view layer only.

For C++ port: the renderer is a stack of well-isolated functions
(`fbm`, `pink_noise`, `gaussian_blur`, `hillshade`, `cast_shadows`,
`apply_craters`, `colourise`). Each is straightforward to port. Two
paths to wire it in:

  * **Bake-time**: port once, run at game start to produce a single
    PNG per planet seed, hand to the existing `LoadMoonTiles()`
    loader. Lower runtime cost, lower fidelity (one zoom level).
  * **Run-time**: port noise + hillshade to a fragment shader, sample
    per-pixel with mip levels. Higher cost, infinite zoom.

Bake-time is the cheaper path; recommended unless infinite zoom
matters.

---

## Future work

Ordered by likely-next-thing-to-touch.

### A. Per-biome procedural presets
The locked dispersion is calibrated for "sparse mare-like". Highland
should be denser. Polar should look icier. Calibrate per-biome
parameter sets via dispersion-sweep-style A/Bs. This becomes
necessary for the hybrid plan's close-up gameplay layer.

### B. Sphere wrap re-shading
The orbital view's lighting is the flat texture's NW sun, baked in.
Disc edges look slightly off. Add a `--reshade` mode that drops the
texture's hillshade and re-applies it for the sphere's sun direction.

### C. Real WAC mosaic albedo
Find a github mirror of LROC WAC tiles, plumb into `generate_real.py`.
Replaces the synthetic mare↔highland gradient with actual reflectance.
Catches features like Copernicus' bright ray system. Sandbox can't
reach NASA/USGS directly.

### D. Complex-crater morphology
Big primaries should have terraced walls + central peaks. The
Gaussian-bump central peak we deleted needs to come back with proper
talus geometry (not a bare dome on a flat floor).

### E. Real biome map
For the hybrid plan's "real_lunar" mode: derive a global biome
classification from LROC mosaic + LOLA elevation. Single bake. Used
to seed `assign_archetype_grid` instead of the noise-field method
when `mode=real_lunar`.

### F. Game integration plumbing
C++ port of the renderer. Bake-time path first (least work).

### G. Lower priority (deferrable)
- Mare basin re-introduction (subtle albedo only, no height)
- Decals revisit — lava-tube voids / KREEP bloom got swamped by texture
- Crater erosion that actually degrades rim profiles, not just damps
- Archetype-aware crater density (covered by A above)

---

## Branch & commits

Branch: `claude/redesign-planet-visuals-kCrSp`

Most-recent meaningful commits (newest first):
  * `b6ec5dc` — sphere wrapping (orbital view)
  * `a2d310e` — wire noise-field biome assignment as default
  * `e7ac8aa` — biome distribution v2 (compare sheet)
  * `d86f8e6` — lock procedural defaults from v4 sweep
  * `3a8b702` — add size_variance parameter
  * `47986e0` — dispersion sweep v4 (depth_scale + size_variance)
  * `619f065` — bump pink-noise texture to amp 0.03 + blur 1.5
  * `f004a07` — switch to Plinius region (real-data)

`git log --oneline -50` for the full picture.
