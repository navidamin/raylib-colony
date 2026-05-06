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

WAC_PATH = os.path.join(DATA_DIR, "moon_color_8k.jpg")


# --- Lunar feature database ----------------------------------------------

@dataclass
class Feature:
    name: str
    lat: float        # degrees
    lon: float        # degrees east (negative = west)
    radius_km: float
    kind: str         # "mare" | "crater" | "landing"


# Carefully curated. lat/lon match standard IAU coords. Radius is
# either the basin/mare half-width or the crater diameter / 2.
FEATURES = [
    # Major maria (rough centroids)
    Feature("Mare Imbrium",         32.8, -15.6, 600, "mare"),
    Feature("Mare Serenitatis",     28.0,  17.5, 350, "mare"),
    Feature("Mare Tranquillitatis",  8.5,  31.4, 350, "mare"),
    Feature("Mare Crisium",         17.0,  59.1, 280, "mare"),
    Feature("Mare Frigoris",        55.0,   0.0, 700, "mare"),
    Feature("Mare Nubium",         -21.3, -16.5, 350, "mare"),
    Feature("Mare Humorum",        -24.4, -38.6, 300, "mare"),
    Feature("Mare Nectaris",       -15.2,  35.3, 175, "mare"),
    Feature("Oceanus Procellarum",  18.4, -57.4, 1500, "mare"),
    # Famous craters
    Feature("Tycho",               -43.3, -11.4,  43, "crater"),
    Feature("Copernicus",            9.6, -20.0,  47, "crater"),
    Feature("Aristarchus",          23.7, -47.4,  20, "crater"),
    Feature("Plato",                51.6,  -9.4,  51, "crater"),
    Feature("Clavius",             -58.4, -14.4, 116, "crater"),
    Feature("Plinius",              15.4,  23.7,  22, "crater"),
    Feature("Aristoteles",          50.2,  17.4,  44, "crater"),
    Feature("Kepler",                8.1, -38.0,  16, "crater"),
    # Apollo landing sites
    Feature("Apollo 11",             0.7,  23.5,  1, "landing"),
    Feature("Apollo 14",            -3.6, -17.5,  1, "landing"),
    Feature("Apollo 15",            26.1,   3.6,  1, "landing"),
    Feature("Apollo 17",            20.2,  30.8,  1, "landing"),
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

def render_orbital_labelled(output_path, output_size=1200):
    """Wrap the WAC mosaic onto a sphere and overlay named feature
    labels, color-coded by feature type."""
    wrap_to_sphere(WAC_PATH, output_size=output_size,
                    output_path=output_path,
                    extent="globe", camera_lon_deg=0.0,
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
        px, py, visible = latlon_to_disc(f.lat, f.lon, output_size)
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
            # Scale dot radius by feature size relative to disc radius
            r_disc = output_size / 2 - 12
            # Moon radius 1737 km. f.radius_km / 1737 * r_disc
            r = max(3, int(f.radius_km / 1737.0 * r_disc * 0.6))
            draw.ellipse([px - r, py - r, px + r, py + r],
                         outline=color, width=1)
            draw.text((px + r + 4, py - 7), f.name, fill=color, font=font_label)
        else:  # mare
            # Mare labels: place label, no marker (mare is a region)
            tx, ty = int(px), int(py)
            # Soft outline for label legibility
            for ox in (-1, 0, 1):
                for oy in (-1, 0, 1):
                    if ox == 0 and oy == 0:
                        continue
                    draw.text((tx + ox, ty + oy), f.name,
                              fill=(0, 0, 0, 200), font=font_label)
            draw.text((tx, ty), f.name, fill=color, font=font_label)

    # Title
    draw.text((20, 20), "Lunar Orbit  —  named features",
              fill=(255, 255, 255, 240), font=font_title)
    # Legend
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


def real_biome_grid(wac_global, planet_grid=PLANET_GRID):
    """Build a 20x20 grid where each cell is the SiteArchetype name,
    classified from the real WAC pixel statistics. Near-side hemisphere
    only: lat -90..+90, lon -90..+90 maps to grid (0..PG, 0..PG)."""
    grid = np.empty((planet_grid, planet_grid), dtype=object)
    span = 180.0  # degrees of lat (and lon for near-side hemisphere)
    half = span / 2
    cell = span / planet_grid
    for gy in range(planet_grid):
        for gx in range(planet_grid):
            # gy=0 is top of grid = north (high lat)
            lat1 = half - gy * cell
            lat0 = lat1 - cell
            lon0 = -half + gx * cell
            lon1 = lon0 + cell
            # Polar override
            if abs((lat0 + lat1) / 2) > 70:
                grid[gy, gx] = "POLAR_VOLATILE"
            else:
                grid[gy, gx] = classify_cell_biome(
                    wac_global, lat0, lat1, lon0, lon1)
    return grid


# --- Output 2: orbital with cell grid overlay ----------------------------

def render_orbital_cellgrid(output_path, output_size=1200):
    """Wrap moon + overlay 20x20 cell grid on top, each cell coloured
    by its real-data biome classification."""
    wrap_to_sphere(WAC_PATH, output_size=output_size,
                    output_path=output_path,
                    extent="globe", camera_lon_deg=0.0,
                    apply_limb_darkening=False)
    base = Image.open(output_path).convert("RGBA")
    overlay = Image.new("RGBA", base.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    wac = np.asarray(Image.open(WAC_PATH).convert("RGB"))
    grid = real_biome_grid(wac)

    # Print summary
    print("  cell biome counts (real-WAC classified):")
    flat = list(grid.flatten())
    for arch in ARCHETYPE_ORDER:
        n = flat.count(arch)
        print(f"    {arch:24s} {n:3d}")

    # For each cell, paint a translucent quad on the sphere by
    # tessellating its lat/lon corners.
    span = 180.0
    half = span / 2
    cell_deg = span / PLANET_GRID
    # Sub-divide each cell into K×K quads so the projection is smooth
    K = 6
    for gy in range(PLANET_GRID):
        for gx in range(PLANET_GRID):
            arch = grid[gy, gx]
            base_color = ARCHETYPES[arch].base
            cell_lat1 = half - gy * cell_deg
            cell_lat0 = cell_lat1 - cell_deg
            cell_lon0 = -half + gx * cell_deg
            cell_lon1 = cell_lon0 + cell_deg
            for ky in range(K):
                for kx in range(K):
                    lat0 = cell_lat0 + (ky / K) * cell_deg
                    lat1 = cell_lat0 + ((ky + 1) / K) * cell_deg
                    lon0 = cell_lon0 + (kx / K) * cell_deg
                    lon1 = cell_lon0 + ((kx + 1) / K) * cell_deg
                    # Project four corners
                    p00 = latlon_to_disc(lat0, lon0, output_size)
                    p01 = latlon_to_disc(lat0, lon1, output_size)
                    p11 = latlon_to_disc(lat1, lon1, output_size)
                    p10 = latlon_to_disc(lat1, lon0, output_size)
                    if not all([p00[2], p01[2], p11[2], p10[2]]):
                        continue
                    poly = [(p00[0], p00[1]), (p01[0], p01[1]),
                            (p11[0], p11[1]), (p10[0], p10[1])]
                    fill = (*base_color, 150)
                    draw.polygon(poly, fill=fill)
            # Cell border: thin lines on the cell boundary
            for i in range(K + 1):
                lat = cell_lat0 + (i / K) * cell_deg
                p_a = latlon_to_disc(lat, cell_lon0, output_size)
                p_b = latlon_to_disc(lat, cell_lon1, output_size)
                if i in (0, K) and p_a[2] and p_b[2]:
                    draw.line([(p_a[0], p_a[1]), (p_b[0], p_b[1])],
                              fill=(255, 255, 255, 140), width=1)
                lon = cell_lon0 + (i / K) * cell_deg
                p_a = latlon_to_disc(cell_lat0, lon, output_size)
                p_b = latlon_to_disc(cell_lat1, lon, output_size)
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

    draw.text((20, 20), "Lunar Orbit  —  20×20 game cells, biomes from real WAC",
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


def render_zoom_progression(output_path, panel_w=560):
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

def main():
    print("== orbital with named features ==")
    render_orbital_labelled(os.path.join(OUT, "zoom_orbital_labelled.png"))
    print("== orbital with cell grid ==")
    render_orbital_cellgrid(os.path.join(OUT, "zoom_orbital_cellgrid.png"))
    print("== zoom progression ==")
    render_zoom_progression(os.path.join(OUT, "zoom_progression.png"))


if __name__ == "__main__":
    main()
