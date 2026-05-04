"""Wrap a flat moon texture onto a sphere viewed head-on.

Two input layouts supported:

  * `extent="hemisphere"`  — square texture covering 180°×180° (the
    near side only). Original use case: the procedural planet_full.png.
  * `extent="globe"`       — 2:1 texture covering 360°×180° (the full
    moon). Use case: real LROC WAC mosaic. Rotate which hemisphere
    is visible with `camera_lon_deg`.

Both pass through the same projection math: for each output pixel
(xs, ys) inside the disc, compute z = √(1 − xs² − ys²), then
lat = asin(ys), lon = atan2(xs, z) (front-hemisphere only). The
texture-sampling map differs only in how lon → tx scales.

Adds limb darkening (brightness × z^0.6) and a soft disc edge over
a starfield.

Outputs:
  output/planet_orbital.png       — procedural planet wrapped
  output/planet_orbital_real.png  — real moon (WAC mosaic) wrapped
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
    return out


def wrap_to_sphere(texture_path, output_size=1200, output_path=None,
                    margin=12, limb_exponent=0.6, edge_softness=2.0,
                    extent="hemisphere", camera_lon_deg=0.0,
                    apply_limb_darkening=True):
    """Project an equirectangular texture onto a sphere viewed head-on.

    Args:
      extent: "hemisphere" (180°×180°) or "globe" (360°×180°).
      camera_lon_deg: only used for "globe" — which longitude is at
        the disc centre. 0 puts the prime meridian dead-on.
      apply_limb_darkening: real WAC mosaics already have natural
        limb shading baked in; setting this to False avoids
        double-darkening at the disc edge.
    """
    src = np.asarray(Image.open(texture_path).convert("RGB"),
                      dtype=np.float32)
    h_src, w_src = src.shape[:2]

    bg = starfield((output_size, output_size))

    yy, xx = np.mgrid[0:output_size, 0:output_size].astype(np.float32)
    cx = cy = output_size / 2
    r_px = output_size / 2 - margin
    u = (xx - cx) / r_px
    v = (cy - yy) / r_px

    d2 = u * u + v * v
    z = np.sqrt(np.clip(1 - d2, 0, 1))

    lat = np.arcsin(np.clip(v, -1, 1))
    lon = np.arctan2(u, np.maximum(z, 1e-6))

    if extent == "hemisphere":
        # 180°×180° texture.  lat -π/2..π/2 → ty 0..h_src,
        #                     lon -π/2..π/2 → tx 0..w_src.
        tx = (lon + math.pi / 2) / math.pi * w_src
    elif extent == "globe":
        # 360°×180° texture. Apply a longitude offset so the chosen
        # hemisphere lands at the disc centre.
        cam_lon_rad = math.radians(camera_lon_deg)
        lon_shifted = (lon + cam_lon_rad + math.pi) % (2 * math.pi) - math.pi
        tx = (lon_shifted + math.pi) / (2 * math.pi) * w_src
    else:
        raise ValueError(f"unknown extent {extent!r}")

    ty = (math.pi / 2 - lat) / math.pi * h_src

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

    if apply_limb_darkening:
        sphere = sampled * (z ** limb_exponent)[..., None]
    else:
        sphere = sampled

    alpha = np.clip(((1.0 - d2) / (2 * edge_softness / r_px)), 0, 1)
    out = sphere * alpha[..., None] + bg * (1 - alpha[..., None])
    out = np.clip(out, 0, 255).astype(np.uint8)

    if output_path is None:
        output_path = os.path.join(OUT, "planet_orbital.png")
    Image.fromarray(out).save(output_path)
    print(f"  wrote {output_path}")
    return out


def main():
    # Procedural wrap (the original use case).
    src_proc = os.path.join(OUT, "planet_full.png")
    if os.path.exists(src_proc):
        print(f"== wrapping procedural planet ==")
        wrap_to_sphere(src_proc, output_size=1200,
                        output_path=os.path.join(OUT, "planet_orbital.png"),
                        extent="hemisphere")

    # Real-moon wrap (LROC WAC mosaic from Solar System Scope mirror).
    src_real = os.path.join(
        os.path.dirname(__file__), "data", "global_moon", "moon_color_8k.jpg")
    if os.path.exists(src_real):
        print(f"== wrapping real WAC mosaic ==")
        wrap_to_sphere(src_real, output_size=1200,
                        output_path=os.path.join(OUT, "planet_orbital_real.png"),
                        extent="globe", camera_lon_deg=0.0,
                        apply_limb_darkening=False)
        # Also produce a far-side view for reference
        wrap_to_sphere(src_real, output_size=1200,
                        output_path=os.path.join(OUT,
                                                  "planet_orbital_real_farside.png"),
                        extent="globe", camera_lon_deg=180.0,
                        apply_limb_darkening=False)


if __name__ == "__main__":
    main()
