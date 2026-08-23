#!/usr/bin/env python3
"""Fetch SLDEM2015 high-resolution lunar DEM crops (GitHub Actions).

Runs on an Actions runner (unrestricted internet) because the dev
containers cannot reach the NASA PDS hosts. Reads REQUEST lines of the
form

    name  latDeg  lonDeg  spanDeg

from prototypes/planet_visuals/data/lola/REQUEST, downloads the
SLDEM2015 512 ppd JP2 tile(s) (LOLA + Kaguya TC merged DEM, ~59 m/px,
+-60 deg latitude) covering each site, crops the requested window
(clipped to the tile), and writes

    sldem_<name>_512.tif    uint16, half-metre units, 10 km offset —
                            the exact encoding LolaDem already decodes
                            (metres = raw * 0.5 - 10000); minimal
                            little-endian strip TIFF, no compression
    sldem_<name>_512.json   bounds + provenance sidecar

into the same directory. A sanity check compares the crop against the
repo's global LDEM_16 before anything is written.

SLDEM2015 tiles: 30 deg lat x 45 deg lon, DN = signed 16-bit,
elevation metres = DN * SCALING_FACTOR (0.5 per the PDS labels), so
the uint16 re-encode is simply DN + 20000.
"""

import json
import math
import os
import re
import struct
import subprocess
import sys
import urllib.request

LOLA_DIR = "prototypes/planet_visuals/data/lola"
REQUEST_PATH = os.path.join(LOLA_DIR, "REQUEST")
GLOBAL_DEM = os.path.join(LOLA_DIR, "ldem_16_uint.tif")
BASE_URLS = [
    "https://imbrium.mit.edu/DATA/SLDEM2015/TILES/JP2",
    "https://pds-geosciences.wustl.edu/lro/lro-l-lola-3-rdr-v1/"
    "lrolol_1xxx/data/sldem2015/tiles/jp2",
]
PPD = 512
MAX_PX = 5120          # cap one crop at 10 deg (52 MB) — GitHub limit is 100


def TileName(lat, lon_e):
    """Tile containing (lat, lonE): 30 x 45 deg, e.g. 512_30S_00S_315_360."""
    lat0 = math.floor(lat / 30.0) * 30            # south edge
    lon0 = math.floor(lon_e / 45.0) * 45
    def LatTag(v):
        return f"{abs(v):02d}{'N' if v >= 0 else 'S'}"
    return (f"SLDEM2015_512_{LatTag(lat0)}_{LatTag(lat0 + 30)}"
            f"_{lon0:03d}_{lon0 + 45:03d}"), lat0, lon0


def Download(url, dest):
    print(f"  fetching {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "fetch-sldem"})
    with urllib.request.urlopen(req, timeout=600) as r, open(dest, "wb") as f:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
    print(f"  {dest}: {os.path.getsize(dest) / 1e6:.1f} MB")


def ReadScalingFactor(lbl_path):
    try:
        text = open(lbl_path, "r", errors="ignore").read()
        m = re.search(r"SCALING_FACTOR\s*=\s*([0-9.eE+-]+)", text)
        if m:
            return float(m.group(1))
    except OSError:
        pass
    return 0.5


def WriteMinimalTiff(path, arr):
    """Little-endian classic TIFF, one strip, uint16 — mirrors the
    subset LolaDem's built-in reader accepts."""
    import numpy as np
    h, w = arr.shape
    data = arr.astype("<u2").tobytes()
    entries = [
        (256, 3, 1, w), (257, 3, 1, h), (258, 3, 1, 16), (259, 3, 1, 1),
        (262, 3, 1, 1), (273, 4, 1, 8), (277, 3, 1, 1), (278, 3, 1, h),
        (279, 4, 1, len(data)),
    ]
    with open(path, "wb") as f:
        f.write(struct.pack("<2sHI", b"II", 42, 8 + len(data)))
        f.write(data)
        f.write(struct.pack("<H", len(entries)))
        for tag, typ, cnt, val in entries:
            f.write(struct.pack("<HHII", tag, typ, cnt, val))
        f.write(struct.pack("<I", 0))


def GlobalElevation(dem, lat, lon):
    """Bilinear read of the repo's LDEM_16 (PIL-free, numpy array)."""
    h, w = dem.shape
    x = (lon + 180.0) / 360.0 * w
    y = (90.0 - lat) / 180.0 * h
    x0, y0 = int(x) % w, min(int(y), h - 2)
    fx, fy = x - int(x), y - y0
    q = dem[[y0, y0, y0 + 1, y0 + 1], [x0, (x0 + 1) % w] * 2].astype(float)
    top = q[0] * (1 - fx) + q[1] * fx
    bot = q[2] * (1 - fx) + q[3] * fx
    return (top * (1 - fy) + bot * fy) * 0.5 - 10000.0


def LoadGlobalDem():
    import numpy as np
    # The global DEM is itself a minimal-style strip TIFF; parse enough
    # of it to get the raster (it may have per-row strips).
    raw = open(GLOBAL_DEM, "rb").read()
    assert raw[:2] == b"II"
    ifd = struct.unpack("<I", raw[4:8])[0]
    n = struct.unpack("<H", raw[ifd:ifd + 2])[0]
    tags = {}
    for i in range(n):
        t, typ, cnt, val = struct.unpack("<HHII",
                                         raw[ifd + 2 + 12 * i:
                                             ifd + 14 + 12 * i])
        tags[t] = (typ, cnt, val)
    w, h = tags[256][2], tags[257][2]
    def ArrayVals(t):
        typ, cnt, val = tags[t]
        size = 2 if typ == 3 else 4
        if size * cnt <= 4:
            return [val]
        fmt = "<" + ("H" if typ == 3 else "I") * cnt
        return list(struct.unpack(fmt, raw[val:val + size * cnt]))
    offs = ArrayVals(273)
    rps = tags.get(278, (0, 0, h))[2]
    out = np.zeros((h, w), dtype=np.uint16)
    row = 0
    for off in offs:
        rows = min(rps, h - row)
        out[row:row + rows] = np.frombuffer(
            raw, dtype="<u2", count=rows * w, offset=off).reshape(rows, w)
        row += rows
    return out


def main():
    import numpy as np
    from osgeo import gdal
    gdal.UseExceptions()

    sites = []
    for line in open(REQUEST_PATH):
        line = line.split("#")[0].strip()
        if not line:
            continue
        name, lat, lon, span = line.split()
        sites.append((name, float(lat), float(lon), float(span)))
    if not sites:
        print("REQUEST is empty — nothing to fetch")
        return

    global_dem = LoadGlobalDem()
    for name, lat, lon, span in sites:
        out_tif = os.path.join(LOLA_DIR, f"sldem_{name}_512.tif")
        out_json = os.path.join(LOLA_DIR, f"sldem_{name}_512.json")
        if os.path.exists(out_tif):
            print(f"{name}: {out_tif} already exists, skipping")
            continue
        print(f"=== {name}: {lat:+.2f} {lon:+.2f} span {span} deg")
        lon_e = lon % 360.0
        tile, tlat0, tlon0 = TileName(lat, lon_e)
        jp2 = f"/tmp/{tile}.JP2"
        if not os.path.exists(jp2):
            last_err = None
            for base in BASE_URLS:
                try:
                    Download(f"{base}/{tile}.JP2", jp2)
                    break
                except Exception as e:      # noqa: BLE001 — try next mirror
                    last_err = e
                    print(f"  {base} failed: {e}")
            else:
                raise SystemExit(f"could not fetch {tile}: {last_err}")
        lbl = f"/tmp/{tile}.LBL"
        if not os.path.exists(lbl):
            for base in BASE_URLS:
                try:
                    Download(f"{base}/{tile}.LBL", lbl)
                    break
                except Exception:           # noqa: BLE001
                    pass
        scale = ReadScalingFactor(lbl)
        print(f"  scaling factor {scale}")

        # Requested window, clipped to the tile.
        lat0 = max(tlat0, lat - span / 2)
        lat1 = min(tlat0 + 30, lat + span / 2)
        lon0 = max(tlon0, lon_e - span / 2)
        lon1 = min(tlon0 + 45, lon_e + span / 2)
        x0 = int((lon0 - tlon0) * PPD)
        x1 = int((lon1 - tlon0) * PPD)
        y0 = int((tlat0 + 30 - lat1) * PPD)
        y1 = int((tlat0 + 30 - lat0) * PPD)
        if x1 - x0 > MAX_PX or y1 - y0 > MAX_PX:
            raise SystemExit(f"{name}: crop {x1-x0}x{y1-y0} exceeds "
                             f"{MAX_PX} px cap")

        ds = gdal.Open(jp2)
        dn = ds.GetRasterBand(1).ReadAsArray(x0, y0, x1 - x0, y1 - y0)
        dn = dn.astype(np.int32)
        metres = dn * scale

        # Sanity: median |crop - LDEM_16| over sample points must be
        # small (the 16 ppd global is heavily smoothed; 300 m covers
        # legitimate differences without letting a mis-crop through).
        rng = np.random.default_rng(7)
        diffs = []
        for _ in range(64):
            sy = rng.integers(0, metres.shape[0])
            sx = rng.integers(0, metres.shape[1])
            slat = lat1 - (sy + 0.5) / PPD
            slon = lon0 + (sx + 0.5) / PPD
            slon = ((slon + 180) % 360) - 180
            diffs.append(abs(metres[sy, sx] -
                             GlobalElevation(global_dem, slat, slon)))
        med = float(np.median(diffs))
        print(f"  elev range {metres.min():.0f}..{metres.max():.0f} m, "
              f"median |diff vs LDEM_16| {med:.0f} m")
        if med > 300.0:
            raise SystemExit(f"{name}: crop disagrees with the global DEM "
                             f"(median {med:.0f} m) — refusing to commit")

        enc = np.clip((metres + 10000.0) * 2.0 + 0.5, 0, 65535)
        WriteMinimalTiff(out_tif, enc.astype(np.uint16))
        json.dump({
            "product": "SLDEM2015 512 ppd (LOLA + Kaguya TC merged DEM)",
            "tile": tile,
            "lat0": lat0, "lat1": lat1,
            "lon0": ((lon0 + 180) % 360) - 180,
            "lon1": ((lon1 + 180) % 360) - 180,
            "ppd": PPD,
            "encoding": "uint16 half-metre: metres = raw * 0.5 - 10000",
            "median_diff_vs_ldem16_m": round(med, 1),
        }, open(out_json, "w"), indent=2)
        print(f"  wrote {out_tif} "
              f"({os.path.getsize(out_tif) / 1e6:.1f} MB) + sidecar")


if __name__ == "__main__":
    sys.exit(main())
