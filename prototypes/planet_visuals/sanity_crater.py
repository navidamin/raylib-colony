"""Sanity test: render ONE big crater. Verify rim/bowl light direction
reads as a depression, not a bump. Iterates parameters and saves a
small comparison sheet."""

import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import (apply_craters, Crater, hillshade, cast_shadows,
                      gaussian_blur, label_image)

OUT = os.path.join(os.path.dirname(__file__), "output")


def render_one(r, smooth_px, z_factor, label, mode="crater"):
    size = 400
    h = np.zeros((size, size), dtype=np.float32)
    rng = np.random.default_rng(0)
    if mode == "crater":
        crater = Crater(cx=size / 2, cy=size / 2, r=r,
                        age=0.05, has_peak=False)
        h = apply_craters(h, [crater], rng)
    elif mode == "hill":
        # raised gaussian dome — opposite sign of a crater bowl
        yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
        d = np.hypot(xx - size / 2, yy - size / 2)
        h = 0.4 * np.exp(-(d / r) ** 2)
    elif mode == "ramp_NW":
        # height increases toward NW corner: this surface should be brightest
        # in the NW under NW sun. If lighting math is right, this looks like
        # a uniform bright gradient.
        yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
        h = ((size - xx) + (size - yy)) / (2.0 * size) * 0.4
    sh = hillshade(h, z_factor=z_factor, smooth_px=smooth_px)
    cast = cast_shadows(h, z_factor=z_factor, max_distance_px=200.0,
                        step_px=1.5)
    # Combine: cast shadow takes a pixel down to 12% of its lit value,
    # matching the colourise pipeline.
    combined = sh * (0.12 + 0.88 * cast)
    rgb = (combined * 255).astype(np.uint8)
    img = Image.fromarray(np.stack([rgb, rgb, rgb], axis=-1))
    img = label_image(img, label, font_size=14)
    return img


def render_crater_size_row(size=400):
    """A row of craters at increasing radii, fixed pipeline params, so we
    can see how depth/shadow scales. Should match real LRO photos:
    small fresh craters' floors mostly in shadow; larger craters lit."""
    radii = [20, 35, 60, 95, 140]
    rng = np.random.default_rng(7)
    cells = []
    for r in radii:
        h = np.zeros((size, size), dtype=np.float32)
        crater = Crater(cx=size / 2, cy=size / 2, r=float(r),
                        age=0.05, has_peak=(r > 60))
        h = apply_craters(h, [crater], rng)
        sh = hillshade(h, z_factor=75.0, smooth_px=1.0)
        cast = cast_shadows(h, z_factor=75.0, max_distance_px=200.0,
                            step_px=1.5)
        combined = sh * (0.12 + 0.88 * cast)
        rgb = (combined * 255).astype(np.uint8)
        img = Image.fromarray(np.stack([rgb, rgb, rgb], axis=-1))
        img = label_image(img, f"r={r}", font_size=14)
        cells.append(img)
    w = size * len(cells) + 6 * (len(cells) - 1)
    row = Image.new("RGB", (w, size), (16, 16, 18))
    x = 0
    for c in cells:
        row.paste(c, (x, 0))
        x += size + 6
    return row


def main():
    rows = []
    rows.append(render_row("NW ramp", "ramp_NW"))
    rows.append(render_row("hill (dome)", "hill"))
    rows.append(render_row("crater (depression)", "crater"))
    rows.append(render_crater_size_row())

    sheet_w = max(r.size[0] for r in rows)
    sheet_h = sum(r.size[1] for r in rows) + 6 * (len(rows) - 1)
    sheet = Image.new("RGB", (sheet_w, sheet_h), (16, 16, 18))
    y = 0
    for r in rows:
        sheet.paste(r, (0, y))
        y += r.size[1] + 6
    sheet.save(os.path.join(OUT, "sanity_crater.png"))
    print("wrote", os.path.join(OUT, "sanity_crater.png"))


def render_row(title, mode):
    cells = []
    for smooth_px, z_factor in [(0.0, 30), (0.6, 50), (1.5, 70)]:
        cells.append(render_one(
            120, smooth_px, z_factor,
            f"{title}  blur={smooth_px} z={z_factor}",
            mode=mode))
    w = 400 * len(cells) + 6 * (len(cells) - 1)
    row = Image.new("RGB", (w, 400), (16, 16, 18))
    x = 0
    for c in cells:
        row.paste(c, (x, 0))
        x += 400 + 6
    return row


if __name__ == "__main__":
    main()
