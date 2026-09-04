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

## Site-selection playtest (`--site`)

```bash
./build/src/lunar_map --site
```

The three-level site-selection ladder, played rather than rendered.
Level 1 is the near-side map: move over the moon and the region under
the cursor names itself — real named features where there are any,
a measured mare/highland reading anywhere else — and the region card
shows its composition, its terrane and its archetype. Click to claim
it. The card then *freezes*: levels 2–3 descend 200 → 25 km (15x from
the globe, then 8x) and never revise it, because resources belong to the region while
terrain belongs to the spot. The level card on the right is the part
that sharpens — measured slope, relief, illumination, Earth link — and
at level 3 the site is judged buildable or refused. Clicking a card
row opens its hint (what a high titanium reading is *for*). `Esc` or
the on-screen BACK button steps back up.

**Descending** zooms the camera into the cursor for 0.45 s before it
builds. The current level's texture is already on the GPU, so the flight
costs nothing but redraws — and on a single-threaded WASM main loop the
build itself blocks everything, so the gap *before* it is the only free
time there is. The new window then arrives as a 512 draft that sharpens
to 2048 a frame later.

**Touch.** A phone has no hover, so a tap that jumps the pointer only
aims; a second tap in the same place commits. The prompt strip
switches to tap wording once it sees this happen, and carries the BACK
button that stands in for `Esc`.

Headless verification of the same state machine:

```bash
tools/lunarmap/lunarmap.sh --siteshot build/lunarmap/step.png
```

`--siteshot` drives the real `UpdateSiteSelect` with a scripted
pointer, one PNG per step, so what is checked is the shipping flow and
not a re-implementation of it. It settles the two-pass build before each
export, and does **not** fly the descent zoom (a click would need ~27
more frames to land) — but it does print where each flight *would* end,
so the geometry stays checkable cheaply:

```
ZOOMCHK from=500km to=100km endVisible=100.00km targetKm=(0.00,-100.00)
```

`--flyshot` renders the zoom itself, a few frames per phase, for the
part arithmetic cannot answer.

### Web playtest (GitHub Pages)

The deploy workflow (.github/workflows/deploy-web.yml) also builds this
tool with Emscripten and publishes it at **/lunarmap/**, alongside the
game and the view-ladder playtest, preloading the LOLA DEM + WAC albedo
(~45 MB download). The browser has no argv, so **the web build comes up
in `--site`**: opening /lunarmap/ on a phone lands straight in site
selection. Shading avoids float textures and uses a GLSL ES 100 shader,
so WebGL1 is enough.

The repo has one Pages site and `main` is not the only branch that
wants it, so deploying a feature branch takes two steps: list the
branch under `on.push.branches` in the workflow, **and** allow it in
the repository's Settings → Environments → `github-pages` deployment
branch rules. Whichever branch pushed last owns the site.

## Review suite (`--playtest`)

```bash
tools/lunarmap/lunarmap.sh --playtest
```

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
| `--site` | interactive site-selection playtest (the three levels) |
| `--siteshot PATH` | scripted walk through `--site`, one PNG per step |
| `--flyshot PATH` | the level-1 descent zoom, one PNG per phase |
| `--out PATH` | render PNG and exit; without it a window opens |
| `--dem PATH` | alternate DEM TIFF |

## Survey ladder

```bash
tools/lunarmap/lunarmap.sh --pick -43.3,-11.4 --ladder \
    --out build/lunarmap/ladder/tycho.png
```

Walks the site-selection descent — 200 km → 25 km window — with the
cursor aimed at one fixed target, writing `tycho_2_DISTRICT.png` and
`tycho_3_SITE.png`. At every level the cursor
is the footprint of the level *below*, so each image's cursor frames
exactly the ground the next image shows. `--place DX,DY` moves the
target (km east/north of `--pick`).

Levels 2–3 only: level 1 is the projected orbital disc, which lives in
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

## What used to be here

Level 1 was a flat plate-carrée map of the near side before it became a
globe, and the site level used to zoom while its cursor refined. Both are
written up in [`docs/graveyard.md`](../../docs/graveyard.md), with the
constants needed to rebuild them.
