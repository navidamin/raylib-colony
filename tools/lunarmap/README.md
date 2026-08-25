# Real-elevation lunar map (`lunar_map`)

Renders the **actual Moon** from NASA's LOLA elevation model — the whole
near side as a map, or any picked region as terrain — in raylib, with a
lunar-specific shading pipeline. A standalone instrument beside the
game: it shares the game's DEM ground truth
(`prototypes/planet_visuals/data/lola/ldem_16_uint.tif`, the CGI Moon
Kit LDEM_16 derived from LRO/LOLA laser altimetry, 16 px/deg ≈ 1.9 km/px)
but links no game code.

```
REAL MOON -> LOLA DEM (billions of laser shots)
          -> heightmap texture + terrain mesh (src/TerrainGen/lola_dem.*)
          -> GLSL lunar shading (per-pixel normals, explicit sun,
             WAC albedo, regolith noise, crater-rim curvature)
          -> top-down map / tilted 3D slab -> PNG or interactive window
```

The heightmap controls the **geometry**; the fragment shader controls
the **lunar appearance**. Normals come per-pixel from the height
texture (finite differences at physical scale), so crater detail
survives even where the mesh grid is coarser than the DEM. Lighting is
harsh Lambert with a tiny ambient term and no haze — bright sunlit
slopes, near-black shadows. The `color` style reproduces the classic
LOLA rainbow elevation map, hillshade-modulated.

## Build and run

```bash
cmake --build build --target lunar_map

# whole near side, shaded relief (headless -> build/lunarmap/lunarmap.png)
tools/lunarmap/lunarmap.sh --nearside

# LOLA-style colour elevation map
tools/lunarmap/lunarmap.sh --nearside --style color

# any region by real coordinates: Tycho, 200 km window
tools/lunarmap/lunarmap.sh --pick -43.3,-11.4 --span 200

# tilted 3D slab presentation
tools/lunarmap/lunarmap.sh --pick -43.3,-11.4 --span 200 --tilt

# interactive (needs a display; run the binary directly)
./build/src/lunar_map --pick 9.6,-20.1 --span 200 --tilt
```

Headless rendering uses the same software-GL wrapper as the other
instruments: `LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
xvfb-run -a ...` (the script applies it).

## Playtest

```bash
tools/lunarmap/lunarmap.sh --playtest
```

### Web playtest (GitHub Pages)

The deploy workflow (.github/workflows/deploy-web.yml) also builds this
tool with Emscripten and publishes it at **/lunarmap/** alongside the
game and the view-ladder playtest, preloading the LOLA DEM + WAC albedo
(~45 MB download). The web build is interactive: tap the near-side map
to dive into a 200 km window at that spot, BACK returns; on-screen
buttons cover style, 3D/map view, sun and exaggeration. Shading avoids
float textures and uses a GLSL ES 100 shader, so WebGL1 is enough.
Deploys run from `main`; a feature branch also needs adding to the
`github-pages` environment's allowed branches before its deploy job
will run.

Renders the review suite into `build/lunarmap/`: the near side in both
styles, then Tycho (top-down + tilted), Copernicus, Aristoteles, Mare
Imbrium (the game's default terrain anchor, both styles) and the south
pole under grazing light. Judge the results against real LRO imagery —
craters must sit where the real ones sit.

## Flags

| Flag | Effect |
|------|--------|
| `--nearside` | whole near side, plate carrée (default) |
| `--pick LAT,LON` | regional window centred on real coordinates |
| `--span KM` | regional window size (default 200) |
| `--style shaded\|color` | photographic relief / LOLA elevation ramp |
| `--sun AZ,EL` | sun azimuth (cw from north) and elevation (default 315,30) |
| `--exag F` | vertical exaggeration (default 2.0; 1.0 = true scale) |
| `--detail F` | sub-floor synthesis strength (default 1.0; 0 = measured data only) |
| `--ambient F` | shadow-side fill light (default 0.06) |
| `--tilt` | tilted 3D slab instead of top-down |
| `--size WxH` | output resolution (default 1200x1200) |
| `--demres N` | height texture resolution (default: auto) |
| `--meshres N` | terrain mesh grid (default 256, max 256) |
| `--place DX,DY` | placement cursor, km east/north of the window centre |
| `--footprint KM` | cursor footprint size (default 1.5) |
| `--ladder` | walk the survey descent, one PNG per level |
| `--out PATH` | render PNG and exit; without it a window opens |
| `--dem PATH` | alternate DEM TIFF |

## Survey ladder

```bash
tools/lunarmap/lunarmap.sh --pick -43.3,-11.4 --ladder \
    --out build/lunarmap/ladder/tycho.png
```

Walks the site-selection descent — 500 km → 100 → 25 → 5 km window —
with the cursor aimed at one fixed target, writing
`tycho_2_REGIONAL.png` … `tycho_5_SITE.png`. At every level the cursor
is the footprint of the level *below*, so each image's cursor frames
exactly the ground the next image shows. `--place DX,DY` moves the
target (km east/north of `--pick`).

Levels 2–5 only: level 1 is the projected orbital disc, which lives in
the game's render path, not in this instrument. Geometry comes from
`src/TerrainGen/survey_cursor.{h,cpp}` (shared with the game;
`survey_cursor_test` is its headless self-test). Design:
`docs/design/site-selection/`.

## Interactive controls

| Input | Action |
|-------|--------|
| mouse drag | orbit (tilt view) |
| wheel | zoom |
| arrow keys | steer the sun (azimuth / elevation) |
| `TAB` | shaded ⇄ colour elevation |
| `T` | top-down ⇄ tilted |
| `Q` / `E` | vertical exaggeration down / up |
| `S` | screenshot (`lunar_map_shot.png`) |

## Notes

- Elevations are metres against the 1737.4 km reference radius; the
  HUD legend and window stats print the real range (global:
  −8.98 … +10.69 km).
- **Sub-floor synthesis** (`--detail`, on by default for regional
  windows): LDEM_16 resolves nothing under ~1.9 km/px, so zoomed
  windows come out soft. Below that floor the window synthesizes
  plausible lunar ground — a fractal regolith spectrum plus a
  clustered small-crater population (power-law sizes, degraded
  parabolic bowls with rims). Deterministic per location (anchored to
  global coordinates, independent of window framing); amplitude fades
  to zero at wavelengths the real data carries, so the LOLA landforms
  are textured, never displaced. Detail below ~2 km is *plausible*,
  not *measured* — use `--detail 0` for the honest instrument view.
- The near-side map is plate carrée (equirectangular), so high-latitude
  ground stretches east-west; regional `--pick` windows compensate with
  the same 1/cos(lat) widening the game's terrain chain uses.
- Albedo comes from the shipped LROC WAC mosaic
  (`src/assets/planet/wac_global.jpg`), pulled toward neutral gray so
  its baked-in sun does not fight the explicit sun vector.
- `src/TerrainGen/lola_dem.{h,cpp}` (the DEM loader + windowing, a C++
  port of `prototypes/planet_visuals/elevation.py`) is game-ready: the
  same class can later feed real relief into the game's terrain chain.
