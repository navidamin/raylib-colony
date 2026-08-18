"""Iteration on the "pink soft" direction the user picked from v2.

Two parameters varied across a 3 (amplitude) x 2 (post-blur) grid =
6 cells. All use the same 1/f pink-noise spectrum.

Output: output/texture_comparison_v3.png  (3 cols x 2 rows + small
baseline reference inset)
"""

from __future__ import annotations

import os
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import (
    PLANET_GRID, apply_craters, assign_archetype_grid,
    cast_shadows, colourise, fbm, gaussian_blur, hillshade,
    label_image, sample_craters,
)
from texture_comparison_v2 import pink_noise

OUT = os.path.join(os.path.dirname(__file__), "output")
SIZE = 800
SEED = 12345


# --- six variants: amplitude x post-blur sigma -----------------------------

AMPS = [0.015, 0.022, 0.030]
BLURS = [0.0, 1.5]


def make_variant(amp, blur_sigma):
    def fn(shape, rng):
        n = pink_noise(shape, rng, exponent=1.0)
        if blur_sigma > 0:
            n = gaussian_blur(n, blur_sigma)
            # Re-normalise after blurring so amp still scales something
            # comparable to the unblurred case.
            s = n.std()
            if s > 1e-6:
                n /= s
        return amp * n
    return fn


METHODS = []
for blur in BLURS:
    for amp in AMPS:
        label = f"amp={amp:.3f}  blur={blur:.1f}"
        METHODS.append((label, make_variant(amp, blur)))


# --- shared base scene -----------------------------------------------------

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

    # 3 cols x 2 rows. Top row: blur=0.0. Bottom row: blur=1.5.
    cols, rows = 3, 2
    panel_w = 600 + 6
    label_strip = 30
    canvas_w = cols * panel_w - 6
    canvas_h = rows * panel_w + label_strip
    canvas = Image.new("RGB", (canvas_w, canvas_h), (16, 16, 18))

    draw = ImageDraw.Draw(canvas)
    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 18)
    except OSError:
        font = ImageFont.load_default()
    draw.text((10, 6),
              "v3: pink-noise softness sweep "
              "(rows = post-blur sigma, cols = amplitude)",
              fill=(220, 220, 220), font=font)

    for i, p in enumerate(panels):
        cx = (i % cols) * panel_w
        cy = label_strip + (i // cols) * panel_w
        canvas.paste(p, (cx, cy))

    out = os.path.join(OUT, "texture_comparison_v3.png")
    canvas.save(out)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
