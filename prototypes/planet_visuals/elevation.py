"""Real lunar elevation queries from the LOLA LDEM_16 model.

Data: NASA CGI Moon Kit `ldem_16_uint.tif` (data/lola/), derived from
the Lunar Orbiter Laser Altimeter (LRO). 5760x2880 equirectangular,
16 pixels per degree (~1.895 km/px at the equator), unsigned 16-bit
values in HALF-METRE units: metres = raw * 0.5 - 10000, relative to
the 1737.4 km reference radius. The 10 km offset was calibrated
empirically against LOLA elevations of Apollo 11 (-1930 m -> implied
offset 10015) and Chang'e 4 (-5935 m -> 10004); decoded global range
-8982..+10686 m matches the real lunar range at 16 ppd smoothing.

This is what binds the zoom-anywhere view to physical ground truth:
any (lat, lon) the player zooms to can be asked for real elevation,
relief and slope.

API:
    dem = LolaDem()                      # loads data/lola/ldem_16_uint.tif
    dem.elevation_m(lat, lon)            # bilinear, metres vs reference
    dem.window_stats(lat, lon, span_deg) # dict of min/max/mean/relief/slopes
    dem.window_arrays(lat, lon, span_deg, out_px)  # (elev_m, slope_deg)
"""

from __future__ import annotations

import math
import os

import numpy as np
from PIL import Image

PROTOTYPE_DIR = os.path.dirname(os.path.abspath(__file__))
DEM_PATH = os.path.join(PROTOTYPE_DIR, "data", "lola", "ldem_16_uint.tif")

MOON_RADIUS_M = 1737.4e3
M_PER_DEG = math.pi * MOON_RADIUS_M / 180.0     # 30,323 m per degree


class LolaDem:
    def __init__(self, path: str = DEM_PATH):
        if not os.path.exists(path):
            raise FileNotFoundError(
                f"{path} missing — run the fetch-dem workflow "
                "(push a change to data/lola/REQUEST) and pull.")
        Image.MAX_IMAGE_PIXELS = None
        img = Image.open(path)
        self.raw = np.asarray(img)
        if self.raw.ndim != 2:
            raise ValueError(f"Expected single-band DEM, got {self.raw.shape}")
        self.h, self.w = self.raw.shape
        self.px_per_deg = self.w / 360.0

    def decode_elevation(self, raw: np.ndarray) -> np.ndarray:
        """CGI Moon Kit uint16 DEMs: half-metre units, 10 km offset
        (empirically calibrated — see module docstring).
        Float variants store kilometres directly — handled too."""
        if self.raw.dtype == np.uint16:
            return raw.astype(np.float32) * 0.5 - 10000.0
        # float tif: kilometres relative to reference radius
        return raw.astype(np.float32) * 1000.0

    def _pixel_coords(self, lat: np.ndarray, lon: np.ndarray):
        x = (np.asarray(lon, dtype=np.float64) + 180.0) / 360.0 * self.w
        y = (90.0 - np.asarray(lat, dtype=np.float64)) / 180.0 * self.h
        return x % self.w, np.clip(y, 0, self.h - 1)

    def elevation_m(self, lat: float, lon: float) -> float:
        """Bilinear-interpolated elevation in metres."""
        x, y = self._pixel_coords(lat, lon)
        x0 = int(np.floor(x)) % self.w
        y0 = min(int(np.floor(y)), self.h - 2)
        x1 = (x0 + 1) % self.w
        fx = float(x - np.floor(x))
        fy = float(y - y0)
        q = self.decode_elevation(
            self.raw[[y0, y0, y0 + 1, y0 + 1], [x0, x1, x0, x1]])
        top = q[0] * (1 - fx) + q[1] * fx
        bot = q[2] * (1 - fx) + q[3] * fx
        return float(top * (1 - fy) + bot * fy)

    def window_arrays(self, lat: float, lon: float, span_deg: float,
                      out_px: int = 300):
        """(elevation_m, slope_deg) arrays for a window square in km
        (lon span widened by 1/cos(lat)), bilinearly resampled to
        out_px. Slope is computed at the DEM's native resolution, then
        resampled — resampling first would flatten it."""
        c = max(0.2, math.cos(math.radians(lat)))
        lon_span = span_deg / c
        x0f = (lon - lon_span / 2 + 180.0) / 360.0 * self.w
        x1f = (lon + lon_span / 2 + 180.0) / 360.0 * self.w
        y0f = (90.0 - (lat + span_deg / 2)) / 180.0 * self.h
        y1f = (90.0 - (lat - span_deg / 2)) / 180.0 * self.h
        y0 = max(0, int(np.floor(y0f)))
        y1 = min(self.h, int(np.ceil(y1f)) + 1)
        x0 = int(np.floor(x0f))
        x1 = int(np.ceil(x1f)) + 1
        xs = np.arange(x0, x1) % self.w
        elev = self.decode_elevation(self.raw[y0:y1][:, xs])

        # Physical pixel sizes at this latitude
        dy_m = M_PER_DEG / self.px_per_deg
        dx_m = dy_m * c
        gy, gx = np.gradient(elev.astype(np.float64), dy_m, dx_m)
        slope = np.degrees(np.arctan(np.hypot(gx, gy)))

        def resample(a):
            im = Image.fromarray(np.ascontiguousarray(a, dtype=np.float32),
                                 mode="F")
            return np.asarray(im.resize((out_px, out_px), Image.BILINEAR))

        return resample(elev), resample(slope)

    def window_stats(self, lat: float, lon: float, span_deg: float) -> dict:
        elev, slope = self.window_arrays(lat, lon, span_deg)
        return {
            "lat": lat, "lon": lon, "span_deg": span_deg,
            "span_km": span_deg * M_PER_DEG / 1000.0,
            "centre_elev_m": self.elevation_m(lat, lon),
            "min_elev_m": float(elev.min()),
            "max_elev_m": float(elev.max()),
            "mean_elev_m": float(elev.mean()),
            "relief_m": float(elev.max() - elev.min()),
            "mean_slope_deg": float(slope.mean()),
            "max_slope_deg": float(slope.max()),
            "dem_px_across": span_deg * 16.0,
        }
