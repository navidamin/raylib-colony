"""Crater-dispersion calibration sweep — v4.

Pinned (from prior iterations):
  density_mult    = 0.12
  depth_variance  = 1.0
  age_alpha       = 4.0
  min_separation  = 1.35

Variable axes (3x2x3 = 18 cells):
  size_scale ∈ {1.0, 1.25, 1.5}     uniform multiplier on every radius
  size_variance ∈ {0.5, 1.5}        spread within each size bucket
  depth_scale ∈ {0.6, 1.0, 1.5}     uniform multiplier on baseline depth

Layout: 3 rows (depth_scale) x 6 cols (size_variance x size_scale).
  Within each row: cols 1..3 = size_variance 0.5 with sizes 1.0/1.25/1.5;
                   cols 4..6 = size_variance 1.5 with sizes 1.0/1.25/1.5.

Output: output/dispersion_sweep.png
"""

from __future__ import annotations

import os
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import (
    ARCHETYPE_ORDER, apply_craters, cast_shadows, colourise, fbm,
    gaussian_blur, hillshade, label_image, pink_noise, sample_craters,
)

OUT = os.path.join(os.path.dirname(__file__), "output")
SIZE = 800
SEED = 12345

BASE_SMALL, BASE_MED, BASE_BIG = 140, 55, 4

DENSITY = 0.12
DEPTH_VARIANCE = 1.0
AGE_ALPHA = 4.0
MIN_SEP = 1.35

SIZE_SCALES = [1.0, 1.25, 1.5]
SIZE_VARIANCES = [0.5, 1.5]
DEPTH_SCALES = [0.6, 1.0, 1.5]


def render_one(size_scale, size_variance, depth_scale):
    seed_hash = (hash((size_scale, size_variance, depth_scale)) & 0xffffffff)
    rng = np.random.default_rng(SEED ^ seed_hash)
    shape = (SIZE, SIZE)

    # Flat baseline (no FBM relief, no biome tint) — we want to compare
    # crater dispersion only, not light-and-shadow patterns from
    # background terrain or warm-vs-cold biome tinting. The full
    # procedural pipeline keeps both of those.
    height = np.zeros(shape, dtype=np.float32)

    cs = max(1, int(BASE_SMALL * DENSITY))
    cm = max(1, int(BASE_MED * DENSITY))
    cb = max(1, int(BASE_BIG * DENSITY))

    craters = sample_craters(shape, rng,
                              count_small=cs, count_med=cm, count_big=cb,
                              min_separation=MIN_SEP, age_alpha=AGE_ALPHA,
                              size_scale=size_scale,
                              size_variance=size_variance)
    h_with = apply_craters(height.copy(), craters, rng,
                            depth_variance=DEPTH_VARIANCE,
                            depth_scale=depth_scale)
    h_with = h_with + 0.03 * gaussian_blur(pink_noise(shape, rng), 1.5)

    sh = hillshade(h_with, z_factor=75.0, smooth_px=1.0)
    cast = cast_shadows(h_with, z_factor=75.0)

    # Uniform single-archetype tint (highland greys) for the calibration
    # sheet. Real planet_full.png keeps the per-cell biome assignment.
    arch_pix = np.zeros(shape, dtype=np.int32) + ARCHETYPE_ORDER.index(
        "HIGHLAND_CONSTRUCTION")

    rgb, _ = colourise(h_with, np.zeros_like(height), sh, arch_pix, rng,
                        cast_mask=cast, albedo_height=height)
    img = Image.fromarray(rgb).resize((600, 600), Image.LANCZOS)
    label = (f"size={size_scale:.2f}  svar={size_variance:.1f}  "
             f"depth={depth_scale:.1f}  ({len(craters)} craters)")
    return label_image(img, label, font_size=14)


def main():
    print(f"== v4 sweep: size x size_variance x depth_scale "
          f"(density={DENSITY}, dvar={DEPTH_VARIANCE}) ==")
    panels = []
    cols_per_row = len(SIZE_VARIANCES) * len(SIZE_SCALES)   # 6
    rows = len(DEPTH_SCALES)                                # 3
    for ds in DEPTH_SCALES:
        for sv in SIZE_VARIANCES:
            for sz in SIZE_SCALES:
                t0 = time.time()
                p = render_one(sz, sv, ds)
                print(f"  size={sz} svar={sv} depth={ds} ({time.time()-t0:.1f}s)")
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
              f"v4  --  rows: depth_scale 0.6/1.0/1.5,  cols: "
              f"(size_variance 0.5 then 1.5, size 1.0/1.25/1.5)  "
              f"[d={DENSITY}, dvar={DEPTH_VARIANCE}, "
              f"age={AGE_ALPHA}, sep={MIN_SEP}]",
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
