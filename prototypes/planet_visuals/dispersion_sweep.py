"""Crater-dispersion calibration sweep.

Generates 18 sample renders varying three parameters that drive how
"natural" the crater field looks at the procedural pipeline's
crater-dense (highland) setting:

  density_mult: total crater count multiplier — 0.30, 0.55, 0.90
                (vs the procedural baseline 140 small / 55 med / 4 big)
  age_alpha:    Beta(α, 1.5) shape for crater age — 1.5, 2.5, 4.0
                (higher → more eroded / older / shallower)
  min_sep:      minimum rim-to-rim separation — 1.15, 1.35
                (1.15 is current; 1.35 spreads craters more)

Layout: 6 cols x 3 rows, each row = one density level. Within a row,
the 6 cells span (3 age levels) x (2 separations). Each cell is
800x800 px, labelled with its parameter triple.

Output: output/dispersion_sweep.png

Real-data path (generate_real.py) is untouched — it bypasses
sample_craters entirely, loading real LOLA elevation directly. So
this calibration only affects the procedural mode.
"""

from __future__ import annotations

import os
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import (
    PLANET_GRID, apply_craters, assign_archetype_grid,
    cast_shadows, colourise, fbm, gaussian_blur, hillshade,
    label_image, pink_noise, sample_craters,
)

OUT = os.path.join(os.path.dirname(__file__), "output")
SIZE = 800
SEED = 12345

# Baseline procedural counts
BASE_SMALL, BASE_MED, BASE_BIG = 140, 55, 4

# Parameter levels
DENSITIES = [0.30, 0.55, 0.90]
AGE_ALPHAS = [1.5, 2.5, 4.0]
MIN_SEPS = [1.15, 1.35]


def render_one(density_mult, age_alpha, min_sep):
    """Render one cell. Same FBM heightmap each time so only the crater
    field varies — easier visual A/B."""
    # Use a fresh rng per call seeded from SEED + a hash so each cell
    # has a different *pattern* of craters. Otherwise we'd compare 18
    # near-identical layouts only differing in count.
    seed_hash = (hash((density_mult, age_alpha, min_sep)) & 0xffffffff)
    rng = np.random.default_rng(SEED ^ seed_hash)
    shape = (SIZE, SIZE)

    base = fbm(shape, 5, 384, 0.5, rng)
    detail = fbm(shape, 3, 128, 0.5, rng)
    height = 0.92 * base + 0.08 * detail
    height = (height - height.mean()) * 0.30

    # Scale crater counts by density_mult
    cs = max(1, int(BASE_SMALL * density_mult))
    cm = max(1, int(BASE_MED * density_mult))
    cb = max(1, int(BASE_BIG * density_mult))

    craters = sample_craters(shape, rng,
                              count_small=cs, count_med=cm, count_big=cb,
                              min_separation=min_sep, age_alpha=age_alpha)
    h_with = apply_craters(height.copy(), craters, rng)
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
    label = (f"density={density_mult:.2f}  "
             f"age={age_alpha:.1f}  sep={min_sep:.2f}  ({len(craters)} craters)")
    return label_image(img, label, font_size=14)


def main():
    print("== rendering 18-cell dispersion sweep ==")
    panels = []
    cells_per_row = len(AGE_ALPHAS) * len(MIN_SEPS)  # 6
    rows = len(DENSITIES)                              # 3
    for d in DENSITIES:
        for ms in MIN_SEPS:
            for a in AGE_ALPHAS:
                t0 = time.time()
                p = render_one(d, a, ms)
                print(f"  d={d} a={a} ms={ms} ({time.time()-t0:.1f}s)")
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
              "Crater-dispersion calibration  --  rows: density, "
              "cols: (age, separation)",
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
