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
