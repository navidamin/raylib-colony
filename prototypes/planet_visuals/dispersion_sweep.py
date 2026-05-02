"""Crater-dispersion calibration sweep — v3.

Sweep: 18 cells. User-requested values:

  density_mult ∈ {0.15, 0.20}
  size_scale ∈ {1.0, 1.25, 1.5}
  depth_variance ∈ {0.5, 0.7, 1.0}

Layout: 3 rows (depth_variance) x 6 cols (density x size_scale).
  Within each row, cols 1-3 = density 0.15 with sizes 1.0/1.25/1.5,
  cols 4-6 = density 0.20 with the same sizes.

age_alpha and min_separation pinned at 4.0 / 1.35 (from v1 pick).

Output: output/dispersion_sweep.png
"""

from __future__ import annotations

import os
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import (
    PLANET_GRID, apply_craters, assign_archetype_grid,
    cast_shadows, colourise, fbm, hillshade, label_image,
    pink_noise, sample_craters,
)

OUT = os.path.join(os.path.dirname(__file__), "output")
SIZE = 800
SEED = 12345

BASE_SMALL, BASE_MED, BASE_BIG = 140, 55, 4

DENSITIES = [0.15, 0.20]
SIZE_SCALES = [1.0, 1.25, 1.5]
DEPTH_VARIANCES = [0.5, 0.7, 1.0]

AGE_ALPHA = 4.0
MIN_SEP = 1.35


def render_one(density_mult, size_scale, depth_variance):
    seed_hash = (hash((density_mult, size_scale, depth_variance)) & 0xffffffff)
    rng = np.random.default_rng(SEED ^ seed_hash)
    shape = (SIZE, SIZE)

    base = fbm(shape, 5, 384, 0.5, rng)
    detail = fbm(shape, 3, 128, 0.5, rng)
    height = 0.92 * base + 0.08 * detail
    height = (height - height.mean()) * 0.30

    cs = max(1, int(BASE_SMALL * density_mult))
    cm = max(1, int(BASE_MED * density_mult))
    cb = max(1, int(BASE_BIG * density_mult))

    craters = sample_craters(shape, rng,
                              count_small=cs, count_med=cm, count_big=cb,
                              min_separation=MIN_SEP, age_alpha=AGE_ALPHA,
                              size_scale=size_scale)
    h_with = apply_craters(height.copy(), craters, rng,
                            depth_variance=depth_variance)
    h_with = h_with + 0.015 * pink_noise(shape, rng)

    sh = hillshade(h_with, z_factor=75.0, smooth_px=1.0)
    cast = cast_shadows(h_with, z_factor=75.0)

    arch_grid = assign_archetype_grid(rng)
    py_idx = np.minimum(
        (np.arange(SIZE) * PLANET_GRID // SIZE), PLANET_GRID - 1)
    arch_pix = arch_grid[py_idx[:, None], py_idx[None, :]]

    rgb, _ = colourise(h_with, np.zeros_like(height), sh, arch_pix, rng,
                        cast_mask=cast, albedo_height=height)
    img = Image.fromarray(rgb).resize((600, 600), Image.LANCZOS)
    label = (f"d={density_mult:.2f}  size={size_scale:.2f}  "
             f"dvar={depth_variance:.1f}  ({len(craters)} craters)")
    return label_image(img, label, font_size=14)


def main():
    print("== v3 sweep: density 0.15/0.20 x size 1.0/1.25/1.5 x dvar 0.5/0.7/1.0 ==")
    panels = []
    cols_per_row = len(DENSITIES) * len(SIZE_SCALES)   # 6
    rows = len(DEPTH_VARIANCES)                         # 3
    for dv in DEPTH_VARIANCES:
        for d in DENSITIES:
            for sz in SIZE_SCALES:
                t0 = time.time()
                p = render_one(d, sz, dv)
                print(f"  d={d} size={sz} dvar={dv} ({time.time()-t0:.1f}s)")
                panels.append(p)

    cell_w = 600 + 6
    canvas = Image.new(
        "RGB",
        (cols_per_row * cell_w - 6,
         rows * cell_w - 6 + 50),
        (16, 16, 18))

    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
    except OSError:
        font = ImageFont.load_default()
    draw = ImageDraw.Draw(canvas)
    draw.text((10, 10),
              "Crater dispersion v3  --  rows: depth_variance 0.5/0.7/1.0,  "
              "cols: (density 0.15/0.20, size 1.0/1.25/1.5)  "
              f"[age_alpha={AGE_ALPHA}, min_sep={MIN_SEP}]",
              fill=(230, 230, 230), font=font)

    for i, p in enumerate(panels):
        col = i % cols_per_row
        row = i // cols_per_row
        x = col * cell_w
        y = 50 + row * cell_w
        canvas.paste(p, (x, y))

    out = os.path.join(OUT, "dispersion_sweep.png")
    canvas.save(out)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
