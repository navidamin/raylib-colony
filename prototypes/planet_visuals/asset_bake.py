"""Bake game-ready planet visual assets and copy them into the
game's assets directory.

Usage: from the prototype directory, run

    python3 asset_bake.py

Produces:
  src/assets/planet/orbital_near.png   — moon disc thumbnail, near side
                                          (1200x1200, used by the Orbital view)
  src/assets/planet/orbital_far.png    — far-side, for completeness
  src/assets/planet/wac_global.jpg     — full-globe equirectangular WAC
                                          mosaic (4096x2048, used at runtime
                                          for region rendering / zoom)

This is the source of truth for the C++ side. Re-run any time the
prototype's renderer changes.
"""

from __future__ import annotations

import os
import shutil

import numpy as np
from PIL import Image

from wrap_to_sphere import wrap_to_sphere

PROTOTYPE_DIR = os.path.dirname(os.path.abspath(__file__))
PROTOTYPE_OUT = os.path.join(PROTOTYPE_DIR, "output")
PROTOTYPE_DATA = os.path.join(PROTOTYPE_DIR, "data", "global_moon")

# Repo-relative game-asset destination. The prototype lives at
# prototypes/planet_visuals/ inside the repo, so go up two levels.
REPO_ROOT = os.path.normpath(os.path.join(PROTOTYPE_DIR, "..", ".."))
GAME_ASSETS = os.path.join(REPO_ROOT, "src", "assets", "planet")


def ensure_dest():
    os.makedirs(GAME_ASSETS, exist_ok=True)


def bake_orbital_disc(label, src_texture, camera_lon_deg, out_filename):
    """Wrap a global texture onto a sphere and save the disc PNG."""
    print(f"  [{label}] camera_lon={camera_lon_deg}°")
    out_path = os.path.join(GAME_ASSETS, out_filename)
    wrap_to_sphere(src_texture,
                    output_size=1200,
                    output_path=out_path,
                    extent="globe",
                    camera_lon_deg=camera_lon_deg,
                    apply_limb_darkening=False)


def copy_global_mosaic():
    """Drop the full equirectangular WAC mosaic alongside the discs.

    The C++ side uses this for runtime per-region rendering when we
    zoom in past the orbital view (Phase E). Picks whichever cached
    resolution is highest.
    """
    src_8k = os.path.join(PROTOTYPE_DATA, "moon_8k.jpg")
    src_4k = os.path.join(PROTOTYPE_DATA, "moon_color_8k.jpg")
    src = src_8k if os.path.exists(src_8k) else src_4k
    dst = os.path.join(GAME_ASSETS, "wac_global.jpg")
    if not os.path.exists(src):
        print(f"  [skip] WAC global not cached at {src}")
        return
    shutil.copy(src, dst)
    sz = os.path.getsize(dst)
    print(f"  copied wac_global.jpg from {os.path.basename(src)} "
          f"({sz / 1024 / 1024:.1f} MB)")


def main():
    ensure_dest()
    # Prefer the higher-res 8K source (1.34 km/pixel) when present;
    # fall back to the legacy 4K-but-mislabeled-as-8k texture.
    src_8k = os.path.join(PROTOTYPE_DATA, "moon_8k.jpg")
    src_4k = os.path.join(PROTOTYPE_DATA, "moon_color_8k.jpg")
    src_texture = src_8k if os.path.exists(src_8k) else src_4k
    if not os.path.exists(src_texture):
        raise SystemExit(
            f"missing source texture: {src_texture}\n"
            f"run wrap_to_sphere.py first or check data/global_moon/")
    print(f"  source: {os.path.basename(src_texture)}")

    print("== bake orbital discs ==")
    bake_orbital_disc("near", src_texture,   0.0, "orbital_near.png")
    bake_orbital_disc("far",  src_texture, 180.0, "orbital_far.png")

    print("== copy global mosaic for runtime use ==")
    copy_global_mosaic()

    print(f"\nbaked into {GAME_ASSETS}")
    for name in sorted(os.listdir(GAME_ASSETS)):
        sz = os.path.getsize(os.path.join(GAME_ASSETS, name))
        print(f"  {name}: {sz / 1024:.1f} KB")


if __name__ == "__main__":
    main()
