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
