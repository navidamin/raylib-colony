// Real lunar elevation from the LOLA LDEM_16 model. See lola_dem.h.

#include "lola_dem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>

// MSVC does not expose M_PI without _USE_MATH_DEFINES; carry our own.
static const double LOLA_PI = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// Minimal TIFF reader
//
// Only what the CGI Moon Kit DEM needs: little-endian classic TIFF,
// compression 1 (none), one 16-bit unsigned sample per pixel, strip
// organised. Anything else is rejected loudly rather than misread.
// ---------------------------------------------------------------------------

namespace
{

struct TiffEntry
{
    uint16_t tag = 0;
    uint16_t type = 0;
    uint32_t count = 0;
    uint32_t value = 0;    // inline value or offset
};

uint16_t ReadU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t ReadU32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// TIFF type sizes we care about: SHORT=3 (2 bytes), LONG=4 (4 bytes).
uint32_t ReadArrayValue(const std::vector<uint8_t>& file,
                        const TiffEntry& e, uint32_t index)
{
    uint32_t elemSize = (e.type == 3) ? 2 : 4;
    uint32_t total = elemSize * e.count;
    const uint8_t* base;
    if (total <= 4)
    {
        // Values packed inline in the entry's value field. The entry
        // was parsed little-endian, so re-derive the raw bytes.
        static uint8_t inlineBytes[4];
        inlineBytes[0] = (uint8_t)(e.value & 0xFF);
        inlineBytes[1] = (uint8_t)((e.value >> 8) & 0xFF);
        inlineBytes[2] = (uint8_t)((e.value >> 16) & 0xFF);
        inlineBytes[3] = (uint8_t)((e.value >> 24) & 0xFF);
        base = inlineBytes;
    }
    else
    {
        base = file.data() + e.value;
    }
    const uint8_t* p = base + elemSize * index;
    return (e.type == 3) ? ReadU16(p) : ReadU32(p);
}

}    // namespace

// Parse a little-endian, uncompressed, strip-organised 16-bit TIFF
// into a raster. Shared by the global DEM and the overlay crops.
static bool ParseTiffU16(const std::string& path, int& outW, int& outH,
                         std::vector<uint16_t>& outRaw)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    if (file.size() < 8 || file[0] != 'I' || file[1] != 'I' ||
        ReadU16(file.data() + 2) != 42)
    {
        std::fprintf(stderr, "LolaDem: %s is not a little-endian TIFF\n",
                     path.c_str());
        return false;
    }

    uint32_t ifdOffset = ReadU32(file.data() + 4);
    if (ifdOffset + 2 > file.size())
    {
        std::fprintf(stderr, "LolaDem: bad IFD offset in %s\n", path.c_str());
        return false;
    }
    uint16_t entryCount = ReadU16(file.data() + ifdOffset);

    TiffEntry stripOffsets, stripCounts;
    int width = 0, height = 0;
    uint32_t bits = 0, compression = 1, samplesPerPixel = 1, rowsPerStrip = 0;
    for (uint16_t i = 0; i < entryCount; i++)
    {
        const uint8_t* p = file.data() + ifdOffset + 2 + 12 * i;
        TiffEntry e;
        e.tag = ReadU16(p);
        e.type = ReadU16(p + 2);
        e.count = ReadU32(p + 4);
        e.value = ReadU32(p + 8);
        switch (e.tag)
        {
            case 256: width = (int)e.value; break;
            case 257: height = (int)e.value; break;
            case 258: bits = e.value; break;
            case 259: compression = e.value; break;
            case 273: stripOffsets = e; break;
            case 277: samplesPerPixel = e.value; break;
            case 278: rowsPerStrip = e.value; break;
            case 279: stripCounts = e; break;
            default: break;
        }
    }

    if (bits != 16 || compression != 1 || samplesPerPixel != 1 ||
        width <= 0 || height <= 0 || stripOffsets.count == 0)
    {
        std::fprintf(stderr,
                     "LolaDem: %s unsupported layout (bits=%u comp=%u "
                     "spp=%u %dx%d)\n",
                     path.c_str(), bits, compression, samplesPerPixel,
                     width, height);
        return false;
    }
    if (rowsPerStrip == 0) rowsPerStrip = (uint32_t)height;

    outRaw.assign((size_t)width * height, 0);
    uint32_t row = 0;
    for (uint32_t s = 0; s < stripOffsets.count && row < (uint32_t)height; s++)
    {
        uint32_t offset = ReadArrayValue(file, stripOffsets, s);
        uint32_t bytes = ReadArrayValue(file, stripCounts, s);
        uint32_t rows = std::min(rowsPerStrip, (uint32_t)height - row);
        uint32_t expect = rows * (uint32_t)width * 2;
        if (bytes < expect || offset + expect > file.size())
        {
            std::fprintf(stderr, "LolaDem: truncated strip %u in %s\n",
                         s, path.c_str());
            return false;
        }
        const uint8_t* src = file.data() + offset;
        uint16_t* dst = outRaw.data() + (size_t)row * width;
        for (uint32_t k = 0; k < rows * (uint32_t)width; k++)
        {
            dst[k] = ReadU16(src + 2 * k);
        }
        row += rows;
    }
    outW = width;
    outH = height;
    return true;
}

bool LolaDem::Load(const std::string& path)
{
    if (!ParseTiffU16(path, width, height, raw))
    {
        std::fprintf(stderr,
                     "LolaDem: %s missing or unreadable — run the fetch-dem "
                     "workflow (push a change to data/lola/REQUEST) and "
                     "pull.\n", path.c_str());
        width = height = 0;
        return false;
    }
    return true;
}

// Pull one numeric field out of a sidecar JSON without a JSON library:
// the sidecars are machine-written, flat, and tiny.
static bool JsonNumber(const std::string& text, const char* key,
                       double* out)
{
    std::string needle = std::string("\"") + key + "\":";
    size_t p = text.find(needle);
    if (p == std::string::npos) return false;
    *out = std::atof(text.c_str() + p + needle.size());
    return true;
}

int LolaDem::LoadOverlays(const std::string& dir)
{
    int loaded = 0;
    // The fetch-dem naming convention is fixed; scan for the sidecars
    // by trying REQUEST-style names is fragile, so instead read the
    // directory (C++17).
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        std::string path = entry.path().string();
        size_t n = path.size();
        if (n < 5 || path.compare(n - 5, 5, ".json") != 0) continue;
        std::string base = entry.path().filename().string();
        if (base.rfind("sldem_", 0) != 0) continue;

        std::ifstream jin(path);
        if (!jin) continue;
        std::string text((std::istreambuf_iterator<char>(jin)),
                         std::istreambuf_iterator<char>());
        DemOverlay ov;
        if (!JsonNumber(text, "lat0", &ov.lat0) ||
            !JsonNumber(text, "lat1", &ov.lat1) ||
            !JsonNumber(text, "lon0", &ov.lon0) ||
            !JsonNumber(text, "lon1", &ov.lon1))
        {
            std::fprintf(stderr, "LolaDem: %s missing bounds, skipped\n",
                         path.c_str());
            continue;
        }
        std::string tif = path.substr(0, n - 5) + ".tif";
        if (!ParseTiffU16(tif, ov.width, ov.height, ov.raw)) continue;
        std::fprintf(stderr,
                     "LolaDem: overlay %s %dx%d (%.2f..%.2f, %.2f..%.2f)\n",
                     base.c_str(), ov.width, ov.height,
                     ov.lat0, ov.lat1, ov.lon0, ov.lon1);
        overlays.push_back(std::move(ov));
        loaded++;
    }
    return loaded;
}

float LolaDem::OverlaySample(const DemOverlay& ov, double latDeg,
                             double lonDeg)
{
    double x = (lonDeg - ov.lon0) / (ov.lon1 - ov.lon0) * ov.width - 0.5;
    double y = (ov.lat1 - latDeg) / (ov.lat1 - ov.lat0) * ov.height - 0.5;
    int x0 = std::clamp((int)std::floor(x), 0, ov.width - 2);
    int y0 = std::clamp((int)std::floor(y), 0, ov.height - 2);
    float fx = std::clamp((float)(x - x0), 0.0f, 1.0f);
    float fy = std::clamp((float)(y - y0), 0.0f, 1.0f);
    const uint16_t* r = ov.raw.data();
    auto dec = [](uint16_t v) { return (float)v * 0.5f - 10000.0f; };
    float q00 = dec(r[(size_t)y0 * ov.width + x0]);
    float q10 = dec(r[(size_t)y0 * ov.width + x0 + 1]);
    float q01 = dec(r[(size_t)(y0 + 1) * ov.width + x0]);
    float q11 = dec(r[(size_t)(y0 + 1) * ov.width + x0 + 1]);
    float top = q00 * (1.0f - fx) + q10 * fx;
    float bot = q01 * (1.0f - fx) + q11 * fx;
    return top * (1.0f - fy) + bot * fy;
}

const LolaDem::DemOverlay* LolaDem::OverlayFor(double latDeg, double lonDeg,
                                               float* feather) const
{
    const double FEATHER_DEG = 0.12;    // blend band inside the boundary
    for (const DemOverlay& ov : overlays)
    {
        if (latDeg < ov.lat0 || latDeg > ov.lat1 ||
            lonDeg < ov.lon0 || lonDeg > ov.lon1) continue;
        double edge = std::min(
            std::min(latDeg - ov.lat0, ov.lat1 - latDeg),
            std::min(lonDeg - ov.lon0, ov.lon1 - lonDeg));
        if (feather)
        {
            *feather = (float)std::clamp(edge / FEATHER_DEG, 0.0, 1.0);
        }
        return &ov;
    }
    if (feather) *feather = 0.0f;
    return nullptr;
}

double LolaDem::NativeKmAt(double latDeg, double lonDeg) const
{
    const DemOverlay* ov = OverlayFor(latDeg, lonDeg, nullptr);
    if (ov != nullptr)
    {
        double ppd = ov->height / (ov->lat1 - ov->lat0);
        return LOLA_M_PER_DEG / ppd / 1000.0;
    }
    return LOLA_M_PER_DEG / (width / 360.0) / 1000.0;
}

float LolaDem::Sample(int x, int y) const
{
    // Longitude wraps, latitude clamps at the poles.
    x = ((x % width) + width) % width;
    y = std::clamp(y, 0, height - 1);
    return Decode(raw[(size_t)y * width + x]);
}

float LolaDem::GlobalElevationM(double latDeg, double lonDeg) const
{
    double x = (lonDeg + 180.0) / 360.0 * width;
    double y = (90.0 - latDeg) / 180.0 * height;
    int x0 = (int)std::floor(x);
    int y0 = (int)std::floor(y);
    float fx = (float)(x - x0);
    float fy = (float)(y - y0);
    float q00 = Sample(x0, y0), q10 = Sample(x0 + 1, y0);
    float q01 = Sample(x0, y0 + 1), q11 = Sample(x0 + 1, y0 + 1);
    float top = q00 * (1.0f - fx) + q10 * fx;
    float bot = q01 * (1.0f - fx) + q11 * fx;
    return top * (1.0f - fy) + bot * fy;
}

float LolaDem::ElevationM(double latDeg, double lonDeg) const
{
    float feather = 0.0f;
    const DemOverlay* ov = OverlayFor(latDeg, lonDeg, &feather);
    float global = GlobalElevationM(latDeg, lonDeg);
    if (ov == nullptr || feather <= 0.0f) return global;
    float fine = OverlaySample(*ov, latDeg, lonDeg);
    // Feather to the global DEM at the overlay boundary — a hard edge
    // would shade as a cliff.
    return global + (fine - global) * feather;
}

// Bilinear resample of a float field (w x h) to (outW x outH).
static std::vector<float> Resample(const std::vector<float>& src,
                                   int w, int h, int outW, int outH)
{
    std::vector<float> dst((size_t)outW * outH);
    for (int oy = 0; oy < outH; oy++)
    {
        double sy = (outH > 1) ? (double)oy / (outH - 1) * (h - 1) : 0.0;
        int y0 = std::min((int)sy, h - 2);
        if (h == 1) y0 = 0;
        float fy = (float)(sy - y0);
        for (int ox = 0; ox < outW; ox++)
        {
            double sx = (outW > 1) ? (double)ox / (outW - 1) * (w - 1) : 0.0;
            int x0 = std::min((int)sx, w - 2);
            if (w == 1) x0 = 0;
            float fx = (float)(sx - x0);
            const float* p0 = src.data() + (size_t)y0 * w + x0;
            const float* p1 = (h == 1) ? p0 : p0 + w;
            float x1 = (w == 1) ? p0[0] : p0[1];
            float x1b = (w == 1) ? p1[0] : p1[1];
            float top = p0[0] * (1.0f - fx) + x1 * fx;
            float bot = p1[0] * (1.0f - fx) + x1b * fx;
            dst[(size_t)oy * outW + ox] = top * (1.0f - fy) + bot * fy;
        }
    }
    return dst;
}

// Separable gaussian blur, clamped edges. Used to smooth upsampled
// windows: differentiating a bilinear upsample (for shading normals)
// otherwise shows the native texel lattice as a grid artifact.
static std::vector<float> GaussKernel(float sigma)
{
    int radius = (int)std::ceil(2.0f * sigma);
    std::vector<float> kernel(2 * radius + 1);
    float sum = 0.0f;
    for (int i = -radius; i <= radius; i++)
    {
        kernel[i + radius] = std::exp(-(float)(i * i) /
                                      (2.0f * sigma * sigma));
        sum += kernel[i + radius];
    }
    for (float& k : kernel) k /= sum;
    return kernel;
}

// Anisotropic: near the poles a window can be upsampled far more in
// one axis than the other, so each axis gets its own sigma.
static void GaussianBlur(std::vector<float>& field, int w, int h,
                         float sigmaX, float sigmaY)
{
    std::vector<float> tmp((size_t)w * h);
    if (sigmaX > 0.25f)
    {
        std::vector<float> kernel = GaussKernel(sigmaX);
        int radius = (int)kernel.size() / 2;
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float acc = 0.0f;
                for (int i = -radius; i <= radius; i++)
                {
                    int sx = std::clamp(x + i, 0, w - 1);
                    acc += field[(size_t)y * w + sx] * kernel[i + radius];
                }
                tmp[(size_t)y * w + x] = acc;
            }
        }
        field.swap(tmp);
    }
    if (sigmaY > 0.25f)
    {
        std::vector<float> kernel = GaussKernel(sigmaY);
        int radius = (int)kernel.size() / 2;
        for (int x = 0; x < w; x++)
        {
            for (int y = 0; y < h; y++)
            {
                float acc = 0.0f;
                for (int i = -radius; i <= radius; i++)
                {
                    int sy = std::clamp(y + i, 0, h - 1);
                    acc += field[(size_t)sy * w + x] * kernel[i + radius];
                }
                tmp[(size_t)y * w + x] = acc;
            }
        }
        field.swap(tmp);
    }
}

// Crop a native-resolution rectangle (may wrap in longitude), then
// compute slope at native scale before any resampling.
LolaWindow LolaDem::WindowDegrees(double lat0, double lat1,
                                 double lon0, double lon1,
                                 int outW, int outH) const
{
    LolaWindow out;
    if (!IsLoaded()) return out;

    int y0 = std::max(0, (int)std::floor((90.0 - lat1) / 180.0 * height));
    int y1 = std::min(height, (int)std::ceil((90.0 - lat0) / 180.0 * height) + 1);
    int x0 = (int)std::floor((lon0 + 180.0) / 360.0 * width);
    int x1 = (int)std::ceil((lon1 + 180.0) / 360.0 * width) + 1;
    int cw = x1 - x0;
    int ch = y1 - y0;
    if (cw < 2 || ch < 2) return out;

    std::vector<float> elev((size_t)cw * ch);
    for (int y = 0; y < ch; y++)
    {
        for (int x = 0; x < cw; x++)
        {
            elev[(size_t)y * cw + x] = Sample(x0 + x, y0 + y);
        }
    }

    // Physical pixel sizes at the window's centre latitude.
    double midLat = (lat0 + lat1) / 2.0;
    double c = std::max(0.2, std::cos(midLat * LOLA_PI / 180.0));
    double pxPerDeg = width / 360.0;
    double dyM = LOLA_M_PER_DEG / pxPerDeg;
    double dxM = dyM * c;

    // Central-difference slope at native resolution (np.gradient
    // convention: one-sided at the edges).
    std::vector<float> slope((size_t)cw * ch);
    for (int y = 0; y < ch; y++)
    {
        int ym = std::max(0, y - 1), yp = std::min(ch - 1, y + 1);
        double sy = dyM * (yp - ym);
        for (int x = 0; x < cw; x++)
        {
            int xm = std::max(0, x - 1), xp = std::min(cw - 1, x + 1);
            double sx = dxM * (xp - xm);
            double gy = (elev[(size_t)yp * cw + x] - elev[(size_t)ym * cw + x]) / sy;
            double gx = (elev[(size_t)y * cw + xp] - elev[(size_t)y * cw + xm]) / sx;
            slope[(size_t)y * cw + x] =
                (float)(std::atan(std::hypot(gx, gy)) * 180.0 / LOLA_PI);
        }
    }

    out.resolution = outW;
    out.latDeg = midLat;
    out.lonDeg = (lon0 + lon1) / 2.0;
    out.spanKm = (lat1 - lat0) * LOLA_M_PER_DEG / 1000.0;
    out.elevationM = Resample(elev, cw, ch, outW, outH);
    out.slopeDeg = Resample(slope, cw, ch, outW, outH);
    // Smooth away the bilinear lattice when the window is upsampled
    // past the DEM's native resolution (per axis: polar windows can be
    // stretched far more vertically than horizontally).
    float upX = (float)outW / cw;
    float upY = (float)outH / ch;
    GaussianBlur(out.elevationM, outW, outH,
                 (upX > 1.5f) ? 0.6f * upX : 0.0f,
                 (upY > 1.5f) ? 0.6f * upY : 0.0f);
    auto mm = std::minmax_element(out.elevationM.begin(), out.elevationM.end());
    out.minElevationM = *mm.first;
    out.maxElevationM = *mm.second;
    return out;
}

// ---------------------------------------------------------------------------
// Detail synthesis below the DEM floor
//
// LDEM_16 resolves nothing under ~1.9 km/px, so zoomed windows come out
// soft. Below that floor we synthesize plausible lunar ground: a
// fractal regolith spectrum plus a scattered small-crater population
// (power-law sizes, parabolic bowls with raised rims). Everything is a
// pure function of global coordinates — quantized lattice cells in a
// km-scaled lat/lon frame — so the same location regenerates the same
// ground for any window centre, span or resolution. Amplitude fades to
// zero at wavelengths the real data already carries; the LOLA
// landforms stay the backbone and are never displaced, only textured.
// ---------------------------------------------------------------------------

static uint32_t DetailHash(int32_t x, int32_t y, uint32_t salt)
{
    uint32_t h = (uint32_t)x * 0x8da6b343u ^ (uint32_t)y * 0xd8163841u ^
                 salt * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x9e3779b1u; h ^= h >> 16;
    return h;
}

static float DetailHash01(int32_t x, int32_t y, uint32_t salt)
{
    return (float)(DetailHash(x, y, salt) & 0xFFFFFF) / 16777215.0f;
}

// Single-octave value noise on a lattice of `waveKm`, smoothstepped.
static float DetailNoise(double u, double v, double waveKm, uint32_t salt)
{
    double gu = u / waveKm, gv = v / waveKm;
    int32_t x0 = (int32_t)std::floor(gu), y0 = (int32_t)std::floor(gv);
    float fx = (float)(gu - x0), fy = (float)(gv - y0);
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float n00 = DetailHash01(x0, y0, salt);
    float n10 = DetailHash01(x0 + 1, y0, salt);
    float n01 = DetailHash01(x0, y0 + 1, salt);
    float n11 = DetailHash01(x0 + 1, y0 + 1, salt);
    float top = n00 + (n10 - n00) * fx;
    float bot = n01 + (n11 - n01) * fx;
    return (top + (bot - top) * fy) * 2.0f - 1.0f;    // -1..1
}

// Crater contribution at (u, v) from the jittered-grid population of
// one size band (cells of `cellKm`). Degraded bowls: parabolic floor,
// gaussian rim. Returns metres.
static float DetailCraters(double u, double v, double cellKm, uint32_t salt)
{
    int32_t cx = (int32_t)std::floor(u / cellKm);
    int32_t cy = (int32_t)std::floor(v / cellKm);
    // Crater fields cluster: a slow density modulation keeps some
    // patches busy and leaves others nearly clean, instead of the
    // uniform bubble-wrap a constant occupancy produces.
    float cluster = 0.5f + 0.5f * DetailNoise(u, v, cellKm * 9.0,
                                              salt + 900u);
    float occupancy = 0.05f + 0.28f * cluster * cluster;
    float h = 0.0f;
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            int32_t gx = cx + dx, gy = cy + dy;
            if (DetailHash01(gx, gy, salt) > occupancy) continue;
            double px = (gx + 0.15 + 0.7 * DetailHash01(gx, gy, salt + 1)) *
                        cellKm;
            double py = (gy + 0.15 + 0.7 * DetailHash01(gx, gy, salt + 2)) *
                        cellKm;
            double diamKm = cellKm * (0.22 + 0.65 *
                                      DetailHash01(gx, gy, salt + 3));
            // Slightly elliptical, so the population is not a field of
            // perfect circles.
            double ex = 1.0 + 0.18 * (DetailHash01(gx, gy, salt + 5) - 0.5);
            double r = std::hypot((u - px) * ex, (v - py) / ex) /
                       (diamKm * 0.5);
            if (r >= 1.5) continue;
            // Age: billions of years of gardening leave most small
            // craters as shallow, soft, rimless dishes — only a rare
            // fresh few keep a deep bowl and a raised rim.
            float age = DetailHash01(gx, gy, salt + 4);
            float freshness = age * age * age;
            float depthM = (float)(diamKm * 1000.0) *
                           (0.055f + 0.075f * freshness);
            if (r < 1.0)
            {
                // Cosine dish: smooth at the centre AND at the edge, so
                // no embossed crest ring where the bowl meets the ground.
                h -= depthM * 0.5f *
                     (1.0f + std::cos((float)r * 3.14159265f));
            }
            float rimM = depthM * 0.30f * freshness;
            if (rimM > 0.001f)
            {
                float rimT = (float)(r - 1.02) / 0.24f;
                h += rimM * std::exp(-rimT * rimT * 4.0f);
            }
        }
    }
    return h;
}

// Total synthetic relief (metres) at one global point. `pixKm` bounds
// the finest band (nothing under ~2 output pixels — it would alias),
// `nativeKm` is the resolution floor of the real data underfoot (the
// synthesis only fades in below what that data resolves), and
// `slopeBoost` roughens crater walls relative to maria.
static float SynthesizeDetail(double u, double v, float pixKm,
                              float nativeKm, float strength,
                              float slopeBoost)
{
    float total = 0.0f;
    double wave = nativeKm * 4.0;          // fade-in starts here
    for (int level = 0; level < 16 && wave >= 2.0 * pixKm; level++)
    {
        // 0 where the DEM already resolves this wavelength, 1 below it.
        float fade = (float)std::clamp(
            (nativeKm * 4.0 - wave) / (nativeKm * 3.0),
            0.0, 1.0);
        if (fade > 0.0f)
        {
            // Fractal regolith undulation.
            float amp = 5.0f * (float)wave;    // metres per km wavelength
            float band = amp * (0.6f + 0.4f * slopeBoost) *
                         DetailNoise(u, v, wave, 0x51u + (uint32_t)level);
            // Hummocky ground: half-rectified noise reads as soft
            // mounds and rock lumps scattered on the plain, not as
            // symmetric static.
            float lumpN = DetailNoise(u, v, wave * 0.7,
                                      0xB00Bu + (uint32_t)level);
            band += 6.5f * (float)wave * std::max(0.0f, lumpN);
            // Craters only above ~a 30-100 m floor — smaller ones read
            // as noise speckle, not landforms.
            if (wave >= 0.12)
            {
                band += DetailCraters(u, v, wave,
                                      0xC7A7E5u + (uint32_t)level * 7u);
            }
            total += fade * band;
        }
        wave *= 0.5;
    }
    return strength * total;
}

// Regional windows sample through an azimuthal equidistant projection
// centred on the pick: every output pixel is a true ground offset in
// km, so windows stay square-in-km at any latitude — including the
// poles, where an equirectangular crop degenerates into a smear.
LolaWindow LolaDem::Window(double latDeg, double lonDeg, double spanKm,
                           int res, float detailStrength) const
{
    LolaWindow out;
    if (!IsLoaded() || res < 2) return out;
    out.resolution = res;
    out.latDeg = latDeg;
    out.lonDeg = lonDeg;
    out.spanKm = spanKm;
    out.elevationM.resize((size_t)res * res);
    out.slopeDeg.resize((size_t)res * res);

    const double DEG = LOLA_PI / 180.0;
    double lat0 = latDeg * DEG;
    double lon0 = lonDeg * DEG;
    double halfM = spanKm * 1000.0 / 2.0;
    // Global km-scaled coordinates per pixel, kept for the synthesis
    // pass: quantizing these (not window-local ones) is what makes the
    // synthetic ground identical across window framings.
    std::vector<double> gu, gv;
    if (detailStrength > 0.0f)
    {
        gu.resize((size_t)res * res);
        gv.resize((size_t)res * res);
    }
    const double kmPerDeg = LOLA_M_PER_DEG / 1000.0;
    for (int j = 0; j < res; j++)
    {
        // Row 0 is the window's northern edge.
        double y = halfM - (double)j / (res - 1) * spanKm * 1000.0;
        for (int i = 0; i < res; i++)
        {
            double x = -halfM + (double)i / (res - 1) * spanKm * 1000.0;
            double rho = std::hypot(x, y);
            double lat, lon;
            if (rho < 1.0)
            {
                lat = lat0;
                lon = lon0;
            }
            else
            {
                double c = rho / LOLA_MOON_RADIUS_M;    // angular distance
                double sinc = std::sin(c), cosc = std::cos(c);
                lat = std::asin(cosc * std::sin(lat0) +
                                y * sinc * std::cos(lat0) / rho);
                lon = lon0 + std::atan2(
                    x * sinc,
                    rho * cosc * std::cos(lat0) -
                        y * sinc * std::sin(lat0));
            }
            out.elevationM[(size_t)j * res + i] =
                ElevationM(lat / DEG, lon / DEG);
            if (detailStrength > 0.0f)
            {
                gu[(size_t)j * res + i] =
                    (lon / DEG) * kmPerDeg * std::cos(lat);
                gv[(size_t)j * res + i] = (lat / DEG) * kmPerDeg;
            }
        }
    }

    // Anti-lattice smoothing, matched to how far the output oversamples
    // the native spacing of the finest data covering the window centre
    // (per axis: global-DEM columns shrink with cos lat; the overlay
    // crops are near-square already).
    double outKm = spanKm / res;
    // The fine native applies only when the WHOLE window sits on an
    // overlay — keying a part-covered window to the fine data would
    // leave the coarse outskirts under-smoothed (lattice artifacts).
    double spanDeg = spanKm * 1000.0 / LOLA_M_PER_DEG;
    double cLat = std::max(0.2, std::cos(lat0));
    bool onOverlay = true;
    for (int cy = -1; cy <= 1 && onOverlay; cy += 2)
    {
        for (int cx = -1; cx <= 1; cx += 2)
        {
            if (OverlayFor(latDeg + cy * spanDeg / 2.0,
                           lonDeg + cx * spanDeg / (2.0 * cLat),
                           nullptr) == nullptr)
            {
                onOverlay = false;
                break;
            }
        }
    }
    double nativeYKm = onOverlay
        ? NativeKmAt(latDeg, lonDeg)
        : LOLA_M_PER_DEG / (width / 360.0) / 1000.0;
    double nativeXKm = onOverlay ? nativeYKm : nativeYKm * cLat;
    float upX = (float)(nativeXKm / outKm);
    float upY = (float)(nativeYKm / outKm);
    GaussianBlur(out.elevationM, res, res,
                 (upX > 1.5f) ? 0.6f * upX : 0.0f,
                 (upY > 1.5f) ? 0.6f * upY : 0.0f);

    // Synthesize sub-floor detail on top of the (smoothed) real ground.
    // Runs after the blur so it is not smoothed away; the real slope of
    // the smoothed field roughens the synthesis on crater walls.
    if (detailStrength > 0.0f)
    {
        float pixKm = (float)(spanKm / res);
        double dMs = spanKm * 1000.0 / (res - 1);
        // Separate buffer: slopeBoost must read the pristine real
        // field, not rows already carrying synthetic relief.
        std::vector<float> detail((size_t)res * res);
        for (int j = 0; j < res; j++)
        {
            int jm = std::max(0, j - 1), jp = std::min(res - 1, j + 1);
            for (int i = 0; i < res; i++)
            {
                int im = std::max(0, i - 1), ip = std::min(res - 1, i + 1);
                size_t k = (size_t)j * res + i;
                double sgx = (out.elevationM[(size_t)j * res + ip] -
                              out.elevationM[(size_t)j * res + im]) /
                             (dMs * (ip - im));
                double sgy = (out.elevationM[(size_t)jp * res + i] -
                              out.elevationM[(size_t)jm * res + i]) /
                             (dMs * (jp - jm));
                // 0 on level maria, ~1 on steep real walls (>= ~8 deg).
                float slopeBoost = (float)std::min(
                    1.0, std::hypot(sgx, sgy) / 0.14);
                detail[k] = SynthesizeDetail(
                    gu[k], gv[k], pixKm, (float)nativeYKm,
                    detailStrength, slopeBoost);
            }
        }
        for (size_t k = 0; k < detail.size(); k++)
        {
            out.elevationM[k] += detail[k];
        }
    }

    // Slope straight off the (smoothed) output grid — uniform metric
    // spacing is the point of the projection.
    double dM = spanKm * 1000.0 / (res - 1);
    for (int j = 0; j < res; j++)
    {
        int jm = std::max(0, j - 1), jp = std::min(res - 1, j + 1);
        for (int i = 0; i < res; i++)
        {
            int im = std::max(0, i - 1), ip = std::min(res - 1, i + 1);
            double gx = (out.elevationM[(size_t)j * res + ip] -
                         out.elevationM[(size_t)j * res + im]) /
                        (dM * (ip - im));
            double gy = (out.elevationM[(size_t)jp * res + i] -
                         out.elevationM[(size_t)jm * res + i]) /
                        (dM * (jp - jm));
            out.slopeDeg[(size_t)j * res + i] =
                (float)(std::atan(std::hypot(gx, gy)) / DEG);
        }
    }
    auto mm = std::minmax_element(out.elevationM.begin(),
                                  out.elevationM.end());
    out.minElevationM = *mm.first;
    out.maxElevationM = *mm.second;
    return out;
}
