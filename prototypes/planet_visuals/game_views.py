"""Game-view correlation: the zoom ladder in the game's own terms.

The game's geographic views (game_enums.h / game_constants.h):

    Orbital -> Planet -> Colony -> Sect  (Unit view is an interior
    panel, not a geographic zoom)

The anchor is the user's constraint (2026-08-13): **the area occupied
by a sect and its units is 5 km in diameter.** In game units a planet
grid cell is SECT_CORE_RADIUS*2 = 100 world units, so:

    1 world unit  = 50 m
    grid cell     = 5.00 km   (sect core diameter)
    PLANET view   = 20 x 20 cells = 100 x 100 km
    COLONY view   = 5 x 5 cells   = 25 x 25 km
    SECT view     = 1 cell        = 5 x 5 km

This maps straight onto the amplification ladder — the Planet view is
(almost exactly) the old "Site" window:

    view    span(lat)   window     px res (300px)   LDEM_16 px across
    PLANET  3.2978 deg  100 km     333 m/px         52.8
    COLONY  0.8245 deg   25 km      83 m/px         13.2
    SECT    0.1649 deg    5 km      17 m/px          2.6  (needs LDEM_118m)

Run:  python3 game_views.py
Output: output/site_synthesis_gameviews.png — per site: orbital
context + the three geographic game views rendered by the amplifier,
with the game grid overlaid and real LOLA stats per view.
"""

from __future__ import annotations

import os

import numpy as np
from PIL import Image, ImageDraw

from elevation import LolaDem
from multi_zoom import latlon_to_disc
from site_synthesis import (KM_PER_DEG, OUT, SYNTH_RES, WAC_PATH, _fonts,
                            apply_ramp, synthesize_chain_spans, window_km)
from wrap_to_sphere import wrap_to_sphere

# --- The scale anchor -----------------------------------------------------

CELL_KM = 5.0                        # sect + its units: 5 km diameter
WORLD_UNIT_M = CELL_KM * 1000.0 / 100.0   # grid cell = 100 world units
PLANET_CELLS = 20                    # PLANET_SIZE
COLONY_CELLS = 5                     # colony neighbourhood shown

PLANET_KM = PLANET_CELLS * CELL_KM   # 100 km
COLONY_KM = COLONY_CELLS * CELL_KM   # 25 km

VIEWS = [
    ("PLANET VIEW", PLANET_KM / KM_PER_DEG),
    ("COLONY VIEW", COLONY_KM / KM_PER_DEG),
    ("SECT VIEW", CELL_KM / KM_PER_DEG),
]

GOLD = (255, 200, 100)
CYAN = (120, 220, 235)


def draw_scale_bar(draw, panel, km_window, bar_km, y_off=26, font=None):
    px_per_km = panel / km_window
    bar_px = bar_km * px_per_km
    x1 = panel - 18
    x0 = x1 - bar_px
    y = panel - y_off
    draw.line([(x0, y), (x1, y)], fill=(240, 240, 240), width=3)
    for x in (x0, x1):
        draw.line([(x, y - 5), (x, y + 5)], fill=(240, 240, 240), width=3)
    label = (f"{bar_km:g} km" if bar_km >= 1 else f"{bar_km * 1000:g} m")
    if font:
        w = draw.textlength(label, font=font)
        draw.text((x1 - w, y - 24), label, fill=(240, 240, 240), font=font)


def render_game_views(sites, panel=560,
                      output_name="site_synthesis_gameviews.png"):
    wac = np.asarray(Image.open(WAC_PATH).convert("RGB"))
    dem = LolaDem()
    font_title, font_cap = _fonts()

    n_cols = 1 + len(VIEWS)           # orbital + three game views
    pad = 10
    title_h = 58
    head_h = 30
    cap_h = 52
    row_h = head_h + panel + cap_h
    total_w = n_cols * panel + (n_cols - 1) * pad
    total_h = title_h + len(sites) * (row_h + pad)
    canvas = Image.new("RGB", (total_w, total_h), (10, 12, 18))
    cdraw = ImageDraw.Draw(canvas)
    cdraw.text((20, 16),
               "Game views on real ground — 1 world unit = 50 m, grid "
               "cell (sect + units) = 5 km. Planet 100 km / Colony "
               "25 km / Sect 5 km, amplified from the real WAC, stats "
               "from LOLA.",
               fill=(220, 220, 220), font=font_title)

    for s, (name, lat, lon) in enumerate(sites):
        y0 = title_h + s * (row_h + pad)
        cdraw.text((8, y0 + 4),
                   f"{name}  —  sect centre ({lat:+.2f}°, {lon:+.2f}°)",
                   fill=GOLD, font=font_cap)
        y = y0 + head_h

        spans = [span for _, span in VIEWS]
        chain = synthesize_chain_spans(wac, lat, lon, spans)

        # Panel 0: orbital context with the planet-view square
        orb_path = os.path.join(OUT, "_gv_tmp_orbital.png")
        wrap_to_sphere(WAC_PATH, output_size=panel, output_path=orb_path,
                       extent="globe", camera_lon_deg=0.0,
                       apply_limb_darkening=False)
        orb = Image.open(orb_path).convert("RGB")
        os.remove(orb_path)
        d = ImageDraw.Draw(orb)
        px, py, vis = latlon_to_disc(lat, lon, panel)
        if vis:
            b = max(5.0, panel * (PLANET_KM / 3476.0) / 2)
            d.rectangle([px - b, py - b, px + b, py + b],
                        outline=GOLD, width=3)
        canvas.paste(orb, (0, y))
        cdraw.text((6, y + panel + 6), "ORBITAL VIEW — whole moon, real",
                   fill=(230, 230, 230), font=font_cap)

        for v, ((vname, span), lum) in enumerate(zip(VIEWS, chain)):
            x = (v + 1) * (panel + pad)
            km = window_km(lat, span)
            img = Image.fromarray(apply_ramp(lum)).resize((panel, panel),
                                                          Image.LANCZOS)
            d = ImageDraw.Draw(img)

            if vname == "PLANET VIEW":
                # 20x20 cell grid; centre cell = the sect we drill into
                step = panel / PLANET_CELLS
                for i in range(1, PLANET_CELLS):
                    p = i * step
                    d.line([(p, 0), (p, panel)], fill=(255, 255, 255, 60),
                           width=1)
                    d.line([(0, p), (panel, p)], fill=(255, 255, 255, 60),
                           width=1)
                c0 = (PLANET_CELLS // 2) * step
                d.rectangle([c0, c0, c0 + step, c0 + step],
                            outline=GOLD, width=3)
                draw_scale_bar(d, panel, km, 20, font=font_cap)
            elif vname == "COLONY VIEW":
                step = panel / COLONY_CELLS
                for i in range(1, COLONY_CELLS):
                    p = i * step
                    d.line([(p, 0), (p, panel)], fill=(255, 255, 255, 60),
                           width=1)
                    d.line([(0, p), (panel, p)], fill=(255, 255, 255, 60),
                           width=1)
                c0 = (COLONY_CELLS // 2) * step
                d.rectangle([c0, c0, c0 + step, c0 + step],
                            outline=GOLD, width=3)
                draw_scale_bar(d, panel, km, 5, font=font_cap)
            else:  # SECT VIEW
                # Sect core: radius 50 world units = 2.5 km — inscribed
                d.ellipse([2, 2, panel - 2, panel - 2],
                          outline=CYAN, width=3)
                d.text((panel // 2 - 90, 10, ),
                       "SECT CORE  r = 2.5 km", fill=CYAN, font=font_cap)
                draw_scale_bar(d, panel, km, 1, font=font_cap)

            canvas.paste(img.convert("RGB"), (x, y))

            st = dem.window_stats(lat, lon, span)
            m_px = km * 1000.0 / SYNTH_RES
            cap1 = (f"{vname} — {km:.1f} km ({m_px:.0f} m/px)")
            cap2 = (f"LOLA: relief {st['relief_m']:.0f} m · "
                    f"elev {st['mean_elev_m']:+.0f} m · "
                    f"slope {st['mean_slope_deg']:.1f}°/"
                    f"{st['max_slope_deg']:.0f}° max")
            cdraw.text((x + 6, y + panel + 6), cap1,
                       fill=(230, 230, 230), font=font_cap)
            cdraw.text((x + 6, y + panel + 28), cap2,
                       fill=(170, 180, 195), font=font_cap)

    path = os.path.join(OUT, output_name)
    canvas.save(path)
    print(f"  wrote {path}")


def main():
    print(f"  1 world unit = {WORLD_UNIT_M:.0f} m; cell = {CELL_KM} km; "
          f"planet view = {PLANET_KM:.0f} km; colony view = "
          f"{COLONY_KM:.0f} km")
    sites = [("Mare Imbrium colony", 32.8, -15.6),
             ("Copernicus rim colony", 9.6, -20.0)]
    render_game_views(sites)


if __name__ == "__main__":
    main()
