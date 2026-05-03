"""Wrap the flat planet_full.png onto a sphere viewed head-on.

Treats the input texture as an equirectangular projection of one
lunar hemisphere (lat ∈ [-90°, +90°], lon ∈ [-90°, +90°]). For each
output pixel inside the disc, computes the corresponding sphere
surface point, converts to lat/lon, and samples the texture.

Adds limb darkening (brightness × z^0.6 where z is the surface
normal's component toward the camera — falls off at the disc edge).
Composites onto a starfield background.

Output: output/planet_orbital.png
"""

from __future__ import annotations

import math
import os

import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "output")


def starfield(shape, density=0.0008, brightness=(60, 200), seed=42):
    """Sparse point-stars on a deep-space background."""
    h, w = shape
    bg = np.array([6, 7, 12], dtype=np.float32)
    out = np.full((h, w, 3), bg, dtype=np.float32)
    rng = np.random.default_rng(seed)
    n = int(h * w * density)
    ys = rng.integers(0, h, n)
    xs = rng.integers(0, w, n)
    bs = rng.uniform(brightness[0], brightness[1], n)
    for y, x, b in zip(ys, xs, bs):
        out[y, x] = (b, b, b * 0.95)
    # Spread a bit so single pixels read at different scales
    return out


def wrap_to_sphere(texture_path, output_size=1200, output_path=None,
                    margin=12, limb_exponent=0.6,
                    edge_softness=2.0):
    """Project a square equirectangular hemisphere texture onto a
    sphere viewed head-on.

    Math:
      For each output pixel (xs, ys) in [-1, 1]² centred on the disc,
      compute z = sqrt(1 - xs² - ys²) for the front hemisphere.
      Convert (xs, ys, z) to lat/lon:  lat = asin(ys),
                                       lon = atan2(xs, z).
      Sample the texture at the corresponding equirectangular pixel.
      Multiply by z^limb_exponent for natural limb darkening.
      Blend over a starfield background with a soft disc edge.
    """
    src = np.asarray(Image.open(texture_path).convert("RGB"),
                      dtype=np.float32)
    h_src, w_src = src.shape[:2]

    bg = starfield((output_size, output_size))

    # Per-pixel coords
    yy, xx = np.mgrid[0:output_size, 0:output_size].astype(np.float32)
    cx = cy = output_size / 2
    r_px = output_size / 2 - margin
    u = (xx - cx) / r_px
    v = (cy - yy) / r_px

    d2 = u * u + v * v
    z = np.sqrt(np.clip(1 - d2, 0, 1))

    # Spherical coords for the front hemisphere
    lat = np.arcsin(np.clip(v, -1, 1))
    lon = np.arctan2(u, np.maximum(z, 1e-6))

    # Equirectangular sample. Texture is treated as a hemisphere:
    #   lat -π/2..π/2 → ty 0..h_src
    #   lon -π/2..π/2 → tx 0..w_src
    tx = (lon + math.pi / 2) / math.pi * w_src
    ty = (math.pi / 2 - lat) / math.pi * h_src

    # Bilinear sampling
    tx0 = np.clip(np.floor(tx).astype(np.int32), 0, w_src - 1)
    ty0 = np.clip(np.floor(ty).astype(np.int32), 0, h_src - 1)
    tx1 = np.clip(tx0 + 1, 0, w_src - 1)
    ty1 = np.clip(ty0 + 1, 0, h_src - 1)
    fx = (tx - tx0)[..., None]
    fy = (ty - ty0)[..., None]
    s00 = src[ty0, tx0]
    s01 = src[ty0, tx1]
    s10 = src[ty1, tx0]
    s11 = src[ty1, tx1]
    sampled = (s00 * (1 - fx) * (1 - fy)
               + s01 * fx * (1 - fy)
               + s10 * (1 - fx) * fy
               + s11 * fx * fy)

    # Limb darkening
    sphere = sampled * (z ** limb_exponent)[..., None]

    # Soft disc edge: alpha goes 1 inside, fades at the boundary
    alpha = np.clip(((1.0 - d2) / (2 * edge_softness / r_px)), 0, 1)

    out = sphere * alpha[..., None] + bg * (1 - alpha[..., None])
    out = np.clip(out, 0, 255).astype(np.uint8)

    if output_path is None:
        output_path = os.path.join(OUT, "planet_orbital.png")
    Image.fromarray(out).save(output_path)
    print(f"  wrote {output_path}")
    return out


def main():
    src_path = os.path.join(OUT, "planet_full.png")
    if not os.path.exists(src_path):
        print(f"missing source: {src_path}\nRun generate.py first.")
        return
    print(f"== wrapping {src_path} onto sphere ==")
    wrap_to_sphere(src_path, output_size=1200)


if __name__ == "__main__":
    main()
