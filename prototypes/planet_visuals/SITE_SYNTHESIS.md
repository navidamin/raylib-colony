# Site Synthesis — zoom-anywhere detail below the WAC floor

Direction set 2026-08-13, after reviewing `zoom_progression.png`: the
procedural style of `generate.py` was rejected as a *replacement* for
real imagery, and the real imagery alone goes blurry at the deepest
zoom. The resolution:

> **Real imagery is the macro truth. Procedural generation is detail
> amplification.** The player zooms anywhere on the orbital moon; the
> real LROC WAC mosaic carries every form it can resolve; below its
> ~1.3 km/px floor, the game *generates* the detail — conditioned on
> the real pixels, deterministic by location, styled as game pixel art.

`site_synthesis.py` is the working prototype. Compare its outputs
against `zoom_progression.png` panel 4 (the blur it replaces).

## The four invariants

1. **Conditioned on real pixels.** The real crop (upsampled, denoised
   at native res, adaptively contrast-expanded) is the low-frequency
   base of the output. Copernicus' rim, terraces and central peaks sit
   exactly where the real moon has them. Detail *modulates* the macro
   (mostly ±25%); it never replaces it.
2. **Only invents below the floor.** Synthesized craters are capped
   around the source floor (~4 km diameter at WAC 8K); anything the
   real data can resolve comes from the real data.
3. **Deterministic by location.** RNG seeded from lat/lon quantised to
   0.01° (`location_seed`). Same site, same ground, forever — no stored
   assets. Verified identical across runs (`check_determinism`).
4. **Terrain-adaptive.** Crater density and roughness are driven by the
   real brightness: dark maria stay sparse, smooth, dark; bright
   highlands get dense cratering. One pipeline, every terrain.

## Pipeline (per ~90 km site window, 300 px internal ≈ 300 m/px)

```
native crop (≈68 px)                       real WAC 8K, no resize
  → gaussian 0.7 px         denoise JPEG artifacts BEFORE upsampling
  → bicubic to 300 px       macro base
  → unsharp 0.40            recover edge contrast
  → adaptive gain ≤2.2      around the crop's own midpoint (maria stay dark)
detail height = craters (sample_craters/apply_craters from generate.py,
                size_scale 0.22, density-filtered by real brightness)
              + quiet pink noise + gentle fbm undulation
shading = hillshade/flat_ref (normalised: flat ground ⇒ ×1.0)
        × cast_shadows (deep only inside fresh bowls)
lum = macro × (0.75 + 0.25·relief) × (0.35 + 0.65·light) × speckle
style = lunar ramp (cool shadow → warm sunlit), continuous or
        14-tone quantised + 3× nearest for pixel art
```

## Tuning lessons (each learned by rendering, per dev-workflow)

- **First attempt drowned the macro**: 314 craters + loud pink noise +
  wide shading range turned Copernicus into uniform gravel. Detail must
  be an order of magnitude quieter than instinct says — the macro is
  the picture, detail is texture.
- **Denoise at native resolution**: unsharp after upsampling amplified
  the source JPEG's ringing into a diagonal ripple across crater
  floors. A 0.7 px blur on the 68 px crop kills it with no visible
  cost.
- **Never full-range stretch**: percentile-stretch to [0,1] turned Mare
  Imbrium's quiet dark plain into blotchy mid-grey. Capped gain around
  the crop's own midpoint keeps maria dark and calm.

## Outputs

| File | Shows |
|------|-------|
| `output/site_synthesis_compare.png` | Real blurry vs continuous / pixel-art / dithered styles |
| `output/site_synthesis_drilldown.png` | The 4-panel zoom strip with panel 4 synthesized |
| `output/site_synthesis_locations.png` | Crater / mare / highland conditioning, real vs synth |

## Correction (2026-08-13, later): no invented craters

After seeing both styles, the user's verdict: the amplification of
real forms works — **the invented craters were the problem** ("you
just created craters where there are none"). Procedural crater
generation is removed from BOTH paths. What remains per style:

- **Photo-real**: native denoise → upsample → unsharp → adaptive
  contrast, plus quiet regolith grain and undulation. Every crater in
  the output is a real one from the WAC data.
- **Pixel art**: the same real-form base, light smoothing (0.7 — the
  earlier hard blur turned forms into amoebas once craters were gone),
  9-tone quantise + ordered dither. Purely tonal stylization.

`render_random_locations` renders N random near-side sites in a
real / photo-real / pixel-art grid (`site_synthesis_random.png`) so
the two surviving styles can be judged across arbitrary terrain.

## Style decision (2026-08-13): stylized pixel art

The photo-real synthesis path was **rejected** by the user: *"I want
pixel art. You don't need to be very precise in reproducing the
surface. The smooth and attractiveness is more important."*

The chosen path is `synthesize_pixelart` + `style_pixelart_v2`:

- **No lighting simulation.** Craters are drawn the way a pixel artist
  draws them: flat floor one tone down, a shadow crescent hugging the
  sun-side inner wall, a lit crescent on the far wall, a thin bright
  rim outside on the sun side. Sun fixed NW.
- **Chunky grid**: 150 px internal, 6x nearest to 900. 9-tone ramp,
  cool blue-violet shadows → warm sunlit highlights.
- **Real crop smoothed HARD** — only the big tonal shapes survive
  (mare edges, main bowls, bright rays). Precision is explicitly not
  the goal; the real data just keeps the map honest per location.
- **Few confident craters** (~27 placed) instead of a sprinkle;
  gentle 2x2 ordered dither (amplitude 0.30) at tone boundaries.

Iteration lessons for this style:
- The first pass lit craters like convex pebbles — for a concave bowl
  the shadow crescent goes on the SUN side inner wall. Get the sun
  vector sign right before tuning anything else.
- Wide crescents turn craters into two-tone cookies; crescents must
  hug the wall (d > ~0.55).
- Ordered dither at 0.55 amplitude read as textile; 0.30 is enough.

## Deep zoom (2026-08-13, later): two more levels, progressive

`synthesize_site_chain` extends the photo-real path below the site
window: **each deeper level takes the centre third of the previous
level's OUTPUT as its macro truth**, re-sharpens it, and adds grain at
the finer scale — real forms flow down, texture becomes structure.
3x per level: ~90 km → ~30 km → ~10 km. Each level salts the location
seed, so the whole chain is still deterministic per site.

Tuning/porting lessons:
- Grain amplitude must GROW with depth (`amp = 1 + 0.7·level`) — the
  macro gets smoother each level, and constant grain reads as fog by
  the second level.
- **Anti-matte relighting**: pure albedo modulation inherits the flat
  washed-out lighting of the upsampled source ("matte", user call).
  Fix: treat the smoothed macro as a height proxy (bright = raised)
  and relight it directionally along with the grain
  (`form_relief = (blur(macro,2.5) − 0.5) × 0.13`), plus a gentle
  S-curve. The first attempt at ×0.30 with heavy cast shadows swung
  to harsh chiaroscuro — the working range is narrow; land between
  0.62+0.38·relief and 0.45+0.55·light.
- PIL `Image.fromarray(mode="F")` reads the raw buffer as contiguous
  float32; feeding it a float64 array (or a strided slice view)
  produces silently binarized garbage. Cast + `ascontiguousarray` at
  the boundary. (Cost one full debugging round; found by printing
  min/max/mean per level — NaNs everywhere.)

Output: `site_synthesis_deepzoom.png` — three sites ×
(regional real → site → local → close).

## Real scale + ground truth (2026-08-13, later)

Every zoom window is now **square in kilometres** (lon span widened by
1/cos(lat); before this they were square in degrees — 27% E-W squash
at Tycho's latitude). Exact scales (Moon radius 1737.4 km →
30.323 km/deg), 300 px internal grid:

| Level | Span | Window | Resolution |
|-------|------|--------|------------|
| Continental | 50° | 1,516 km | 5,054 m/px |
| Regional | 10° | 303.2 km | 1,011 m/px |
| Site | 3° | 90.97 km | 303 m/px |
| Local | 1° | 30.32 km | 101 m/px |
| Close | 1/3° | 10.11 km | 34 m/px |

**Elevation ground truth**: `elevation.py` loads NASA's LOLA LDEM_16
model (`data/lola/ldem_16_uint.tif`, 16 px/deg ≈ 1.9 km/px, decode
`metres = raw × 0.5 − 10000`, offset calibrated against Apollo 11 and
Chang'e 4 LOLA elevations). Any (lat, lon) the player zooms to can be
queried for real elevation, relief, and slope — `terrain_report.py`
renders amplified view / elevation map / slope map per level with the
numbers (`output/site_synthesis_terrain.png`). Copernicus checks out:
floor −3.5 km, relief 4,081 m, rim wall slopes to 33.5°.

Data provenance: the container's egress cannot reach NASA/USGS hosts,
so `fetch-dem.yml` downloads the DEM on a GitHub Actions runner
(full internet) and commits it to the branch — triggered by pushing a
change to `data/lola/REQUEST`. DEM caveat: at 16 px/deg the Local and
Close windows only span 16 and 5 DEM pixels — real but coarse; the
118 m/px LDEM would need the same runner trick with tiling (8 GB).

## Game-view correlation (2026-08-13): 1 world unit = 50 m

The game's geographic views are Orbital → Planet → Colony → Sect
(`game_enums.h`; Unit view is an interior panel, not a zoom level).
The scale anchor is the user's constraint: **a sect and its units
occupy 5 km in diameter.** Since a planet grid cell is
`SECT_CORE_RADIUS × 2 = 100` world units (`game_constants.h`), this
fixes everything:

| Game view | Window | Cells | Resolution (300 px) | LDEM_16 px |
|-----------|--------|-------|---------------------|-----------|
| ORBITAL | whole moon (3,476 km disc) | — | baked frames | — |
| PLANET | 100 km (3.298°) | 20×20 of 5 km | 333 m/px | 52.8 |
| COLONY | 25 km | 5×5 of 5 km | 83 m/px | 13.2 |
| SECT | 5 km (0.165°) | 1 cell, core r = 2.5 km | 17 m/px | 2.6 |

Derived sizes: 1 world unit = 50 m; SECT_CORE_RADIUS (50 u) = 2.5 km;
a unit building ~2–6 world units = 100–300 m. The old "Site" window
(3°, 90.97 km) is superseded by PLANET (100 km) — nearly the same
window, now cell-aligned.

`synthesize_chain_spans` generalises the deep-zoom chain to this
ladder (100 → 25 → 5 km; arbitrary descending spans), and
`game_views.py` renders it per site with the 20×20 / 5×5 grids and
sect-core circle overlaid and LOLA stats per view
(`output/site_synthesis_gameviews.png`).

Gameplay payoff visible in the demo sites: a Mare Imbrium sect cell is
relief 26 m / slope 0.2° (ideal build site), a Copernicus central-peak
cell is relief 513 m / slope 4.4° with 12° maxima — site selection
can score real buildability. DEM caveat: at SECT scale LDEM_16 has
only ~2.6 px; slopes there need the 118 m LOLA product (runner-fetch
+ per-site tiles).

## In-game embedding (2026-08-13): src/TerrainGen

The amplifier is ported to C++ and wired into the game:

- `src/TerrainGen/terrain_synthesis.{h,cpp}` — the photo-real chain
  (crop → denoise → sharpen → adaptive contrast → relight + grain,
  100 → 25 → 5 km ladder) reading the shipped 8K
  `src/assets/planet/wac_global.jpg`. Deterministic per location
  (same quantised lat/lon seed as the prototype). Playfield anchor:
  the 20×20 planet grid is centred on Mare Imbrium
  (`TERRAIN_ANCHOR_LAT/LON`), `TerrainGridCellToLatLon` gives every
  cell real coordinates.
- `RenderManager::DrawSectTerrainBackground` — Sect view generates
  its cell's 5 km ground on first entry (cached per cell, regenerated
  on cell change) and draws it under the dome and units.
- `tools/preview`: `--view sect --cell X,Y` renders the game's Sect
  view on the generated ground and dumps the raw terrain alongside
  (`.ground.png`) for comparison against the prototype.

Porting lessons:
- raylib disables JPG loading by default — `CUSTOMIZE_BUILD ON` +
  `SUPPORT_FILEFORMAT_JPG ON` in the CPM options, or the WAC silently
  fails to `LoadImage` and the ground falls back to flat grey.
- Plain FBM is NOT a pink-noise substitute: normalised FBM
  concentrates variance in smooth blobs and the ground rendered flat
  (std 0.010 vs the prototype's 0.031). Mix in a fine per-pixel
  component (`GrainNoise`), then the C++ ground matches the Python
  output statistically (mean 0.219/0.236, std 0.028/0.031) and
  visually.
- C++ output is *visually* equivalent, not bit-identical: different
  RNG and bilinear (vs bicubic/Lanczos) resampling. Determinism holds
  within the game, which is what matters.

## Final surface decisions (2026-08-13, pre-PR)

- **No procedural craters at any level** — after trying dense (noisy)
  and overlap-rejected (cleaner) small-crater fields at the sect zoom,
  the user removed them entirely. The surface is carried by regolith
  grain, undulation, boulder speckle, real-form relighting, cast
  shadows and albedo speckle, all exposed as `TerrainTuning`
  (`terrain_synthesis.h`). Presets renderable via
  `tools/preview --view sect --tune <name>`.
- **Baseline tuning is the game default.**
- The one-shot `fetch-dem.yml` workflow was removed after the DEM
  landed; re-add it from git history if the 118 m LOLA product is
  wanted later.

## Open questions

- Palette warmth/hue — currently blue-violet shadows; could shift
  toward the game UI's dark sci-fi palette.
- Crater size mix and count per terrain.
- Intermediate zooms: panels 2–3 are real-only today; a lighter
  stylization pass could unify the whole drill-down's look.

## Game integration sketch (not built)

C++ port mirrors the Python stages 1:1 — crop from the shipped
`wac_global.jpg`, integer-hash seed from quantised lat/lon, crater
carving + hillshade into a `RenderTexture2D`, generated on zoom-in and
cached per site. All stages are simple array math; no dependency
beyond raylib. Target: generate a 300 px site tile in well under a
frame budget at zoom-transition time (async or 1-frame hitch
acceptable in prototype).
