# Global real relief at 128 px/deg from the USGS Kaguya TC stereo DTMs.
#
#   build_relief.py [BAND_TOP_LAT ...]      default: every band, tops 86N..82S (so 86N..86S)
#
# Run in tools/relief with the four catalogue .parquet files beside it
# (README.md says where from).
#
# Output: tiles/r128_<row>_<col>.jpg, 512x512, 4 x 4 degrees, row 0 at 90N,
# col 0 at 180W. Each pixel is the height above the shipped LOLA model, in
# 7 m steps about 128: detail = round((z - base) / 7) + 128, where base is
# the BILINEAR sample of src/assets/planet/lola/ldem_16_uint.tif (metres =
# 0.5 v - 10000) at the pixel centre -- the game rebuilds the same base.
import os, sys, math, time, numpy as np, pandas as pd, rasterio
from rasterio.vrt import WarpedVRT
from rasterio.enums import Resampling
from rasterio.transform import from_origin
from rasterio.windows import Window
from rasterio.warp import transform_bounds
from concurrent.futures import ThreadPoolExecutor
from PIL import Image
from scipy import ndimage
from scipy.interpolate import RectBivariateSpline
PPD, TILE, STEP = 128, 512, 7.0
FEATHER = 8.0          # px: a DTM's weight ramps up over its outer 2 km
EDGE_TRIM = 3          # px trimmed off every DTM's edge (see the alignment loop)
MARGIN = 64            # px (0.5 deg) worked beyond the band's top and bottom, then cut off
AGREE_M = 25.0         # m: a DTM further than this from the median is outvoted
LOLA_AGREE_M = 80.0    # m rms: a DTM smoothed to LOLA's scale must agree with LOLA this well
TDEG = TILE / PPD                                  # 4 degrees
W = 360 * PPD
ROWS = TILE + 2 * MARGIN                           # rows a band is worked on
os.makedirs('tiles', exist_ok=True); os.makedirs('done', exist_ok=True)
LOLA_TIF = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../src/assets/planet/lola/ldem_16_uint.tif')
lola = np.array(Image.open(LOLA_TIF)).astype(np.float64) * 0.5 - 10000.0
LH, LW = lola.shape
def base_rows(top):
    # Bilinear LOLA at the pixel centres of ROWS rows from `top`, lon
    # wrapping, lat clamped.
    lat = top - (np.arange(ROWS) + 0.5) / PPD
    lon = -180.0 + (np.arange(W) + 0.5) / PPD
    y = (90.0 - lat) / 180.0 * LH - 0.5; x = (lon + 180.0) / 360.0 * LW - 0.5
    y0 = np.floor(y).astype(int); fy = (y - y0)[:, None]
    x0 = np.floor(x).astype(int); fx = (x - x0)[None, :]
    yA, yB = np.clip(y0, 0, LH - 1), np.clip(y0 + 1, 0, LH - 1)
    xA, xB = x0 % LW, (x0 + 1) % LW
    r0 = lola[yA][:, xA] * (1 - fx) + lola[yA][:, xB] * fx
    r1 = lola[yB][:, xA] * (1 - fx) + lola[yB][:, xB] * fx
    return r0 * (1 - fy) + r1 * fy
def smooth_rows(top):
    # LOLA through a cubic spline instead: the ground where no DTM has
    # measured it. Bilinear is what the tiles are stored above, but drawn
    # on its own it is facets -- a 16 km crater (Birt) was a checkerboard
    # of 1.9 km squares wherever its shadowed floor had no stereo.
    lat = top - (np.arange(ROWS) + 0.5) / PPD
    lon = -180.0 + (np.arange(W) + 0.5) / PPD
    y = (90.0 - lat) / 180.0 * LH - 0.5; x = (lon + 180.0) / 360.0 * LW - 0.5
    pad = 12
    ya, yb = max(0, int(np.floor(y.min())) - pad), min(LH, int(np.ceil(y.max())) + pad + 1)
    sub = np.concatenate([lola[ya:yb, -pad:], lola[ya:yb], lola[ya:yb, :pad]], axis=1)
    spl = RectBivariateSpline(np.arange(ya, yb), np.arange(-pad, LW + pad), sub, kx=3, ky=3, s=0)
    return spl(np.clip(y, ya, yb - 1), x)
cats = ['kaguya_terrain_camera_usgs_dtms_v2_equi', 'kaguya_terrain_camera_usgs_dtms',
        'kaguya_terrain_camera_usgs_dtms_v2_northpolar', 'kaguya_terrain_camera_usgs_dtms_v2_southpolar']
frames = []
for pri, f in enumerate(cats):
    d = pd.read_parquet(f + '.parquet', columns=['id', 'bbox', 'gsd', 'assets'])
    bb = pd.DataFrame(d['bbox'].tolist()); d = pd.concat([d.drop(columns=['bbox']), bb], axis=1)
    d['pri'] = 0 if 'v2' in f else 1
    d['href'] = [a['dtm']['href'] for a in d['assets']]
    frames.append(d.drop(columns=['assets']))
cat = pd.concat(frames, ignore_index=True)
cat = cat[(cat.gsd < 100) & ((cat.xmax - cat.xmin) < 30)]
# A catalogue footprint that stops at exactly 0 E belongs to a DTM that
# straddles the prime meridian: the catalogue kept only one half. read()
# takes the extent from the file itself, so the other half is not lost.
geo = rasterio.crs.CRS.from_proj4('+proj=longlat +a=1737400 +b=1737400 +no_defs')
def pick(band):
    # Enough DTMs that every 0.25 deg cell is covered twice, preferring v2.
    b = band.sort_values(['pri', 'gsd'])
    cell = 0.25; cover = {}
    keep = []
    for i, r in b.iterrows():
        xs = np.arange(math.floor(r.xmin / cell), math.ceil(r.xmax / cell))
        ys = np.arange(math.floor(r.ymin / cell), math.ceil(r.ymax / cell))
        cells = [(x, y) for x in xs for y in ys]
        if any(cover.get(c, 0) < 2 for c in cells):
            keep.append(i)
            for c in cells: cover[c] = cover.get(c, 0) + 1
    return band.loc[keep]
def run_band(lat_top):
    tag = f'done/{lat_top:+03d}'
    if os.path.exists(tag): return
    t0 = time.time()
    lat_bot = lat_top - TDEG
    # The band is worked MARGIN rows beyond its edges and cut back at the
    # end, so a DTM crossing into the next band is fitted to LOLA over
    # nearly the same ground in both, and the hole fill sees across: done
    # to the edge, each band fitted its own half and they met in a step.
    top = lat_top + MARGIN / PPD
    band = cat[(cat.ymax > lat_bot - MARGIN / PPD) & (cat.ymin < top)]
    print('band', lat_top, len(band), 'listed', flush=True)
    sel = pick(band)
    print('picked', len(sel), 'in %.0f s' % (time.time() - t0), flush=True)
    tf = from_origin(-180.0, top, 1.0 / PPD, 1.0 / PPD)
    def read(r):
        for attempt in range(4):
            try:
                with rasterio.open('/vsicurl/' + r.href) as s0:
                    ovs = s0.overviews(1)
                    xmin, ymin, xmax, ymax = transform_bounds(s0.crs, geo, *s0.bounds)
                if xmax - xmin > 30: xmin, xmax = r.xmin, r.xmax
                c0 = max(0, int((xmin + 180.0) * PPD) - 2); c1 = min(W, int((xmax + 180.0) * PPD) + 3)
                r0 = max(0, int((top - ymax) * PPD) - 2); r1 = min(ROWS, int((top - ymin) * PPD) + 3)
                if c1 <= c0 or r1 <= r0: return None
                lvl = max([i for i, f in enumerate(ovs) if f <= 8], default=None)
                kw = {} if lvl is None else {'overview_level': lvl}
                with rasterio.open('/vsicurl/' + r.href, **kw) as src:
                    nod = src.nodata if src.nodata is not None else -3.4e38
                    with WarpedVRT(src, crs=geo, transform=tf, width=W, height=ROWS, resampling=Resampling.average,
                                   src_nodata=nod, nodata=np.nan, dtype='float32') as vrt:
                        a = vrt.read(1, window=Window(c0, r0, c1 - c0, r1 - r0))
                a[~np.isfinite(a) | (np.abs(a) > 20000)] = np.nan
                return (r0, c0, a) if np.isfinite(a).any() else None
            except Exception:
                time.sleep(3 + 3 * attempt)
        return None
    # Every DTM onto LOLA before they are combined: its own offset and tilt
    # fitted against the base and taken off. LOLA is right at these scales
    # and a good DTM fits it with a near-zero plane; one that sat 100 m high
    # showed its frame as a rectangle wherever it had no neighbour to be
    # outvoted by. Done as each arrives, and only the difference from the
    # base kept, in half floats (0.5 m at a kilometre, against the tiles'
    # 7 m step): holding every DTM whole until all had arrived ran a
    # high-latitude band, where each spans degrees of longitude, out of
    # memory.
    base = base_rows(top)
    rms_all, drops = [], []
    def align(p):
        r0, c0, a = p
        b = base[r0:r0 + a.shape[0], c0:c0 + a.shape[1]]
        # The outer pixels are not ground: the DTM's overviews averaged its
        # edge with the empty frame around it, which left a ring of heights
        # tens of metres off -- drawn, the model's outline as a step.
        ok = ndimage.binary_erosion(np.isfinite(a), iterations=EDGE_TRIM)
        a = np.where(ok, a, np.nan)
        n = int(ok.sum())
        if n < 64: return None
        yy, xx = np.nonzero(ok)
        d = (a[ok] - b[ok]).astype(np.float64)
        if n >= 2000:
            ym, xm = yy.mean(), xx.mean()
            A = np.stack([np.ones(n), xx - xm, yy - ym], axis=1)
            p = np.linalg.lstsq(A, d, rcond=None)[0]
            Y, X = np.mgrid[0:a.shape[0], 0:a.shape[1]]
            plane = p[0] + p[1] * (X - xm) + p[2] * (Y - ym)
        else:
            plane = np.median(d)
        # And one more check it is the ground it claims to be: smoothed to
        # LOLA's own scale it must agree with LOLA. A misplaced or broken
        # model does not, and is dropped rather than outvoted.
        det = a - plane - b
        resid = np.where(ok, det, 0.0)
        wsum = ndimage.gaussian_filter(ok.astype(np.float64), 4)
        lo = ndimage.gaussian_filter(resid, 4) / np.maximum(wsum, 1e-6)
        core = ok & (wsum > 0.5)
        rms = float(np.sqrt(np.mean(lo[core] ** 2))) if core.any() else 0.0
        rms_all.append(rms)
        if rms > LOLA_AGREE_M:
            drops.append(1)
            return None
        # Its weight fades to nothing over its last FEATHER pixels: stereo
        # DTMs are least sure at their edges, and a hard edge is where the
        # set being combined changes -- a step of a few metres, which the
        # hillshade shows as the model's outline.
        wgt = np.clip(ndimage.distance_transform_edt(ok) / FEATHER, 0.0, 1.0)
        return (r0, c0, det.astype(np.float16), np.round(wgt * 255).astype(np.uint8))
    def load(r):
        p = read(r)
        return align(p) if p is not None else None
    with ThreadPoolExecutor(32) as ex:
        parts = [p for p in ex.map(load, [r for _, r in sel.iterrows()]) if p is not None]
    dropped = len(drops)
    z = np.full((ROWS, W), np.nan, np.float32)
    for col in range(W // TILE):
        x0, x1 = col * TILE, (col + 1) * TILE
        layers, weights = [], []
        for r0, c0, a, wg in parts:
            if c0 >= x1 or c0 + a.shape[1] <= x0: continue
            L = np.full((ROWS, TILE), np.nan, np.float32)
            Wt = np.zeros((ROWS, TILE), np.float32)
            lo, hi = max(x0, c0), min(x1, c0 + a.shape[1])
            L[r0:r0 + a.shape[0], lo - x0:hi - x0] = a[:, lo - c0:hi - c0]
            Wt[r0:r0 + a.shape[0], lo - x0:hi - x0] = wg[:, lo - c0:hi - c0] / 255.0
            layers.append(L); weights.append(Wt)
        if not layers: continue
        Ls = np.stack(layers); Ws = np.stack(weights)
        med = np.nanmedian(Ls, axis=0)
        # Those that agree with the median, feathered; the median where
        # nothing agrees with any weight.
        agree = np.isfinite(Ls) & (np.abs(Ls - med[None]) < AGREE_M)
        w = np.where(agree, Ws, 0.0)
        sw = w.sum(axis=0)
        mean = np.where(agree, Ls, 0.0).__mul__(w).sum(axis=0) / np.maximum(sw, 1e-6)
        z[:, x0:x1] = np.where(sw > 1e-3, mean, med)
    ok = np.isfinite(z)                   # z is already the detail above the base
    det = np.where(ok, z, 0.0)
    # Holes: the neighbours' detail fades out into LOLA's smooth spline --
    # never into the bilinear the detail is measured from, which is facets.
    wgt = ndimage.gaussian_filter(ok.astype(np.float64), 6, mode=('nearest', 'wrap'))
    fill = ndimage.gaussian_filter(det, 6, mode=('nearest', 'wrap')) / np.maximum(wgt, 1e-6)
    a = np.clip(wgt * 3.0, 0, 1)
    det = np.where(ok, det, fill * a + (smooth_rows(top) - base) * (1.0 - a))
    det, ok = det[MARGIN:MARGIN + TILE], ok[MARGIN:MARGIN + TILE]
    code = np.clip(np.round(det / STEP) + 128, 0, 255).astype(np.uint8)
    row = int(round((90.0 - lat_top) / TDEG))
    n = 0
    for col in range(W // TILE):
        tok = ok[:, col * TILE:(col + 1) * TILE]
        if tok.mean() < 0.02: continue
        Image.fromarray(code[:, col * TILE:(col + 1) * TILE]).save(f'tiles/r128_{row:02d}_{col:02d}.jpg', quality=85, optimize=True)
        n += 1
    open(tag, 'w').write('%d parts, %d tiles, coverage %.1f%%\n' % (len(parts), n, 100 * ok.mean()))
    print('band %+d: %d of %d DTMs used (%d dropped, LOLA rms p50 %.0f p99 %.0f m), %d tiles, coverage %.1f%%, %.0f s'
          % (lat_top, len(parts), len(sel), dropped, np.percentile(rms_all, 50), np.percentile(rms_all, 99), n, 100 * ok.mean(), time.time() - t0), flush=True)
bands = [int(a) for a in sys.argv[1:]] or [90 - 4 * k for k in range(1, 44)]   # 86N..82S: the claimable moon and a margin
for b in bands: run_band(b)
