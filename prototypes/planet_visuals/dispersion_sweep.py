"""Crater-dispersion calibration sweep — v2 (size + depth-variance axes).

User feedback on v1:
  * Density 0.30 was still too dense — drop to 0.20 / 0.25.
  * Add `crater size` and `depth variance` as parameters too, since
    those visibly affect "naturalness" as much as count does.

Sweep: 18 cells = 3 (density) x 3 (size_scale) x 2 (depth_variance).
Outer rows = density; within each row 6 columns = (size x depth).

  density_mult ∈ {0.20, 0.25, 0.30}
    Multiplier on the procedural baseline crater counts (140 small +
    55 med + 4 big primaries). 0.20 ≈ 40 craters total.
  size_scale ∈ {0.7, 1.0, 1.3}
    Multiplier on every crater's radius. 0.7 = smaller craters,
    1.3 = bigger.
  depth_variance ∈ {0.7, 1.4}
    Spread of the per-crater depth_jitter around its mean. 0.7 =
    most craters land at similar depth (less drama). 1.4 = wide
    spread (some very shallow, some very deep).

age_alpha and min_separation are held at the values picked from v1
(4.0 and 1.35) so the sweep focuses on the new axes.

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

DENSITIES = [0.20, 0.25, 0.30]
SIZE_SCALES = [0.7, 1.0, 1.3]
DEPTH_VARIANCES = [0.7, 1.4]

# Pinned from v1 — picked as the most natural-looking
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
    label = (f"d={density_mult:.2f}  size={size_scale:.1f}  "
             f"dvar={depth_variance:.1f}  ({len(craters)} craters)")
    return label_image(img, label, font_size=14)


def main():
    print("== v2 sweep: density x size_scale x depth_variance ==")
    panels = []
    cells_per_row = len(SIZE_SCALES) * len(DEPTH_VARIANCES)  # 6
    rows = len(DENSITIES)                                      # 3
    for d in DENSITIES:
        for dv in DEPTH_VARIANCES:
            for sz in SIZE_SCALES:
                t0 = time.time()
                p = render_one(d, sz, dv)
                print(f"  d={d} size={sz} dvar={dv} ({time.time()-t0:.1f}s)")
                panels.append(p)

    cell_w = 600 + 6
    canvas = Image.new(
        "RGB",
        (cells_per_row * cell_w - 6,
         rows * cell_w - 6 + 50),
        (16, 16, 18))

    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
    except OSError:
        font = ImageFont.load_default()
    draw = ImageDraw.Draw(canvas)
    draw.text((10, 10),
              "Crater-dispersion v2  --  rows: density 0.20/0.25/0.30, "
              "cols: (depth_variance, size_scale)  "
              f"[age_alpha={AGE_ALPHA}, min_sep={MIN_SEP}]",
              fill=(230, 230, 230), font=font)

    for i, p in enumerate(panels):
        col = i % cells_per_row
        row = i // cells_per_row
        x = col * cell_w
        y = 50 + row * cell_w
        canvas.paste(p, (x, y))

    out = os.path.join(OUT, "dispersion_sweep.png")
    canvas.save(out)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
