#include "terrain_synthesis.h"
#include "detail_noise.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// Global switch for the site disturbance (playtest comparisons).
static bool g_siteDisturbEnabled = true;
void SetSiteDisturbanceEnabled(bool e) { g_siteDisturbEnabled = e; }
bool IsSiteDisturbanceEnabled() { return g_siteDisturbEnabled; }

// Global switch for the world-anchored sub-floor, same shape and for the
// same reason: the game's callers do not pass a tuning, so this is how an
// instrument renders the whole game both ways without the default moving.
//
// OFF while only the CPU chain can draw it. Turning it on now would make
// the ground depend on which path GetTerrainPath() picked -- the new look
// on a software rasteriser, the old one on a real GPU -- and a look that
// changes with the hardware is worse than a look that has not landed yet.
static bool g_subFloorEnabled = false;
void SetSubFloorEnabled(bool e) { g_subFloorEnabled = e; }
bool IsSubFloorEnabled() { return g_subFloorEnabled; }

// ---------------------------------------------------------------------------
// Deterministic RNG (xorshift128) — the seed is the location, so the
// same spot always regenerates the same ground.
// ---------------------------------------------------------------------------

struct TerrainRng
{
    uint32_t s[4];

    explicit TerrainRng(uint32_t seed)
    {
        // SplitMix32 expansion of the seed into state
        uint32_t x = seed;
        for (int i = 0; i < 4; i++)
        {
            x += 0x9E3779B9u;
            uint32_t z = x;
            z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
            z = (z ^ (z >> 13)) * 0xC2B2AE35u;
            s[i] = z ^ (z >> 16);
        }
    }

    uint32_t Next()
    {
        uint32_t t = s[3];
        uint32_t v = s[0];
        s[3] = s[2];
        s[2] = s[1];
        s[1] = v;
        t ^= t << 11;
        t ^= t >> 8;
        s[0] = t ^ v ^ (v >> 19);
        return s[0];
    }

    float Uniform() { return (Next() >> 8) * (1.0f / 16777216.0f); }
};

// Where a level's pixels sit on the moon, so invented detail can be a
// function of GROUND rather than of the window that happens to frame it.
//
// Two windows over the same site -- a pick a kilometre off, the game's
// cell grid against the instrument's ladder -- have to invent the same
// detail there, or the ground a player judged changes when they come
// back to it. That only holds if the noise lattice is pinned to the
// moon: every lattice value is a hash of its own world cell, not the
// next number out of a per-window stream.
//
// The frame is a local tangent plane in km, x east and y south, with
// longitude scaled by cos(lat) at the window centre -- the same
// approximation the game's own cell grid makes (TerrainGridCellToLatLon).
// Two windows near each other differ only to second order in it.
struct NoiseFrame
{
    // The window's geographic extent, so any pixel can be turned into
    // the ground it covers. Latitude decreases with y: north is up.
    double lat0Deg = 0.0;      // at y = 0
    double dLatPerPx = 0.0;    // negative
    double lon0Deg = 0.0;      // at x = 0
    double dLonPerPx = 0.0;
    double kmPerPx = 1.0;      // this level's scale, for cell sizes
    uint32_t salt = 0;         // which layer this is
};

// lola_dem's frame, exactly (see SynthesizeDetail): u is the east-west
// arc from the prime meridian AT THIS PIXEL'S OWN LATITUDE, v the
// north-south arc from the equator. Sharing it means the two
// synthesizers quantise the same ground into the same cells.
//
// The cos belongs to the pixel and not to the window. Taking it once at
// the window centre looks equivalent and is not: it makes the east-west
// scale a function of where the window sits, so a window moved in
// latitude slides the whole lattice sideways. Measured before this was
// fixed, a 0.064 degree shift at longitude 5 moved it 72 m -- a third of
// a pixel at the 100 km level, and 7.4 pixels at 5 km, where the sect
// level stopped anchoring at all.
static void FrameWorldKm(const NoiseFrame& f, double x, double y,
                         double* u, double* v)
{
    double lat = f.lat0Deg + y * f.dLatPerPx;
    double lon = f.lon0Deg + x * f.dLonPerPx;
    *v = lat * MOON_KM_PER_DEG;
    *u = lon * MOON_KM_PER_DEG * std::cos(lat * DEG2RAD);
}

// Layers used to be decorrelated by the order they drew from one stream.
// A hash has to be told instead.
enum : uint32_t
{
    NOISE_GRAIN      = 0x9E3779B9u,
    NOISE_GRAIN_FINE = 0x85EBCA6Bu,
    NOISE_UNDULATION = 0xC2B2AE35u,
    NOISE_SPECKLE    = 0x27D4EB2Fu,
    NOISE_LUMPS      = 0x165667B1u,
    NOISE_BOULDER    = 0xD3A2646Cu,
    NOISE_SITE       = 0xFD7046C5u,
};

static NoiseFrame MakeNoiseFrame(double latDeg, double lonDeg,
                                 double spanKm, int res, uint32_t salt)
{
    // The same extent CropMacro cuts: spanKm north-south, widened by
    // 1/cos so the window is square on the ground.
    NoiseFrame f;
    double latSpanDeg = spanKm / MOON_KM_PER_DEG;
    double c = std::max(0.2, std::cos(latDeg * DEG2RAD));
    double lonSpanDeg = latSpanDeg / c;
    f.lat0Deg = latDeg + latSpanDeg * 0.5;
    f.dLatPerPx = -latSpanDeg / (double)res;
    f.lon0Deg = lonDeg - lonSpanDeg * 0.5;
    f.dLonPerPx = lonSpanDeg / (double)res;
    f.kmPerPx = spanKm / (double)res;
    f.salt = salt;
    return f;
}

// One value per world lattice cell, in 0..1. The same cell hashes the
// same however it is reached, which is the whole point.
static float HashCell(int64_t cx, int64_t cy, uint32_t salt)
{
    uint32_t h = (uint32_t)((uint64_t)cx * 73856093u)
               ^ (uint32_t)((uint64_t)cy * 19349663u)
               ^ (salt * 83492791u);
    h ^= h >> 16; h *= 0x45D9F3Bu;
    h ^= h >> 16; h *= 0x45D9F3Bu;
    h ^= h >> 16;
    return (float)(h >> 8) * (1.0f / 16777216.0f);
}

// ---------------------------------------------------------------------------
// Float-field helpers. All fields are res*res, row-major.
// ---------------------------------------------------------------------------

typedef std::vector<float> Field;

// The exact separable gaussian. Cost is O(w * h * sigma), because the
// kernel radius follows sigma — which is why it is not called directly
// for the wide blurs; see GaussianBlur below.
static void GaussianBlurExact(Field& a, int w, int h, float sigma)
{
    if (sigma <= 0.05f) return;
    int radius = (int)std::ceil(sigma * 3.0f);
    std::vector<float> kernel(2 * radius + 1);
    float norm = 0.0f;
    for (int i = -radius; i <= radius; i++)
    {
        float v = std::exp(-0.5f * (i * i) / (sigma * sigma));
        kernel[i + radius] = v;
        norm += v;
    }
    for (float& k : kernel) k /= norm;

    Field tmp(a.size());
    // Horizontal pass
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            float acc = 0.0f;
            for (int i = -radius; i <= radius; i++)
            {
                int xi = std::clamp(x + i, 0, w - 1);
                acc += a[y * w + xi] * kernel[i + radius];
            }
            tmp[y * w + x] = acc;
        }
    }
    // Vertical pass
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            float acc = 0.0f;
            for (int i = -radius; i <= radius; i++)
            {
                int yi = std::clamp(y + i, 0, h - 1);
                acc += tmp[yi * w + x] * kernel[i + radius];
            }
            a[y * w + x] = acc;
        }
    }
}

// Gaussian blur that does not get more expensive as the field grows.
//
// Feature sizes are held constant across resolutions by scaling every
// pixel radius with k = res / 300, so the wide blurs run at sigma 8.5
// at res 512 and 17 at res 1024 — a 105-tap kernel. Since the exact
// filter costs O(w * h * sigma), that made terrain synthesis cubic in
// resolution rather than quadratic.
//
// A wide blur is a low-pass filter by definition: nothing finer than
// sigma survives it. So decimate first, run the REAL kernel on the
// small field at sigma/f, then bilinear-upsample. Work falls by about
// f^3 and the result is indistinguishable — measured against the exact
// filter over a full 512x512 render, no pixel differs by more than
// 3/255. Narrow blurs keep the exact path, where it is already cheap.
static void GaussianBlur(Field& a, int w, int h, float sigma)
{
    if (sigma <= 0.05f) return;
    if (sigma < 3.0f)
    {
        GaussianBlurExact(a, w, h, sigma);
        return;
    }

    // Keep at least 32 samples per axis, so the decimated field still
    // resolves the shape the blur is meant to preserve.
    int f = (int)(sigma / 2.0f);
    while (f > 1 && (w / f < 32 || h / f < 32)) f--;
    if (f <= 1)
    {
        GaussianBlurExact(a, w, h, sigma);
        return;
    }

    const int dw = w / f;
    const int dh = h / f;
    Field small((size_t)dw * dh, 0.0f);

    // Box-average each f x f block down.
    for (int y = 0; y < dh; y++)
    {
        for (int x = 0; x < dw; x++)
        {
            float acc = 0.0f;
            int n = 0;
            for (int j = 0; j < f; j++)
            {
                int sy = y * f + j;
                if (sy >= h) break;
                for (int i = 0; i < f; i++)
                {
                    int sx = x * f + i;
                    if (sx >= w) break;
                    acc += a[(size_t)sy * w + sx];
                    n++;
                }
            }
            small[(size_t)y * dw + x] = (n > 0) ? acc / (float)n : 0.0f;
        }
    }

    GaussianBlurExact(small, dw, dh, sigma / (float)f);

    // Bilinear back up, sampling on pixel centres.
    for (int y = 0; y < h; y++)
    {
        float gy = ((y + 0.5f) / f) - 0.5f;
        int y0 = (int)std::floor(gy);
        float ty = gy - y0;
        int y1 = std::clamp(y0 + 1, 0, dh - 1);
        y0 = std::clamp(y0, 0, dh - 1);
        for (int x = 0; x < w; x++)
        {
            float gx = ((x + 0.5f) / f) - 0.5f;
            int x0 = (int)std::floor(gx);
            float tx = gx - x0;
            int x1 = std::clamp(x0 + 1, 0, dw - 1);
            x0 = std::clamp(x0, 0, dw - 1);
            float v00 = small[(size_t)y0 * dw + x0];
            float v10 = small[(size_t)y0 * dw + x1];
            float v01 = small[(size_t)y1 * dw + x0];
            float v11 = small[(size_t)y1 * dw + x1];
            a[(size_t)y * w + x] = (v00 * (1.0f - tx) + v10 * tx) * (1.0f - ty)
                                 + (v01 * (1.0f - tx) + v11 * tx) * ty;
        }
    }
}

static Field ResizeBilinear(const Field& src, int sw, int sh, int dw, int dh)
{
    Field dst((size_t)dw * dh);
    for (int y = 0; y < dh; y++)
    {
        float fy = (y + 0.5f) * sh / dh - 0.5f;
        int y0 = std::clamp((int)std::floor(fy), 0, sh - 1);
        int y1 = std::min(y0 + 1, sh - 1);
        float ty = fy - y0;
        for (int x = 0; x < dw; x++)
        {
            float fx = (x + 0.5f) * sw / dw - 0.5f;
            int x0 = std::clamp((int)std::floor(fx), 0, sw - 1);
            int x1 = std::min(x0 + 1, sw - 1);
            float tx = fx - x0;
            float top = src[y0 * sw + x0] * (1 - tx) + src[y0 * sw + x1] * tx;
            float bot = src[y1 * sw + x0] * (1 - tx) + src[y1 * sw + x1] * tx;
            dst[y * dw + x] = top * (1 - ty) + bot * ty;
        }
    }
    return dst;
}

// Single-octave smooth value noise: coarse random lattice, bilinear
// upsample, light blur — matches the prototype's value_noise closely
// enough for grain purposes.
// scale stays in PIXELS, so a feature keeps its physical size; what
// moved is the lattice it lands on. The window's corner falls somewhere
// inside a world cell, and sampling at that sub-cell phase is what pins
// the result to the ground instead of to the frame.
static Field ValueNoise(int res, int scale, const NoiseFrame& frame,
                        uint32_t octaveSalt)
{
    scale = std::max(2, scale);
    double cellKm = std::max(1e-12, scale * frame.kmPerPx);

    // The frame warps with latitude, so the lattice covering the window
    // is a bounding box rather than a corner plus a stride. Four corners
    // bound it; a cell of margin each way covers the interpolation.
    double uc[4], vc[4];
    FrameWorldKm(frame, 0.0,        0.0,        &uc[0], &vc[0]);
    FrameWorldKm(frame, (double)res, 0.0,       &uc[1], &vc[1]);
    FrameWorldKm(frame, 0.0,        (double)res, &uc[2], &vc[2]);
    FrameWorldKm(frame, (double)res, (double)res, &uc[3], &vc[3]);
    double uMin = uc[0], uMax = uc[0], vMin = vc[0], vMax = vc[0];
    for (int i = 1; i < 4; i++)
    {
        uMin = std::min(uMin, uc[i]); uMax = std::max(uMax, uc[i]);
        vMin = std::min(vMin, vc[i]); vMax = std::max(vMax, vc[i]);
    }
    int64_t i0 = (int64_t)std::floor(uMin / cellKm);
    int64_t j0 = (int64_t)std::floor(vMin / cellKm);
    int gw = (int)((int64_t)std::floor(uMax / cellKm) - i0) + 3;
    int gh = (int)((int64_t)std::floor(vMax / cellKm) - j0) + 3;
    gw = std::max(2, gw); gh = std::max(2, gh);

    Field grid((size_t)gh * gw);
    uint32_t salt = frame.salt ^ octaveSalt;
    for (int j = 0; j < gh; j++)
        for (int i = 0; i < gw; i++)
            grid[(size_t)j * gw + i] = HashCell(i0 + i, j0 + j, salt);

    // Per row: the latitude and its cosine. Per column: the longitude
    // already in km. u is then one multiply per pixel.
    std::vector<double> rowV(res), rowCos(res), colLonKm(res);
    for (int y = 0; y < res; y++)
    {
        double lat = frame.lat0Deg + (y + 0.5) * frame.dLatPerPx;
        rowV[y] = lat * MOON_KM_PER_DEG / cellKm - (double)j0;
        rowCos[y] = std::cos(lat * DEG2RAD);
    }
    for (int x = 0; x < res; x++)
        colLonKm[x] = (frame.lon0Deg + (x + 0.5) * frame.dLonPerPx)
                      * MOON_KM_PER_DEG / cellKm;

    Field out((size_t)res * res);
    for (int y = 0; y < res; y++)
    {
        double gy = rowV[y];
        int jy = std::clamp((int)std::floor(gy), 0, gh - 2);
        float ty = (float)(gy - jy);
        double cosLat = rowCos[y];
        for (int x = 0; x < res; x++)
        {
            double gx = colLonKm[x] * cosLat - (double)i0;
            int ix = std::clamp((int)std::floor(gx), 0, gw - 2);
            float tx = (float)(gx - ix);
            const float* row0 = &grid[(size_t)jy * gw + ix];
            const float* row1 = &grid[(size_t)(jy + 1) * gw + ix];
            float a = row0[0] + (row0[1] - row0[0]) * tx;
            float b = row1[0] + (row1[1] - row1[0]) * tx;
            out[(size_t)y * res + x] = a + (b - a) * ty;
        }
    }
    GaussianBlur(out, res, res, scale * 0.45f);
    return out;
}

static Field Fbm(int res, int octaves, int baseScale, float persistence,
                 const NoiseFrame& frame)
{
    Field out((size_t)res * res, 0.0f);
    float amp = 1.0f;
    float norm = 0.0f;
    int scale = baseScale;
    for (int o = 0; o < octaves; o++)
    {
        Field n = ValueNoise(res, scale, frame,
                             0x9E3779B9u * (uint32_t)(o + 1));
        for (size_t i = 0; i < out.size(); i++) out[i] += amp * n[i];
        norm += amp;
        amp *= persistence;
        scale = std::max(2, scale / 2);
    }
    for (float& v : out) v /= norm;
    return out;
}

static void NormalizeField(Field& g)
{
    double mean = 0.0;
    for (float v : g) mean += v;
    mean /= g.size();
    double var = 0.0;
    for (float v : g) var += (v - mean) * (v - mean);
    float stdev = (float)std::sqrt(var / g.size());
    if (stdev < 1e-6f) stdev = 1.0f;
    for (float& v : g) v = (float)((v - mean) / stdev);
}

// Zero-mean, unit-std noise standing in for the prototype's FFT pink
// noise (regolith grain). Plain FBM concentrates its variance in
// smooth low-frequency blobs; pink noise carries equal energy per
// octave down to the pixel, so mix in a fine per-pixel component —
// without it the ground renders flat (C++ std was 3x below Python's).
static Field GrainNoise(int res, const NoiseFrame& frame)
{
    NoiseFrame coarse = frame; coarse.salt ^= NOISE_GRAIN;
    Field g = Fbm(res, 5, 64, 0.8f, coarse);
    NormalizeField(g);
    // The finest layer is one value per pixel, so its lattice cell IS
    // the pixel: hash the world cell the pixel covers, not its index.
    Field fine((size_t)res * res);
    {
        double cellKm = std::max(1e-12, frame.kmPerPx);
        for (int y = 0; y < res; y++)
        {
            for (int x = 0; x < res; x++)
            {
                double u, v;
                FrameWorldKm(frame, x + 0.5, y + 0.5, &u, &v);
                fine[(size_t)y * res + x] =
                    HashCell((int64_t)std::floor(u / cellKm),
                             (int64_t)std::floor(v / cellKm),
                             frame.salt ^ NOISE_GRAIN_FINE) - 0.5f;
            }
        }
    }
    GaussianBlur(fine, res, res, 0.5f);
    NormalizeField(fine);
    for (size_t i = 0; i < g.size(); i++)
        g[i] = 0.55f * g[i] + 0.75f * fine[i];
    NormalizeField(g);
    return g;
}

// Lambertian hillshade. The sun defaults to azimuth 315 (NW), altitude
// 35 deg -- the light every constant in this file was chosen under.
static Field Hillshade(const Field& height, int res, float zFactor,
                       float smoothPx,
                       float sunAzDeg = 315.0f, float sunAltDeg = 35.0f)
{
    Field h = height;
    GaussianBlur(h, res, res, smoothPx);
    const float az = (float)((360.0 - sunAzDeg + 90.0) * DEG2RAD);
    const float alt = sunAltDeg * (float)DEG2RAD;
    Field out((size_t)res * res);
    for (int y = 0; y < res; y++)
    {
        int ym = std::max(0, y - 1), yp = std::min(res - 1, y + 1);
        for (int x = 0; x < res; x++)
        {
            int xm = std::max(0, x - 1), xp = std::min(res - 1, x + 1);
            // np.gradient convention: dy along axis 0, dx along axis 1
            float dy = (h[yp * res + x] - h[ym * res + x]) * zFactor
                       / (float)(yp - ym);
            float dx = (h[y * res + xp] - h[y * res + xm]) * zFactor
                       / (float)(xp - xm);
            float slope = std::atan(std::hypot(dx, dy));
            float aspect = std::atan2(dy, -dx);
            float v = std::cos(slope) * std::sin(alt)
                      + std::sin(slope) * std::cos(alt)
                        * std::cos(az - aspect);
            out[y * res + x] = std::clamp(v, 0.0f, 1.0f);
        }
    }
    return out;
}

// Horizon ray-march toward the sun: 1 = lit, 0 = blocked. Gives crater
// floors and slope bases their soft cast shadows.
static Field CastShadows(const Field& height, int res, float zFactor,
                         float maxDistPx, float stepPx,
                         float sunAzDeg = 315.0f, float sunAltDeg = 35.0f,
                         float blurPx = 0.8f)
{
    const float az = (float)((360.0 - sunAzDeg + 90.0) * DEG2RAD);
    float sx = std::cos(az);
    float syImage = -std::sin(az);
    float tanAlt = std::tan(sunAltDeg * (float)DEG2RAD);

    int nSteps = (int)(maxDistPx / stepPx);
    Field light((size_t)res * res);
    for (int y = 0; y < res; y++)
    {
        for (int x = 0; x < res; x++)
        {
            float hHere = height[y * res + x] * zFactor;
            float maxBlock = -1e9f;
            for (int s = 1; s <= nSteps; s++)
            {
                float dist = s * stepPx;
                int sxp = std::clamp((int)(x + sx * dist), 0, res - 1);
                int syp = std::clamp((int)(y + syImage * dist), 0, res - 1);
                float blockSlope =
                    (height[syp * res + sxp] * zFactor - hHere) / dist;
                if (blockSlope > maxBlock) maxBlock = blockSlope;
            }
            float band = tanAlt * 0.35f;
            float shadow = std::clamp((maxBlock - tanAlt) / band, 0.0f, 1.0f);
            light[y * res + x] = 1.0f - shadow;
        }
    }
    GaussianBlur(light, res, res, blurPx);
    for (float& v : light) v = std::clamp(v, 0.0f, 1.0f);
    return light;
}

// ---------------------------------------------------------------------------
// WAC source (grayscale float, loaded once)
// ---------------------------------------------------------------------------

// One byte per texel, not one float. The source is an 8-bit JPEG, so the
// float copy spent four bytes carrying three bits of rounding: 134 MB
// where 33.5 MB says the same thing. site-ground-texture.md 3.5 calls
// this the binding constraint on the web and the allocation most likely
// to fail on a phone. Readers scale by 1/255 as they sample.
static std::vector<unsigned char> g_wac;
static int g_wacW = 0;
static int g_wacH = 0;

static bool EnsureWacLoaded()
{
    if (g_wacW > 0) return true;
    Image img = LoadImage("src/assets/planet/wac_global.jpg");
    if (img.data == nullptr) return false;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    g_wacW = img.width;
    g_wacH = img.height;
    g_wac.resize((size_t)g_wacW * g_wacH);
    const unsigned char* px = (const unsigned char*)img.data;
    for (size_t i = 0; i < g_wac.size(); i++)
    {
        int r = px[i * 3 + 0], g = px[i * 3 + 1], b = px[i * 3 + 2];
        g_wac[i] = (unsigned char)((r + g + b + 1) / 3);
    }
    UnloadImage(img);
    TraceLog(LOG_INFO, "TERRAIN: WAC loaded %dx%d (%.1f MB)", g_wacW, g_wacH,
             g_wac.size() / (1024.0 * 1024.0));
    return true;
}

// Native-resolution crop of a window square in km (lon widened by
// 1/cos(lat)), denoised, then upsampled to res.
// Where a window's pixels land in a texel-aligned WAC block. One
// definition, used by the CPU resample and handed to the GPU, so the two
// cannot disagree about which ground a pixel shows.
static double TexelOriginX(double lon0, int blockX0)
{
    return (lon0 + 180.0) / 360.0 * g_wacW - 0.5 - blockX0;
}
static double TexelOriginY(double lat1, int blockY0)
{
    return (90.0 - lat1) / 180.0 * g_wacH - 0.5 - blockY0;
}
static double TexelStepX(double lonSpan, int res)
{
    return (lonSpan / res) / 360.0 * g_wacW;
}
static double TexelStepY(double spanDeg, int res)
{
    return (spanDeg / res) / 180.0 * g_wacH;
}

static Field CropMacro(double latDeg, double lonDeg, double spanDeg, int res)
{
    double c = std::max(0.2, std::cos(latDeg * DEG2RAD));
    double lonSpan = spanDeg / c;
    double lat0 = latDeg - spanDeg / 2.0, lat1 = latDeg + spanDeg / 2.0;
    double lon0 = lonDeg - lonSpan / 2.0, lon1 = lonDeg + lonSpan / 2.0;
    // Two texels of margin: the block is a buffer to sample from, not
    // the window itself, and bilinear needs a neighbour past each edge.
    const int MARGIN = 2;
    int y0 = (int)std::floor((90.0 - lat1) / 180.0 * g_wacH) - MARGIN;
    int y1 = (int)std::floor((90.0 - lat0) / 180.0 * g_wacH) + 1 + MARGIN;
    int x0 = (int)std::floor((lon0 + 180.0) / 360.0 * g_wacW) - MARGIN;
    int x1 = (int)std::floor((lon1 + 180.0) / 360.0 * g_wacW) + 1 + MARGIN;
    int cw = std::max(2, x1 - x0);
    int ch = std::max(2, y1 - y0);
    Field crop((size_t)cw * ch);
    for (int y = 0; y < ch; y++)
    {
        int wy = std::clamp(y0 + y, 0, g_wacH - 1);
        for (int x = 0; x < cw; x++)
        {
            int wx = ((x0 + x) % g_wacW + g_wacW) % g_wacW;
            crop[(size_t)y * cw + x] =
                g_wac[(size_t)wy * g_wacW + wx] * (1.0f / 255.0f);
        }
    }
    GaussianBlur(crop, cw, ch, 0.7f);         // denoise JPEG artifacts

    // Sample the block at each output pixel's OWN latitude and longitude
    // instead of stretching it edge to edge. The block's bounds are whole
    // texels and the window's are not, so stretching made the mapping
    // from pixel to ground depend on where those texels happened to
    // fall: two windows over the same ground disagreed by up to half a
    // texel, 0.67 km here and about 75 px at the 5 km level. Sampling
    // geographically makes the imagery a pure function of position --
    // the property the noise lattice already has.
    Field out((size_t)res * res);
    double tx0 = TexelOriginX(lon0, x0), dtx = TexelStepX(lonSpan, res);
    double ty0 = TexelOriginY(lat1, y0), dty = TexelStepY(spanDeg, res);
    for (int y = 0; y < res; y++)
    {
        double fy = ty0 + (y + 0.5) * dty;
        int iy = std::clamp((int)std::floor(fy), 0, ch - 2);
        float wy = (float)std::clamp(fy - iy, 0.0, 1.0);
        for (int x = 0; x < res; x++)
        {
            double fx = tx0 + (x + 0.5) * dtx;
            int ix = std::clamp((int)std::floor(fx), 0, cw - 2);
            float wx = (float)std::clamp(fx - ix, 0.0, 1.0);
            const float* r0 = &crop[(size_t)iy * cw + ix];
            const float* r1 = &crop[(size_t)(iy + 1) * cw + ix];
            float a = r0[0] + (r0[1] - r0[0]) * wx;
            float b = r1[0] + (r1[1] - r1[0]) * wx;
            out[(size_t)y * res + x] = a + (b - a) * wy;
        }
    }
    return out;
}

// Unsharp + adaptive contrast around the crop's own midpoint (capped
// gain — maria must stay dark, calm plains).
static void SharpenAdaptive(Field& macro, int res)
{
    float k = res / 300.0f;
    Field blur = macro;
    GaussianBlur(blur, res, res, 5.0f * k);
    for (size_t i = 0; i < macro.size(); i++)
        macro[i] = std::clamp(macro[i] + 0.40f * (macro[i] - blur[i]),
                              0.0f, 1.0f);

    Field sorted = macro;
    std::sort(sorted.begin(), sorted.end());
    float pLo = sorted[(size_t)(sorted.size() * 0.02)];
    float pHi = sorted[(size_t)(sorted.size() * 0.98)];
    float spread = std::max(pHi - pLo, 1e-4f);
    float gain = std::min(2.2f, std::max(1.0f, 0.60f / spread));
    float mid = 0.5f * (pHi + pLo);
    for (float& v : macro)
        v = std::clamp(mid + (v - mid) * gain, 0.0f, 1.0f);
}

// Small-crater field for zoom levels BELOW the real-data floor
// (~1.3 km/px): sub-resolution craters exist everywhere on the real
// moon but the source cannot resolve them, so here invention is
// honest — it never contradicts data. Real lunar crater profile:
// flat floor (d < 0.70), power-law wall, tiny gaussian rim.
[[maybe_unused]] static void CarveSmallCraters(Field& height, int res, TerrainRng& rng,
                              int count, float rMinPx, float rMaxPx,
                              float depthScale)
{
    // Placed craters, for overlap rejection: an overlapping crater
    // field reads as noise, separated bowls read as ground.
    std::vector<float> px, py, pr;
    px.reserve(count);
    py.reserve(count);
    pr.reserve(count);

    for (int c = 0; c < count; c++)
    {
        // Power-law-ish size mix: most craters small, a few large.
        float u = rng.Uniform();
        float r = rMinPx * std::pow(rMaxPx / rMinPx,
                                    std::pow(u, 2.2f));
        // Rejection placement: keep a clear margin to every earlier
        // crater (1.25x their summed radii); big first would claim
        // space better, but a few attempts per crater is enough.
        float cx = 0.0f, cy = 0.0f;
        bool placed = false;
        for (int attempt = 0; attempt < 8 && !placed; attempt++)
        {
            cx = rng.Uniform() * res;
            cy = rng.Uniform() * res;
            placed = true;
            for (size_t i = 0; i < px.size(); i++)
            {
                float ddx = px[i] - cx;
                float ddy = py[i] - cy;
                float minD = (pr[i] + r) * 1.25f;
                if (ddx * ddx + ddy * ddy < minD * minD)
                {
                    placed = false;
                    break;
                }
            }
        }
        if (!placed) continue;
        px.push_back(cx);
        py.push_back(cy);
        pr.push_back(r);
        float age = rng.Uniform();            // 0 fresh .. 1 eroded
        float sharp = 1.0f - 0.7f * age;
        float depth = -depthScale * sharp * (0.5f + rng.Uniform());
        // A fresh deep minority gives the field its punch — without
        // them everything reads as uniform soft dimples.
        if (rng.Uniform() < 0.12f) depth *= 1.9f;
        float rimAmp = 0.05f * sharp * std::fabs(depth) / depthScale;
        float wallP = 3.5f;

        int x0 = std::max(0, (int)(cx - r * 1.1f));
        int x1 = std::min(res - 1, (int)(cx + r * 1.1f) + 1);
        int y0 = std::max(0, (int)(cy - r * 1.1f));
        int y1 = std::min(res - 1, (int)(cy + r * 1.1f) + 1);
        for (int y = y0; y <= y1; y++)
        {
            for (int x = x0; x <= x1; x++)
            {
                float dx = (x - cx) / r;
                float dy = (y - cy) / r;
                float d = std::sqrt(dx * dx + dy * dy);
                float delta = 0.0f;
                if (d < 0.70f)
                {
                    delta = depth;
                }
                else if (d < 0.95f)
                {
                    float wu = (d - 0.70f) / 0.25f;
                    delta = depth * (1.0f - std::pow(wu, wallP));
                }
                else if (d < 1.05f)
                {
                    float g = (d - 1.00f) / 0.05f;
                    delta = rimAmp * depthScale * std::exp(-g * g);
                }
                height[y * res + x] += delta;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// The world-anchored sub-floor.
//
// Ported from prototypes/planet_visuals/regolith_chain.js, which is where it
// was designed and where every constant here was chosen against the real
// mosaic. Read that file's comments for the reasoning; this is the same
// arithmetic in C++.
//
// The idea in one line: the mosaic stops resolving at about 1.3 km/px, so
// below that the ground is invented -- but invented in WORLD coordinates, at
// wavelengths in kilometres and on populations pinned to a lattice on the
// moon. That is the difference between zooming in and getting closer, and
// zooming in and getting a different picture of the same size.
// ---------------------------------------------------------------------------

const double FLOOR_KM = 1.3325;      // one WAC texel at the equator
const int SUB_MAX_OCTAVES = 16;

// 0 where the mosaic still resolves this wavelength, 1 well below it.
// Crossing over rather than switching is what keeps the invented detail from
// fighting the real landforms it sits under.
static float SubFade(double lambdaKm)
{
    return (float)std::clamp(std::log2(FLOOR_KM * 2.0 / lambdaKm) / 1.5, 0.0, 1.0);
}

// Every pixel's world position, once.
//
// u needs the cosine of ITS OWN row: taking one cosine for the whole window
// makes the east-west scale depend on where the window is, which slid the
// lattice by 7 px at the sect level when the shader did it that way. So
// latitude and its cosine are per row, and longitude-in-km per column.
struct WorldGrid
{
    std::vector<double> vRow, cosRow, lonKm;

    WorldGrid(const NoiseFrame& frame, int res)
        : vRow(res), cosRow(res), lonKm(res)
    {
        for (int y = 0; y < res; y++)
        {
            double lat = frame.lat0Deg + (y + 0.5) * frame.dLatPerPx;
            vRow[y] = lat * MOON_KM_PER_DEG;
            cosRow[y] = std::cos(lat * DEG2RAD);
        }
        for (int x = 0; x < res; x++)
            lonKm[x] = (frame.lon0Deg + (x + 0.5) * frame.dLonPerPx) * MOON_KM_PER_DEG;
    }
};

// The rim and its ejecta blanket, as a function of distance past the rim.
static float RimRise(float t, float invW, float ejectaR)
{
    const float EJECTA_SHARE = 0.45f;
    float g = std::exp(-t * t * invW);
    if (t <= 0.0f || ejectaR <= 0.001f) return g;
    float x = std::min(1.0f, t / ejectaR);
    float sf = 1.0f - x * x * (3.0f - 2.0f * x);
    return (1.0f - EJECTA_SHARE) * g + EJECTA_SHARE * sf;
}

// Bowl profile in units of the crater's own depth, r in radii.
//
// The parabola -(1 - r^2) is zero AT the rim but not FLAT at it: it arrives
// with a slope of -2/(1-flat), so the bowl meets the plain in a crease. On a
// bench drawn at one scale that is invisible; on a saturated population under
// a hillshade every crater gets a drawn black circle around it, which is what
// sent the bench's first deep-zoom renders wrong. The cosine dish has zero
// slope at both ends -- flat at the floor, tangent at the rim -- and is the
// profile lola_dem.cpp already uses for the same reason.
static float CraterProfile(float r, float flat, int cosine)
{
    if (r >= 1.0f) return 0.0f;
    if (r <= flat) return -1.0f;
    float u = (r - flat) / std::max(1e-4f, 1.0f - flat);
    return cosine ? -0.5f * (1.0f + std::cos((float)PI * u))
                  : -(1.0f - u * u);
}

// The fractal residual: the ground's own roughness continued downward.
// Relief at a wavelength is a FRACTION of that wavelength, which is what
// makes it scale-free -- the same rule at 1 km and at 3 m. Metres, added.
static void SubFloorNoise(Field& outM, int res, const WorldGrid& W,
                          double kmPerPx, float roughFrac,
                          const Field& roughMask)
{
    double lambda = FLOOR_KM * 2.0;
    for (int o = 0; o < SUB_MAX_OCTAVES && lambda >= 3.0 * kmPerPx;
         o++, lambda *= 0.5)
    {
        float w = SubFade(lambda);
        if (w <= 0.001f) continue;
        float ampM = (float)(w * roughFrac * lambda * 1000.0);
        uint32_t salt = (uint32_t)(0x51u + (uint32_t)o * 2654435761u);
        for (int y = 0; y < res; y++)
        {
            double v = W.vRow[y], c = W.cosRow[y];
            size_t row = (size_t)y * res;
            for (int x = 0; x < res; x++)
            {
                double u = W.lonKm[x] * c;
                outM[row + x] += ampM * roughMask[row + x] *
                                 DetailNoise(u, v, lambda, salt);
            }
        }
    }
}

// The window's world bounds, from its four corners.
static void FrameWorldBounds(const NoiseFrame& frame, int res,
                             double* uMin, double* uMax,
                             double* vMin, double* vMax)
{
    *uMin = *vMin = 1e300; *uMax = *vMax = -1e300;
    const double cx[4] = {0.0, (double)res, 0.0, (double)res};
    const double cy[4] = {0.0, 0.0, (double)res, (double)res};
    for (int i = 0; i < 4; i++)
    {
        double u, v;
        FrameWorldKm(frame, cx[i], cy[i], &u, &v);
        *uMin = std::min(*uMin, u); *uMax = std::max(*uMax, u);
        *vMin = std::min(*vMin, v); *vMax = std::max(*vMax, v);
    }
}

// The impact population, band by band: sizes halving from the mosaic's
// floor down to about three pixels, each band a jittered world lattice.
//
// Deepest-wins inside a band, which a saturated field needs -- summing
// overlapping bowls digs runaway pits. Rims add, because ejecta does.
// Returns how many craters landed in the window.
static int CraterPopulation(Field& outM, int res, const NoiseFrame& frame,
                            double spanKm, const TerrainTuning& P)
{
    double kmPerPx = spanKm / res;
    double uMin, uMax, vMin, vMax;
    FrameWorldBounds(frame, res, &uMin, &uMax, &vMin, &vMax);

    Field bowl((size_t)res * res);
    float rOuter = 1.0f + std::max(3.0f * P.rimWidth, P.ejecta);
    int placed = 0;
    double diamKm = FLOOR_KM * 1.4;
    double bandFloor = P.popPx > 0.0f ? P.popPx : 2.5;
    for (int b = 0; b < SUB_MAX_OCTAVES && diamKm >= bandFloor * kmPerPx;
         b++, diamKm *= 0.5)
    {
        float w = SubFade(diamKm);
        if (w <= 0.001f) continue;
        double cellKm = diamKm / 0.55;
        uint32_t salt = (uint32_t)(0xC7A7E5u + (uint32_t)b * 7919u);
        int64_t i0 = (int64_t)std::floor(uMin / cellKm) - 1;
        int64_t i1 = (int64_t)std::floor(uMax / cellKm) + 1;
        int64_t j0 = (int64_t)std::floor(vMin / cellKm) - 1;
        int64_t j1 = (int64_t)std::floor(vMax / cellKm) + 1;
        std::fill(bowl.begin(), bowl.end(), 0.0f);
        for (int64_t cj = j0; cj <= j1; cj++)
        {
            for (int64_t ci = i0; ci <= i1; ci++)
            {
                // Crater fields cluster: a slow density modulation leaves some
                // patches busy and some nearly clean, instead of bubble wrap.
                //
                // Evaluated at the CELL, not at the pixel. Hoisting it to the
                // pixel was 17% faster and drew straight lines across the
                // ground: a crater near the density threshold was included in
                // one lattice cell and dropped in the next, so it got cut off
                // along an axis-aligned boundary. There is no per-pixel
                // shortcut here -- the clustering has to be the crater's own.
                float cluster = 0.5f + 0.5f *
                    DetailNoise((ci + 0.5) * cellKm, (cj + 0.5) * cellKm,
                                cellKm * 11.0, salt + 900u);
                if (DetailHash01((int32_t)ci, (int32_t)cj, salt) >
                    P.popDensity * cluster) continue;
                double u = (ci + 0.12 + 0.76 * DetailHash01((int32_t)ci, (int32_t)cj, salt + 1u)) * cellKm;
                double v = (cj + 0.12 + 0.76 * DetailHash01((int32_t)ci, (int32_t)cj, salt + 2u)) * cellKm;
                double lat = v / MOON_KM_PER_DEG;
                double cosLat = std::cos(lat * DEG2RAD);
                if (std::fabs(cosLat) < 1e-6) continue;
                double lon = u / (MOON_KM_PER_DEG * cosLat);
                double py = (lat - frame.lat0Deg) / frame.dLatPerPx - 0.5;
                double px = (lon - frame.lon0Deg) / frame.dLonPerPx - 0.5;
                double dKm = cellKm * (0.30 + 0.70 * DetailHash01((int32_t)ci, (int32_t)cj, salt + 3u));
                double R = (dKm * 0.5) / kmPerPx;
                if (R < 0.9) continue;
                int x0 = std::max(0, (int)std::floor(px - R * rOuter));
                int x1 = std::min(res - 1, (int)std::ceil(px + R * rOuter));
                int y0 = std::max(0, (int)std::floor(py - R * rOuter));
                int y1 = std::min(res - 1, (int)std::ceil(py + R * rOuter));
                if (x1 < x0 || y1 < y0) continue;
                placed++;
                // Most craters are ancient: age^3 keeps the fresh, sharp,
                // rimmed ones rare, which is what a gardened surface looks like.
                float age = DetailHash01((int32_t)ci, (int32_t)cj, salt + 4u);
                float fresh = age * age * age;
                float depthM = (float)(w * P.subCraters * dKm * 1000.0 *
                                       (P.dMin + (P.dMax - P.dMin) * fresh));
                float rimM = depthM * P.rim * fresh;
                float invW = 1.0f / (2.0f * P.rimWidth * P.rimWidth);
                for (int y = y0; y <= y1; y++)
                {
                    float dy = (float)((y - py) / R);
                    size_t row = (size_t)y * res;
                    for (int x = x0; x <= x1; x++)
                    {
                        float dx = (float)((x - px) / R);
                        float r = std::sqrt(dx * dx + dy * dy);
                        if (r >= rOuter) continue;
                        size_t k = row + x;
                        if (r < 1.0f)
                        {
                            float bv = CraterProfile(r, P.floorFlat, P.cosineBowl) * depthM;
                            if (bv < bowl[k]) bowl[k] = bv;
                        }
                        if (rimM > 0.001f)
                            outM[k] += rimM * RimRise(r - 1.0f, invW, P.ejecta);
                    }
                }
            }
        }
        for (size_t i = 0; i < outM.size(); i++) outM[i] += bowl[i];
    }
    return placed;
}

// Clasts: the scattered angular grit a regolith texture normally PAINTS as
// little bright specks with a lit lower-right edge -- it has to paint them,
// because a texture tile has no light of its own.
//
// Here they are RELIEF: small domes, so the same sun that shades the craters
// gives each one its lit face and its shadow. And they are on the same world
// lattice as everything else -- bands halving from a few tens of metres down
// to the pixel -- so zooming turns a speck into a rock and finds new specks
// under it. Returns how many landed.
static int ClastBands(Field& outM, int res, const NoiseFrame& frame,
                      double spanKm, const TerrainTuning& P)
{
    double kmPerPx = spanKm / res;
    double uMin, uMax, vMin, vMax;
    FrameWorldBounds(frame, res, &uMin, &uMax, &vMin, &vMax);

    int placed = 0;
    double diamKm = 0.045;                    // 45 m: a big rock, and rare
    double clastFloor = P.clastPx > 0.0f ? P.clastPx : 2.2;
    for (int b = 0; b < 12 && diamKm >= clastFloor * kmPerPx; b++, diamKm *= 0.5)
    {
        double cellKm = diamKm / 0.42;
        uint32_t salt = (uint32_t)(0x5EED17u + (uint32_t)b * 26417u);
        // Bigger rocks are rarer, by about the same power law the craters use.
        float occ = std::min(0.9f, P.clastDensity * (0.35f + 0.65f * b / 4.0f));
        int64_t i0 = (int64_t)std::floor(uMin / cellKm) - 1;
        int64_t i1 = (int64_t)std::floor(uMax / cellKm) + 1;
        int64_t j0 = (int64_t)std::floor(vMin / cellKm) - 1;
        int64_t j1 = (int64_t)std::floor(vMax / cellKm) + 1;
        for (int64_t cj = j0; cj <= j1; cj++)
        {
            for (int64_t ci = i0; ci <= i1; ci++)
            {
                if (DetailHash01((int32_t)ci, (int32_t)cj, salt) > occ) continue;
                double u = (ci + 0.15 + 0.70 * DetailHash01((int32_t)ci, (int32_t)cj, salt + 1u)) * cellKm;
                double v = (cj + 0.15 + 0.70 * DetailHash01((int32_t)ci, (int32_t)cj, salt + 2u)) * cellKm;
                double lat = v / MOON_KM_PER_DEG;
                double cosLat = std::cos(lat * DEG2RAD);
                if (std::fabs(cosLat) < 1e-6) continue;
                double lon = u / (MOON_KM_PER_DEG * cosLat);
                double py = (lat - frame.lat0Deg) / frame.dLatPerPx - 0.5;
                double px = (lon - frame.lon0Deg) / frame.dLonPerPx - 0.5;
                double dKm = diamKm * (0.55 + 0.75 * DetailHash01((int32_t)ci, (int32_t)cj, salt + 3u));
                double R = (dKm * 0.5) / kmPerPx;
                if (R < 0.7) continue;
                int x0 = std::max(0, (int)std::floor(px - R));
                int x1 = std::min(res - 1, (int)std::ceil(px + R));
                int y0 = std::max(0, (int)std::floor(py - R));
                int y1 = std::min(res - 1, (int)std::ceil(py + R));
                if (x1 < x0 || y1 < y0) continue;
                placed++;
                // A rock sits ON the ground: a third of its width proud of it.
                float hM = (float)(P.clasts * dKm * 1000.0 * 0.33 *
                                   (0.6 + 0.8 * DetailHash01((int32_t)ci, (int32_t)cj, salt + 4u)));
                for (int y = y0; y <= y1; y++)
                {
                    float dy = (float)((y - py) / R);
                    size_t row = (size_t)y * res;
                    for (int x = x0; x <= x1; x++)
                    {
                        float dx = (float)((x - px) / R);
                        float q = 1.0f - dx * dx - dy * dy;
                        if (q > 0.0f) outM[row + x] += hM * std::sqrt(q);
                    }
                }
            }
        }
    }
    return placed;
}

// Relief the mosaic cannot carry, in chain units, added to the height field
// where the engine's pixel-anchored grain and undulation used to go.
//
// heightScaleM is the chain's own: the hillshade multiplies height by z = 110
// and differences it per pixel, so one unit is 110 pixel-widths of rise.
// Converting through it is what makes a 400 m crater 400 m deep at every
// level instead of three different depths.
static int SubFloorRelief(Field& height, int res, const NoiseFrame& frame,
                          double spanKm, const TerrainTuning& tune,
                          const Field& density)
{
    double kmPerPx = spanKm / res;
    double heightScaleM = 110.0 * (spanKm * 1000.0 / res);
    WorldGrid W(frame, res);
    Field accM((size_t)res * res, 0.0f);
    Field roughMask((size_t)res * res);
    for (size_t i = 0; i < roughMask.size(); i++)
        roughMask[i] = 0.45f + 0.55f * density[i];   // bright ground is rough ground

    if (tune.subRough > 0.0f)
        SubFloorNoise(accM, res, W, kmPerPx, tune.subRough, roughMask);
    if (tune.clasts > 0.001f)
        ClastBands(accM, res, frame, spanKm, tune);
    int craters = 0;
    if (tune.subCraters > 0.001f)
        craters = CraterPopulation(accM, res, frame, spanKm, tune);

    // The last octave is the pixel itself, and nothing world-anchored can
    // live there: this is the grit the previous zoom could not show.
    Field grit((size_t)res * res, 0.0f);
    double cellKm = std::max(1e-12, kmPerPx);
    if (tune.subGrit > 0.0f)
    {
        for (int y = 0; y < res; y++)
        {
            double v = W.vRow[y], c = W.cosRow[y];
            size_t row = (size_t)y * res;
            for (int x = 0; x < res; x++)
                grit[row + x] = DetailHash01(
                    (int32_t)std::floor(W.lonKm[x] * c / cellKm),
                    (int32_t)std::floor(v / cellKm), 0x6A09E667u) - 0.5f;
        }
        // Crisp keeps the finest term at one pixel: blurring it is what makes
        // a hard-pixel upscale look like blocky mush rather than like grain.
        if (!tune.crisp) GaussianBlur(grit, res, res, 0.55f);
    }
    float gritM = (float)(tune.subGrit * 1.4 * kmPerPx * 1000.0);   // ~1.4 px of relief
    for (size_t i = 0; i < height.size(); i++)
        height[i] += (float)((accM[i] + gritM * grit[i] * roughMask[i]) / heightScaleM);
    return craters;
}

// The regolith's own tone: broad mottled patches over fine grit, in world
// wavelengths so it too only gains detail as you come down. ALBEDO only --
// fed into the macro it would be read back as relief by formRelief.
static Field SubFloorMottle(int res, const NoiseFrame& frame, double spanKm,
                            const TerrainTuning& tune)
{
    double kmPerPx = spanKm / res;
    WorldGrid W(frame, res);
    Field out((size_t)res * res, 0.0f);
    // Four octaves, not everything down to the pixel: mottling is a broad
    // tone, its fine end is invisible under the grit, and measured at res 640
    // the extra octaves cost more than the crater population does.
    const int MOTTLE_OCTAVES = 4;
    double lambda = FLOOR_KM * 0.7;
    float amp = 1.0f, norm = 0.0f;
    for (int o = 0; o < MOTTLE_OCTAVES && lambda >= 4.0 * kmPerPx;
         o++, lambda *= 0.5)
    {
        float w = SubFade(lambda * 2.0) * amp;
        norm += amp;
        if (w > 0.001f)
        {
            uint32_t salt = (uint32_t)(0x2545F491u + (uint32_t)o * 40503u);
            for (int y = 0; y < res; y++)
            {
                double v = W.vRow[y], c = W.cosRow[y];
                size_t row = (size_t)y * res;
                for (int x = 0; x < res; x++)
                    out[row + x] += w * DetailNoise(W.lonKm[x] * c, v, lambda, salt);
            }
        }
        amp *= 0.62f;
    }
    if (norm > 0.0f)
        for (size_t i = 0; i < out.size(); i++) out[i] *= tune.subMottle / norm;
    return out;
}

// Boulder speckle: tiny sharp bumps; the shared relighting gives each
// one its lit face and cast-shadow pixel automatically.
// One boulder per world cell, on a hashed pixel inside it -- the scheme
// the GPU shader already uses, and the only one that puts a boulder on
// the same rock twice. count sets the cell size: a boulder is a
// landmark once the chain IS the ground, so where it sits has to be a
// property of the moon and not of the frame.
static void SprinkleBoulders(Field& height, int res, const NoiseFrame& frame,
                             int count, float amp)
{
    if (count <= 0) return;
    int cellPx = std::max(2, (int)std::lround(res / std::sqrt((double)count)));
    double cellKm = std::max(1e-12, cellPx * frame.kmPerPx);

    // Bound the world cells the window can see, then put each one's
    // boulder back on a pixel. The frame inverts exactly: v gives the
    // latitude, and the latitude gives the cosine that u needs.
    double uc[4], vc[4];
    FrameWorldKm(frame, 0.0,         0.0,         &uc[0], &vc[0]);
    FrameWorldKm(frame, (double)res, 0.0,         &uc[1], &vc[1]);
    FrameWorldKm(frame, 0.0,         (double)res, &uc[2], &vc[2]);
    FrameWorldKm(frame, (double)res, (double)res, &uc[3], &vc[3]);
    double uMin = uc[0], uMax = uc[0], vMin = vc[0], vMax = vc[0];
    for (int i = 1; i < 4; i++)
    {
        uMin = std::min(uMin, uc[i]); uMax = std::max(uMax, uc[i]);
        vMin = std::min(vMin, vc[i]); vMax = std::max(vMax, vc[i]);
    }
    int64_t i0 = (int64_t)std::floor(uMin / cellKm);
    int64_t j0 = (int64_t)std::floor(vMin / cellKm);
    int64_t i1 = (int64_t)std::floor(uMax / cellKm) + 1;
    int64_t j1 = (int64_t)std::floor(vMax / cellKm) + 1;

    uint32_t salt = frame.salt ^ NOISE_BOULDER;
    for (int64_t cj = j0; cj <= j1; cj++)
    {
        for (int64_t ci = i0; ci <= i1; ci++)
        {
            float jx = HashCell(ci, cj, salt);
            float jy = HashCell(ci, cj, salt ^ 0x5BD1E995u);
            double u = ((double)ci + jx) * cellKm;
            double v = ((double)cj + jy) * cellKm;

            double lat = v / MOON_KM_PER_DEG;
            double cosLat = std::cos(lat * DEG2RAD);
            if (std::fabs(cosLat) < 1e-6) continue;
            double lon = u / (MOON_KM_PER_DEG * cosLat);
            double py = (lat - frame.lat0Deg) / frame.dLatPerPx - 0.5;
            double px = (lon - frame.lon0Deg) / frame.dLonPerPx - 0.5;

            int x = (int)std::lround(px), y = (int)std::lround(py);
            if (x < 1 || y < 1 || x >= res - 1 || y >= res - 1) continue;
            float a = amp * (0.4f + HashCell(ci, cj, salt ^ 0x27220A95u));
            height[(size_t)y * res + x] += a;
            if (HashCell(ci, cj, salt ^ 0x165667B1u) < 0.5f)
                height[(size_t)y * res + x + 1] += a * 0.6f;
            if (HashCell(ci, cj, salt ^ 0x9E3779B1u) < 0.3f)
                height[(size_t)(y + 1) * res + x] += a * 0.5f;
        }
    }
}

// Smooth falloff of the site's influence: full effect through the
// middle, fading to nothing at the site radius.
static float SiteWeight(float x, float y, float cx, float cy,
                        float workedR, float outerR)
{
    float d = std::hypot(x - cx, y - cy);
    if (d <= workedR) return 1.0f;          // fully worked ground
    if (d >= outerR) return 0.0f;           // untouched
    float t = 1.0f - (d - workedR) / std::max(1e-3f, outerR - workedR);
    return t * t * (3.0f - 2.0f * t);
}

// Calm the imagery inside the site before any relief is derived from
// it. The macro drives both the base albedo and the form relief, so
// pulling its contrast toward the local mean levels off the deep
// natural shadows the site would otherwise sit in.
static void LevelSiteMacro(Field& macro, int res, float pxPerKm,
                           const TerrainSiteDisturbance& site)
{
    const float cx = res * 0.5f, cy = res * 0.5f;
    const float workedR = site.workedRadiusKm * pxPerKm;
    const float outerR = (site.workedRadiusKm + site.fadeKm) * pxPerKm;
    if (outerR < 2.0f || site.toneLevelAmount <= 0.0f) return;

    double sum = 0.0, wsum = 0.0;
    for (int y = 0; y < res; y++)
        for (int x = 0; x < res; x++)
        {
            float w = SiteWeight((float)x, (float)y, cx, cy, workedR, outerR);
            if (w <= 0.0f) continue;
            sum += macro[(size_t)y * res + x] * w;
            wsum += w;
        }
    if (wsum < 1e-6) return;
    float mean = (float)(sum / wsum);

    for (int y = 0; y < res; y++)
        for (int x = 0; x < res; x++)
        {
            float w = SiteWeight((float)x, (float)y, cx, cy, workedR, outerR);
            if (w <= 0.0f) continue;
            size_t i = (size_t)y * res + x;
            float k = site.toneLevelAmount * w;
            macro[i] = macro[i] * (1.0f - k) + mean * k;
        }
}

// Work the ground over a little where the colony operates.
//
// The natural terrain is kept — nothing is levelled. Around the core
// and each unit dome the surface picks up a shallow mound or hollow, a
// patch of extra roughness, and a gentle undulation across the site as
// a whole. Everything goes into the HEIGHT field, so the shared sun
// gives it the shading and small shadows for free.
static void ApplySiteDisturbance(Field& height, int res, float pxPerKm,
                                 const NoiseFrame& frame,
                                 const TerrainSiteDisturbance& site)
{
    const float cx = res * 0.5f;
    const float cy = res * 0.5f;
    const float workedR = site.workedRadiusKm * pxPerKm;
    const float outerR = (site.workedRadiusKm + site.fadeKm) * pxPerKm;
    if (outerR < 2.0f) return;         // site smaller than a pixel here

    // Worked spots: the central core plus the ring of unit domes.
    struct Spot { float x, y, r, amp; };
    std::vector<Spot> spots;
    // The spots sit at fixed offsets from the site, which is itself at
    // a fixed place on the moon, so only their amplitudes needed a
    // number: index them instead of drawing them.
    uint32_t siteSalt = frame.salt ^ NOISE_SITE;
    auto spotAmp = [&](int i) {
        return site.spotAmp * (HashCell(i, 0, siteSalt) - 0.5f) * 2.0f;
    };
    spots.push_back({cx, cy, site.coreRadiusKm * pxPerKm, spotAmp(0)});
    for (int i = 0; i < site.domeCount; i++)
    {
        // Same layout the sect view draws: 8 units, 45 deg apart,
        // starting at the top and going clockwise.
        float ang = (90.0f - i * (360.0f / site.domeCount)) * DEG2RAD;
        float ring = site.ringRadiusKm * pxPerKm;
        spots.push_back({cx + ring * std::cos(ang),
                         cy - ring * std::sin(ang),
                         site.domeWorkKm * pxPerKm,
                         spotAmp(i + 1)});
    }

    // Level the natural elevation swings down to a calmer baseline
    // first, so the worked undulations laid on top actually read
    // instead of being buried under the wild terrain.
    if (site.levelAmount > 0.0f)
    {
        double hsum = 0.0, hw = 0.0;
        for (int y = 0; y < res; y++)
            for (int x = 0; x < res; x++)
            {
                float w = SiteWeight((float)x, (float)y, cx, cy, workedR, outerR);
                if (w <= 0.0f) continue;
                hsum += height[(size_t)y * res + x] * w;
                hw += w;
            }
        if (hw > 1e-6)
        {
            float mean = (float)(hsum / hw);
            for (int y = 0; y < res; y++)
                for (int x = 0; x < res; x++)
                {
                    float w = SiteWeight((float)x, (float)y, cx, cy, workedR, outerR);
                    if (w <= 0.0f) continue;
                    size_t i = (size_t)y * res + x;
                    float k = site.levelAmount * w;
                    height[i] = height[i] * (1.0f - k) + mean * k;
                }
        }
    }

    // Two noise fields: soft lumps for undulation, fine grain for the
    // random alterations. Sampled, not re-rolled per pixel, so the
    // result stays deterministic for the location.
    NoiseFrame lumpFrame = frame; lumpFrame.salt ^= NOISE_LUMPS;
    Field lumps = Fbm(res, 3, std::max(4, (int)(res / 12)), 0.55f, lumpFrame);
    Field fine = GrainNoise(res, frame);

    for (int y = 0; y < res; y++)
    {
        for (int x = 0; x < res; x++)
        {
            size_t i = (size_t)y * res + x;

            float siteW = SiteWeight((float)x, (float)y, cx, cy, workedR, outerR);

            // Per-dome worked patches, strongest at each dome.
            float domeW = 0.0f;
            float spotH = 0.0f;
            for (const Spot& sp : spots)
            {
                float d = std::hypot(x - sp.x, y - sp.y) / std::max(1.0f, sp.r);
                if (d >= 1.0f) continue;
                float t = 1.0f - d;
                float w = t * t * (3.0f - 2.0f * t);
                domeW = std::max(domeW, w);
                spotH += sp.amp * w;      // shallow mound or hollow
            }

            if (siteW <= 0.0f && domeW <= 0.0f) continue;

            // Gentle undulation over the whole site.
            height[i] += site.undulationAmp * (lumps[i] - 0.5f) * 2.0f * siteW;
            // Random alterations, concentrated around the domes.
            height[i] += site.roughAmp * fine[i]
                         * (0.35f * siteW + 0.65f * domeW);
            height[i] += spotH;
        }
    }
}

// The anti-matte relight + grain stage (port of _texture_modulate).
// boulderCount > 0 adds sub-resolution boulder speckle — only used on
// zoom levels below the real-data floor.
// outHeight/outAlbedo, when both are given, take the two fields the
// chain builds on its way to the picture and skip the lighting -- see
// TerrainChainFields. macro is left alone in that case.
static void TextureModulate(Field& macro, int res, const NoiseFrame& frame,
                            float amp,
                            const TerrainTuning& tune,
                            int boulderCount = 0,
                            const TerrainSiteDisturbance* site = nullptr,
                            float pxPerKm = 0.0f,
                            Field* outHeight = nullptr,
                            Field* outAlbedo = nullptr,
                            bool lastRung = false)
{
    // Pixel-based sizes below are tuned at 300 px; k rescales them so
    // physical feature sizes stay fixed at other resolutions.
    float k = res / 300.0f;
    // The world-anchored stack runs only on the rung being LOOKED at.
    // Each rung's macro is the LIT output of the rung above, and
    // formRelief reads that shading back as height -- so relief carved at
    // rung after rung is shaded once per rung. Measured in the bench on a
    // mare crater: peak luminance 196/255 carved at four rungs against 50
    // carved once. The population is world-anchored and deterministic, so
    // the deepest rung regenerates all of it anyway; only the compounding
    // is lost, and the compounding was the bug.
    const bool worldFloor = (tune.subFloor != 0) && lastRung;
    const double spanKm = frame.kmPerPx * res;
    if (site && site->enabled && g_siteDisturbEnabled && pxPerKm > 0.0f)
        LevelSiteMacro(macro, res, pxPerKm, *site);

    Field density((size_t)res * res);
    for (size_t i = 0; i < density.size(); i++)
        density[i] = std::clamp((macro[i] - 0.22f) / 0.45f, 0.15f, 1.0f);

    // Height field: smoothed macro as relief proxy + grain + undulation
    Field height = macro;
    // Smoothed hard by default so the imagery's own noise does not become
    // terrain; smoothed less when the picture will be shown at its own
    // resolution and can afford the detail.
    GaussianBlur(height, res, res, (tune.crisp ? 1.2f : 2.5f) * k);
    for (float& v : height) v = (v - 0.5f) * 0.13f * tune.formRelief;

    if (worldFloor)
    {
        SubFloorRelief(height, res, frame, spanKm, tune, density);
    }
    // Note what is NOT here: with the world stack on, an INTERMEDIATE rung
    // gets neither. Not an oversight -- the rung below is a crop of this
    // one's lit output, and formRelief reads that shading back as height, so
    // pixel-anchored grain laid here would come back as terrain down there
    // and compound rung after rung. The world stack is regenerated whole at
    // whatever rung is being looked at, so nothing is lost by leaving the
    // ones above it clean.
    //
    // Collapsing this into a plain else was the one bug the cross-port
    // comparison caught: with subFloor on, levels the sub-floor never
    // touches diverged from the bench by RMS 9.3, and the crop carried it
    // down. With subFloor off both chains agree to RMS 0.71 everywhere.
    else if (!tune.subFloor)
    {
        Field grain = GrainNoise(res, frame);
        NoiseFrame undulFrame = frame; undulFrame.salt ^= NOISE_UNDULATION;
        Field undul = Fbm(res, tune.octaves,
                          std::max(2, (int)(tune.featureScale * k)),
                          0.5f, undulFrame);
        for (size_t i = 0; i < height.size(); i++)
        {
            float rough = 0.45f + 0.55f * density[i];
            height[i] += 0.004f * amp * tune.grain * grain[i] * rough;
            height[i] += 0.02f * amp * tune.undulation * (undul[i] - 0.5f) * rough;
        }
    }

    // The clasts are the same idea done properly -- world-anchored rocks
    // that grow as you come down -- so the pixel-anchored sprinkle stands
    // down when they are running.
    if (boulderCount > 0 && !worldFloor)
        SprinkleBoulders(height, res, frame,
                         (int)(boulderCount * tune.boulders),
                         0.010f * tune.boulderAmp);

    if (site && site->enabled && g_siteDisturbEnabled && pxPerKm > 0.0f)
        ApplySiteDisturbance(height, res, pxPerKm, frame, *site);

    if (outHeight && outAlbedo)
    {
        // The speckle belongs to the albedo, not to the light, so it
        // comes across; the hillshade and the shadow march do not.
        NoiseFrame sf = frame; sf.salt ^= NOISE_SPECKLE;
        Field speckle = worldFloor ? SubFloorMottle(res, frame, spanKm, tune)
                                   : Fbm(res, 2, 4, 0.5f, sf);
        // The mottle is already signed and already scaled by subMottle; the
        // fbm is 0..1 and takes the old gain. Same shape, two conventions.
        float speckMid = worldFloor ? 0.0f : 0.5f;
        float speckGain = worldFloor ? 1.0f : 0.04f * tune.speckle;
        *outHeight = height;
        outAlbedo->resize((size_t)res * res);
        for (size_t i = 0; i < macro.size(); i++)
        {
            float rough = 0.45f + 0.55f * density[i];
            (*outAlbedo)[i] = std::clamp(
                macro[i] * (1.0f + speckGain * std::min(amp, 1.6f)
                                  * (speckle[i] - speckMid) * rough),
                0.0f, 1.0f);
        }
        return;
    }

    const float z = 110.0f;
    // crisp drops the softening steps that only make sense when the picture
    // is going to be resampled anyway -- a tile drawn at its own resolution
    // is blurred for nothing.
    Field hs = Hillshade(height, res, z, tune.crisp ? 0.0f : 0.6f,
                         tune.sunAz, tune.sunAlt);
    float flatRef = std::sin(tune.sunAlt * (float)DEG2RAD);
    // The march is the largest per-pixel cost in the chain and had no lever
    // until now. Off means fully lit, not black: it is a measurement switch.
    Field light;
    if (tune.shadows == 0) light.assign((size_t)res * res, 1.0f);
    else light = CastShadows(height, res, z, 22.0f * k, 1.5f,
                             tune.sunAz, tune.sunAlt,
                             tune.crisp ? 0.25f : 0.8f);

    NoiseFrame speckleFrame = frame; speckleFrame.salt ^= NOISE_SPECKLE;
    Field speckle = worldFloor ? SubFloorMottle(res, frame, spanKm, tune)
                               : Fbm(res, 2, 4, 0.5f, speckleFrame);
    float speckMid = worldFloor ? 0.0f : 0.5f;
    float speckGain = worldFloor ? 1.0f : 0.04f * tune.speckle;
    for (size_t i = 0; i < macro.size(); i++)
    {
        float rel = std::clamp(hs[i] / flatRef, 0.0f, 1.6f);
        float rough = 0.45f + 0.55f * density[i];
        // relWeight and lightWeight say how much of the picture is the
        // shading and how much is the imagery's own tone. They were in
        // TerrainTuning all along and nothing read them -- the numbers
        // below were the defaults written out longhand, so every preset
        // that set them (silky, dramatic) was quietly ignored.
        float lum = macro[i] * ((1.0f - tune.relWeight) + tune.relWeight * rel)
                    * ((1.0f - tune.lightWeight) + tune.lightWeight * light[i]);
        lum *= 1.0f + speckGain * std::min(amp, 1.6f)
                    * (speckle[i] - speckMid) * rough;
        lum = std::clamp(lum, 0.0f, 1.0f);
        // Gentle S-curve: deepen shadows, keep highlights
        float sc = lum * lum * (3.0f - 2.0f * lum);
        float outv = std::clamp(sc * tune.sCurve + lum * (1.0f - tune.sCurve),
                                0.0f, 1.0f);
        macro[i] = outv;
    }
}

// Lunar tone ramp: cool shadow -> regolith grey -> warm sunlit.
static Color RampColor(float t)
{
    const float lo[3] = {16, 17, 24};
    const float mid[3] = {108, 105, 102};
    const float hi[3] = {236, 232, 220};
    float rgb[3];
    if (t < 0.5f)
    {
        float u = t / 0.5f;
        for (int k = 0; k < 3; k++) rgb[k] = lo[k] + (mid[k] - lo[k]) * u;
    }
    else
    {
        float u = (t - 0.5f) / 0.5f;
        for (int k = 0; k < 3; k++) rgb[k] = mid[k] + (hi[k] - mid[k]) * u;
    }
    return Color{(unsigned char)rgb[0], (unsigned char)rgb[1],
                 (unsigned char)rgb[2], 255};
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

static double g_anchorLat = TERRAIN_ANCHOR_LAT;
static double g_anchorLon = TERRAIN_ANCHOR_LON;
static unsigned int g_anchorVersion = 1;

void SetTerrainAnchor(double latDeg, double lonDeg)
{
    // Keep the playfield off the poles, where the 1/cos(lat) longitude
    // stretch blows up and the grid would smear.
    g_anchorLat = std::clamp(latDeg, -78.0, 78.0);
    g_anchorLon = lonDeg;
    g_anchorVersion++;
    TraceLog(LOG_INFO, "TERRAIN: anchor -> %.3f, %.3f (v%u)",
             g_anchorLat, g_anchorLon, g_anchorVersion);
}

void GetTerrainAnchor(double* latDeg, double* lonDeg)
{
    if (latDeg) *latDeg = g_anchorLat;
    if (lonDeg) *lonDeg = g_anchorLon;
}

unsigned int GetTerrainAnchorVersion() { return g_anchorVersion; }

// ---------------------------------------------------------------------------
// The orbital globe's projection
//
// Orthographic, because that is what a body seen from far away looks
// like and what the baked discs were: no perspective, the silhouette is
// a circle, and the scale is one number (the radius in pixels). It also
// keeps the inverse exact, so picking is not a search.
//
// A surface point is
//     P(lat, lon) = (cos lat sin lon, sin lat, cos lat cos lon)
// with +Z toward the viewer. Turning the globe to face (lat0, lon0) is
//     V = Rx(lat0) . Ry(-lon0) . P
// which is the identity at (0, 0) -- so the near-side view the old fixed
// disc gave is just this camera's default, not a special case.
// V.z > 0 is the visible hemisphere; screen x is V.x, screen y is -V.y
// (screen y grows downward).
//
// The fragment shader in lunar_globe.cpp runs this same math per pixel,
// inverted. Both live off GetOrbitalCamera(), so the picture and the
// pick cannot drift apart.
// ---------------------------------------------------------------------------

static OrbitalCamera g_orbitalCamera;

const OrbitalCamera& GetOrbitalCamera() { return g_orbitalCamera; }

void SetOrbitalCamera(const OrbitalCamera& camera)
{
    g_orbitalCamera = camera;
    // Latitude past the poles would flip the globe's handedness, and
    // longitude is periodic; normalising here means callers can add a
    // delta per frame without ever thinking about it.
    g_orbitalCamera.subLatDeg = std::clamp(g_orbitalCamera.subLatDeg, -89.0, 89.0);
    while (g_orbitalCamera.subLonDeg > 180.0) g_orbitalCamera.subLonDeg -= 360.0;
    while (g_orbitalCamera.subLonDeg < -180.0) g_orbitalCamera.subLonDeg += 360.0;
    g_orbitalCamera.zoom = std::clamp(g_orbitalCamera.zoom,
                                      ORBITAL_ZOOM_MIN, ORBITAL_ZOOM_MAX);
}

// 0.46 of the smaller dimension: the whole moon at zoom 1 with a margin
// that leaves room for the HUD. The old disc was a fixed 588 px radius
// regardless of the window, so on a 1280x720 screen it was cropped top
// and bottom -- a globe you can turn should not have its poles off
// screen.
double OrbitalDiscRadiusPx(int screenWidth, int screenHeight)
{
    double minDim = (double)std::min(screenWidth, screenHeight);
    return minDim * 0.46 * g_orbitalCamera.zoom;
}

// The viewport cancels: a span fills the height when the disc's diameter
// is (moon / span) times it, and the disc is 0.92 of the height at
// zoom 1. So 2*1737.4 / (0.92 * spanKm).
double OrbitalZoomForSpan(double spanKm)
{
    if (spanKm <= 0.0) return ORBITAL_ZOOM_MAX;
    double z = (2.0 * 1737.4) / (0.92 * spanKm);
    return std::clamp(z, ORBITAL_ZOOM_MIN, ORBITAL_ZOOM_MAX);
}

bool OrbitalPickToLatLon(float screenX, float screenY,
                         int screenWidth, int screenHeight,
                         double* latDeg, double* lonDeg)
{
    double cx = screenWidth / 2.0;
    double cy = screenHeight / 2.0;
    double r = OrbitalDiscRadiusPx(screenWidth, screenHeight);
    if (r <= 0.0) return false;

    double xn = (screenX - cx) / r;
    double yn = -(screenY - cy) / r;          // screen y grows downward
    double d2 = xn * xn + yn * yn;
    if (d2 > 0.985 * 0.985) return false;     // outside, or on the limb

    // Unproject to the visible hemisphere, then turn the globe back.
    double zn = std::sqrt(std::max(0.0, 1.0 - d2));

    double lat0 = g_orbitalCamera.subLatDeg * DEG2RAD;
    double lon0 = g_orbitalCamera.subLonDeg * DEG2RAD;
    double cl = std::cos(lat0), sl = std::sin(lat0);
    double co = std::cos(lon0), so = std::sin(lon0);

    // Rx(-lat0)
    double y1 = yn * cl + zn * sl;
    double z1 = -yn * sl + zn * cl;
    double x1 = xn;
    // Ry(lon0)
    double x0 = x1 * co + z1 * so;
    double z0 = -x1 * so + z1 * co;

    if (latDeg) *latDeg = std::asin(std::clamp(y1, -1.0, 1.0)) / DEG2RAD;
    if (lonDeg) *lonDeg = std::atan2(x0, z0) / DEG2RAD;
    return true;
}

bool OrbitalLatLonToScreen(double latDeg, double lonDeg,
                           int screenWidth, int screenHeight,
                           float* screenX, float* screenY)
{
    double lat = latDeg * DEG2RAD;
    double lon = lonDeg * DEG2RAD;
    while (lon > PI) lon -= 2.0 * PI;
    while (lon < -PI) lon += 2.0 * PI;

    double x = std::cos(lat) * std::sin(lon);
    double y = std::sin(lat);
    double z = std::cos(lat) * std::cos(lon);

    double lat0 = g_orbitalCamera.subLatDeg * DEG2RAD;
    double lon0 = g_orbitalCamera.subLonDeg * DEG2RAD;
    double cl = std::cos(lat0), sl = std::sin(lat0);
    double co = std::cos(lon0), so = std::sin(lon0);

    // Ry(-lon0)
    double x1 = x * co - z * so;
    double z1 = x * so + z * co;
    double y1 = y;
    // Rx(lat0)
    double y2 = y1 * cl - z1 * sl;
    double z2 = y1 * sl + z1 * cl;

    if (z2 <= 0.0) return false;              // turned away from the viewer

    double r = OrbitalDiscRadiusPx(screenWidth, screenHeight);
    if (screenX) *screenX = (float)(screenWidth / 2.0 + x1 * r);
    if (screenY) *screenY = (float)(screenHeight / 2.0 - y2 * r);
    return true;
}

void TerrainGridCellToLatLon(int gx, int gy, double* latDeg, double* lonDeg)
{
    double cellDeg = TERRAIN_CELL_KM / MOON_KM_PER_DEG;   // 0.16489 deg
    // Grid centre is between cells 9 and 10; gy grows south.
    double offX = (gx - 9.5);
    double offY = (gy - 9.5);
    double lat = g_anchorLat - offY * cellDeg;
    double c = std::max(0.2, std::cos(g_anchorLat * DEG2RAD));
    double lon = g_anchorLon + offX * cellDeg / c;
    *latDeg = lat;
    *lonDeg = lon;
}

// Shared engine: walk a span ladder, writing an Image for every level
// requested. Level i+1 is the centre crop of level i's OUTPUT, so real
// forms flow down and the levels are registered to each other by
// construction — that is what makes zooming continuous.
static void GenerateChainInternal(double latDeg, double lonDeg, int res,
                                  const TerrainTuning& tune,
                                  Image* outLevels, int wantLevels,
                                  const TerrainSiteDisturbance* site,
                                  const TerrainChainSpans& ladder,
                                  Field* outHeight = nullptr,
                                  Field* outAlbedo = nullptr)
{
    // A kilometre is a different number of pixels in each level, because
    // every level is res wide and they cover different ground.
    const float* levelSpanKm = ladder.km;
    const int levelCount = std::clamp(ladder.count, 1,
                                      TERRAIN_CHAIN_MAX_LEVELS);
    if (wantLevels > levelCount) wantLevels = levelCount;

    // The sect view draws the settlement at 0.63x the physical size the
    // colony view draws it (both use fixed screen fractions). The site
    // geometry is calibrated for the colony view, so shrink it to match
    // for the sect level — otherwise the worked patches sit outside the
    // 5 km window entirely and the effect cannot be seen there.
    const float SECT_SITE_SCALE = 0.63f;
    TerrainSiteDisturbance sectSite;
    // The sect shrink belongs to the 5 km level, whichever index that
    // lands on -- an instrument's ladder may not have one at all.
    const TerrainSiteDisturbance* siteForLevel[TERRAIN_CHAIN_MAX_LEVELS] =
        {site, site, site};
    if (site)
    {
        sectSite = *site;
        sectSite.ringRadiusKm *= SECT_SITE_SCALE;
        sectSite.coreRadiusKm *= SECT_SITE_SCALE;
        sectSite.domeWorkKm *= SECT_SITE_SCALE;
        sectSite.workedRadiusKm *= SECT_SITE_SCALE;
        sectSite.fadeKm *= SECT_SITE_SCALE;
        for (int i = 0; i < levelCount; i++)
            if (levelSpanKm[i] <= 5.0f + 1e-3f) siteForLevel[i] = &sectSite;
    }
    if (!EnsureWacLoaded())
    {
        for (int i = 0; i < wantLevels; i++)
            outLevels[i] = GenImageColor(res, res, Color{40, 40, 48, 255});
        return;
    }

    double t0 = GetTime();

    double spans[TERRAIN_CHAIN_MAX_LEVELS] = {};
    for (int i = 0; i < levelCount; i++)
        spans[i] = levelSpanKm[i] / MOON_KM_PER_DEG;

    // No per-window seed any more: every layer hashes the ground it
    // covers, so the same site invents the same detail from any window
    // that frames it. LocationSeed survives only for the GPU macro crop.
    Field lum = CropMacro(latDeg, lonDeg, spans[0], res);
    SharpenAdaptive(lum, res);
    // The fields belong to the LAST level; every level above it still
    // has to be lit, because the next one down is a crop of its output.
    const bool wantFields = (outHeight != nullptr && outAlbedo != nullptr);

    // One level's lighting. Factored out because with the world stack on a
    // level may be lit twice, for two different jobs -- see below.
    auto lightLevel = [&](Field& f, int lvl, bool lastRung,
                          Field* oh, Field* oa)
    {
        float kk = res / 300.0f;
        int boulderBase = (levelSpanKm[lvl] <= 5.0f + 1e-3f)
                          ? (int)(120 * kk * kk) : 0;
        TextureModulate(f, res,
                        MakeNoiseFrame(latDeg, lonDeg, levelSpanKm[lvl], res,
                                       0x9E3779B9u * (uint32_t)lvl),
                        1.0f + 0.7f * lvl, tune, boulderBase, siteForLevel[lvl],
                        (float)res / levelSpanKm[lvl], oh, oa, lastRung);
    };

    auto emit = [&](int level, const Field& src)
    {
        if (level >= wantLevels) return;
        Image img = GenImageColor(res, res, BLACK);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        Color* px = (Color*)img.data;
        for (int i = 0; i < res * res; i++) px[i] = RampColor(src[i]);
        outLevels[level] = img;
    };

    // Every level of this ladder is a picture somebody LOOKS at. PLANET,
    // COLONY and SECT are three views, not three steps on the way to one --
    // and that is where the game parts company with the bench it was ported
    // from.
    //
    // The bench only ever displays its deepest rung, so it can leave the
    // ones above it bare: no grain and no world stack, so nothing invented
    // up there comes back as height when the next crop re-reads its shading.
    // Do the same here and the PLANET view loses all its texture and goes to
    // mush -- rendered, it was the most visible thing the sub-floor did, and
    // it was a regression rather than the point.
    //
    // So the ladder and the picture are separated. `lum` stays the ladder:
    // lit at every level because the next level is a crop of it, and carrying
    // no invented detail so none of it compounds. Each level that is shown is
    // that same macro lit AGAIN, with the world stack at its own scale. Which
    // is exactly the split the bench's live chain makes between a cached rung
    // and a view -- reached here from the other direction.
    const bool splitShown = (tune.subFloor != 0);

    auto doLevel = [&](int lvl)
    {
        const bool last = (lvl == levelCount - 1);
        if (splitShown && lvl < wantLevels)
        {
            Field shown = lum;
            lightLevel(shown, lvl, true,
                       (wantFields && last) ? outHeight : nullptr,
                       (wantFields && last) ? outAlbedo : nullptr);
            emit(lvl, shown);
            // The ladder copy is only worth lighting if something below
            // is going to be cropped out of it.
            if (!last) lightLevel(lum, lvl, false, nullptr, nullptr);
            else lum = std::move(shown);
        }
        else
        {
            lightLevel(lum, lvl, last,
                       (wantFields && last) ? outHeight : nullptr,
                       (wantFields && last) ? outAlbedo : nullptr);
            emit(lvl, lum);
        }
    };

    doLevel(0);

    for (int lvl = 1; lvl < levelCount; lvl++)
    {
        float k = res / 300.0f;
        // Crop to the level's exact span, not to whole pixels of the
        // level above. Rounding the bounds made a level cover 102 of
        // 102.4 pixels -- 4.98 km where it claimed 5 -- so the imagery
        // ran at a scale the noise lattice, which knows the real span,
        // did not share. Sub-pixel, constant, and enough to stop the
        // sect level registering once everything above it did.
        double frac = spans[lvl] / spans[lvl - 1];
        double half = frac * res / 2.0;
        double lo = res / 2.0 - half;
        double step = 2.0 * half / res;
        Field next((size_t)res * res);
        for (int y = 0; y < res; y++)
        {
            double sy = lo + (y + 0.5) * step - 0.5;
            int iy = std::clamp((int)std::floor(sy), 0, res - 2);
            float ty = (float)std::clamp(sy - iy, 0.0, 1.0);
            for (int x = 0; x < res; x++)
            {
                double sx = lo + (x + 0.5) * step - 0.5;
                int ix = std::clamp((int)std::floor(sx), 0, res - 2);
                float tx = (float)std::clamp(sx - ix, 0.0, 1.0);
                const float* r0 = &lum[(size_t)iy * res + ix];
                const float* r1 = &lum[(size_t)(iy + 1) * res + ix];
                float a = r0[0] + (r0[1] - r0[0]) * tx;
                float b = r1[0] + (r1[1] - r1[0]) * tx;
                next[(size_t)y * res + x] = a + (b - a) * ty;
            }
        }
        lum = std::move(next);
        GaussianBlur(lum, res, res, 0.6f * k);
        Field blur = lum;
        GaussianBlur(blur, res, res, 5.0f * k);
        for (size_t i = 0; i < lum.size(); i++)
            lum[i] = std::clamp(lum[i] + 0.40f * (lum[i] - blur[i]),
                                0.0f, 1.0f);
        doLevel(lvl);
    }

    TraceLog(LOG_INFO,
             "TERRAIN: %d level(s) at (%.3f, %.3f) in %.0f ms",
             wantLevels, latDeg, lonDeg, (GetTime() - t0) * 1000.0);
}

// The 100 km crop at the mosaic's own resolution, for the GPU path.
// Mirrors CropMacro up to (not including) the upsample, then derives the
// SharpenAdaptive stats the way that function does -- unsharp at the
// same physical radius (5k px at res is cw/60 px here), then the 2/98
// percentiles -- so the GPU's gain and midpoint match the CPU's.
bool GetTerrainMacroCrop(double latDeg, double lonDeg, TerrainMacroCrop* out,
                         double spanKm)
{
    if (!out || !EnsureWacLoaded()) return false;
    double spanDeg = spanKm / MOON_KM_PER_DEG;
    double c = std::max(0.2, std::cos(latDeg * DEG2RAD));
    double lonSpan = spanDeg / c;
    double lat0 = latDeg - spanDeg / 2.0, lat1 = latDeg + spanDeg / 2.0;
    double lon0 = lonDeg - lonSpan / 2.0, lon1 = lonDeg + lonSpan / 2.0;
    const int MARGIN = 2;
    int y0 = (int)std::floor((90.0 - lat1) / 180.0 * g_wacH) - MARGIN;
    int y1 = (int)std::floor((90.0 - lat0) / 180.0 * g_wacH) + 1 + MARGIN;
    int x0 = (int)std::floor((lon0 + 180.0) / 360.0 * g_wacW) - MARGIN;
    int x1 = (int)std::floor((lon1 + 180.0) / 360.0 * g_wacW) + 1 + MARGIN;
    int cw = std::max(2, x1 - x0);
    int ch = std::max(2, y1 - y0);
    Field crop((size_t)cw * ch);
    for (int y = 0; y < ch; y++)
    {
        int wy = std::clamp(y0 + y, 0, g_wacH - 1);
        for (int x = 0; x < cw; x++)
        {
            int wx = ((x0 + x) % g_wacW + g_wacW) % g_wacW;
            crop[y * cw + x] =
                g_wac[(size_t)wy * g_wacW + wx] * (1.0f / 255.0f);
        }
    }
    GaussianBlur(crop, cw, ch, 0.7f);         // denoise JPEG artifacts

    Field sharp = crop;
    Field blur = crop;
    int innerW = std::max(1, cw - 2 * MARGIN);
    int innerH = std::max(1, ch - 2 * MARGIN);
    GaussianBlur(blur, cw, ch, std::max(0.3f, innerW / 60.0f));
    for (size_t i = 0; i < sharp.size(); i++)
        sharp[i] = std::clamp(sharp[i] + 0.40f * (sharp[i] - blur[i]),
                              0.0f, 1.0f);
    // Percentiles over the WINDOW, not the sampling margin, so the gain
    // and midpoint do not drift with a buffer the player never sees.
    std::vector<float> inner;
    inner.reserve((size_t)innerW * innerH);
    for (int y = MARGIN; y < ch - MARGIN; y++)
        for (int x = MARGIN; x < cw - MARGIN; x++)
            inner.push_back(sharp[(size_t)y * cw + x]);
    if (inner.empty()) inner = sharp;
    std::sort(inner.begin(), inner.end());
    float pLo = inner[(size_t)(inner.size() * 0.02)];
    float pHi = inner[(size_t)(inner.size() * 0.98)];
    float spread = std::max(pHi - pLo, 1e-4f);
    out->gain = std::min(2.2f, std::max(1.0f, 0.60f / spread));
    out->mid = 0.5f * (pHi + pLo);

    Image img = GenImageColor(cw, ch, BLACK);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    unsigned char* px = (unsigned char*)img.data;
    for (size_t i = 0; i < crop.size(); i++)
    {
        int q = (int)std::lround(std::clamp(crop[i], 0.0f, 1.0f) * 65535.0f);
        px[i * 3 + 0] = (unsigned char)(q >> 8);
        px[i * 3 + 1] = (unsigned char)(q & 0xFF);
        px[i * 3 + 2] = 0;
    }
    out->image = img;
    out->originX = (float)TexelOriginX(lon0, x0);
    out->originY = (float)TexelOriginY(lat1, y0);
    out->spanX = (float)(lonSpan / 360.0 * g_wacW);
    out->spanY = (float)(spanDeg / 180.0 * g_wacH);
    return true;
}

bool TerrainWarmMosaic()
{
    return EnsureWacLoaded();
}

TerrainChainSpans TerrainChainSpansForWindow(double spanKm)
{
    TerrainChainSpans s;
    if (spanKm >= 100.0)
    {
        // Nothing above it to crop from: the macro IS the window.
        s.count = 1;
        s.km[0] = (float)spanKm;
    }
    else
    {
        s.count = 2;
        s.km[0] = 100.0f;
        s.km[1] = (float)spanKm;
    }
    return s;
}

void GenerateTerrainChain(double latDeg, double lonDeg, int res,
                          Image outLevels[3],
                          const TerrainSiteDisturbance* site,
                          const TerrainChainSpans* spans,
                          const TerrainTuning* tune)
{
    TerrainTuning defaults;
    defaults.subFloor = g_subFloorEnabled ? 1 : 0;
    TerrainChainSpans game;
    GenerateChainInternal(latDeg, lonDeg, res, tune ? *tune : defaults,
                          outLevels, 3, site, spans ? *spans : game);
}

bool GenerateTerrainFields(double latDeg, double lonDeg, int res,
                           double spanKm, TerrainChainFields* out,
                           const TerrainSiteDisturbance* site)
{
    if (!out || res < 8 || spanKm <= 0.0) return false;
    TerrainTuning defaults;
    defaults.subFloor = g_subFloorEnabled ? 1 : 0;
    TerrainChainSpans ladder = TerrainChainSpansForWindow(spanKm);
    Field height, albedo;
    GenerateChainInternal(latDeg, lonDeg, res, defaults, nullptr, 0, site,
                          ladder, &height, &albedo);
    if (height.empty() || albedo.empty()) return false;
    out->height = std::move(height);
    out->albedo = std::move(albedo);
    out->res = res;
    // The hillshade multiplies this field by z = 110 and differences it
    // per pixel for a slope, so one unit of it is 110 pixel-widths of
    // rise. That is the scale at which the chain ALREADY treats the field
    // as terrain; any other number here would light the ground
    // differently from the way the chain drew it.
    out->heightScaleM = 110.0f * (float)(spanKm * 1000.0 / res);
    return true;
}

Image GenerateSectTerrain(double latDeg, double lonDeg, int res,
                          const TerrainTuning* tuning)
{
    TerrainTuning defaults;
    defaults.subFloor = g_subFloorEnabled ? 1 : 0;
    const TerrainTuning& tune = tuning ? *tuning : defaults;
    Image levels[3] = {};
    TerrainChainSpans game;
    GenerateChainInternal(latDeg, lonDeg, res, tune, levels, 3, nullptr, game);
    UnloadImage(levels[0]);
    UnloadImage(levels[1]);
    return levels[2];
}
