"""Terrain report: bind the zoom-anywhere view to physical ground truth.

For each site this renders one row per zoom level:
  [ amplified view | LOLA elevation map | LOLA slope map ]
with the level's exact centre coordinates, window size in km, and real
min/max/mean elevation, relief and slope statistics from the LOLA
LDEM_16 model. This is the answer to: "correlate the zoomed point with
exact coordinates on the moon and with the moon's elevation maps to
inquire actual terrain elevations and slopes."

Run:  python3 terrain_report.py
Output: output/site_synthesis_terrain.png + a printed report.
"""

from __future__ import annotations

import os

import numpy as np
from PIL import Image, ImageDraw

from elevation import LolaDem
from site_synthesis import (OUT, SITE_SPAN_DEG, WAC_PATH, KM_PER_DEG,
                            SYNTH_RES, _fonts, apply_ramp, lon_span_for,
                            print_scale_table, synthesize_site_chain,
                            window_km)

# Zoom levels: (name, lat_span_deg). Chain level i has span 3 / 3^i.
LEVELS = [("Site", SITE_SPAN_DEG),
          ("Local", SITE_SPAN_DEG / 3.0),
          ("Close", SITE_SPAN_DEG / 9.0)]


def elev_colormap(elev: np.ndarray, lo: float, hi: float) -> np.ndarray:
    """Hypsometric ramp: deep blue-grey -> green-grey -> tan -> white."""
    t = np.clip((elev - lo) / max(hi - lo, 1e-6), 0.0, 1.0)
    stops = np.array([
        (48, 62, 115),      # deep
        (70, 107, 112),
        (121, 133, 87),
        (176, 148, 103),
        (222, 205, 170),
        (250, 250, 245),    # high
    ], dtype=np.float32)
    idx = t * (len(stops) - 1)
    i0 = np.clip(idx.astype(np.int32), 0, len(stops) - 2)
    f = (idx - i0)[..., None]
    return (stops[i0] * (1 - f) + stops[i0 + 1] * f).astype(np.uint8)


def slope_colormap(slope: np.ndarray, max_deg: float = 25.0) -> np.ndarray:
    """Buildability-flavoured ramp: dark (flat) -> gold -> red (steep)."""
    t = np.clip(slope / max_deg, 0.0, 1.0)
    stops = np.array([
        (25, 28, 40),       # flat
        (52, 80, 72),
        (196, 168, 62),
        (214, 108, 40),
        (188, 42, 42),      # steep
    ], dtype=np.float32)
    idx = t * (len(stops) - 1)
    i0 = np.clip(idx.astype(np.int32), 0, len(stops) - 2)
    f = (idx - i0)[..., None]
    return (stops[i0] * (1 - f) + stops[i0 + 1] * f).astype(np.uint8)


def render_terrain_report(sites, panel=430,
                          output_name="site_synthesis_terrain.png"):
    wac = np.asarray(Image.open(WAC_PATH).convert("RGB"))
    dem = LolaDem()

    font_title, font_cap = _fonts()
    pad = 8
    title_h = 56
    head_h = 30
    text_w = 360
    n_cols = 3
    row_h = panel + pad
    site_h = head_h + len(LEVELS) * row_h + 8

    total_w = n_cols * panel + (n_cols - 1) * pad + text_w
    total_h = title_h + len(sites) * site_h
    canvas = Image.new("RGB", (total_w, total_h), (10, 12, 18))
    draw = ImageDraw.Draw(canvas)
    draw.text((20, 16),
              "Terrain ground truth per zoom level — LOLA LDEM_16 "
              "(1.9 km/px). Columns: amplified view / real elevation / "
              "real slope.",
              fill=(220, 220, 220), font=font_title)

    report = []
    for s, (name, lat, lon) in enumerate(sites):
        sy = title_h + s * site_h
        draw.text((8, sy + 4), f"{name}  —  centre ({lat:+.2f}°, "
                  f"{lon:+.2f}°)", fill=(255, 200, 100), font=font_cap)

        chain = synthesize_site_chain(wac, lat, lon,
                                      extra_levels=len(LEVELS) - 1)
        for li, ((lname, span), lum) in enumerate(zip(LEVELS, chain)):
            y = sy + head_h + li * row_h
            elev, slope = dem.window_arrays(lat, lon, span, out_px=panel)
            stats = dem.window_stats(lat, lon, span)
            km = window_km(lat, span)
            m_px = km * 1000.0 / SYNTH_RES

            panels = [
                Image.fromarray(apply_ramp(lum)).resize((panel, panel),
                                                        Image.LANCZOS),
                Image.fromarray(elev_colormap(elev, stats["min_elev_m"],
                                              stats["max_elev_m"])),
                Image.fromarray(slope_colormap(slope)),
            ]
            for c, p in enumerate(panels):
                canvas.paste(p.convert("RGB"), (c * (panel + pad), y))

            tx = n_cols * (panel + pad) + 6
            lines = [
                f"{lname}:  {km:.1f} x {km:.1f} km   ({m_px:.0f} m/px)",
                f"lat span {span:.3f}°, lon span "
                f"{lon_span_for(lat, span):.3f}°",
                f"elev  min {stats['min_elev_m']:+7.0f} m"
                f"   max {stats['max_elev_m']:+7.0f} m",
                f"      mean {stats['mean_elev_m']:+7.0f} m"
                f"   centre {stats['centre_elev_m']:+7.0f} m",
                f"relief {stats['relief_m']:7.0f} m",
                f"slope  mean {stats['mean_slope_deg']:4.1f}°"
                f"   max {stats['max_slope_deg']:4.1f}°",
                f"(DEM: {stats['dem_px_across']:.0f} px across window)",
            ]
            for k, line in enumerate(lines):
                draw.text((tx, y + 8 + k * 22), line,
                          fill=(215, 215, 225), font=font_cap)
            report.append((name, lname, stats))

    path = os.path.join(OUT, output_name)
    canvas.save(path)
    print(f"  wrote {path}")
    return report


def main():
    print("== real scale of every level ==")
    print_scale_table()
    sites = [("Copernicus", 9.6, -20.0),
             ("Crater chain", -32.6, 39.3),
             ("Mare Imbrium", 32.8, -15.6)]
    print("== terrain report ==")
    report = render_terrain_report(sites)
    for name, lname, st in report:
        print(f"  {name:14s} {lname:6s} "
              f"[{st['span_km']:6.2f} km]  "
              f"elev {st['min_elev_m']:+7.0f}..{st['max_elev_m']:+7.0f} m  "
              f"relief {st['relief_m']:6.0f} m  "
              f"slope mean {st['mean_slope_deg']:4.1f}° "
              f"max {st['max_slope_deg']:4.1f}°")


if __name__ == "__main__":
    main()
