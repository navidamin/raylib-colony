# Terrain synthesis prototypes

**This folder is not the game, and nothing here is a level.** The game's
one level ladder is Globe → District (200 km) → Site (25 km)
(`src/TerrainGen/survey_cursor.h`, `SURVEY_LEVEL_COUNT = 3`); see
[`docs/design/site-selection/README.md`](../../docs/design/site-selection/README.md).

What is here is where the terrain synthesis was designed and is still
checked:

| | |
|---|---|
| [`regolith_craters.html`](regolith_craters.html) | the bench: the real mosaic beside what the chain makes of it, eight regions, free zoom. Deployed at `/regolith/`. |
| `regolith_chain.js`, `regolith_blocks.js` | the chain in JS, and the mosaic blocks it reads |
| `regolith_craters_render.mjs` | the bench, headless, in Node |
| `regolith_craters_block.py` | cuts `regolith_blocks.js` out of `src/assets/planet/wac_global.jpg` |
| `site_synthesis.py`, [`SITE_SYNTHESIS.md`](SITE_SYNTHESIS.md) | where the imagery chain started, and its design record |
| `elevation.py` | the LOLA reader `src/TerrainGen/lola_dem.cpp` was ported from |
| `asset_bake.py`, `wrap_to_sphere.py` | made `src/assets/planet/wac_global.jpg` |
| `zones_db.py` | made `src/assets/planet/zones.json`, the named regions |
| `data/lola/` | the LOLA DEM and the SLDEM2015 crops the game and `lunar_map` read |

The older contents — a procedural planet from before the surface came
from real imagery, several zoom-view prototypes, and a second descent page
with its own 50 km → 1 km zoom — were removed on 2026-09-23 because they
were other ladders and kept being mistaken for the real one. Graveyard
entry 10 has what each did and the numbers to rebuild it.

## The bench: `regolith_craters.html`

This takes the terrain the game *actually*
ships — `src/TerrainGen/terrain_synthesis.cpp`, which amplifies the real
LROC WAC mosaic and invents no landforms at all — puts **the mosaic on
the left and what the chain made of it on the right**, and asks what it
looks like with a crater population carved back in.

Open [`regolith_craters.html`](regolith_craters.html) directly in a
browser. No server, no build, no network: the page carries its own pieces
of the moon.

**Eight real places**, one 341 km block of the mosaic each:

| | |
|---|---|
| Plinius | 43 km crater on the Serenitatis / Tranquillitatis shore |
| Mare Imbrium | the playfield's default anchor — flat mare, few landforms |
| Tranquility Base | Apollo 11; about as flat as the moon gets |
| Copernicus | 93 km, terraced walls and central peaks |
| Tycho | 85 km and the freshest of the big ones; rays and rough highlands |
| Aristarchus | the brightest feature on the moon, cut by Vallis Schröteri |
| Hadley / Apennines | Apollo 15; mountain front and a sinuous rille |
| Aristoteles | 87 km terraced crater in the northern highlands |

**Free zoom, 200 km to 500 m.** Drag the ground, scroll to zoom into the
point under the cursor, shift-click to drop the selected crater.
`TerrainChainSpansForWindow` gives an arbitrary window two steps — the
100 km macro, then one crop straight to it — and the game's own ladder is
three (100 / 25 / 5). Both are here, because every step re-sharpens and
re-lights what the step above produced and the difference shows. The
right-hand column draws whatever ladder the current zoom asked for, each
rung with the box the next one is cut from, and says when a ladder has
gone deeper than the engine's three-level cap.

**The comparison is the point.** At 100 km the mosaic panel is 75 real
texels and carries the landforms; at 25 km it is 19; at 5 km it is under
four, and at 500 m it is *0.4 of one texel* — two flat greys. Everything
on the right below about eight texels is invention, and the footer says
so. The left panel can be auto-levelled with the same 2/98 stretch
`SharpenAdaptive` applies, so what is left between the two panels is the
synthesis rather than a contrast difference; the chip says which is on.

**What is in it**

- **The chain, ported.** `CropMacro`, `SharpenAdaptive`, the
  world-anchored `ValueNoise`/`Fbm`/`GrainNoise`, `Hillshade`,
  `CastShadows`, `SprinkleBoulders`, `TextureModulate`, `RampColor` and
  the crop ladder are the C++ functions with the C++ constants, in
  JavaScript. Sun azimuth and altitude are hard constants there and
  levers here, because a crater is read almost entirely by the shadow it
  throws.
- **The imagery.** 256×256 texels per region out of
  `src/assets/planet/wac_global.jpg` (22.7556 texels/degree, ~1.33
  km/texel), grayscaled by `EnsureWacLoaded`'s own `(r+g+b+1)/3` and cut
  on whole texels so the JS addresses it with the same global texel
  arithmetic the C++ uses. Regenerate, move or extend with
  [`regolith_craters_block.py`](regolith_craters_block.py)
  (`--add "Marius Hills" 14.2 -56.2 --write`).
- **The craters.** The Layer Block bench's `Bowl()` — parabolic
  excavation, gaussian rim — carved into the height field *before* the
  hillshade and the shadow march, so they are lit by the same sun as
  everything else instead of being pasted on afterwards. Three things
  were added to make it mean something on real ground:
  1. **Scale.** A crater is a diameter in km at a lat/lon, not a
     fraction of a frame. Each rung projects it through its own frame
     and converts metres to chain units through that rung's own
     `heightScaleM` (`110 × spanKm·1000/res`), so one crater is the same
     object, the same depth, at every zoom.
  2. **Depth from diameter.** d/D from 0.03 (ancient) to 0.20 (fresh) —
     the same law `lola_dem.cpp`'s `DetailCraterField` uses, so the two
     synthesizers agree about how deep a 2 km crater is.
  3. **A flat floor, optionally.** `floorFlat = 0` is the Layer Block
     bowl exactly; turning it up walks the same expression to the
     flat-floor / smooth-wall / low-rim profile this directory measured
     against LRO imagery.
  Ejecta apron and a fresh-ejecta albedo lift are levers too. The albedo
  is kept out of the height field on purpose: fed into the macro it
  would be read back by `formRelief` and lift the crater into a mesa.
- **A couple of big and a handful of small, nested.** BRAVO (9.6 km) and
  ALFA (5.2 km) carry a 100 km frame; CHARLIE, DELTA, ECHO, FOXTROT,
  GOLF and HOTEL step down to 55 m, each placed close enough to the
  origin to be in frame at its own zoom, so there is always something to
  look at on the way down. Everything under about 1.3 km is below the
  mosaic's resolution floor, which is where inventing it is honest — it
  cannot contradict data that does not resolve it.
- **Boulders, anchored to the ground.** The engine sprinkles `120·k²` of
  them and only ever runs that at its 5 km level, which is 5.44 per km².
  A bench that zooms has to say which of those two numbers it meant, so
  this one holds the density per km² — at 5 km it is the engine's own
  count, and below it they are the same rocks seen closer rather than a
  fresh scattering every time you zoom.

**Headless renders.** `regolith_craters_render.mjs` runs `regolith_chain.js`
-- the same file the pages load -- in Node, so a change can be looked at without a
browser:

```
node prototypes/planet_visuals/regolith_craters_render.mjs --list
node ... --region tycho --span 5 --res 400
node ... --span 1 --no-craters --out build/off
node ... --set floorFlat=0,rim=0.6 --window
```

It writes one PNG per chain step, `source.png` (the mosaic at the same
window) and a `compare.png` sheet, with each rung's measured relief.

**Is the port faithful?** Checked against the C++, not asserted. Build
`colony_preview`, render the sect ground at the game's default anchor,
then render the same location through the JS:

```
tools/preview/preview.sh --view sect --cell 10,10 --out build/preview/ref.png
# -> "Sect on cell (10,10) -> lat 32.7176, lon -15.5019"
# -> build/preview/ref.png.ground.png is GenerateSectTerrain at res 512
python3 - <<'EOF'   # cut a block at that lat/lon for --block
# ... see regolith_craters_block.py for the same texel arithmetic
EOF
node prototypes/planet_visuals/regolith_craters_render.mjs \
  --block anchor.bin --lat 32.7176 --lon -15.5019 \
  --res 512 --spans 100,25,5 --no-craters --out build/jsport
```

Measured 2026-09: mean |Δ| **3.6/255**, p95 9, max 28, standard
deviation identical to two decimal places (6.64 both), and the
difference image is structureless high-frequency noise — float32 vs
float64 accumulation plus stb_image and libjpeg disagreeing about the
same JPEG by a unit or two per texel. The landforms, the grain and the
lighting are the same ground.

**What it says.** With craters off, the 5 km frame carries 77 m of relief
across the whole cell and no landform at all — the WAC cannot resolve
anything at that scale. With the population on, the same frame carries
326 m and has objects in it. That case was made here and won: the crater
population is in the shipped chain now, as part of the world-anchored
regolith (`terrain_synthesis.cpp`, `CraterPopulation`), ported from this
file and checked against it.
