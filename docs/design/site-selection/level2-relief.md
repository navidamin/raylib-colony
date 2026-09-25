# Level 2 — the Moon's real relief

*Design record, 2026-09-24. Status: built — the district level (200 km)
is drawn from measured heights wherever the Moon has them, in the game,
its walk and `lunar_map`, on the desktop and in the browser. Code:
`src/TerrainGen/relief.{h,cpp}`; data: `data/relief/`; builder:
`tools/relief/build_relief.py`.*

## 1. The decision

At 200 km the terrain synthesizer is the wrong tool. What it adds —
the regolith, the crater population, the speckle — is sized for the
25 km site and the 5 km sect, and at a quarter of a kilometre per pixel
it reads as noise over a soft picture: the WAC mosaic under it is
1.33 km/px, the global LOLA model 1.9 km. So the district is drawn from
**real relief** instead: heights measured by stereo from Kaguya's
Terrain Camera, 237 m/px, lit by the game's own sun, coloured by the
mosaic's albedo. No synthesized detail at that scale.

## 2. What was compared

Reproduced the district view at Sinus Medii, then at Tycho, Hadley and
Tsiolkovskiy (screenshots in the session of 2026-09-24):

| Option | Verdict |
|--------|---------|
| Today's synthesis | The baseline. Noise over a soft picture. |
| The mosaic alone, no synthesis | Better than the synthesis at this scale — nothing invented. Soft. |
| The mosaic supersampled / ML upscaling (EDSR, ESPCN, FSRCNN) | A mild gain at 4x the cost; the networks add nothing real. |
| Kaguya Terrain Camera *images* | Unusable: a 6° sun, visible strips, ~218 MB. |
| **Kaguya stereo *heights*, lit by the game** | **Clearly best at all four.** 86–120 MB for the whole Moon. |

## 3. The lighting

- **The sun is the game's:** fixed, from the north-west, 35° up — the
  one the chain already lights every level with (`FS_FUSED`,
  `Hillshade`). It is cartographic lighting: a north-west light is the
  one under which relief reads the right way up, and it keeps the
  district consistent with the site below it and the globe above.
- **The colour is the mosaic blurred 2.5 km** (`RELIEF_ALBEDO_BLUR_KM`).
  The mosaic was photographed under its own sun — fitted at Sinus
  Medii to the west, 45° up (correlation 0.56 with the relief's
  hillshade). Left sharp, both lights shade every crater and each reads
  twice, as a ghost. Blurred, only the albedo is left (mare dark,
  ejecta bright) and all the shading is the relief's. Taking the
  photographed shading out of the sharp mosaic ("de-shading") was tried
  and left ghosts of its own.
- **A moving sun is possible later**, because the relief is real: any
  sun angle lights it correctly, which no photograph allows. It is not
  for site selection, where the player is comparing ground and a fixed
  light keeps comparisons fair. If it comes, it belongs with a
  day/night clock, and the site and sect levels would need the same.

## 4. The data

| | |
|---|---|
| Source | USGS Astrogeology's Kaguya TC stereo DTMs (the ARD bucket `astrogeo-ard`, catalogues in `collectionindices/`): v2 equatorial (±61°), v1 (±70°), v2 polar |
| Read at | each DTM's overview no coarser than 8x (~240 m) |
| Grid | 128 px/deg (237 m at the equator), tiles 512 x 512 = 4 x 4°, `r128_<row>_<col>.jpg`, row 0 at 90° N, col 0 at 180° W |
| Stored as | height **above the shipped LOLA model** in 7 m steps about 128: `code = round((z - base) / 7) + 128`, base = bilinear LOLA at the pixel centre (`LolaDem::GlobalBilinearM`) |
| Size | 3 870 tiles, 85 MB, in the repo |
| Coverage | 93.8 % of the ground between 72° N and S measured; 92.4 % of 86° N–S; the polar bands 41–78 % |

Storing the difference from LOLA rather than the height is what makes
8 bits and a JPEG enough: the detail is a few hundred metres at most,
the kilometres are in the model the game already ships.

### How the builder combines DTMs

Many DTMs overlap; each band of tiles picks enough that every 0.25°
cell is covered twice, preferring v2. Each DTM, before combining:

1. **Edge trim** (3 px): the overviews average a DTM's edge with the
   empty frame around it; the ring was tens of metres off.
2. **Onto LOLA:** its offset and tilt fitted against the base and taken
   off. A good DTM fits with a near-zero plane.
3. **Checked against LOLA:** smoothed to about 1 km, its residual must
   be under 80 m rms, or it is dropped rather than outvoted — a
   misplaced model with no neighbour shows as a rectangle.
4. **Feathered** over its outer 2 km, so where the set being combined
   changes there is no step.

Then per pixel: the median of the DTMs there, and the weighted mean of
those within 25 m of it.

**Holes** — mostly the shadowed floors of deep craters, where stereo
finds nothing to match — fade from their neighbours' detail into LOLA
through a **cubic spline**, not into the bilinear LOLA the detail is
measured from. Bilinear LOLA drawn on its own is facets: Birt (16 km
across, 3.5 km deep) was a checkerboard of 1.9 km squares until this.

**Bands** are built 4° at a time, but worked half a degree beyond their
edges and cut back: a DTM crossing into the next band is fitted to LOLA
over nearly the same ground in both, where fitting each band's own half
met in a step every 4°.

**The catalogue's prime meridian bug.** 573 catalogue footprints stop
at exactly 0° E. Their DTMs straddle the meridian; the catalogue kept
one half. The builder takes every DTM's extent from the file itself.

## 5. How the game uses it

- A window at least `RELIEF_MIN_SPAN_KM` (150 km) tall is drawn from
  relief if at least half its tiles exist: the district (built 355.6 km
  across a landscape screen). The site, Colony and Sect views keep the
  synthesizer.
- **GPU** (`terrain_gpu.cpp`): the heights are uploaded packed 16-bit
  into R:G, the same as every height the chain passes between passes;
  `uReliefOn` makes the fused and fields passes read them instead of
  the synthesized field, and turns the speckle off. The tone is the
  macro blurred 2.5 km.
- **CPU** (`TextureModulate`): the same, through `Hillshade` and
  `CastShadows`.
- **Streaming:** tiles are never preloaded. The desktop reads them from
  `data/relief/`; the browser fetches them one at a time from
  `window.COLONY_RELIEF_URL` (set by the deploy), and keeps 96 decoded
  (24 MB). A district built before its tiles arrive is built without
  relief and marked `reliefPending`; `EnsureTerrainAt` builds it again
  once they are all in. A drag at level 2 prefetches under the view.
- The log says which it got: `TERRAIN: 355.6 km window at 1280 px,
  GPU, real relief, ... ms`. The browser test
  (`web_site_level_test.mjs`) requires it.

## 6. Limits

- Where DTMs are missing (a few percent of the claimable Moon) the
  ground is LOLA's, smooth; where under half a window is covered, the
  window keeps the synthesizer.
- Seams of a few metres between DTMs remain in places; they read as
  faint straight lines under the low sun.
- Polar caps beyond the claimable latitude are not built.
- The tiles are JPEG (quality 85): under `lunar_map`'s harsh Lambert
  light their 8-pixel blocks (1.9 km) show faintly as a lattice. The
  game's softer light does not show them.
- A playtest packed as a claude.ai Artifact (`tools/artifact/`) carries
  no tiles — an Artifact holds 64 MB — so its district falls back to
  the synthesizer, as it would anywhere the tiles are absent.
