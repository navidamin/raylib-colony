"""Multi-zoom real-moon imagery for game UI prototyping.

Three outputs:

  output/zoom_orbital_labelled.png
    Moon disc with named features (Mare Imbrium, Tycho, Apollo sites,
    etc.) projected from real lat/lon onto the sphere.

  output/zoom_orbital_cellgrid.png
    Same disc with the 20x20 game cell grid overlaid, each cell
    coloured by its real-data biome classification (sampled from
    the LROC WAC mosaic).

  output/zoom_progression.png
    Same area at four zoom levels: orbital → continental (1500 km) →
    regional (300 km) → site (90 km), arranged left-to-right with
    breadcrumb labels.

These are the visual specs for Phase C/D/E in the game integration.
The game will reproduce these views in C++ using the cached assets
in src/assets/planet/.
"""

from __future__ import annotations

import math
import os
from dataclasses import dataclass
from typing import Optional

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import ARCHETYPES, ARCHETYPE_ORDER, PLANET_GRID
from wrap_to_sphere import wrap_to_sphere

PROTOTYPE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(PROTOTYPE_DIR, "data", "global_moon")
OUT = os.path.join(PROTOTYPE_DIR, "output")

# Use the higher-res 8K mirror when available — gives ~1.34 km/pixel
# at equator vs ~2.7 km/pixel for the legacy moon_color_8k.jpg (which
# was actually 4K despite the filename).
WAC_PATH_8K = os.path.join(DATA_DIR, "moon_8k.jpg")
WAC_PATH_4K = os.path.join(DATA_DIR, "moon_color_8k.jpg")
WAC_PATH = WAC_PATH_8K if os.path.exists(WAC_PATH_8K) else WAC_PATH_4K


# --- Lunar feature database ----------------------------------------------

@dataclass
class Feature:
    name: str
    lat: float        # degrees
    lon: float        # degrees east (negative = west)
    radius_km: float
    kind: str         # "mare" | "crater" | "landing"


# Comprehensive feature database — near side AND far side. lat/lon are
# IAU planetary nomenclature standard. radius_km is feature half-width
# (mare diameter / 2 or crater diameter / 2). Add/remove freely.
FEATURES = [
    # === Near-side maria ===
    Feature("Mare Imbrium",         32.8, -15.6, 600, "mare"),
    Feature("Mare Serenitatis",     28.0,  17.5, 350, "mare"),
    Feature("Mare Tranquillitatis",  8.5,  31.4, 350, "mare"),
    Feature("Mare Crisium",         17.0,  59.1, 280, "mare"),
    Feature("Mare Frigoris",        55.0,   0.0, 700, "mare"),
    Feature("Mare Nubium",         -21.3, -16.5, 350, "mare"),
    Feature("Mare Humorum",        -24.4, -38.6, 300, "mare"),
    Feature("Mare Nectaris",       -15.2,  35.3, 175, "mare"),
    Feature("Mare Vaporum",         13.3,   3.6, 120, "mare"),
    Feature("Mare Cognitum",       -10.0, -23.1, 175, "mare"),
    Feature("Mare Marginis",         13.3,  86.1, 210, "mare"),
    Feature("Mare Smythii",         -1.3,  87.5, 185, "mare"),
    Feature("Mare Australe",       -38.9,  93.0, 300, "mare"),
    Feature("Mare Fecunditatis",    -7.8,  51.3, 460, "mare"),
    Feature("Oceanus Procellarum",  18.4, -57.4, 1500, "mare"),
    Feature("Sinus Iridum",         44.1, -31.5, 130, "mare"),
    Feature("Sinus Medii",           2.4,   1.7,  70, "mare"),
    # === Far-side maria + basins ===
    Feature("Mare Moscoviense",     27.3, 147.9, 140, "mare"),
    Feature("Mare Ingenii",        -33.7, 163.5, 160, "mare"),
    Feature("Mare Orientale",      -19.4, -92.8, 160, "mare"),  # near limb
    Feature("South Pole–Aitken",   -53.0, 191.0, 1200, "mare"),  # the biggest
    Feature("Mare Hertzsprung",      0.6,-128.7, 280, "mare"),
    # === Near-side famous craters ===
    Feature("Tycho",               -43.3, -11.4,  43, "crater"),
    Feature("Copernicus",            9.6, -20.0,  47, "crater"),
    Feature("Aristarchus",          23.7, -47.4,  20, "crater"),
    Feature("Plato",                51.6,  -9.4,  51, "crater"),
    Feature("Clavius",             -58.4, -14.4, 116, "crater"),
    Feature("Plinius",              15.4,  23.7,  22, "crater"),
    Feature("Aristoteles",          50.2,  17.4,  44, "crater"),
    Feature("Kepler",                8.1, -38.0,  16, "crater"),
    Feature("Posidonius",           31.8,  29.9,  48, "crater"),
    Feature("Theophilus",          -11.4,  26.4,  55, "crater"),
    Feature("Cyrillus",            -13.3,  24.0,  49, "crater"),
    Feature("Catharina",           -18.1,  23.4,  50, "crater"),
    Feature("Endymion",             53.6,  56.5,  62, "crater"),
    Feature("Atlas",                46.7,  44.4,  44, "crater"),
    Feature("Hercules",             46.7,  39.1,  34, "crater"),
    Feature("Cleomedes",            27.7,  56.0,  62, "crater"),
    Feature("Langrenus",            -8.9,  60.9,  66, "crater"),
    Feature("Petavius",            -25.3,  60.4,  88, "crater"),
    Feature("Schickard",           -44.4, -54.6, 113, "crater"),
    Feature("Bailly",              -66.5, -69.1, 153, "crater"),
    Feature("Maginus",             -50.0,  -6.2,  79, "crater"),
    Feature("Walter",              -33.1,   1.0,  70, "crater"),
    Feature("Ptolemaeus",           -9.3,  -1.9,  76, "crater"),
    Feature("Alphonsus",           -13.7,  -2.8,  60, "crater"),
    Feature("Arzachel",            -18.2,  -1.9,  48, "crater"),
    # === Far-side craters ===
    Feature("Tsiolkovsky",         -20.4, 129.1, 92, "crater"),
    Feature("Korolev",              -4.4, -157.4, 220, "crater"),
    Feature("Mendeleev",             5.7, 140.9, 156, "crater"),
    Feature("Apollo (crater)",     -36.1, -151.8, 247, "crater"),
    Feature("Hertzsprung",           1.4, -128.7, 285, "crater"),
    Feature("Gagarin",             -19.5, 149.2, 130, "crater"),
    Feature("Daedalus",             -5.9, 179.4,  47, "crater"),
    Feature("Jules Verne",         -35.0, 147.0,  72, "crater"),
    Feature("Belyaev",              23.3, 143.5,  27, "crater"),
    # === Polar craters ===
    Feature("Shackleton",          -89.7, 110.0,  10, "crater"),  # south pole
    Feature("Peary",                88.6,  33.0,  37, "crater"),  # north pole
    Feature("Faustini",            -87.2,  77.0,  20, "crater"),
    # === Apollo landing sites (USA) ===
    Feature("Apollo 11",             0.7,  23.5,  1, "landing"),
    Feature("Apollo 12",            -3.2, -23.4,  1, "landing"),
    Feature("Apollo 14",            -3.6, -17.5,  1, "landing"),
    Feature("Apollo 15",            26.1,   3.6,  1, "landing"),
    Feature("Apollo 16",            -8.9,  15.5,  1, "landing"),
    Feature("Apollo 17",            20.2,  30.8,  1, "landing"),
    # === Luna landers (USSR) ===
    Feature("Luna 9",                7.1, -64.4,  1, "landing"),
    Feature("Luna 16",              -0.7,  56.4,  1, "landing"),
    Feature("Luna 17",              38.3, -35.0,  1, "landing"),
    Feature("Luna 21",              25.9,  30.5,  1, "landing"),
    # === Other notable landings ===
    Feature("Chang'e 4",           -45.5, 177.6,  1, "landing"),  # first far-side
    Feature("Chang'e 5",            43.1, -51.9,  1, "landing"),
    Feature("Surveyor 3",           -3.0, -23.3,  1, "landing"),
    Feature("Chandrayaan-3",       -69.4,  32.3,  1, "landing"),  # near south pole
]


# --- Math helpers --------------------------------------------------------

def latlon_to_disc(lat_deg, lon_deg, output_size, margin=12,
                     camera_lon_deg=0.0):
    """Project a lat/lon onto the orbital disc. Returns (px, py, visible).
    `visible` is False if the point is on the far side."""
    lat = math.radians(lat_deg)
    cam = math.radians(camera_lon_deg)
    lon = math.radians(lon_deg) - cam
    # Wrap lon to [-π, π]
    while lon > math.pi:
        lon -= 2 * math.pi
    while lon < -math.pi:
        lon += 2 * math.pi
    # Sphere point in camera frame, +z toward camera
    x = math.cos(lat) * math.sin(lon)
    y = math.sin(lat)
    z = math.cos(lat) * math.cos(lon)
    visible = z > 0
    cx = cy = output_size / 2
    r = output_size / 2 - margin
    px = cx + x * r
    py = cy - y * r
    return px, py, visible


# --- Output 1: orbital with named features -------------------------------

def render_orbital_labelled(output_path, output_size=1600,
                              camera_lon_deg=0.0, title=None):
    """Wrap the WAC mosaic onto a sphere and overlay named feature
    labels, color-coded by feature type."""
    wrap_to_sphere(WAC_PATH, output_size=output_size,
                    output_path=output_path,
                    extent="globe", camera_lon_deg=camera_lon_deg,
                    apply_limb_darkening=False)
    img = Image.open(output_path).convert("RGBA")
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    try:
        font_label = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14)
        font_title = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
    except OSError:
        font_label = ImageFont.load_default()
        font_title = ImageFont.load_default()

    type_colour = {
        "mare":    (200, 220, 255, 220),
        "crater":  (255, 220, 180, 220),
        "landing": (255, 120, 120, 240),
    }

    for f in FEATURES:
        px, py, visible = latlon_to_disc(
            f.lat, f.lon, output_size, camera_lon_deg=camera_lon_deg)
        if not visible:
            continue
        color = type_colour[f.kind]
        if f.kind == "landing":
            r = 6
            draw.ellipse([px - r, py - r, px + r, py + r],
                         outline=color, width=2)
            draw.line([(px - r - 4, py), (px - r + 1, py)],
                      fill=color, width=2)
            draw.line([(px + r - 1, py), (px + r + 4, py)],
                      fill=color, width=2)
            draw.text((px + r + 6, py - 7), f.name, fill=color, font=font_label)
        elif f.kind == "crater":
            r_disc = output_size / 2 - 12
            r = max(3, int(f.radius_km / 1737.0 * r_disc * 0.6))
            draw.ellipse([px - r, py - r, px + r, py + r],
                         outline=color, width=1)
            draw.text((px + r + 4, py - 7), f.name, fill=color, font=font_label)
        else:  # mare
            tx, ty = int(px), int(py)
            for ox in (-1, 0, 1):
                for oy in (-1, 0, 1):
                    if ox == 0 and oy == 0:
                        continue
                    draw.text((tx + ox, ty + oy), f.name,
                              fill=(0, 0, 0, 200), font=font_label)
            draw.text((tx, ty), f.name, fill=color, font=font_label)

    if title is None:
        side = "near side" if abs(camera_lon_deg) < 90 else "far side"
        title = f"Lunar Orbit  —  named features  ({side}, lon {int(camera_lon_deg):+d}°)"
    draw.text((20, 20), title,
              fill=(255, 255, 255, 240), font=font_title)
    legend_y = output_size - 80
    for i, (label, key) in enumerate(
            [("mare basin", "mare"),
             ("crater", "crater"),
             ("Apollo site", "landing")]):
        c = type_colour[key]
        x = 20 + i * 180
        draw.ellipse([x, legend_y, x + 12, legend_y + 12], fill=c)
        draw.text((x + 18, legend_y - 2), label,
                  fill=(220, 220, 220, 240), font=font_label)

    img = Image.alpha_composite(img, overlay)
    img.convert("RGB").save(output_path)
    print(f"  wrote {output_path}")


# --- Real-data biome classification per game cell ------------------------

def classify_cell_biome(wac_global, lat0, lat1, lon0, lon1):
    """Classify a lat/lon rectangle as one of the SiteArchetype values
    by looking at the WAC pixel statistics."""
    h_src, w_src = wac_global.shape[:2]
    # Pixel coords (equirect: lon -180..+180 → x 0..w, lat +90..-90 → y 0..h)
    x0 = int((lon0 + 180.0) / 360.0 * w_src) % w_src
    x1 = int((lon1 + 180.0) / 360.0 * w_src) % w_src
    y0 = int((90.0 - lat1) / 180.0 * h_src)
    y1 = int((90.0 - lat0) / 180.0 * h_src)
    if x0 == x1 or y0 == y1:
        return "MIXED"
    if x0 > x1:                                   # crosses ±180°
        patch = np.concatenate([wac_global[y0:y1, x0:],
                                 wac_global[y0:y1, :x1]], axis=1)
    else:
        patch = wac_global[y0:y1, x0:x1]
    if patch.size == 0:
        return "MIXED"
    brightness = patch.mean()                      # 0..255

    # Polar latitude band — handled by the caller (polar overrides)
    # Mare: very dark basaltic basin
    if brightness < 100:
        return "MARE_INDUSTRIAL"
    # Highland: bright cratered terrain
    if brightness > 145:
        return "HIGHLAND_CONSTRUCTION"
    # In between → MIXED (boundary)
    return "MIXED"


LAT_CELLS = 20      # 20 cells covering 180° latitude → 9° per cell
LON_CELLS = 40      # 40 cells covering 360° longitude → 9° per cell, square
                    # Whole moon = 800 cells. Game phase will need to extend
                    # PLANET_SIZE (single value) to PLANET_LAT_CELLS / LON_CELLS.


def real_biome_grid(wac_global, lat_cells=LAT_CELLS, lon_cells=LON_CELLS):
    """Build a (lat_cells × lon_cells) grid covering the *whole moon*,
    each cell classified into a SiteArchetype from real WAC pixel stats.

    Default 20×40 = 800 cells, each 9° × 9° (square at the equator).
    """
    grid = np.empty((lat_cells, lon_cells), dtype=object)
    cell_lat = 180.0 / lat_cells
    cell_lon = 360.0 / lon_cells
    for gy in range(lat_cells):
        for gx in range(lon_cells):
            # gy=0 is top of grid = north (high lat)
            lat1 = 90.0 - gy * cell_lat
            lat0 = lat1 - cell_lat
            lon0 = -180.0 + gx * cell_lon
            lon1 = lon0 + cell_lon
            if abs((lat0 + lat1) / 2) > 75:
                grid[gy, gx] = "POLAR_VOLATILE"
            else:
                grid[gy, gx] = classify_cell_biome(
                    wac_global, lat0, lat1, lon0, lon1)
    return grid


# --- Output 2: orbital with cell grid overlay ----------------------------

def render_orbital_cellgrid(output_path, output_size=1600,
                              camera_lon_deg=0.0):
    """Wrap moon + overlay 20x20 cell grid on top, each cell coloured
    by its real-data biome classification."""
    wrap_to_sphere(WAC_PATH, output_size=output_size,
                    output_path=output_path,
                    extent="globe", camera_lon_deg=camera_lon_deg,
                    apply_limb_darkening=False)
    base = Image.open(output_path).convert("RGBA")
    overlay = Image.new("RGBA", base.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    wac = np.asarray(Image.open(WAC_PATH).convert("RGB"))
    grid = real_biome_grid(wac)
    lat_cells, lon_cells = grid.shape

    # Print summary
    print(f"  cell biome counts ({lat_cells}×{lon_cells} = "
          f"{lat_cells * lon_cells} cells, real-WAC classified):")
    flat = list(grid.flatten())
    for arch in ARCHETYPE_ORDER:
        n = flat.count(arch)
        print(f"    {arch:24s} {n:3d}")

    # For each cell, paint a translucent quad on the sphere by
    # tessellating its lat/lon corners.
    cell_lat_deg = 180.0 / lat_cells
    cell_lon_deg = 360.0 / lon_cells
    K = 4   # sub-divide each cell into K×K quads for smooth projection
    for gy in range(lat_cells):
        for gx in range(lon_cells):
            arch = grid[gy, gx]
            base_color = ARCHETYPES[arch].base
            cell_lat1 = 90.0 - gy * cell_lat_deg
            cell_lat0 = cell_lat1 - cell_lat_deg
            cell_lon0 = -180.0 + gx * cell_lon_deg
            cell_lon1 = cell_lon0 + cell_lon_deg
            # Skip cells entirely on the far side of the camera —
            # cheap visibility test using the centre point. Saves a
            # lot of polygon work on the half of the moon we can't see.
            cx_lat = (cell_lat0 + cell_lat1) / 2
            cx_lon = (cell_lon0 + cell_lon1) / 2
            _, _, ctr_visible = latlon_to_disc(
                cx_lat, cx_lon, output_size, camera_lon_deg=camera_lon_deg)
            if not ctr_visible:
                continue
            cell_deg = cell_lat_deg  # used by inner loops below for lat steps
            for ky in range(K):
                for kx in range(K):
                    lat0 = cell_lat0 + (ky / K) * cell_deg
                    lat1 = cell_lat0 + ((ky + 1) / K) * cell_deg
                    lon0 = cell_lon0 + (kx / K) * cell_deg
                    lon1 = cell_lon0 + ((kx + 1) / K) * cell_deg
                    # Project four corners
                    p00 = latlon_to_disc(lat0, lon0, output_size,
                                          camera_lon_deg=camera_lon_deg)
                    p01 = latlon_to_disc(lat0, lon1, output_size,
                                          camera_lon_deg=camera_lon_deg)
                    p11 = latlon_to_disc(lat1, lon1, output_size,
                                          camera_lon_deg=camera_lon_deg)
                    p10 = latlon_to_disc(lat1, lon0, output_size,
                                          camera_lon_deg=camera_lon_deg)
                    if not all([p00[2], p01[2], p11[2], p10[2]]):
                        continue
                    poly = [(p00[0], p00[1]), (p01[0], p01[1]),
                            (p11[0], p11[1]), (p10[0], p10[1])]
                    fill = (*base_color, 150)
                    draw.polygon(poly, fill=fill)
            # Cell border: thin lines on the cell boundary
            for i in range(K + 1):
                lat = cell_lat0 + (i / K) * cell_deg
                p_a = latlon_to_disc(lat, cell_lon0, output_size,
                                      camera_lon_deg=camera_lon_deg)
                p_b = latlon_to_disc(lat, cell_lon1, output_size,
                                      camera_lon_deg=camera_lon_deg)
                if i in (0, K) and p_a[2] and p_b[2]:
                    draw.line([(p_a[0], p_a[1]), (p_b[0], p_b[1])],
                              fill=(255, 255, 255, 140), width=1)
                lon = cell_lon0 + (i / K) * cell_deg
                p_a = latlon_to_disc(cell_lat0, lon, output_size,
                                      camera_lon_deg=camera_lon_deg)
                p_b = latlon_to_disc(cell_lat1, lon, output_size,
                                      camera_lon_deg=camera_lon_deg)
                if i in (0, K) and p_a[2] and p_b[2]:
                    draw.line([(p_a[0], p_a[1]), (p_b[0], p_b[1])],
                              fill=(255, 255, 255, 140), width=1)

    try:
        font_title = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
        font_label = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 13)
    except OSError:
        font_title = ImageFont.load_default()
        font_label = ImageFont.load_default()

    side = "near side" if abs(camera_lon_deg) < 90 else "far side"
    draw.text((20, 20),
              f"Lunar Orbit  —  {lat_cells}×{lon_cells} game cells "
              f"({lat_cells * lon_cells}) from real WAC  "
              f"({side}, lon {int(camera_lon_deg):+d}°)",
              fill=(255, 255, 255, 240), font=font_title)

    # Legend with biome counts
    ly = output_size - 130
    for i, name in enumerate(ARCHETYPE_ORDER):
        n = flat.count(name)
        c = (*ARCHETYPES[name].base, 255)
        x = 20 + (i % 3) * 220
        y = ly + (i // 3) * 22
        draw.rectangle([x, y, x + 14, y + 14], fill=c)
        short = {"MARE_INDUSTRIAL": "MARE", "HIGHLAND_CONSTRUCTION": "HIGHLAND",
                 "POLAR_VOLATILE": "POLAR", "KREEP_SCIENTIFIC": "KREEP",
                 "LAVA_TUBE": "LAVA", "MIXED": "MIXED"}[name]
        draw.text((x + 20, y - 1), f"{short}  ({n})",
                  fill=(220, 220, 220, 240), font=font_label)

    out = Image.alpha_composite(base, overlay)
    out.convert("RGB").save(output_path)
    print(f"  wrote {output_path}")
    return grid


# --- Output 3: zoom progression -----------------------------------------

def crop_equirect_region(wac_global, lat_centre, lon_centre,
                          lat_span, lon_span, output_w, output_h):
    """Crop an equirectangular texture to a centred lat/lon window
    and resize to output dimensions."""
    h_src, w_src = wac_global.shape[:2]
    lat0 = lat_centre - lat_span / 2
    lat1 = lat_centre + lat_span / 2
    lon0 = lon_centre - lon_span / 2
    lon1 = lon_centre + lon_span / 2
    y0 = max(0, int((90.0 - lat1) / 180.0 * h_src))
    y1 = min(h_src, int((90.0 - lat0) / 180.0 * h_src))
    x0 = int((lon0 + 180.0) / 360.0 * w_src)
    x1 = int((lon1 + 180.0) / 360.0 * w_src)
    if x0 < 0 or x1 > w_src:
        # Wrap longitudes — for our use cases unlikely
        x0 = max(0, x0)
        x1 = min(w_src, x1)
    crop = wac_global[y0:y1, x0:x1]
    img = Image.fromarray(crop)
    return img.resize((output_w, output_h), Image.LANCZOS)


def render_zoom_progression(output_path, panel_w=900):
    """Show the same area at four zoom levels. Pick a region with
    something interesting at every scale."""
    target_lat = 9.6   # Copernicus region — has Mare Imbrium nearby,
    target_lon = -20.0  # Copernicus crater itself, plus surrounding rays.

    panels = []
    captions = []

    # Panel 0: orbital, with target region highlighted
    orb_path = os.path.join(OUT, "_zoom_tmp_orbital.png")
    wrap_to_sphere(WAC_PATH, output_size=panel_w,
                    output_path=orb_path,
                    extent="globe", camera_lon_deg=0.0,
                    apply_limb_darkening=False)
    img = Image.open(orb_path).convert("RGBA")
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    px, py, _ = latlon_to_disc(target_lat, target_lon, panel_w)
    box = panel_w * 0.18 / 2
    draw.rectangle([px - box, py - box, px + box, py + box],
                   outline=(255, 200, 100, 240), width=3)
    panels.append(Image.alpha_composite(img, overlay))
    captions.append("Orbital  (~3,500 km across)")

    # Panel 1: continental zoom (~1500 km)
    wac = np.asarray(Image.open(WAC_PATH).convert("RGB"))
    cont = crop_equirect_region(wac, target_lat, target_lon,
                                  lat_span=50, lon_span=50,
                                  output_w=panel_w, output_h=panel_w)
    cont = cont.convert("RGBA")
    overlay = Image.new("RGBA", cont.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    box = panel_w * 0.20 / 2
    draw.rectangle([panel_w / 2 - box, panel_w / 2 - box,
                     panel_w / 2 + box, panel_w / 2 + box],
                    outline=(255, 200, 100, 240), width=3)
    panels.append(Image.alpha_composite(cont, overlay))
    captions.append("Continental  (~1,500 km)")

    # Panel 2: regional zoom (~300 km)
    reg = crop_equirect_region(wac, target_lat, target_lon,
                                 lat_span=10, lon_span=10,
                                 output_w=panel_w, output_h=panel_w)
    reg = reg.convert("RGBA")
    overlay = Image.new("RGBA", reg.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    box = panel_w * 0.30 / 2
    draw.rectangle([panel_w / 2 - box, panel_w / 2 - box,
                     panel_w / 2 + box, panel_w / 2 + box],
                    outline=(255, 200, 100, 240), width=3)
    panels.append(Image.alpha_composite(reg, overlay))
    captions.append("Regional  (~300 km)")

    # Panel 3: site zoom (~90 km), real WAC at native resolution
    site = crop_equirect_region(wac, target_lat, target_lon,
                                  lat_span=3, lon_span=3,
                                  output_w=panel_w, output_h=panel_w)
    panels.append(site.convert("RGBA"))
    captions.append("Site  (~90 km)")

    # Compose horizontal strip
    pad = 10
    title_h = 60
    cap_h = 30
    total_w = len(panels) * panel_w + (len(panels) - 1) * pad
    total_h = title_h + panel_w + cap_h
    canvas = Image.new("RGB", (total_w, total_h), (10, 12, 18))
    try:
        font_title = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
        font_cap = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16)
    except OSError:
        font_title = ImageFont.load_default()
        font_cap = ImageFont.load_default()
    draw = ImageDraw.Draw(canvas)
    draw.text((20, 18),
              "Drill-down  —  Copernicus region.  Each panel is the centre "
              "box of the previous panel, cropped from the LROC WAC mosaic.",
              fill=(220, 220, 220), font=font_title)
    for i, (p, cap) in enumerate(zip(panels, captions)):
        x = i * (panel_w + pad)
        canvas.paste(p.convert("RGB"), (x, title_h))
        cw = draw.textlength(cap, font=font_cap)
        draw.text((x + (panel_w - cw) // 2, title_h + panel_w + 6),
                  cap, fill=(230, 230, 230), font=font_cap)
    canvas.save(output_path)
    print(f"  wrote {output_path}")
    # Cleanup tmp
    try:
        os.remove(orb_path)
    except OSError:
        pass


# ---------------------------------------------------------------------------

def render_rotation_storyboard(output_path, n_frames=8, panel_w=460):
    """Strip of `n_frames` orbital views rotating around the moon.
    Visualises what the game's orbital-view rotation will feel like
    once Phase D pre-bakes the angles."""
    print(f"  baking {n_frames} rotation frames...")
    panels = []
    captions = []
    for i in range(n_frames):
        lon = i * (360.0 / n_frames)
        # Wrap to [-180, 180] for display
        display_lon = lon if lon <= 180 else lon - 360
        tmp = os.path.join(OUT, f"_rot_tmp_{i:02d}.png")
        wrap_to_sphere(WAC_PATH, output_size=panel_w,
                        output_path=tmp,
                        extent="globe", camera_lon_deg=lon,
                        apply_limb_darkening=False)
        panels.append(Image.open(tmp).convert("RGB"))
        captions.append(f"lon {int(display_lon):+d}°")
        os.remove(tmp)

    # Layout: two rows of n/2 each
    cols = n_frames // 2 if n_frames % 2 == 0 else n_frames
    rows = 2 if n_frames % 2 == 0 else 1
    pad = 8
    title_h = 60
    cap_h = 28
    total_w = cols * panel_w + (cols - 1) * pad
    total_h = title_h + rows * (panel_w + cap_h) + (rows - 1) * pad

    canvas = Image.new("RGB", (total_w, total_h), (10, 12, 18))
    try:
        font_title = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
        font_cap = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16)
    except OSError:
        font_title = ImageFont.load_default()
        font_cap = ImageFont.load_default()
    draw = ImageDraw.Draw(canvas)
    draw.text((20, 18),
              f"Orbital rotation storyboard  ({n_frames} frames at "
              f"{int(360 / n_frames)}° steps)",
              fill=(220, 220, 220), font=font_title)

    for i, (p, cap) in enumerate(zip(panels, captions)):
        r = i // cols
        c = i % cols
        x = c * (panel_w + pad)
        y = title_h + r * (panel_w + cap_h + pad)
        canvas.paste(p, (x, y))
        cw = draw.textlength(cap, font=font_cap)
        draw.text((x + (panel_w - cw) // 2, y + panel_w + 4),
                  cap, fill=(230, 230, 230), font=font_cap)

    canvas.save(output_path)
    print(f"  wrote {output_path}")


def bake_rotation_frames_for_game(n_frames=12, panel_w=1200):
    """Pre-bake the per-angle moon discs that the game's orbital view
    will swap between. Phase D uses these directly — drop them into
    src/assets/planet/orbital_rotation/{frame_NN.png}.

    n_frames=12 → 30° steps. Smooth enough to read as rotation when
    the player drags or scrolls."""
    repo_root = os.path.normpath(os.path.join(PROTOTYPE_DIR, "..", ".."))
    out_dir = os.path.join(repo_root, "src", "assets", "planet",
                            "orbital_rotation")
    os.makedirs(out_dir, exist_ok=True)
    print(f"  baking {n_frames} game-asset rotation frames into {out_dir}")
    for i in range(n_frames):
        lon = i * (360.0 / n_frames)
        out = os.path.join(out_dir, f"frame_{i:02d}.png")
        wrap_to_sphere(WAC_PATH, output_size=panel_w,
                        output_path=out,
                        extent="globe", camera_lon_deg=lon,
                        apply_limb_darkening=False)
    print(f"  done: {n_frames} frames at {panel_w}px")


def main():
    print("== orbital with named features (near side) ==")
    render_orbital_labelled(
        os.path.join(OUT, "zoom_orbital_labelled.png"),
        camera_lon_deg=0.0)
    print("== orbital with named features (far side) ==")
    render_orbital_labelled(
        os.path.join(OUT, "zoom_orbital_labelled_farside.png"),
        camera_lon_deg=180.0)

    print("== orbital with cell grid (near side) ==")
    render_orbital_cellgrid(
        os.path.join(OUT, "zoom_orbital_cellgrid.png"),
        camera_lon_deg=0.0)
    print("== orbital with cell grid (far side) ==")
    render_orbital_cellgrid(
        os.path.join(OUT, "zoom_orbital_cellgrid_farside.png"),
        camera_lon_deg=180.0)

    print("== zoom progression ==")
    render_zoom_progression(os.path.join(OUT, "zoom_progression.png"))

    print("== rotation storyboard ==")
    render_rotation_storyboard(
        os.path.join(OUT, "zoom_rotation_storyboard.png"),
        n_frames=8)

    print("== bake 12 rotation frames for game asset pipeline ==")
    bake_rotation_frames_for_game(n_frames=12)


if __name__ == "__main__":
    main()
