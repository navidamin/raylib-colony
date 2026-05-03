"""Biome-distribution comparison.

Old: assign_archetype_grid() — Voronoi from random centroids. Geological
nonsense, all biomes equally probable, polar bias hand-coded.

New: noise-field-based assignment with hard constraints.

  * FBM noise field defines mare-strength. Cells above the threshold
    become mare. Connected blobs because the noise is low-frequency.
    Threshold tuned so mare ~ 20% of cells.
  * Polar forced at top-2 / bottom-2 rows (latitude clamp).
  * KREEP at 1-2 random radial hotspots, ~3-5 cells each.
  * Lava tube at 1-3 randomly scattered cells (rare).
  * Mixed auto-assigned in the boundary band where mare-strength is
    close to the threshold (transition zone between mare and highland).
  * Highland is the default fill for everything else.

Output: output/biome_compare.png — side by side
  - left: Voronoi (current)
  - right: noise-field (proposed)
Each shows the biome grid as a coloured tile-grid AND the full
procedural planet rendered with that biome layout.
"""

from __future__ import annotations

import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

from generate import (
    ARCHETYPES, ARCHETYPE_ORDER, PLANET_GRID, PX_PER_CELL, SIZE,
    apply_craters, archetype_pixel_map, assign_archetype_grid,
    cast_shadows, colourise, fbm, gaussian_blur, hillshade,
    label_image, pink_noise, sample_craters,
)

OUT = os.path.join(os.path.dirname(__file__), "output")
SEED = 12345


# ---------------------------------------------------------------------------
# New biome assignment
# ---------------------------------------------------------------------------

def assign_archetype_grid_v2(rng):
    """Noise-field assignment with hard constraints. Returns a
    (PLANET_GRID, PLANET_GRID) int array of indices into ARCHETYPE_ORDER."""
    g = PLANET_GRID
    shape = (g, g)

    # Mare strength field — low-frequency FBM so we get connected blobs
    # rather than scattered cells.
    mare_strength = fbm(shape, 3, 4, 0.5, rng)
    mare_thresh = float(np.percentile(mare_strength, 80))   # ~20% mare

    # Polar bands at top/bottom 2 rows
    yy = np.arange(g).reshape(-1, 1).repeat(g, axis=1)
    polar_mask = (yy < 2) | (yy >= g - 2)

    # KREEP hotspots: 1-2 small radial blobs of ~3-5 cells each
    kreep_mask = np.zeros(shape, dtype=bool)
    n_hotspots = int(rng.integers(1, 3))
    for _ in range(n_hotspots):
        cx = int(rng.integers(3, g - 3))
        cy = int(rng.integers(3, g - 3))
        radius = float(rng.uniform(1.5, 2.5))
        ys, xs = np.mgrid[0:g, 0:g]
        d = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
        kreep_mask |= (d < radius)

    # Lava tube: 1-3 random isolated cells
    lava_mask = np.zeros(shape, dtype=bool)
    n_lava = int(rng.integers(1, 4))
    for _ in range(n_lava):
        x = int(rng.integers(0, g))
        y = int(rng.integers(0, g))
        lava_mask[y, x] = True

    # Mixed boundary band: where mare_strength is within 0.04 of the
    # threshold, that's a mare-highland transition zone.
    mixed_mask = (
        (mare_strength > mare_thresh - 0.04)
        & (mare_strength < mare_thresh + 0.04)
    )

    # Assign by priority (later overrides earlier)
    grid = np.full(shape, ARCHETYPE_ORDER.index("HIGHLAND_CONSTRUCTION"),
                    dtype=np.int32)
    grid[mare_strength > mare_thresh] = ARCHETYPE_ORDER.index("MARE_INDUSTRIAL")

    overridable = ~(polar_mask | kreep_mask | lava_mask)
    grid[mixed_mask & overridable] = ARCHETYPE_ORDER.index("MIXED")
    grid[kreep_mask] = ARCHETYPE_ORDER.index("KREEP_SCIENTIFIC")
    grid[polar_mask] = ARCHETYPE_ORDER.index("POLAR_VOLATILE")
    grid[lava_mask] = ARCHETYPE_ORDER.index("LAVA_TUBE")

    return grid


# ---------------------------------------------------------------------------
# Visualise grids and full renders
# ---------------------------------------------------------------------------

def grid_to_swatch(arch_grid, cell_px=24):
    """Render a (PLANET_GRID, PLANET_GRID) archetype grid as a coloured
    tile preview (no shading)."""
    g = arch_grid.shape[0]
    img = Image.new("RGB", (g * cell_px, g * cell_px), (16, 16, 18))
    draw = ImageDraw.Draw(img)
    for y in range(g):
        for x in range(g):
            arch = ARCHETYPES[ARCHETYPE_ORDER[arch_grid[y, x]]]
            color = arch.base
            draw.rectangle(
                [x * cell_px, y * cell_px,
                 (x + 1) * cell_px - 1, (y + 1) * cell_px - 1],
                fill=color)
    # Thin grid lines for cell boundaries
    for i in range(g + 1):
        draw.line([(i * cell_px, 0), (i * cell_px, g * cell_px)],
                  fill=(40, 40, 45), width=1)
        draw.line([(0, i * cell_px), (g * cell_px, i * cell_px)],
                  fill=(40, 40, 45), width=1)
    return img


def render_planet_with_grid(arch_grid, rng):
    """Run the full procedural pipeline using the given archetype grid."""
    shape = (SIZE, SIZE)
    base = fbm(shape, 5, 384, 0.5, rng)
    detail = fbm(shape, 3, 128, 0.5, rng)
    height = 0.92 * base + 0.08 * detail
    height = (height - height.mean()) * 0.30

    craters = sample_craters(shape, rng)
    h_with = apply_craters(height.copy(), craters, rng)
    h_with = h_with + 0.03 * gaussian_blur(pink_noise(shape, rng), 1.5)

    sh = hillshade(h_with)
    cast = cast_shadows(h_with)

    arch_pix = archetype_pixel_map(arch_grid)
    rgb, _ = colourise(h_with, np.zeros_like(height), sh, arch_pix, rng,
                        cast_mask=cast, albedo_height=height)
    return Image.fromarray(rgb)


def biome_legend(width):
    """Small colour-key strip showing each biome's base colour."""
    h = 30
    img = Image.new("RGB", (width, h), (16, 16, 18))
    draw = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 12)
    except OSError:
        font = ImageFont.load_default()
    pad = 8
    x = pad
    for name in ARCHETYPE_ORDER:
        a = ARCHETYPES[name]
        sw = 18
        draw.rectangle([x, 8, x + sw, h - 4], fill=a.base)
        short = {"MARE_INDUSTRIAL": "MARE", "HIGHLAND_CONSTRUCTION": "HIGH",
                 "POLAR_VOLATILE": "POLAR", "KREEP_SCIENTIFIC": "KREEP",
                 "LAVA_TUBE": "LAVA", "MIXED": "MIX"}[name]
        draw.text((x + sw + 4, 9), short, fill=(220, 220, 220), font=font)
        x += sw + 4 + 50
    return img


def main():
    rng_old = np.random.default_rng(SEED)
    rng_new = np.random.default_rng(SEED)
    grid_old = assign_archetype_grid(rng_old)
    grid_new = assign_archetype_grid_v2(rng_new)

    # Tally biome counts for each
    print("Voronoi grid biome counts:")
    for i, name in enumerate(ARCHETYPE_ORDER):
        n = int((grid_old == i).sum())
        print(f"  {name:24s} {n:3d}")
    print("Noise-field grid biome counts:")
    for i, name in enumerate(ARCHETYPE_ORDER):
        n = int((grid_new == i).sum())
        print(f"  {name:24s} {n:3d}")

    swatch_old = grid_to_swatch(grid_old)
    swatch_new = grid_to_swatch(grid_new)

    print("\n== rendering full planets ==")
    rng_render_old = np.random.default_rng(SEED + 1)
    rng_render_new = np.random.default_rng(SEED + 1)
    planet_old = render_planet_with_grid(grid_old, rng_render_old)
    planet_new = render_planet_with_grid(grid_new, rng_render_new)

    # Layout: 2 columns. Each column has a 480 px swatch on top and
    # a 1200 px planet render below.
    col_w = 1200
    swatch_resized_w = col_w
    swatch_h = swatch_resized_w * swatch_old.height // swatch_old.width
    swatch_old = swatch_old.resize((swatch_resized_w, swatch_h), Image.NEAREST)
    swatch_new = swatch_new.resize((swatch_resized_w, swatch_h), Image.NEAREST)
    swatch_old = label_image(swatch_old, "OLD: Voronoi grid", font_size=20)
    swatch_new = label_image(swatch_new, "NEW: noise-field grid", font_size=20)

    planet_old = planet_old.resize((col_w, col_w), Image.LANCZOS)
    planet_new = planet_new.resize((col_w, col_w), Image.LANCZOS)
    planet_old = label_image(planet_old,
                              "Full planet rendered with OLD grid",
                              font_size=18)
    planet_new = label_image(planet_new,
                              "Full planet rendered with NEW grid",
                              font_size=18)

    pad = 12
    legend = biome_legend(col_w * 2 + pad)
    canvas_h = swatch_h + pad + col_w + pad + legend.height
    canvas = Image.new("RGB", (col_w * 2 + pad, canvas_h), (16, 16, 18))
    canvas.paste(swatch_old, (0, 0))
    canvas.paste(swatch_new, (col_w + pad, 0))
    canvas.paste(planet_old, (0, swatch_h + pad))
    canvas.paste(planet_new, (col_w + pad, swatch_h + pad))
    canvas.paste(legend, (0, swatch_h + pad + col_w + pad))

    out = os.path.join(OUT, "biome_compare.png")
    canvas.save(out)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
