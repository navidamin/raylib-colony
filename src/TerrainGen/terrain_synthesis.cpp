#include "terrain_synthesis.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

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

static uint32_t LocationSeed(double latDeg, double lonDeg)
{
    // Same quantisation as the Python prototype: 0.01 deg (~300 m).
    int qlat = (int)std::lround((latDeg + 90.0) * 100.0);
    int qlon = (int)std::lround((lonDeg + 180.0) * 100.0);
    uint32_t x = ((uint32_t)(qlat * 73856093)) ^ ((uint32_t)(qlon * 19349663));
    x = (x ^ (x >> 16)) * 0x45D9F3Bu;
    x = (x ^ (x >> 16)) * 0x45D9F3Bu;
    return x ^ (x >> 16);
}

// ---------------------------------------------------------------------------
// Float-field helpers. All fields are res*res, row-major.
// ---------------------------------------------------------------------------

typedef std::vector<float> Field;

static void GaussianBlur(Field& a, int w, int h, float sigma)
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
static Field ValueNoise(int res, int scale, TerrainRng& rng)
{
    int g = std::max(2, res / scale + 2);
    Field grid((size_t)g * g);
    for (float& v : grid) v = rng.Uniform();
    Field up = ResizeBilinear(grid, g, g, res, res);
    GaussianBlur(up, res, res, scale * 0.45f);
    return up;
}

static Field Fbm(int res, int octaves, int baseScale, float persistence,
                 TerrainRng& rng)
{
    Field out((size_t)res * res, 0.0f);
    float amp = 1.0f;
    float norm = 0.0f;
    int scale = baseScale;
    for (int o = 0; o < octaves; o++)
    {
        Field n = ValueNoise(res, scale, rng);
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
static Field GrainNoise(int res, TerrainRng& rng)
{
    Field g = Fbm(res, 5, 64, 0.8f, rng);
    NormalizeField(g);
    Field fine((size_t)res * res);
    for (float& v : fine) v = rng.Uniform() - 0.5f;
    GaussianBlur(fine, res, res, 0.5f);
    NormalizeField(fine);
    for (size_t i = 0; i < g.size(); i++)
        g[i] = 0.55f * g[i] + 0.75f * fine[i];
    NormalizeField(g);
    return g;
}

// Lambertian hillshade, sun from azimuth 315 (NW), altitude 35 deg.
static Field Hillshade(const Field& height, int res, float zFactor,
                       float smoothPx)
{
    Field h = height;
    GaussianBlur(h, res, res, smoothPx);
    const float az = (float)((360.0 - 315.0 + 90.0) * DEG2RAD);
    const float alt = 35.0f * DEG2RAD;
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
                         float maxDistPx, float stepPx)
{
    const float az = (float)((360.0 - 315.0 + 90.0) * DEG2RAD);
    float sx = std::cos(az);
    float syImage = -std::sin(az);
    float tanAlt = std::tan(35.0f * DEG2RAD);

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
    GaussianBlur(light, res, res, 0.8f);
    for (float& v : light) v = std::clamp(v, 0.0f, 1.0f);
    return light;
}

// ---------------------------------------------------------------------------
// WAC source (grayscale float, loaded once)
// ---------------------------------------------------------------------------

static Field g_wac;
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
        g_wac[i] = (r + g + b) / (3.0f * 255.0f);
    }
    UnloadImage(img);
    TraceLog(LOG_INFO, "TERRAIN: WAC loaded %dx%d", g_wacW, g_wacH);
    return true;
}

// Native-resolution crop of a window square in km (lon widened by
// 1/cos(lat)), denoised, then upsampled to res.
static Field CropMacro(double latDeg, double lonDeg, double spanDeg, int res)
{
    double c = std::max(0.2, std::cos(latDeg * DEG2RAD));
    double lonSpan = spanDeg / c;
    double lat0 = latDeg - spanDeg / 2.0, lat1 = latDeg + spanDeg / 2.0;
    double lon0 = lonDeg - lonSpan / 2.0, lon1 = lonDeg + lonSpan / 2.0;
    int y0 = std::max(0, (int)((90.0 - lat1) / 180.0 * g_wacH));
    int y1 = std::min(g_wacH, (int)((90.0 - lat0) / 180.0 * g_wacH) + 1);
    int x0 = (int)((lon0 + 180.0) / 360.0 * g_wacW);
    int x1 = (int)((lon1 + 180.0) / 360.0 * g_wacW) + 1;
    int cw = std::max(2, x1 - x0);
    int ch = std::max(2, y1 - y0);
    Field crop((size_t)cw * ch);
    for (int y = 0; y < ch; y++)
    {
        int wy = std::clamp(y0 + y, 0, g_wacH - 1);
        for (int x = 0; x < cw; x++)
        {
            int wx = ((x0 + x) % g_wacW + g_wacW) % g_wacW;
            crop[y * cw + x] = g_wac[(size_t)wy * g_wacW + wx];
        }
    }
    GaussianBlur(crop, cw, ch, 0.7f);         // denoise JPEG artifacts
    return ResizeBilinear(crop, cw, ch, res, res);
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
static void CarveSmallCraters(Field& height, int res, TerrainRng& rng,
                              int count, float rMinPx, float rMaxPx,
                              float depthScale)
{
    for (int c = 0; c < count; c++)
    {
        float cx = rng.Uniform() * res;
        float cy = rng.Uniform() * res;
        // Power-law-ish size mix: most craters small, a few large.
        float u = rng.Uniform();
        float r = rMinPx * std::pow(rMaxPx / rMinPx,
                                    std::pow(u, 2.2f));
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

// Boulder speckle: tiny sharp bumps; the shared relighting gives each
// one its lit face and cast-shadow pixel automatically.
static void SprinkleBoulders(Field& height, int res, TerrainRng& rng,
                             int count, float amp)
{
    for (int b = 0; b < count; b++)
    {
        int x = 1 + (int)(rng.Uniform() * (res - 2));
        int y = 1 + (int)(rng.Uniform() * (res - 2));
        float a = amp * (0.4f + rng.Uniform());
        height[y * res + x] += a;
        if (rng.Uniform() < 0.5f) height[y * res + x + 1] += a * 0.6f;
        if (rng.Uniform() < 0.3f) height[(y + 1) * res + x] += a * 0.5f;
    }
}

// The anti-matte relight + grain stage (port of _texture_modulate).
// craterCount/boulderCount > 0 adds sub-resolution surface features —
// only used on zoom levels below the real-data floor.
static void TextureModulate(Field& macro, int res, TerrainRng& rng, float amp,
                            int craterCount = 0, float craterRMinPx = 1.5f,
                            float craterRMaxPx = 12.0f,
                            float craterDepth = 0.015f,
                            int boulderCount = 0)
{
    // Pixel-based sizes below are tuned at 300 px; k rescales them so
    // physical feature sizes stay fixed at other resolutions.
    float k = res / 300.0f;
    Field density((size_t)res * res);
    for (size_t i = 0; i < density.size(); i++)
        density[i] = std::clamp((macro[i] - 0.22f) / 0.45f, 0.15f, 1.0f);

    // Height field: smoothed macro as relief proxy + grain + undulation
    Field height = macro;
    GaussianBlur(height, res, res, 2.5f * k);
    for (float& v : height) v = (v - 0.5f) * 0.13f;

    Field grain = GrainNoise(res, rng);
    Field undul = Fbm(res, 3, (int)(64 * k), 0.5f, rng);
    for (size_t i = 0; i < height.size(); i++)
    {
        float rough = 0.45f + 0.55f * density[i];
        height[i] += 0.004f * amp * grain[i] * rough;
        height[i] += 0.02f * amp * (undul[i] - 0.5f) * rough;
    }

    if (craterCount > 0)
        CarveSmallCraters(height, res, rng, craterCount,
                          craterRMinPx, craterRMaxPx, craterDepth);
    if (boulderCount > 0)
        SprinkleBoulders(height, res, rng, boulderCount, 0.010f);

    const float z = 110.0f;
    Field hs = Hillshade(height, res, z, 0.6f);
    float flatRef = std::sin(35.0f * DEG2RAD);
    Field light = CastShadows(height, res, z, 22.0f * k, 1.5f);

    TerrainRng rng2(rng.Next());
    Field speckle = Fbm(res, 2, 4, 0.5f, rng2);
    for (size_t i = 0; i < macro.size(); i++)
    {
        float rel = std::clamp(hs[i] / flatRef, 0.0f, 1.6f);
        float rough = 0.45f + 0.55f * density[i];
        float lum = macro[i] * (0.62f + 0.38f * rel)
                    * (0.45f + 0.55f * light[i]);
        lum *= 1.0f + 0.04f * std::min(amp, 1.6f)
                    * (speckle[i] - 0.5f) * rough;
        lum = std::clamp(lum, 0.0f, 1.0f);
        // Gentle S-curve: deepen shadows, keep highlights
        float s = lum * lum * (3.0f - 2.0f * lum);
        macro[i] = std::clamp(s * 0.20f + lum * 0.80f, 0.0f, 1.0f);
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

void TerrainGridCellToLatLon(int gx, int gy, double* latDeg, double* lonDeg)
{
    double cellDeg = TERRAIN_CELL_KM / MOON_KM_PER_DEG;   // 0.16489 deg
    // Grid centre is between cells 9 and 10; gy grows south.
    double offX = (gx - 9.5);
    double offY = (gy - 9.5);
    double lat = TERRAIN_ANCHOR_LAT - offY * cellDeg;
    double c = std::max(0.2, std::cos(TERRAIN_ANCHOR_LAT * DEG2RAD));
    double lon = TERRAIN_ANCHOR_LON + offX * cellDeg / c;
    *latDeg = lat;
    *lonDeg = lon;
}

Image GenerateSectTerrain(double latDeg, double lonDeg, int res)
{
    Image fallback = GenImageColor(res, res, Color{40, 40, 48, 255});
    if (!EnsureWacLoaded()) return fallback;

    double t0 = GetTime();

    // The game-view span ladder: PLANET 100 km -> COLONY 25 km ->
    // SECT 5 km. Each level crops the centre fraction of the previous
    // level's OUTPUT — real forms flow down; texture becomes structure.
    const double spans[3] = {100.0 / MOON_KM_PER_DEG,
                             25.0 / MOON_KM_PER_DEG,
                             5.0 / MOON_KM_PER_DEG};

    uint32_t seed = LocationSeed(latDeg, lonDeg);
    TerrainRng rng0(seed);
    Field lum = CropMacro(latDeg, lonDeg, spans[0], res);
    SharpenAdaptive(lum, res);
    TextureModulate(lum, res, rng0, 1.0f);

    for (int lvl = 1; lvl < 3; lvl++)
    {
        float frac = (float)(spans[lvl] / spans[lvl - 1]);
        float half = frac * res / 2.0f;
        int lo = (int)std::lround(res / 2.0f - half);
        int hi = std::max(lo + 2, (int)std::lround(res / 2.0f + half));
        int cw = hi - lo;
        Field crop((size_t)cw * cw);
        for (int y = 0; y < cw; y++)
            for (int x = 0; x < cw; x++)
                crop[y * cw + x] = lum[(size_t)(lo + y) * res + (lo + x)];
        float k = res / 300.0f;
        lum = ResizeBilinear(crop, cw, cw, res, res);
        GaussianBlur(lum, res, res, 0.6f * k);
        Field blur = lum;
        GaussianBlur(blur, res, res, 5.0f * k);
        for (size_t i = 0; i < lum.size(); i++)
            lum[i] = std::clamp(lum[i] + 0.40f * (lum[i] - blur[i]),
                                0.0f, 1.0f);
        TerrainRng rng(seed ^ (0x9E3779B9u * (uint32_t)lvl));
        if (lvl == 1)
        {
            // COLONY 25 km, 83 m/px: light sub-resolution cratering
            // (125-500 m bowls), no boulders yet.
            TextureModulate(lum, res, rng, 1.0f + 0.7f * lvl,
                            (int)(40 * k * k), 1.5f * k, 6.0f * k,
                            0.012f, 0);
        }
        else
        {
            // SECT 5 km, 17 m/px: the ground the player builds on —
            // dense small cratering (25-370 m) and boulder speckle.
            // Deep enough that the biggest bowls catch cast shadow.
            TextureModulate(lum, res, rng, 1.0f + 0.7f * lvl,
                            (int)(150 * k * k), 1.5f * k, 22.0f * k,
                            0.036f, (int)(230 * k * k));
        }
    }

    Image out = GenImageColor(res, res, BLACK);
    ImageFormat(&out, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color* px = (Color*)out.data;
    for (int i = 0; i < res * res; i++) px[i] = RampColor(lum[i]);

    UnloadImage(fallback);
    TraceLog(LOG_INFO,
             "TERRAIN: sect ground (%.3f, %.3f) generated in %.0f ms",
             latDeg, lonDeg, (GetTime() - t0) * 1000.0);
    return out;
}
