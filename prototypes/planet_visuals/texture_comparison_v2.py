"""Iteration on texture_comparison.py — softer variants of pink noise
and Worley, since the user picked #4 (pink) for style/shade and #3
(Worley) as second-best, but #4 came out too ragged.

Six new candidates, all in the "noise + cellular" family but with
either lower amplitude, redshifted spectrum, or post-blur to tame
high-frequency raggedness.

Output: output/texture_comparison_v2.png
"""

from __future__ import annotations

import math
import os
import time

import numpy as np
from PIL import Image

from generate import (
    PLANET_GRID, apply_craters, assign_archetype_grid,
    cast_shadows, colourise, fbm, gaussian_blur, hillshade,
    label_image, pink_noise, sample_craters,
)

OUT = os.path.join(os.path.dirname(__file__), "output")
SIZE = 800
SEED = 12345


# ---------------------------------------------------------------------------
# Building blocks reused below (pink_noise lives in generate.py now)
# ---------------------------------------------------------------------------

def worley_field(shape, rng, *, n_cells=4000, mode="F1"):
    """Worley/cellular noise. mode='F1' returns distance to nearest
    cell point; mode='F2-F1' returns 2nd-nearest minus nearest, which
    gives a ridge network instead of bumps."""
    h, w = shape
    pts = np.column_stack([rng.uniform(0, w, n_cells),
                           rng.uniform(0, h, n_cells)]).astype(np.float32)
    cell_size = max(8, int(math.sqrt(h * w / n_cells) * 1.6))
    grid_h = h // cell_size + 1
    grid_w = w // cell_size + 1
    buckets: list[list[list[tuple[float, float]]]] = \
        [[[] for _ in range(grid_w)] for _ in range(grid_h)]
    for x, y in pts:
        buckets[int(y) // cell_size][int(x) // cell_size].append((x, y))
    out = np.zeros(shape, dtype=np.float32)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    for gy in range(grid_h):
        y0 = gy * cell_size
        y1 = min(h, y0 + cell_size)
        for gx in range(grid_w):
            x0 = gx * cell_size
            x1 = min(w, x0 + cell_size)
            local: list[tuple[float, float]] = []
            for dy in (-1, 0, 1):
                ny = gy + dy
                if 0 <= ny < grid_h:
                    for dx in (-1, 0, 1):
                        nx = gx + dx
                        if 0 <= nx < grid_w:
                            local.extend(buckets[ny][nx])
            if not local:
                continue
            local_pts = np.asarray(local, dtype=np.float32)
            patch_xx = xx[y0:y1, x0:x1]
            patch_yy = yy[y0:y1, x0:x1]
            dx = patch_xx[..., None] - local_pts[:, 0]
            dy = patch_yy[..., None] - local_pts[:, 1]
            dist = np.sqrt(dx * dx + dy * dy)
            if mode == "F1":
                out[y0:y1, x0:x1] = dist.min(axis=-1) / cell_size
            elif mode == "F2-F1":
                # 2nd-nearest minus nearest. Need at least 2 points.
                if dist.shape[-1] >= 2:
                    sorted_d = np.partition(dist, 1, axis=-1)
                    f2_f1 = (sorted_d[..., 1] - sorted_d[..., 0]) / cell_size
                    out[y0:y1, x0:x1] = f2_f1
                else:
                    out[y0:y1, x0:x1] = dist.min(axis=-1) / cell_size
    out -= out.mean()
    s = out.std()
    if s > 1e-6:
        out /= s
    return out


# ---------------------------------------------------------------------------
# Six candidates
# ---------------------------------------------------------------------------

def t1_pink_soft(shape, rng):
    """Original pink noise but lower amplitude."""
    return 0.030 * pink_noise(shape, rng, exponent=1.0)


def t2_pink_redshift(shape, rng):
    """Pink with steeper 1/f^1.5 spectrum — more low-freq weight, less
    pixel-scale grain. Same energy, smoother feel."""
    return 0.045 * pink_noise(shape, rng, exponent=1.5)


def t3_brown(shape, rng):
    """Brown noise (1/f^2). Heavily redshifted — largest features
    dominate, fine grain almost gone."""
    return 0.055 * pink_noise(shape, rng, exponent=2.0)


def t4_pink_smoothed(shape, rng):
    """Pure pink noise then Gaussian blur to remove pixel-scale rag."""
    return 0.038 * gaussian_blur(pink_noise(shape, rng, exponent=1.0), 1.4)


def t5_worley_smoothed(shape, rng):
    """Worley with cell-boundary Gaussian blur — softens the cellular
    look from comparison v1."""
    return 0.05 * gaussian_blur(worley_field(shape, rng, n_cells=3500), 2.2)


def t6_worley_pink_blend(shape, rng):
    """Soft Worley structure (low-freq) plus quiet pink fine grain.
    Best of both: cellular character at large scale, granular feel up
    close. Worley contribution is heavier."""
    w = gaussian_blur(worley_field(shape, rng, n_cells=2500), 2.5)
    p = pink_noise(shape, rng, exponent=1.3)
    return 0.040 * w + 0.020 * p


METHODS = [
    ("1. pink soft  (amp 0.030)",            t1_pink_soft),
    ("2. pink redshift  (1/f^1.5)",          t2_pink_redshift),
    ("3. brown  (1/f^2.0)",                  t3_brown),
    ("4. pink + blur  (sigma 1.4)",          t4_pink_smoothed),
    ("5. Worley smoothed",                   t5_worley_smoothed),
    ("6. Worley + pink blend",               t6_worley_pink_blend),
]


# ---------------------------------------------------------------------------
# Shared base scene + render
# ---------------------------------------------------------------------------

def build_base_scene(seed):
    rng = np.random.default_rng(seed)
    shape = (SIZE, SIZE)
    base = fbm(shape, 5, 384, 0.5, rng)
    detail = fbm(shape, 3, 128, 0.5, rng)
    height = 0.92 * base + 0.08 * detail
    height = (height - height.mean()) * 0.30
    craters = sample_craters(shape, rng,
                              count_small=70, count_med=28, count_big=2)
    height_with_craters = apply_craters(height.copy(), craters, rng)
    arch_grid = assign_archetype_grid(rng)
    py = np.repeat(np.arange(PLANET_GRID), SIZE // PLANET_GRID)
    px = np.repeat(np.arange(PLANET_GRID), SIZE // PLANET_GRID)
    arch_pix = arch_grid[py[:, None], px[None, :]]
    return rng, height, height_with_craters, arch_pix


def render_with_texture(rng_master, fbm_height, height_with_craters, arch_pix,
                         method_name, method_fn):
    rng = np.random.default_rng(rng_master.integers(0, 2**31))
    t0 = time.time()
    texture = method_fn(height_with_craters.shape, rng)
    elapsed = time.time() - t0
    h_textured = height_with_craters + texture
    sh = hillshade(h_textured)
    cast = cast_shadows(h_textured)
    rgb, _ = colourise(h_textured, np.zeros_like(fbm_height), sh, arch_pix,
                        rng, cast_mask=cast, albedo_height=fbm_height)
    img = Image.fromarray(rgb).resize((600, 600), Image.LANCZOS)
    img = label_image(img, f"{method_name}  ({elapsed:.1f}s)", font_size=18)
    return img


def main():
    print("== building shared base scene ==")
    rng, fbm_h, h_with_craters, arch_pix = build_base_scene(SEED)

    print("== rendering 6 candidates ==")
    panels = []
    for name, fn in METHODS:
        print(f"  {name}...")
        panels.append(render_with_texture(rng, fbm_h, h_with_craters,
                                            arch_pix, name, fn))

    print("  baseline (no texture)...")
    sh = hillshade(h_with_craters)
    cast = cast_shadows(h_with_craters)
    rgb, _ = colourise(h_with_craters, np.zeros_like(fbm_h), sh, arch_pix,
                        rng, cast_mask=cast, albedo_height=fbm_h)
    baseline = Image.fromarray(rgb).resize((600, 600), Image.LANCZOS)
    baseline = label_image(baseline, "0. baseline", font_size=18)

    cols, rows = 4, 2
    panel_w = 600 + 6
    canvas = Image.new("RGB",
                       (cols * panel_w - 6, rows * panel_w - 6),
                       (16, 16, 18))
    all_panels = [baseline] + panels
    for i, p in enumerate(all_panels):
        cx = (i % cols) * panel_w
        cy = (i // cols) * panel_w
        canvas.paste(p, (cx, cy))
    out = os.path.join(OUT, "texture_comparison_v2.png")
    canvas.save(out)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
