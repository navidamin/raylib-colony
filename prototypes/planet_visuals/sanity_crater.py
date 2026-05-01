"""Sanity test: render ONE big crater. Verify rim/bowl light direction
reads as a depression, not a bump. Iterates parameters and saves a
small comparison sheet."""

import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import (apply_craters, Crater, hillshade,
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
    rgb = (sh * 255).astype(np.uint8)
    img = Image.fromarray(np.stack([rgb, rgb, rgb], axis=-1))
    img = label_image(img, label, font_size=14)
    return img


def main():
    rows = []
    # row 1: NW ramp — should be uniform-bright if hillshade math is right
    rows.append(render_row("NW ramp", "ramp_NW"))
    # row 2: hill (raised dome) — NW side should be bright, SE dark
    rows.append(render_row("hill (dome)", "hill"))
    # row 3: crater (depression) — should look like inner SE bright,
    # inner NW dark
    rows.append(render_row("crater (depression)", "crater"))

    sheet_h = sum(r.size[1] for r in rows) + 6 * (len(rows) - 1)
    sheet = Image.new("RGB", (rows[0].size[0], sheet_h), (16, 16, 18))
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
