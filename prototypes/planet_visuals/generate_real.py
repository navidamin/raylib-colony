"""Render the Plinius crater region of the Moon using real LOLA-derived
elevation data, through our existing pipeline.

Source: jaanga/moon-heightmaps-256p-ne (1° tiles at 256ppd, LOLA GLD100
re-encoded as 16-bit elevation packed into PNG R+255*G).

Region: lat 14..16°N, lon 22..24°E (~91 km x 88 km), centred on Plinius
(15.4°N, 23.7°E, 43 km diameter, ~2 km deep, fresh sharp-rimmed crater
on the Mare Tranquillitatis / Mare Serenitatis boundary). The crater
fits comfortably in a 3x3 patch with surrounding mare basin terrain
visible.

Why Plinius (not Aristoteles): Aristoteles is 87 km wide so its rim
sits *outside* even a 5x5 patch in longitude, AND its depth/diameter
ratio is only 0.038 — visually unimpressive. Plinius is 43 km / 2 km
= 0.047, sharper rim morphology, and the surrounding mare is flat so
the crater contrast is stronger.
"""

from __future__ import annotations

import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from generate import (
    cast_shadows, colourise, fbm, gaussian_blur, hillshade,
    label_image, pink_noise,
)

DATA_DIR = os.path.join(os.path.dirname(__file__), "data", "plinius")
OUT = os.path.join(os.path.dirname(__file__), "output")

# Region: lat 14..16°N, lon 22..24°E, 3x3 tiles
LAT_RANGE = (14, 16)
LON_RANGE = (22, 24)
LANDMARKS = [
    ("Plinius",  15.4, 23.7),       # 43 km — the centerpiece
    ("Dawes",    17.2, 26.3),       # 18 km — east of Plinius, just outside frame
]
REGION_SLUG = "plinius"             # used for output filenames
REGION_TITLE = "Plinius / Mare Tranquillitatis-Serenitatis edge"
SEED = 12345


# ---------------------------------------------------------------------------
# Load + stitch the heightmap tiles
# ---------------------------------------------------------------------------

def _align_seams(tiles):
    """Per-tile offset alignment so adjacent-tile boundary pixels match.

    Each tile in `tiles` is a 2D float array; `tiles` is a list-of-lists
    indexed [row][col] where row=0 is the *top* (north) tile. Returns
    a single stitched 2D array with seam offsets eliminated.

    Algorithm:
      * pin the centre tile at offset 0
      * for every other tile, propagate an additive offset so its edge
        with an already-aligned neighbour matches in mean
    """
    nrows = len(tiles)
    ncols = len(tiles[0])
    offsets = [[None] * ncols for _ in range(nrows)]
    cy, cx = nrows // 2, ncols // 2
    offsets[cy][cx] = 0.0

    # BFS outward from the centre
    from collections import deque
    queue = deque([(cy, cx)])
    while queue:
        r, c = queue.popleft()
        for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            nr, nc = r + dr, c + dc
            if not (0 <= nr < nrows and 0 <= nc < ncols):
                continue
            if offsets[nr][nc] is not None:
                continue
            t_known = tiles[r][c]
            t_new = tiles[nr][nc]
            if dr == -1:        # neighbour is NORTH (above) — known.bottom row vs new.top row
                edge_known = t_known[0, :]
                edge_new = t_new[-1, :]
            elif dr == 1:       # neighbour is SOUTH (below)
                edge_known = t_known[-1, :]
                edge_new = t_new[0, :]
            elif dc == -1:      # neighbour is WEST (left)
                edge_known = t_known[:, 0]
                edge_new = t_new[:, -1]
            else:               # neighbour is EAST (right)
                edge_known = t_known[:, -1]
                edge_new = t_new[:, 0]
            # offset_new such that  edge_new + offset_new + offsets[r][c]
            #                    ≈  edge_known + offsets[r][c]
            #              => offset_new = mean(edge_known) - mean(edge_new)
            offsets[nr][nc] = (offsets[r][c]
                                + float(edge_known.mean() - edge_new.mean()))
            queue.append((nr, nc))

    # Apply the offsets and stitch
    rows_arr = []
    for r in range(nrows):
        cols_arr = [tiles[r][c] + offsets[r][c] for c in range(ncols)]
        rows_arr.append(np.hstack(cols_arr))
    return np.vstack(rows_arr)


def load_elevation():
    """Stitch the 3x3 PNG tiles into one 768x768 elevation array.

    Encoding (per the jaanga rover code):  height = R + 255 * G
    where R/G are the red/green channels of the PNG.

    The PNG tiles have ~700-unit DC offset jumps at every 1° boundary
    — the per-tile encoding doesn't preserve absolute elevation across
    tiles. We fix by aligning each tile's offset to its neighbours so
    boundary pixels match in mean, then stitching.

    Image rows go top->bottom (north->south), so lat=2 is at the top.
    """
    tiles = []
    for lat in range(LAT_RANGE[1], LAT_RANGE[0] - 1, -1):
        row = []
        for lon in range(LON_RANGE[0], LON_RANGE[1] + 1):
            path = os.path.join(DATA_DIR, f"lat{lat}_lon{lon}.png")
            arr = np.asarray(Image.open(path), dtype=np.int32)
            h = arr[..., 0] + 255 * arr[..., 1]
            row.append(h.astype(np.float32))
        tiles.append(row)

    elevation = _align_seams(tiles)

    # BFS mean-alignment leaves per-column / per-row residuals along
    # each seam. Killing them cleanly: at each horizontal seam compute
    # the per-column step (elevation[s] - elevation[s-1]), smooth that
    # 1-D residual along the column axis, then *subtract* it from the
    # bottom half of the seam fading out over a band. Same for vertical
    # seams. Result: pixel-by-pixel continuity across boundaries
    # without losing in-tile detail.
    h, w = elevation.shape
    tile_size = 256
    band = 24

    def _fade(n):
        # Smooth fade from 1 at the seam to 0 at the edge of the band.
        x = np.arange(n, dtype=np.float32) / max(1, n - 1)
        return (1.0 - x) ** 2

    fade = _fade(band)
    # Cap each per-column / per-row correction so a chance large step
    # in real terrain at the seam doesn't get amplified into a deep
    # outlier. After BFS mean alignment, residuals should be small —
    # anything bigger than ~80 units almost certainly is real terrain
    # that happens to cross the seam, not encoding error. (For mostly
    # flat regions like Mare Tranquillitatis the cap is what keeps the
    # algorithm from carving artificial pits at corners.)
    correction_cap = 80.0

    def _capped(step_arr):
        return np.clip(step_arr, -correction_cap, correction_cap)

    for r in range(tile_size, h, tile_size):
        step = elevation[r, :] - elevation[r - 1, :]
        step = gaussian_blur(step[None, :], 6.0)[0]
        step = _capped(step)
        for i in range(band):
            if r + i < h:
                elevation[r + i, :] -= 0.5 * step * fade[i]
            if r - 1 - i >= 0:
                elevation[r - 1 - i, :] += 0.5 * step * fade[i]
    for c in range(tile_size, w, tile_size):
        step = elevation[:, c] - elevation[:, c - 1]
        step = gaussian_blur(step[:, None], 6.0)[:, 0]
        step = _capped(step)
        for i in range(band):
            if c + i < w:
                elevation[:, c + i] -= 0.5 * step * fade[i]
            if c - 1 - i >= 0:
                elevation[:, c - 1 - i] += 0.5 * step * fade[i]

    # Tiny finishing blur to suppress single-pixel encoding noise
    elevation = gaussian_blur(elevation, 0.8)

    # The "regional" trend (low-pass) is meaningful for albedo: low =
    # mare basalt (Tranquillitatis), high = surrounding highland.
    low_pass = gaussian_blur(elevation, 50.0)

    # Centre and scale to roughly the FBM amplitude that hillshade
    # expects.
    elevation = elevation - elevation.mean()
    target_std = 0.13
    scale = target_std / max(1e-6, elevation.std())
    elevation *= scale
    return elevation, low_pass


# ---------------------------------------------------------------------------
# Synthetic albedo derived from elevation
# ---------------------------------------------------------------------------

def albedo_from_elevation(low_pass):
    """Real moon has a strong elevation-albedo correlation: mare basalt
    is low and dark, highland anorthosite is high and bright. We use
    the low-pass elevation (the smooth regional trend) to drive a
    gradient between mare and highland greys.

    Per-tile DC artifacts in the raw data are still present in the
    low-pass — but at the tile-degree scale they DO correspond to real
    regional differences, so this still gives a meaningful mare↔highland
    map. (The seams are too sharp for albedo though, so we soften.)"""
    smooth = gaussian_blur(low_pass, 30.0)
    norm = smooth - smooth.min()
    span = max(1e-6, smooth.max() - smooth.min())
    t = (norm / span)[..., None]
    mare = np.array([108, 105, 100], dtype=np.float32)
    highland = np.array([170, 165, 156], dtype=np.float32)
    albedo = mare * (1 - t) + highland * t
    return albedo.astype(np.float32)


# ---------------------------------------------------------------------------
# Soft circular boundary
# ---------------------------------------------------------------------------

def soft_boundary_mask(shape, edge_softness=0.18, organic_seed=42):
    """A soft, slightly-organic disc that fades the rectangular tile to
    the background. `edge_softness` is the fraction of the radius over
    which the mask transitions from 1.0 inside to 0.0 outside. A small
    FBM jitter on the boundary radius keeps it from looking like a
    perfect circle."""
    h, w = shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cx, cy = w / 2, h / 2
    base_radius = min(w, h) / 2 * 0.95
    rng = np.random.default_rng(organic_seed)
    jitter = (fbm((h, w), 4, max(2, w // 32), 0.5, rng) - 0.5) * 0.05
    d = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2)
    radius_at_pixel = base_radius * (1.0 + jitter)
    edge = base_radius * edge_softness
    t = (radius_at_pixel - d) / edge
    mask = np.clip(t, 0.0, 1.0)
    return mask


# ---------------------------------------------------------------------------
# Apollo 11 marker
# ---------------------------------------------------------------------------

def annotate_landmarks(rgb_image, image_shape):
    """Draw a small crosshair + label at each named landmark that
    falls inside the rendered patch."""
    h, w = image_shape
    img = rgb_image.copy()
    draw = ImageDraw.Draw(img, "RGBA")
    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14)
    except OSError:
        font = ImageFont.load_default()
    for name, lat, lon in LANDMARKS:
        # Image y is flipped: top = north (LAT_RANGE[1]), bottom = south.
        rel_x = (lon - LON_RANGE[0]) / (LON_RANGE[1] - LON_RANGE[0] + 1)
        rel_y = 1.0 - (lat - LAT_RANGE[0]) / (
            LAT_RANGE[1] - LAT_RANGE[0] + 1)
        if not (0.02 < rel_x < 0.98 and 0.02 < rel_y < 0.98):
            continue
        px = int(rel_x * w)
        py = int(rel_y * h)
        r = 14
        draw.ellipse([px - r, py - r, px + r, py + r],
                     outline=(255, 80, 80, 220), width=2)
        draw.line([(px - r - 6, py), (px - r + 2, py)],
                  fill=(255, 80, 80, 220), width=2)
        draw.line([(px + r - 2, py), (px + r + 6, py)],
                  fill=(255, 80, 80, 220), width=2)
        draw.line([(px, py - r - 6), (px, py - r + 2)],
                  fill=(255, 80, 80, 220), width=2)
        draw.line([(px, py + r - 2), (px, py + r + 6)],
                  fill=(255, 80, 80, 220), width=2)
        draw.text((px + r + 8, py - 8), name,
                  fill=(255, 220, 200, 240), font=font)
    return img


# ---------------------------------------------------------------------------
# Render
# ---------------------------------------------------------------------------

def render_real():
    rng = np.random.default_rng(SEED)
    elevation, low_pass = load_elevation()
    print(f"  elevation: shape={elevation.shape} "
          f"std={elevation.std():.3f} range=[{elevation.min():.3f}, {elevation.max():.3f}]")

    # Save raw elevation as debug (normalized for visibility)
    e_norm = (elevation - elevation.min())
    e_norm /= max(1e-6, e_norm.max())
    Image.fromarray((e_norm * 255).astype(np.uint8), mode="L").save(
        os.path.join(OUT, f"{REGION_SLUG}_heightmap.png"))

    # Pink-noise regolith micro-texture, but at lower amplitude than
    # the procedural pipeline. Real LOLA data already encodes the
    # large-scale terrain; we just want a hint of granular regolith
    # without drowning subtle features like Plinius' shallow bowl.
    elevation_with_texture = elevation + 0.005 * pink_noise(
        elevation.shape, rng)

    # Higher z_factor than the procedural pipeline because real terrain
    # at this scale + this region has small relief amplitude — Plinius
    # is genuinely a shallow ~2km crater on a flat ~30km mare basin.
    # Need vertical exaggeration to make the bowl read as a bowl.
    sh = hillshade(elevation_with_texture, z_factor=110.0, smooth_px=1.0)
    cast = cast_shadows(elevation_with_texture, z_factor=110.0,
                         max_distance_px=80.0)

    # Albedo: derived from low-pass elevation (mare↔highland gradient).
    albedo = albedo_from_elevation(low_pass)

    # Compose: albedo modulated by hillshade and cast shadow, plus AO.
    h, w = elevation.shape
    sh_3 = sh[..., None]
    contrast = 0.55
    shaded = albedo * (1.0 - contrast + contrast * (0.35 + 1.30 * sh_3))

    # AO from local depth-below-baseline (smoothed elevation)
    smooth = gaussian_blur(elevation, 8.0)
    depth_below = np.clip(smooth - elevation_with_texture, 0.0, 1.5)
    ao = 1.0 - np.clip(depth_below * 0.85, 0.0, 0.45)
    shaded = shaded * ao[..., None]

    # Cast shadows (down to 22% in shadow)
    cm = cast[..., None]
    shaded = shaded * (0.22 + 0.78 * cm)

    rgb = np.clip(shaded, 0, 255).astype(np.uint8)

    # Soft circular boundary on a deep-space background
    mask = soft_boundary_mask((h, w), edge_softness=0.20)
    bg = np.array([8, 9, 14], dtype=np.float32)            # near-black
    out = rgb.astype(np.float32) * mask[..., None] \
          + bg * (1.0 - mask)[..., None]
    out = np.clip(out, 0, 255).astype(np.uint8)

    # Upscale to 1200 px for nicer output
    img = Image.fromarray(out).resize((1200, 1200), Image.LANCZOS)
    img = annotate_landmarks(img, (1200, 1200))
    img.save(os.path.join(OUT, f"{REGION_SLUG}_real.png"))
    print(f"  wrote {os.path.join(OUT, f'{REGION_SLUG}_real.png')}")
    return img


def render_compare(real_img):
    """Render the procedural pipeline on the same shape, side-by-side
    with the real-data render, for an A/B comparison."""
    from generate import (
        apply_craters, archetype_pixel_map, assign_archetype_grid,
        sample_craters, PLANET_GRID,
    )
    rng = np.random.default_rng(SEED)
    size = 768
    shape = (size, size)
    base = fbm(shape, 5, 384, 0.5, rng)
    detail = fbm(shape, 3, 128, 0.5, rng)
    height = (0.92 * base + 0.08 * detail - (
        0.92 * base + 0.08 * detail).mean()) * 0.30
    craters = sample_craters(shape, rng,
                              count_small=70, count_med=28, count_big=2)
    h_with = apply_craters(height.copy(), craters, rng)
    h_with = h_with + 0.015 * pink_noise(shape, rng)
    sh = hillshade(h_with, z_factor=75.0, smooth_px=1.0)
    cast = cast_shadows(h_with, z_factor=75.0)
    arch_grid = assign_archetype_grid(rng)
    # Stretch the 20x20 archetype grid to fit our exact `size` x `size`
    # canvas without truncation.
    py_idx = np.minimum(
        (np.arange(size) * PLANET_GRID // size), PLANET_GRID - 1)
    px_idx = py_idx
    arch_pix = arch_grid[py_idx[:, None], px_idx[None, :]]
    rgb, _ = colourise(h_with, np.zeros_like(height), sh, arch_pix, rng,
                        cast_mask=cast, albedo_height=height)
    mask = soft_boundary_mask(shape, edge_softness=0.20)
    bg = np.array([8, 9, 14], dtype=np.float32)
    proc = rgb.astype(np.float32) * mask[..., None] \
        + bg * (1.0 - mask)[..., None]
    proc = np.clip(proc, 0, 255).astype(np.uint8)
    proc_img = Image.fromarray(proc).resize((1200, 1200), Image.LANCZOS)

    # Side by side
    pad = 12
    composite = Image.new("RGB", (1200 * 2 + pad, 1200 + 50), (8, 9, 14))
    proc_img = label_image(proc_img,
                            "Procedural (FBM + Bridson craters)",
                            font_size=20)
    real_img2 = label_image(real_img,
                             f"Real LOLA elevation — {REGION_TITLE} "
                             f"(lat {LAT_RANGE[0]}..{LAT_RANGE[1]}°N, "
                             f"lon {LON_RANGE[0]}..{LON_RANGE[1]}°E)",
                             font_size=20)
    composite.paste(proc_img, (0, 50))
    composite.paste(real_img2, (1200 + pad, 50))
    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
    except OSError:
        font = ImageFont.load_default()
    draw = ImageDraw.Draw(composite)
    draw.text((10, 12), f"raylib-colony  --  procedural vs real-moon "
              f"({REGION_TITLE})",
              fill=(230, 230, 230), font=font)
    composite.save(os.path.join(OUT, f"{REGION_SLUG}_compare.png"))
    print(f"  wrote {os.path.join(OUT, f'{REGION_SLUG}_compare.png')}")


def main():
    print(f"== rendering {REGION_TITLE} from LOLA elevation ==")
    real_img = render_real()
    print("== procedural side-by-side ==")
    render_compare(real_img)
    print("done.")


if __name__ == "__main__":
    main()
