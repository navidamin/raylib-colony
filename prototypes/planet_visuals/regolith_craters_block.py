#!/usr/bin/env python3
"""Cut the WAC block that regolith_craters.html carries inside itself.

The bench needs a piece of the real mosaic and has to open from file://
with no server, so the imagery travels in the page as a base64 PNG. A
100 km window is only ~75 texels at the WAC's 22.7556 texels/degree, so
a block big enough to pan around in is still small enough to embed.

    python3 prototypes/planet_visuals/regolith_craters_block.py
    python3 ... --lat -43.3 --lon -11.4 --size 320 --write

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
BENCH = Path(__file__).with_name("regolith_craters.html")
KM_PER_DEG = 30.32268


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lat", type=float, default=15.4, help="block centre latitude")
    ap.add_argument("--lon", type=float, default=23.7, help="block centre longitude")
    ap.add_argument("--size", type=int, default=320, help="block edge in WAC texels")
    ap.add_argument("--write", action="store_true",
                    help="patch the block into regolith_craters.html")
    args = ap.parse_args()

    from PIL import Image
    import numpy as np
    Image.MAX_IMAGE_PIXELS = None

    if not WAC.exists():
        sys.exit(f"{WAC} not found")
    im = Image.open(WAC).convert("RGB")
    w, h = im.size
    rgb = np.asarray(im).astype(np.int32)
    # EnsureWacLoaded's own grayscale, so the block is byte-identical to
    # what the C++ reads out of the same JPEG.
    gray = ((rgb[:, :, 0] + rgb[:, :, 1] + rgb[:, :, 2] + 1) // 3).astype(np.uint8)

    n = args.size
    x0 = int(round((args.lon + 180.0) / 360.0 * w - n / 2))
    y0 = int(round((90.0 - args.lat) / 180.0 * h - n / 2))
    if x0 < 0 or y0 < 0 or x0 + n > w or y0 + n > h:
        sys.exit("block runs off the mosaic; move the centre or shrink --size")
    block = gray[y0:y0 + n, x0:x0 + n]

    buf = io.BytesIO()
    Image.fromarray(block, "L").save(buf, format="PNG", optimize=True)
    b64 = base64.b64encode(buf.getvalue()).decode()

    span_km = n / (w / 360.0) * KM_PER_DEG
    lon_km = span_km * math.cos(math.radians(args.lat))
    print(f"mosaic  {w}x{h}  {w / 360.0:.4f} texels/deg  "
          f"{KM_PER_DEG / (w / 360.0):.3f} km/texel")
    print(f"block   {n}x{n} texels at ({x0}, {y0})  "
          f"{span_km:.0f} km N-S x {lon_km:.0f} km E-W")
    print(f"png     {len(buf.getvalue())} bytes  -> {len(b64)} base64")

    meta = {"wacW": w, "wacH": h, "x0": x0, "y0": y0, "w": n, "h": n,
            "lat": args.lat, "lon": args.lon, "png": b64}
    payload = json.dumps(meta, separators=(",", ":"))

    if not args.write:
        print("\n(dry run -- pass --write to patch the bench)")
        return
    src = BENCH.read_text()
    patched, count = re.subn(
        r'(<script id="wac-block" type="application/json">)[\s\S]*?(</script>)',
        lambda m: m.group(1) + "\n" + payload + "\n" + m.group(2), src, count=1)
    if count != 1:
        sys.exit("could not find the wac-block script tag in the bench")
    BENCH.write_text(patched)
    print(f"\nwrote {BENCH.relative_to(ROOT)}")
    print("REGION / CRATER_ORIGIN / FOCUS in the bench still point at the old "
          "ground -- move them too, or the window will sit outside the block.")


if __name__ == "__main__":
    main()
