#!/usr/bin/env python3
"""Cut the WAC blocks that regolith_craters.html carries inside itself.

The bench needs real imagery and has to open from file:// with no
server, so the mosaic travels in the page: one square block per region,
base64 PNG. A 100 km window is only ~75 texels at the WAC's 22.7556
texels/degree, so a block big enough to pan and zoom out in is still
small enough to embed -- 256 texels is 341 km and about 50 kB.

    python3 prototypes/planet_visuals/regolith_craters_block.py
    python3 ... --write
    python3 ... --add "Marius Hills" 14.2 -56.2 --write

Writes regolith_blocks.js, which both benches load.

Without --write it prints what it would do and leaves the bench alone.
Requires Pillow and numpy (the game itself needs neither).
"""

import argparse
import base64
import io
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WAC = ROOT / "src/assets/planet/wac_global.jpg"
BLOCKS_JS = Path(__file__).with_name("regolith_blocks.js")
KM_PER_DEG = 30.32268

# key, name, lat, lon, what the ground is, and where the view starts
# relative to the region centre in km east / km south. The offset exists
# because for a region NAMED after a crater the interesting ground for a
# synthesis bench is usually next to it, not inside it.
REGIONS = [
    ("plinius", "Plinius", 15.40, 23.70,
     "43 km crater on the Serenitatis / Tranquillitatis shore", 33.0, 12.0),
    ("imbrium", "Mare Imbrium", 32.80, -15.60,
     "the playfield's default anchor -- flat mare, few landforms", 0.0, 0.0),
    ("tranquility", "Tranquility Base", 0.67, 23.47,
     "Apollo 11 -- about as flat as the moon gets", 0.0, 0.0),
    ("copernicus", "Copernicus", 9.62, -20.08,
     "93 km, terraced walls and central peaks", 0.0, 0.0),
    ("tycho", "Tycho", -43.31, -11.36,
     "85 km and the freshest of the big ones; rays and rough highlands", 0.0, 0.0),
    ("aristarchus", "Aristarchus", 23.70, -47.40,
     "the brightest feature on the moon, on a plateau cut by Vallis Schröteri",
     0.0, 0.0),
    ("hadley", "Hadley / Apennines", 26.13, 3.63,
     "Apollo 15 -- mountain front and a sinuous rille", 0.0, 0.0),
    ("aristoteles", "Aristoteles", 50.20, 17.40,
     "87 km terraced crater in the northern highlands", 0.0, 0.0),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=256, help="block edge in WAC texels")
    ap.add_argument("--add", nargs=3, metavar=("NAME", "LAT", "LON"), action="append",
                    help="an extra region to carry")
    ap.add_argument("--write", action="store_true",
                    help="rewrite regolith_blocks.js")
    args = ap.parse_args()

    from PIL import Image
    import numpy as np
    Image.MAX_IMAGE_PIXELS = None

    if not WAC.exists():
        sys.exit(f"{WAC} not found")
    im = Image.open(WAC).convert("RGB")
    w, h = im.size
    rgb = np.asarray(im).astype(np.int32)
    # EnsureWacLoaded's own grayscale, so a block is byte-identical to
    # what the C++ reads out of the same JPEG.
    gray = ((rgb[:, :, 0] + rgb[:, :, 1] + rgb[:, :, 2] + 1) // 3).astype(np.uint8)

    regions = list(REGIONS)
    for extra in (args.add or []):
        name, lat, lon = extra[0], float(extra[1]), float(extra[2])
        key = re.sub(r"[^a-z0-9]+", "", name.lower())
        regions.append((key, name, lat, lon, "", 0.0, 0.0))

    n = args.size
    span_km = n / (w / 360.0) * KM_PER_DEG
    print(f"mosaic  {w}x{h}  {w / 360.0:.4f} texels/deg  "
          f"{KM_PER_DEG / (w / 360.0):.3f} km/texel")
    print(f"blocks  {n}x{n} texels = {span_km:.0f} km north-south each\n")

    out, total = [], 0
    for key, name, lat, lon, note, eKm, sKm in regions:
        x0 = int(round((lon + 180.0) / 360.0 * w - n / 2))
        y0 = int(round((90.0 - lat) / 180.0 * h - n / 2))
        if y0 < 0 or y0 + n > h:
            sys.exit(f"{name}: block runs off the mosaic in latitude")
        cols = [(x0 + i) % w for i in range(n)]          # longitude wraps
        block = gray[y0:y0 + n][:, cols]
        buf = io.BytesIO()
        Image.fromarray(block, "L").save(buf, format="PNG", optimize=True)
        b64 = base64.b64encode(buf.getvalue()).decode()
        total += len(b64)
        lon_km = span_km * math.cos(math.radians(lat))
        print(f"  {name:22s} {lat:7.2f} {lon:8.2f}   "
              f"{lon_km:.0f} km E-W   {len(buf.getvalue()) // 1024:3d} kB")
        out.append({"key": key, "name": name, "note": note,
                    "lat": lat, "lon": lon, "eKm": eKm, "sKm": sKm,
                    "x0": x0, "y0": y0, "png": b64})

    print(f"\ntotal base64 {total / 1024:.0f} kB across {len(out)} regions")
    payload = json.dumps({"wacW": w, "wacH": h, "size": n, "regions": out},
                         separators=(",", ":"))

    if not args.write:
        print("(dry run -- pass --write to rewrite regolith_blocks.js)")
        return
    BLOCKS_JS.write_text(HEADER + payload + ";\n")
    print(f"wrote {BLOCKS_JS.relative_to(ROOT)} "
          f"({BLOCKS_JS.stat().st_size // 1024} kB)")


HEADER = """// Eight real pieces of the moon, carried as data so the benches open from
// file:// with no server and no network.
//
// One 256x256-texel block per region out of src/assets/planet/wac_global.jpg
// (8192x4096 LROC WAC global mosaic, 22.7556 texels/degree, ~1.33 km/texel),
// grayscaled by EnsureWacLoaded's own (r+g+b+1)/3 and cut on whole texels, so
// the JS addresses it with the SAME global texel arithmetic the C++ uses. Each
// block is 341 km north-south, which leaves room to pan and to zoom out past
// the 100 km macro.
//
// Regenerate or extend with regolith_craters_block.py --write.
var WAC_BLOCKS =
"""


if __name__ == "__main__":
    main()
