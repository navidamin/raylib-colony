# Graveyard

Things that were removed on purpose, with enough of their shape written
down to rebuild them if the reason they went away stops being true.

This is not a changelog. Git already remembers *that* something was
deleted; what git is bad at is telling you **what it did, what numbers it
used, and why it stopped being worth keeping** — which is what you need
at the moment you are wondering whether to bring it back.

Everything here was removed during the orbital-globe work (2026-09-02 to
2026-09-03) on `claude/lunar-elevation-lola-dem-1dcdtj`. Each entry names
the commit-era file it lived in, so `git log -S'<symbol>'` will find the
real code.

---

## 1. The flat near-side map that was level 1

**Was:** the site-selection playtest's top rung. `lunar_map --site`
opened on a plate-carrée map of the whole near side, and you picked a
region by moving over it.

**Replaced by:** the orbital globe (`src/TerrainGen/lunar_globe.{h,cpp}`),
which reaches the far side and shares its projection with the picker.

**Shape, if it is ever wanted back**

- Built by `BuildSiteScene()` → `BuildScene()` with `options.nearside = true`:
  - `scene.lat0/lat1 = -90 / +90`, `scene.lon0/lon1 = -90 / +90`
  - `scene.worldWidthKm = worldHeightKm = 180 * LOLA_M_PER_DEG / 1000` (≈ 5458 km)
  - `texRes = clamp(demRes, 64, 2880)`, default 2048
  - elevation from `LolaDem::WindowDegrees()`, drawn as a mesh through
    the same lunar fragment shader the windowed levels use
- Screen mapping was plate carrée, **not** a projected sphere:
  - `DiscPxPerDeg(h, zoom) = h * zoom / 180`
  - `DiscToScreen: x = w/2 + lon*ppd, y = h/2 - lat*ppd`
  - `ScreenToLatLon` inverted the top-down ortho camera and rejected
    `|lat| > 89` or `|lon| > 90`
- `DiscFitZoom(w, h) = (aspect < 1) ? aspect : 1` — letterboxed the map
  on a portrait phone so the eastern and western near side stayed
  reachable on the one screen where a region gets picked. **Removed with
  its last caller**; `DiscToScreen`/`DiscPxPerDeg`/`DrawDiscFeatureOutlines`
  survive because the static `--demo` / `--ladder` renderer still draws
  level 1 flat.

**Why it went:** the globe superseded it, but the build did not stop —
level 0 kept extracting a 2048 px DEM window and shading it into the
scene cache, then drew the globe straight over the top. About 1.9 s a
visit for pixels nobody saw.

**Might it come back?** The *map* is unlikely to; the globe does its job
better. The **flat-map helpers are still live** for `RenderLadder`, so if
you ever move `--ladder` onto the globe, check this entry before deleting
them too — that is the moment they all become dead together.

---

## 2. `orbital_far.png` — the baked far-side disc

**Was:** a 1200×1200 PNG (2.4 MB) of the far side, loaded into
`RenderManager::orbitalFarTexture` on entering the orbital view.

**Important:** it was **never drawn**. It was loaded, null-checked and
unloaded — 2.4 MB of VRAM held for the process lifetime for nothing. The
comment in `asset_bake.py` calls it "far-side, for completeness".

**Reproduce:**

```python
# prototypes/planet_visuals/asset_bake.py
bake_orbital_disc("far", src_texture, 180.0, "orbital_far.png")
#   -> wrap_to_sphere(WAC_PATH, output_size=1200, extent="globe",
#                     camera_lon_deg=180.0)
```

**Might it come back?** No. The globe renders any longitude from the
mosaic, so a baked far side is strictly less useful than what replaced it.

---

## 3. `orbital_rotation/` — 12 baked rotation frames

**Was:** `frame_00.png` … `frame_11.png`, each 1200×1200 (≈ 27 MB total),
committed and shipped.

**Important:** **zero code ever referenced them.** `multi_zoom.py` says
"Phase D uses these directly" — Phase D used a real sphere instead.

**Reproduce:**

```python
# prototypes/planet_visuals/multi_zoom.py:623
bake_rotation_frames_for_game(n_frames=12, panel_w=1200)
#   12 frames at 30 deg steps, wrap_to_sphere(..., extent="globe",
#   camera_lon_deg=i*30, apply_limb_darkening=False)
```

**Might it come back?** Only as a fallback for a device that cannot
compile the globe shader — and even then, 30° steps read as stepping, not
spinning, and 12 × 1200² RGBA is ~69 MB of VRAM against the globe's one
mosaic. If you need that fallback, prefer re-baking at more frames and
smaller size than restoring these.

---

## 4. `moon_full.png`

**Was:** a 2000×2000 PNG (132 KB) in `src/assets/`.

**Important:** unreferenced by any code, script, doc or workflow in the
repo at the time of removal, and **older than the globe work** — its
provenance is not recorded anywhere I could find. If you know what made
it, write that here rather than restoring it blind.

**Might it come back?** Unknown, which is the honest answer. Nothing
pointed at it.

---

## 5. Zoom that changed level (the continuous scroll)

**Was:** the rungs handed over to each other by zooming, so the descent
from the playfield to the build footprint was one uninterrupted scroll.

**Removed 2026-09-03** in favour of bounded per-rung zoom: crossing a
level is always a click, and `SurveyZoomMax(level)` keeps each rung's
tightest view wider than the window below it.

**Shape**

- Descend: `if (zoomable && !siteRung && zoomK >= rungRatio)` →
  `SurveyDescend()`, `siteLevel++`, `zoomK = 1`
- Ascend: `if (siteLevel > 1 && zoomK <= 1 && wheel < 0)` →
  `SurveyAscend()`, `siteLevel--`, and
  `zoomK = max(1, (window[n]/window[n+1]) * 0.98)` — landing *just
  inside* the rung above so the picture did not jump
- `rungRatio = window[level] / window[level+1]` (8× at DISTRICT), and for
  the site rung `window / SURVEY_SITE_VIEW_KM` (5×)
- `zoomK *= pow(1.25f, wheel)` per notch

**Might it come back?** Plausibly, as an option. It was removed because
the wheel changing level under the player felt like the map moving on its
own, not because the mechanism was wrong. If it returns, it should be
opt-in and it must not fight the click.

---

## 6. The site level's zoom-driven cursor refinement

**Was:** the site rung held its 25 km window while the *view* zoomed to
5 km, and the cursor refined with it — from a snapped 5 km cell down to
the free 1.5 km build footprint — staying inside the 15–30% legibility
band the whole way.

**Replaced 2026-09-03** by: the site level does not zoom at all and
arrives holding the 1.5 km footprint directly. One question, one answer.

**Shape**

- `SURVEY_SITE_VIEW_KM = 5.0` — the view span the zoom stopped at, which
  put the 1.5 km footprint at 30% of the screen (the band ceiling). **This
  constant was removed**; it has no other use.
- Per frame, on the site rung:
  ```cpp
  double visibleKm = c->windowSpanKm / app.zoomK;
  double fp = SurveyFootprintForSpan(visibleKm);
  fp = clamp(fp, SURVEY_BUILD_FOOTPRINT_KM, ladder[level].footprintKm);
  c->footprintKm = fp;
  c->snapToGrid  = (fp >= ladder[level].footprintKm - 1e-9);
  ```
- A click on an unrefined cursor zoomed instead of building:
  `app.zoomK = min(rungRatio, app.zoomK * 1.6f)`
- Founding was gated on `footprintKm <= SURVEY_BUILD_FOOTPRINT_KM * 1.05`

`SurveyFootprintForSpan()` — "the largest ladder footprint that fits the
band at this span, else 20% of it" — was the piece that did the refining.
It outlived the feature by a day: nothing in production ever called it,
only its own self-test, and both were removed on 2026-09-04. It is the
first thing to write again if this returns.

The ladder outlived the feature too, and that one was a live bug. The
`LADDER` row for SITE stayed at `{ 25.0, 5.0, snap }` — the *start* of the
old refinement — while `survey_cursor.h`, the master design and the
instrument all said the cursor was the free 1.5 km footprint, and
`lunar_map` compensated by overwriting `footprintKm` and `snapToGrid` on
the cursor every frame. The self-test asserted the stale values and
passed. Fixed 2026-09-04: the row is
`{ 25.0, SURVEY_BUILD_FOOTPRINT_KM, no snap }`, the override is gone, and
the band and tiling checks now apply only to the rungs that snap.

**Might it come back?** Yes, if the 1.5 km rectangle at ~6% of the window
turns out to be too small to aim with. The trade was made deliberately:
drawing it larger would misreport the ground the buildability verdict is
measured over.

---

## 7. The fixed 1200 px orbital disc projection

**Was:** `OrbitalPickToLatLon` / `OrbitalLatLonToScreen` assumed a disc of
fixed size, centred, always showing the near side.

**Shape**

- `ORBITAL_DISC_PX = 1200.0`, `ORBITAL_MARGIN_PX = 12.0` → radius 588 px
  regardless of window size (so it was cropped top and bottom on a
  1280×720 screen)
- `xn = (sx - w/2) / r`, `yn = -(sy - h/2) / r`, miss if `xn² + yn² > 0.985²`
- `lat = asin(yn)`, `lon = atan2(xn, sqrt(1 - xn² - yn²))` — camera
  longitude fixed at 0

**Replaced by:** the same orthographic maths generalised with a sub-viewer
point and a zoom (`OrbitalCamera`, `OrbitalDiscRadiusPx`), which reduces
*exactly* to the above at `subLat = subLon = 0, zoom = 1` — so this entry
is really "the special case that became the general one".

**Might it come back?** No. It is a strict subset of what replaced it.

---

## 8. Per-window seeded noise (`LocationSeed`)

**Was:** the chain made one RNG per window — `TerrainRng rng(LocationSeed(lat,
lon))`, the seed quantised to 0.01° (~300 m) — and every layer drew from
it in order. Layers were decorrelated by *when* they drew, and the GPU
mirrored it with `SeedVec()` turning the same seed into a per-layer
`vec2` added to the lattice index.

**Replaced 2026-09-04** by a world frame (`NoiseFrame` on the CPU,
`uWorldPx` in the shader): every lattice value is a hash of the world
cell it covers, so the same ground invents the same detail from any
window that frames it.

**Why it went**

The detail belonged to the frame, not the ground. `terrain_probe --res
512` at two windows 1.953 km apart, compared over their overlap:

| | before, aligned / control | after, aligned / control |
|---|---|---|
| planet 100 km, CPU | 15.23 / 18.59 | 9.39 / 18.66 |
| colony 25 km, CPU | 20.84 / 23.87 | 12.63 / 24.17 |
| sect 5 km, CPU | **11.45 / 11.45** | 5.18 / 14.63 |
| sect 5 km, GPU | **11.89 / 11.94** | 4.05 / 11.88 |

The sect rows are the whole argument: aligning the ground made *no
difference at all*, because nothing in that picture tracked the ground.

**Shape, if it is ever wanted back**

```cpp
static uint32_t LocationSeed(double latDeg, double lonDeg)   // 0.01 deg
{
    int qlat = (int)std::lround((latDeg + 90.0) * 100.0);
    int qlon = (int)std::lround((lonDeg + 180.0) * 100.0);
    uint32_t x = ((uint32_t)(qlat * 73856093)) ^ ((uint32_t)(qlon * 19349663));
    x = (x ^ (x >> 16)) * 0x45D9F3Bu;
    x = (x ^ (x >> 16)) * 0x45D9F3Bu;
    return x ^ (x >> 16);
}
```

- `TerrainMacroCrop::seed` carried it to the GPU; the struct's comment
  called it one of "three things the CPU keeps to itself".
- `TerrainLocationSeed()` exposed it publicly. Nothing ever called it.
- `ValueNoise` filled a `g x g` grid from `rng.Uniform()` and stretched
  it with `ResizeBilinear`, so the lattice spacing was `res/(g-1)` rather
  than exactly `scale` — a few percent wider than it claimed.
- `SprinkleBoulders` placed `count` boulders at `rng.Uniform() * res`.
  The world-cell version is more even, which is why the sect level's
  deviation went 0.012 -> 0.016 and CPU/GPU agreement there 3.5 -> 4.1
  out of 255. Planet and colony were unmoved at 2.9 and 7.0.

**Might it come back?** Only for something that genuinely is per-window
and not per-ground. Nothing in the chain is: it invents what a place
looks like, and a place does not change because you framed it
differently.

---

## 9. `SynthesizeDetail` — the LOLA path's own sub-floor

**Was:** a second, independent invention of ground below the data floor,
living in `lola_dem.cpp` and reachable only through `lunar_map --detail
F`. It sat on the *elevation* model (LDEM_16, ~1.9 km/px) where the
chain's regolith sits on the *imagery* (WAC, ~1.33 km/px), so the two
never met in code and nobody noticed they were both running.

Its distinguishing idea — and the reason this entry exists — is that it
did not guess an amplitude. It **measured the real ground's spectrum in
the window** and continued it downward:

- **Hurst exponent**, one number per window: RMS height difference grows
  as `L^H`, so `H = log2(rms(2L) / rms(L))`. Sampled at **4x and 8x the
  native spacing, deliberately not 1x and 2x** — a stereo DEM rolls off
  approaching its own grid and reaches only ~0.74 / 0.80 of its true
  power law at 1x / 2x. Clamped to `[0.35, 1.0]`.
- **Local roughness**, per pixel: RMS relief the real data carries over
  one native sample *at that point*, blurred by the sample lag, so a
  crater wall and the mare beside it got different synthetic amplitudes.
- Band amplitude was then `roughM * (wave / nativeKm)^hurst * 0.30`,
  starting **at** the data floor (not above it) — octaves coarser than
  one native sample are the real data's job, and synthesizing there
  double-counts relief already present. An earlier build that started
  above the floor "looked like bark".

A `--texture craters` mode (`LolaTexture`, `g_texture`,
`DetailCraterField`) swapped the noise carpet for a saturated impact
population where the deepest bowl wins rather than bowls summing.

**Removed 2026-09-18**, having been parked (default `--detail 0`) on
2026-09-08 by "Park lola_dem's sub-floor: one synthesis, not two".

**Why it went**

Two syntheses were inventing the same thing below the same floor and
summing. Measured: what `--detail` added on top of `--chain` was the
same as what it added to raw ground (RMS 13.32 vs 13.49 out of 255) —
fully independent and additive, which is to say double-counted.

The chain's regolith is the one kept: it carries the crater population
with clustering and age, the clast bands and the grit; it is shared with
the game's imagery chain and with the GPU shader and the JS bench, so
there is one synthesis to improve rather than two to keep in step.

**What was actually lost**, `--detail 0` vs `--detail 1`, `--pick
32.8,-15.6`, 700 px render, no chain:

| span | std | RMS difference |
|---|---|---|
| 25 km | 18.582 → 18.678 | 3.49 / 255 |
| 5 km | 11.261 → 13.857 | 8.11 / 255 |

At 5 km it was contributing a quarter of the picture's variance. The
chain's regolith covers that ground now (`--subfloor 1` at the 25 km
site rung: 3.3 m relief → 9.3 m), but it covers it with a *calibrated*
amplitude, not a measured one.

**Might it come back? The Hurst half: no. Tried 2026-09-18.**

This entry used to say that folding the measurement into `SubFloorRelief`
was "worth doing, and the one reason to read this entry". It was tried,
and the answer is no — not from the mosaic. Writing down why, because the
idea is attractive enough to occur to the next person too.

The deleted code measured a HEIGHT field. The chain amplifies IMAGERY.
Luminance is shading that saturates, not height that accumulates, and its
structure function has no single slope to read an exponent off. Measured
over seven very different terrains (`terrain_probe` prints it per
location), the slope falls the whole way out and is nearly the same
everywhere — the signature of a field decorrelating rather than a power
law:

| lag, floor samples | 1→2 | 2→4 | 4→8 | 8→16 |
|---|---|---|---|---|
| Imbrium, mare | 0.76 | 0.37 | 0.16 | 0.14 |
| Tycho, fresh ejecta | 0.69 | 0.38 | 0.22 | 0.12 |
| Apennines | 0.75 | 0.38 | 0.18 | 0.19 |
| Procellarum, flat mare | 0.79 | 0.46 | 0.24 | 0.14 |
| far-side highlands | 0.81 | 0.49 | 0.19 | 0.08 |

Used as a Hurst exponent, what is measured here puts **3.9 m of relief on
a 10 m wavelength** — a 40% slope, where real regolith carries 0.2–0.5 m.
A real exponent needs a height field: LOLA, not the WAC. That is a
different change — the game's web build ships no DEM — and a bigger one.

**The roughness half: yes, but not the way it was tried.** What the mosaic
does answer is amplitude. `MeasureGroundSpectrum` survives as an
instrument and reports `reliefAtFloor`, the RMS luminance swing over one
floor sample — 0.009 on a flat mare against 0.039 on Tycho's ejecta, a
4.5x spread that is real information about the ground.

**Done 2026-09-18.** The roughness field is built once per chain at a span
where the mosaic still resolves the floor and sampled by lat/lon at every
rung, on both the CPU and the GPU. `BuildRoughField` in
`terrain_synthesis.cpp` carries the reasoning and the four wrong turns
taken to get there — normalising by the place's own average, anchoring on
the moon's average, reading the calibration back off a clamped field, and
using a single unblurred difference. Regional spread now reaches the
picture: Procellarum 0.90x Imbrium, Apennines 3.44x, Tycho 4.29x.

So of the two constants this entry pointed at, one is measured and one
stays a constant on purpose.

`git log -S'SynthesizeDetail'` for the real code.

---

## 10. The other ladders (removed 2026-09-23)

**Why they all went together.** This branch held four zoom ladders at
once besides the one that ships, and the docs described old ones as
current. Sessions kept aiming at the wrong one — including the session
that removed them. There is now **one** level ladder: Globe → District
(200 km) → Site (25 km), `SURVEY_LEVEL_COUNT = 3` in
`src/TerrainGen/survey_cursor.h`. See
`docs/design/site-selection/README.md`. The game's own grid views
(Planet 100 km → Colony 25 km → Sect 5 km) are still the game's code in
this branch and are views, not a ladder; replacing them is the
`lunarmap-wiring-site-selection` branch's work.

What was removed, and the shape of each if it is ever wanted back:

**`prototypes/planet_visuals/regolith_playtest.html`** (+ `regolith_worker.js`,
`regolith_gpu.js`), deployed at `/regolith/`. A third descent: 50 km →
1 km (`SPAN_MAX = 50`, `SPAN_MIN = 1`), log-scaled zoom, tiles built in a
worker, the per-pixel chain as WebGL shaders. It carried the only
**supersampling** anywhere: build the tile at N tile pixels per screen
pixel and let the browser average it down, because the chain's finest
content (3 px noise, 2.5 px craters) sits at Nyquist and aliases when
drawn 1:1. Measured at a 2.5 km window against a ratio-3 reference, RMS
out of 255: ratio 1.0 → 11.07 (2.6 s), 1.3 → 10.05 (4.3 s), 1.6 → 8.40
(8.3 s), 2.0 → 6.05 (14.8 s); band-limiting the octaves instead → 11.43,
worse *and* vaguer. `SUPER_STEPS = [0, 1.0, 1.3, 1.6, 2.0, 2.5]`,
`FINE_MARGIN = 1.04`. Parked by decision (2026-09): the game uses a
0.8-of-window chain and no supersample. `/regolith/` now serves the
crater bench, which says on its first line that it is not a level.

**`lunar_map --ladder` and `--demo NAME`** (`RenderLadder`,
`DrawSurveyCursorNav`, `DrawTestNote`, the `DEMO_SITES` table for
imbrium / apennine / shackleton, `--maxlevel`). A second walker beside
`--siteshot`. It drew an older version of the descent: its level 2 card
read "WHICH GROUND?" where the real one reads "WHICH MIX?", and the
`--demo` walk began on the retired flat near-side map (entry 1). The
card's data struct survived as `SiteCard`; `--siteshot` and `--flyshot`
walk the real flow.

**The map explorer's dive.** `lunar_map` run bare opened the flat
near-side map (entry 1) as an interactive explorer: click → a 200 km
window, BACK → the flat map. A two-rung ladder of its own, and it was the
tool's default. Bare `lunar_map` now opens the site-selection descent;
`--pick LAT,LON` still opens one window to inspect (tilt, sun,
exaggeration) with no dive and no BACK; `--nearside --out` still renders
the flat overview as a picture.

**The pre-imagery planet prototypes.** `generate.py`, `generate_real.py`,
`biome_compare.py`, `dispersion_sweep.py`, `texture_comparison{,_v2,_v3}.py`,
`sanity_crater.py`, `SURFACE_DESIGN.md`, `index.html` — the procedural
planet (biomes, 20×20 archetype grid) from before the surface came from
the WAC mosaic. `multi_zoom.py`, `game_views.py`, `regional_view.py`,
`terrain_report.py`, `zone_info_panel.py`, `BUILDABILITY_CPP_SPEC.md` —
zoom and view prototypes whose successors are `lunar_map`,
`terrain_probe` and `LolaDem::EvaluateSite`. With them: 62 of the 67
images in `output/` (the five `SITE_SYNTHESIS.md` shows are kept), and
`data/aristoteles`, `data/plinius`, `moon_color_1k.jpg`, which nothing
read. Kept because something current is built from them: `elevation.py`
(ported as `lola_dem`), `site_synthesis.py` (the chain's origin),
`asset_bake.py` + `wrap_to_sphere.py` (made `wac_global.jpg`),
`zones_db.py` (made `zones.json`), and the crater bench.

**`docs/SNAPSHOT_00{1,2,3}_5-Dec-25.md`** — game-state snapshots from
December 2025 describing the 20×20 world as current.

`git show 138f009:<path>` has every file as it was.
