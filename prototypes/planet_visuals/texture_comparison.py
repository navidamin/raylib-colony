"""Surface-texture comparison sheet.

Goal: real lunar regolith between craters has a fine pebbled / stippled
texture from countless sub-resolution impacts and granular regolith.
The current generator leaves inter-crater terrain visually flat. This
script renders one base planet (same craters, same lighting) under six
different mathematical texture treatments, side by side, so we can pick
the look that best matches LRO close-up imagery.

Output: output/texture_comparison.png  (2 rows x 3 cols, 600 px each)
"""

from __future__ import annotations

import math
import os
import time

import numpy as np
from PIL import Image, ImageFilter

from generate import (
    ARCHETYPES, ARCHETYPE_ORDER, PX_PER_CELL, PLANET_GRID,
    apply_craters, archetype_pixel_map, assign_archetype_grid,
    cast_shadows, colourise, fbm, gaussian_blur, hillshade,
    label_image, sample_craters,
)

OUT = os.path.join(os.path.dirname(__file__), "output")
SIZE = 800              # rendered tile per panel (downscaled to 600 for output)
SEED = 12345


# ---------------------------------------------------------------------------
# Six texture methods. Each returns a heightfield delta to add to the
# base height (range typically +/- 0.05). Same shape as the input.
# ---------------------------------------------------------------------------

def texture_fbm(shape, rng, *, scale=10, octaves=4, amp=0.045):
    """Method 1 — high-frequency multi-octave FBM noise."""
    n = fbm(shape, octaves, scale, 0.5, rng)
    return amp * (n - n.mean())


def texture_micro_craters(shape, rng, *, count=2200,
                           r_range=(1.4, 3.5), depth_amp=0.06):
    """Method 2 — stamp many sub-resolution craters as tiny bowls.
    Geologically the truest because real regolith *is* made of these."""
    h, w = shape
    out = np.zeros(shape, dtype=np.float32)
    cx_arr = rng.uniform(0, w, count)
    cy_arr = rng.uniform(0, h, count)
    r_arr = rng.uniform(*r_range, count)
    d_arr = rng.uniform(0.4, 1.0, count) * depth_amp
    for cx, cy, r, d in zip(cx_arr, cy_arr, r_arr, d_arr):
        x0 = max(0, int(cx - r * 1.1))
        x1 = min(w, int(cx + r * 1.1) + 1)
        y0 = max(0, int(cy - r * 1.1))
        y1 = min(h, int(cy + r * 1.1) + 1)
        if x1 <= x0 or y1 <= y0:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        dn = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / r
        bowl = np.where(dn < 1.0, -d * (1.0 - dn ** 2), 0.0)
        out[y0:y1, x0:x1] += bowl
    return out


def texture_worley(shape, rng, *, n_cells=4000, amp=0.05):
    """Method 3 — Worley/cellular noise. Pebbled-stone look from
    distance-to-nearest-cell-point."""
    h, w = shape
    pts = np.column_stack([rng.uniform(0, w, n_cells),
                           rng.uniform(0, h, n_cells)]).astype(np.float32)
    # Coarse-grid acceleration: bucket points into cells, only check
    # pixels in nearby buckets.
    cell_size = max(8, int(math.sqrt(h * w / n_cells) * 1.6))
    grid_h = h // cell_size + 1
    grid_w = w // cell_size + 1
    buckets: list[list[list[tuple[float, float]]]] = \
        [[[] for _ in range(grid_w)] for _ in range(grid_h)]
    for x, y in pts:
        buckets[int(y) // cell_size][int(x) // cell_size].append((x, y))
    out = np.zeros(shape, dtype=np.float32)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    # For each grid cell, collect candidate points from its 3x3
    # neighbourhood and find the per-pixel min distance.
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
            min_dist = dist.min(axis=-1)
            # Map to 0..1 with cell_size as the natural unit, then push
            # to a positive bump shape (centres are low, edges high).
            out[y0:y1, x0:x1] = min_dist / cell_size
    out = out - out.mean()
    return amp * out


def texture_pink_noise(shape, rng, *, amp=0.05, exponent=1.0):
    """Method 4 — pink (1/f) noise via FFT spectrum shaping. The 1/f
    power spectrum is the statistical signature of natural rough
    surfaces (mountain landscapes, cratered terrain, etc.)."""
    h, w = shape
    white = rng.standard_normal(shape).astype(np.float32)
    spec = np.fft.fft2(white)
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0       # avoid div by zero at DC
    filt = 1.0 / (f ** exponent)
    filt[0, 0] = 0.0    # zero out DC (preserve mean)
    pink = np.real(np.fft.ifft2(spec * filt)).astype(np.float32)
    pink -= pink.mean()
    pink /= max(1e-6, pink.std())
    return amp * pink


def texture_gaussian_bumps(shape, rng, *, count=4500,
                            sigma_range=(0.8, 2.0), amp=0.04):
    """Method 5 — sparse Gaussian bumps, half positive half negative
    (boulders + pits), random sigmas."""
    h, w = shape
    out = np.zeros(shape, dtype=np.float32)
    cx_arr = rng.uniform(0, w, count)
    cy_arr = rng.uniform(0, h, count)
    sig_arr = rng.uniform(*sigma_range, count)
    sgn_arr = np.where(rng.random(count) < 0.55, -1.0, 1.0)  # slight pit bias
    for cx, cy, sig, sgn in zip(cx_arr, cy_arr, sig_arr, sgn_arr):
        rad = sig * 3.0
        x0 = max(0, int(cx - rad))
        x1 = min(w, int(cx + rad) + 1)
        y0 = max(0, int(cy - rad))
        y1 = min(h, int(cy + rad) + 1)
        if x1 <= x0 or y1 <= y0:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        d2 = (xx - cx) ** 2 + (yy - cy) ** 2
        out[y0:y1, x0:x1] += sgn * amp * np.exp(-d2 / (2 * sig * sig))
    return out


def texture_warped_fbm(shape, rng, *, scale=14, warp_scale=22,
                        octaves=3, warp_amp=12.0, amp=0.045):
    """Method 6 — domain-warped FBM. An FBM x-displaced and y-displaced
    by another FBM. Breaks up the smooth radial-symmetric look of plain
    FBM and gives an organic, almost flow-like grain."""
    h, w = shape
    wx = (fbm(shape, octaves, warp_scale, 0.5, rng) - 0.5) * warp_amp
    wy = (fbm(shape, octaves, warp_scale, 0.5, rng) - 0.5) * warp_amp
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    sample_x = np.clip(xx + wx, 0, w - 1).astype(np.int32)
    sample_y = np.clip(yy + wy, 0, h - 1).astype(np.int32)
    base = fbm(shape, octaves + 1, scale, 0.5, rng)
    warped = base[sample_y, sample_x]
    warped -= warped.mean()
    return amp * warped


METHODS = [
    ("1. high-freq FBM",          texture_fbm),
    ("2. micro-crater swarm",     texture_micro_craters),
    ("3. Worley / cellular",      texture_worley),
    ("4. pink (1/f) noise",       texture_pink_noise),
    ("5. Gaussian bump field",    texture_gaussian_bumps),
    ("6. domain-warped FBM",      texture_warped_fbm),
]


# ---------------------------------------------------------------------------
# Shared base scene (same heightmap + craters for every panel)
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
    """Render the same planet with one texture method's contribution
    added to the heightmap."""
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
    rng, fbm_height, h_with_craters, arch_pix = build_base_scene(SEED)

    print("== rendering 6 texture variants ==")
    panels = []
    for name, fn in METHODS:
        print(f"  {name}...")
        panels.append(render_with_texture(rng, fbm_height, h_with_craters,
                                            arch_pix, name, fn))

    # Also render an "off" baseline for reference (no texture).
    print("  baseline (no texture)...")
    sh = hillshade(h_with_craters)
    cast = cast_shadows(h_with_craters)
    rgb, _ = colourise(h_with_craters, np.zeros_like(fbm_height), sh, arch_pix,
                        rng, cast_mask=cast, albedo_height=fbm_height)
    baseline = Image.fromarray(rgb).resize((600, 600), Image.LANCZOS)
    baseline = label_image(baseline, "0. baseline (no texture)", font_size=18)

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
    out = os.path.join(OUT, "texture_comparison.png")
    canvas.save(out)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
