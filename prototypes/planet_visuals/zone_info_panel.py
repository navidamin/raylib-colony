"""Zone-info-panel mockup.

Generates a series of mockups showing what the player sees when they
hover/click a labeled zone in the orbital view: a sidebar opens up
with the zone's terrain / regolith / lighting / history info.

This is the visual spec for the C++ UI implementation. The C++ side
will:
  * Load src/assets/planet/zones.json at startup
  * On orbital-view hover, find the zone whose lat/lon is closest to
    the cursor (with a small max-distance threshold)
  * Render a similar sidebar in raylib

Outputs:
  output/zone_info_apollo11.png       — landing-site example
  output/zone_info_tycho.png          — fresh crater example
  output/zone_info_mare_imbrium.png   — mare example
  output/zone_info_shackleton.png     — polar example
  output/zone_info_spa.png            — far-side basin example
  output/zone_info_grid.png           — six panels in one sheet
"""

from __future__ import annotations

import os
import math
import textwrap

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from zones_db import lookup_by_name, ALL_ZONES, ZoneInfo
from multi_zoom import latlon_to_disc, FEATURES, WAC_PATH
from wrap_to_sphere import wrap_to_sphere

PROTOTYPE_DIR = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(PROTOTYPE_DIR, "output")


def _font(sz, bold=True):
    name = "Bold" if bold else "Regular"
    try:
        return ImageFont.truetype(
            f"/usr/share/fonts/truetype/dejavu/DejaVuSans-{name}.ttf", sz)
    except OSError:
        return ImageFont.load_default()


def _wrap(text, font, max_width, draw):
    """Wrap `text` so each rendered line fits in max_width pixels."""
    if not text:
        return []
    words = text.split()
    lines, cur = [], ""
    for w in words:
        trial = (cur + " " + w).strip()
        if draw.textlength(trial, font=font) <= max_width:
            cur = trial
        else:
            if cur:
                lines.append(cur)
            cur = w
    if cur:
        lines.append(cur)
    return lines


def _draw_section(draw, x, y, w, title, lines, font_h, font_b, fg=(220, 220, 230)):
    """Draw a titled section starting at (x, y). Returns the next y."""
    draw.text((x, y), title.upper(),
              fill=(140, 180, 220), font=font_h)
    y += 22
    for line in lines:
        draw.text((x, y), line, fill=fg, font=font_b)
        y += int(font_b.size * 1.45)
    return y + 8


def _format_num(val, suffix=""):
    if val is None:
        return None
    return f"{val:.1f}{suffix}"


def _prep_orbital_thumb(lat, lon, size=320, camera_lon_deg=None):
    """Render a small orbital disc centred on the zone (or near-side
    by default)."""
    if camera_lon_deg is None:
        # Aim camera at the zone so it's visible
        camera_lon_deg = lon if -90 < lon < 90 else (lon if lon > 0 else lon)
    tmp = os.path.join(OUT, f"_zone_thumb_tmp_{abs(hash((lat, lon))) % 9999}.png")
    wrap_to_sphere(WAC_PATH, output_size=size,
                    output_path=tmp,
                    extent="globe", camera_lon_deg=camera_lon_deg,
                    apply_limb_darkening=False)
    img = Image.open(tmp).convert("RGB")
    try:
        os.remove(tmp)
    except OSError:
        pass
    # Mark the zone with a crosshair
    draw = ImageDraw.Draw(img.convert("RGBA"))
    px, py, visible = latlon_to_disc(lat, lon, size,
                                       camera_lon_deg=camera_lon_deg)
    return img, (px, py, visible), camera_lon_deg


def render_zone_panel(zone: ZoneInfo, output_path, size=(900, 720)):
    """Render a zone-info panel mockup. Layout:
       Left half: orbital thumbnail + zone marker + small map.
       Right half: stacked sections (Identity / Terrain / Composition
                    / Lighting / History).
    """
    W, H = size
    canvas = Image.new("RGB", (W, H), (16, 18, 24))
    draw = ImageDraw.Draw(canvas)

    # Decide camera aim (near-side: 0°, far-side: 180°, polar: lon).
    if not zone.earth_visible:
        cam_lon = zone.lon
    else:
        cam_lon = 0.0

    # === Left half: orbital thumbnail ====================================
    thumb_size = 380
    thumb, (px, py, visible), cam_lon_used = _prep_orbital_thumb(
        zone.lat, zone.lon, size=thumb_size, camera_lon_deg=cam_lon)
    canvas.paste(thumb, (24, 60))

    # Zone marker on the thumbnail
    overlay = Image.new("RGBA", (thumb_size, thumb_size), (0, 0, 0, 0))
    odraw = ImageDraw.Draw(overlay)
    if visible:
        r = max(8, int(zone.diameter_km / 1737.0 * thumb_size / 2 * 0.6))
        marker_col = {"mare": (200, 220, 255, 240),
                       "crater": (255, 220, 180, 240),
                       "landing": (255, 120, 120, 250),
                       "basin": (255, 200, 120, 240)}.get(
            zone.feature_type, (255, 255, 255, 240))
        odraw.ellipse([px - r, py - r, px + r, py + r],
                      outline=marker_col, width=3)
        odraw.line([(px - r - 6, py), (px - r + 2, py)],
                   fill=marker_col, width=3)
        odraw.line([(px + r - 2, py), (px + r + 6, py)],
                   fill=marker_col, width=3)
        odraw.line([(px, py - r - 6), (px, py - r + 2)],
                   fill=marker_col, width=3)
        odraw.line([(px, py + r - 2), (px, py + r + 6)],
                   fill=marker_col, width=3)
    canvas.paste(Image.alpha_composite(thumb.convert("RGBA"), overlay)
                 .convert("RGB"), (24, 60))

    # Caption under the thumbnail
    side = "near side" if zone.earth_visible else "far side"
    caption = f"{zone.lat:+.1f}°, {zone.lon:+.1f}°  ({side})"
    draw.text((24, 60 + thumb_size + 8), caption,
              fill=(180, 180, 200), font=_font(15))

    # === Right half: zone metadata =======================================
    rx = 440
    rw = W - rx - 30

    # Title block
    draw.text((rx, 28),
              zone.name, fill=(255, 255, 255), font=_font(28))
    type_label = {"mare": "MARE BASIN", "crater": "IMPACT CRATER",
                   "landing": "LANDING SITE", "basin": "IMPACT BASIN"}.get(
        zone.feature_type, zone.feature_type.upper())
    draw.text((rx, 64), type_label,
              fill=(140, 180, 220), font=_font(14))
    draw.text((rx, 84), f"Ø {zone.diameter_km:,.0f} km"
              + (f"   ·   age {zone.age_ga:.1f} Ga" if zone.age_ga else ""),
              fill=(180, 180, 200), font=_font(14))

    y = 130
    fh = _font(14)
    fb = _font(13, bold=False)

    # Terrain section
    if zone.terrain or zone.elevation_floor_km or zone.elevation_rim_km:
        lines = []
        if zone.elevation_floor_km or zone.elevation_rim_km:
            lines.append(
                f"Elevation:  floor {zone.elevation_floor_km:+.1f} km  "
                f"rim {zone.elevation_rim_km:+.1f} km")
        if zone.terrain:
            lines.extend(_wrap(zone.terrain, fb, rw, draw))
        y = _draw_section(draw, rx, y, rw, "Terrain", lines, fh, fb)

    # Composition section
    if zone.dominant_rock or zone.composition_notes \
            or zone.iron_pct is not None:
        lines = []
        if zone.dominant_rock:
            lines.append(f"Dominant rock:  {zone.dominant_rock}")
        composition_kv = []
        if zone.iron_pct is not None:
            composition_kv.append(f"Fe {zone.iron_pct:.1f}%")
        if zone.titanium_pct is not None:
            composition_kv.append(f"Ti {zone.titanium_pct:.1f}%")
        if zone.thorium_ppm is not None:
            composition_kv.append(f"Th {zone.thorium_ppm:.1f} ppm")
        if composition_kv:
            lines.append("Regolith:  " + "   ".join(composition_kv))
        if zone.composition_notes:
            lines.extend(_wrap(zone.composition_notes, fb, rw, draw))
        y = _draw_section(draw, rx, y, rw,
                           "Composition / regolith", lines, fh, fb)

    # Lighting section
    if zone.permanently_shadowed or zone.max_sun_altitude_deg \
            or zone.lighting_notes:
        lines = []
        if zone.max_sun_altitude_deg:
            lines.append(f"Max sun altitude:  "
                         f"{zone.max_sun_altitude_deg:.0f}°")
        lines.append("Earth visible:  "
                     f"{'yes' if zone.earth_visible else 'no (far side)'}")
        if zone.permanently_shadowed:
            lines.append("Permanently shadowed regions present.")
        if zone.lighting_notes:
            lines.extend(_wrap(zone.lighting_notes, fb, rw, draw))
        y = _draw_section(draw, rx, y, rw, "Lighting", lines, fh, fb)

    # History / significance section
    history_lines = []
    if zone.formed_by:
        history_lines.append(f"Formed by:  {zone.formed_by}")
    if zone.formation_notes:
        history_lines.extend(_wrap(zone.formation_notes, fb, rw, draw))
    if zone.missions:
        history_lines.append(
            "Missions:  " + ", ".join(zone.missions))
    if zone.significance:
        history_lines.extend(_wrap(zone.significance, fb, rw, draw))
    if zone.named_after:
        history_lines.extend(_wrap(f"Named after: {zone.named_after}",
                                     fb, rw, draw))
    if history_lines:
        y = _draw_section(draw, rx, y, rw,
                           "History / significance", history_lines, fh, fb)

    # Footer with action hints
    footer_y = H - 36
    draw.line([(24, footer_y - 8), (W - 24, footer_y - 8)],
              fill=(50, 60, 80), width=1)
    draw.text((24, footer_y),
              "ENTER  land at this zone",
              fill=(180, 220, 180), font=_font(13))
    draw.text((W - 200, footer_y),
              "ESC  back to orbit",
              fill=(180, 180, 220), font=_font(13))

    canvas.save(output_path)
    print(f"  wrote {output_path}")
    return canvas


# ===========================================================================
# Main — render representative panels + a 6-up sheet
# ===========================================================================

def main():
    examples = ["Apollo 11", "Tycho", "Mare Imbrium", "Shackleton",
                "South Pole–Aitken basin", "Apollo 17"]

    panels = []
    for name in examples:
        z = lookup_by_name(name)
        if z is None:
            print(f"  [skip] no zone named {name!r}")
            continue
        slug = (name.lower()
                .replace(" ", "_").replace("'", "").replace("–", "-")
                .replace("(", "").replace(")", ""))
        out = os.path.join(OUT, f"zone_info_{slug}.png")
        panel = render_zone_panel(z, out)
        panels.append(panel)

    if len(panels) < 2:
        return

    # 2x3 grid of all panels
    cols, rows = 3, 2
    pad = 12
    title_h = 50
    panel_w, panel_h = panels[0].size
    canvas = Image.new("RGB",
                        (cols * panel_w + (cols - 1) * pad,
                         title_h + rows * panel_h + (rows - 1) * pad),
                        (10, 12, 18))
    d = ImageDraw.Draw(canvas)
    d.text((20, 14),
           "Zone-info-panel mockups  —  shown for representative "
           "feature types (mare, crater, polar, basin, landing)",
           fill=(220, 220, 220), font=_font(20))
    for i, p in enumerate(panels):
        c = i % cols
        r = i // cols
        x = c * (panel_w + pad)
        y = title_h + r * (panel_h + pad)
        canvas.paste(p, (x, y))
    out_path = os.path.join(OUT, "zone_info_grid.png")
    canvas.save(out_path)
    print(f"  wrote {out_path}")


if __name__ == "__main__":
    main()
