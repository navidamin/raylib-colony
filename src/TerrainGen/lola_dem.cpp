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

// Catmull-Rom: a smooth curve through the four samples around the
// interval between p1 and p2. Unlike a linear ramp its slope varies
// continuously, so upsampled windows need no anti-lattice blur — the
// single biggest source of the out-of-focus look bilinear had.
static float CatmullRom(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

// Uniform cubic B-spline: approximates (does not pass through) the
// points — the smoothest reconstruction, zero overshoot.
static float BSpline(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return ((1.0f - 3.0f * t + 3.0f * t2 - t3) * p0 +
            (4.0f - 6.0f * t2 + 3.0f * t3) * p1 +
            (1.0f + 3.0f * t + 3.0f * t2 - 3.0f * t3) * p2 +
            t3 * p3) / 6.0f;
}

// Lanczos-3 windowed sinc over 6 taps: preserves the most in-band
// frequency content of any practical filter (crispest, mild ringing).
static float Lanczos6(const float* p, float t)
{
    float acc = 0.0f, wsum = 0.0f;
    for (int k = -2; k <= 3; k++)
    {
        float x = (float)k - t;
        float w;
        if (std::fabs(x) < 1e-5f) w = 1.0f;
        else
        {
            float pix = 3.14159265f * x;
            w = 3.0f * std::sin(pix) * std::sin(pix / 3.0f) / (pix * pix);
        }
        acc += w * p[k + 2];
        wsum += w;
    }
    return acc / wsum;
}

static LolaInterp g_interp = LolaInterp::CATROM;

void LolaSetInterpolation(LolaInterp mode) { g_interp = mode; }

// One reconstruction step over a 6-tap row (taps at offsets -2..3
// around the interval [0,1] between taps 2 and 3 of p).
static float Interp1D(const float* p, float t)
{
    switch (g_interp)
    {
        case LolaInterp::BSPLINE: return BSpline(p[1], p[2], p[3], p[4], t);
        case LolaInterp::LANCZOS: return Lanczos6(p, t);
        default: return CatmullRom(p[1], p[2], p[3], p[4], t);
    }
}



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

// Remove SLDEM2015's coherent along-track striping (Kaguya TC seams):
// a stripe is a whole column (or row) offset by a fraction of a metre
// to a few metres. Averaging each line over the full crop isolates
// that offset from real terrain (which averages out over thousands of
// samples); subtracting the high-pass of the line means kills the
// stripes while leaving genuine regional slope untouched. Runs once at
// load, in half-metre integer units.
static void DestripeLines(std::vector<double>& mean, int n)
{
    // High-pass: line mean minus a ~31-sample smoothed version.
    std::vector<double> smooth(n);
    const int r = 15;
    for (int i = 0; i < n; i++)
    {
        double acc = 0.0;
        int cnt = 0;
        for (int k = -r; k <= r; k++)
        {
            int idx = std::clamp(i + k, 0, n - 1);
            acc += mean[idx];
            cnt++;
        }
        smooth[i] = acc / cnt;
    }
    for (int i = 0; i < n; i++) mean[i] -= smooth[i];
}

static bool g_despeckle = true;

void LolaSetDespeckle(bool enabled) { g_despeckle = enabled; }

void LolaDem::DestripeOverlay(DemOverlay& ov)
{
    int w = ov.width, h = ov.height;
    // 3x3 median despeckle at NATIVE resolution first: SLDEM carries
    // per-pixel stereo-correlation noise (Kaguya TC matching), which a
    // slope-continuous upsample faithfully magnifies into rectilinear
    // crunch. A median kills single-pixel outliers but keeps edges —
    // unlike blurring after the upsample, which smeared everything.
    if (g_despeckle)
    {
        std::vector<uint16_t> src = ov.raw;
        for (int y = 0; y < h; y++)
        {
            int ym = std::max(0, y - 1), yp = std::min(h - 1, y + 1);
            for (int x = 0; x < w; x++)
            {
                int xm = std::max(0, x - 1), xp = std::min(w - 1, x + 1);
                uint16_t v[9] = {
                    src[(size_t)ym * w + xm], src[(size_t)ym * w + x],
                    src[(size_t)ym * w + xp], src[(size_t)y * w + xm],
                    src[(size_t)y * w + x],   src[(size_t)y * w + xp],
                    src[(size_t)yp * w + xm], src[(size_t)yp * w + x],
                    src[(size_t)yp * w + xp] };
                std::nth_element(v, v + 4, v + 9);
                ov.raw[(size_t)y * w + x] = v[4];
            }
        }
    }
    std::vector<double> colMean(w, 0.0), rowMean(h, 0.0);
    for (int y = 0; y < h; y++)
    {
        const uint16_t* row = ov.raw.data() + (size_t)y * w;
        double acc = 0.0;
        for (int x = 0; x < w; x++)
        {
            acc += row[x];
            colMean[x] += row[x];
        }
        rowMean[y] = acc / w;
    }
    for (int x = 0; x < w; x++) colMean[x] /= h;
    DestripeLines(colMean, w);
    DestripeLines(rowMean, h);
    for (int y = 0; y < h; y++)
    {
        uint16_t* row = ov.raw.data() + (size_t)y * w;
        for (int x = 0; x < w; x++)
        {
            double v = row[x] - colMean[x] - rowMean[y];
            row[x] = (uint16_t)std::clamp(v + 0.5, 0.0, 65535.0);
        }
    }
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
        DestripeOverlay(ov);
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
    int x1 = (int)std::floor(x);
    int y1 = (int)std::floor(y);
    float fx = std::clamp((float)(x - x1), 0.0f, 1.0f);
    float fy = std::clamp((float)(y - y1), 0.0f, 1.0f);
    const uint16_t* r = ov.raw.data();
    auto dec = [&](int xx, int yy)
    {
        xx = std::clamp(xx, 0, ov.width - 1);
        yy = std::clamp(yy, 0, ov.height - 1);
        return (float)r[(size_t)yy * ov.width + xx] * 0.5f - 10000.0f;
    };
    float rows[6], taps[6];
    for (int j = 0; j < 6; j++)
    {
        int yy = y1 - 2 + j;
        for (int i = 0; i < 6; i++) taps[i] = dec(x1 - 2 + i, yy);
        rows[j] = Interp1D(taps, fx);
    }
    return Interp1D(rows, fy);
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
    double x = (lonDeg + 180.0) / 360.0 * width - 0.5;
    double y = (90.0 - latDeg) / 180.0 * height - 0.5;
    int x1 = (int)std::floor(x);
    int y1 = (int)std::floor(y);
    float fx = (float)(x - x1);
    float fy = (float)(y - y1);
    float rows[6], taps[6];
    for (int j = 0; j < 6; j++)
    {
        int yy = y1 - 2 + j;
        for (int i = 0; i < 6; i++) taps[i] = Sample(x1 - 2 + i, yy);
        rows[j] = Interp1D(taps, fx);
    }
    return Interp1D(rows, fy);
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
// synthesis only fades in below what that data resolves). `roughM` is
// the RMS relief the REAL data actually carries over one native
// sample right here, and `hurst` the exponent of its measured
// power law — together they continue the ground's own spectrum below
// the data floor instead of guessing an amplitude.
static LolaTexture g_texture = LolaTexture::NOISE;

void LolaSetTextureMode(LolaTexture mode) { g_texture = mode; }

// A SATURATED impact population, used as the primary relief rather than
// as decoration on a noise carpet. Two things make this read as ground
// rather than as brushwork:
//   - the surface is mostly flat and interrupted by discrete objects,
//     which is what the real Moon is at 10-500 m; a noise field is the
//     opposite (everywhere undulating, nothing anywhere to lock onto)
//   - depth is set by the crater's own diameter (d/D 0.03 ancient to
//     0.20 fresh), so it is physically absolute, not spectrally scaled
// Later impacts erase earlier ones, so the deepest bowl wins rather
// than bowls summing — without that, a saturated field digs runaway
// pits wherever craters overlap.
static float DetailCraterField(double u, double v, double cellKm,
                               uint32_t salt)
{
    int32_t cx = (int32_t)std::floor(u / cellKm);
    int32_t cy = (int32_t)std::floor(v / cellKm);
    float cluster = 0.5f + 0.5f * DetailNoise(u, v, cellKm * 11.0,
                                              salt + 900u);
    float occupancy = 0.30f + 0.45f * cluster;
    float bowl = 0.0f;      // deepest wins
    float relief = 0.0f;    // rims and ejecta accumulate
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            int32_t gx = cx + dx, gy = cy + dy;
            if (DetailHash01(gx, gy, salt) > occupancy) continue;
            double px = (gx + 0.12 + 0.76 * DetailHash01(gx, gy, salt + 1)) *
                        cellKm;
            double py = (gy + 0.12 + 0.76 * DetailHash01(gx, gy, salt + 2)) *
                        cellKm;
            double diamKm = cellKm * (0.30 + 0.70 *
                                      DetailHash01(gx, gy, salt + 3));
            double ex = 1.0 + 0.16 * (DetailHash01(gx, gy, salt + 5) - 0.5);
            double r = std::hypot((u - px) * ex, (v - py) / ex) /
                       (diamKm * 0.5);
            if (r >= 1.8) continue;
            // Most craters are ancient: age^3 keeps the fresh, sharp,
            // rimmed ones rare, which is what a gardened surface looks
            // like after billions of years.
            float age = DetailHash01(gx, gy, salt + 4);
            float freshness = age * age * age;
            float depthM = (float)(diamKm * 1000.0) *
                           (0.030f + 0.170f * freshness);
            if (r < 1.0)
            {
                float b = -depthM * 0.5f *
                          (1.0f + std::cos((float)r * 3.14159265f));
                bowl = std::min(bowl, b);
            }
            float rimM = depthM * 0.34f * freshness;
            if (rimM > 0.001f)
            {
                float rimT = (float)(r - 1.03) / 0.22f;
                relief += rimM * std::exp(-rimT * rimT * 4.0f);
                // Ejecta apron — the low skirt that makes a fresh crater
                // read as an object sitting ON the ground rather than a
                // dent punched into it.
                if (r > 1.0)
                {
                    float e = (float)(r - 1.0) / 0.8f;
                    float k = std::max(0.0f, 1.0f - e);
                    relief += rimM * 0.35f * k * k;
                }
            }
        }
    }
    return bowl + relief;
}

static float SynthesizeDetail(double u, double v, float pixKm,
                              float nativeKm, float strength,
                              float roughM, float hurst)
{
    if (g_texture == LolaTexture::CRATERS)
    {
        // Craters carry the relief; noise is only the grain between
        // them. Bands start ABOVE the data floor here (unlike the noise
        // path) because the DEM's own response is already rolling off
        // for a decade above its grid — 60-500 m craters are weak in
        // the data and absent from synthesis, so this fills that gap
        // progressively: nothing at 4x native, full weight at 1x down.
        float total = 0.0f;
        double wave = nativeKm * 4.0;
        for (int level = 0; level < 18 && wave >= 3.0 * pixKm; level++)
        {
            float w = (float)std::clamp(
                std::log2(nativeKm * 4.0 / wave) / 2.0, 0.0, 1.0);
            if (w > 0.0f)
            {
                total += w * DetailCraterField(
                    u, v, wave, 0xC7A7E5u + (uint32_t)level * 7u);
                float amp = roughM *
                            std::pow((float)(wave / nativeKm), hurst) * 0.30f;
                total += w * 0.25f * amp *
                         DetailNoise(u, v, wave, 0x51u + (uint32_t)level);
            }
            wave *= 0.5;
        }
        return strength * total;
    }

    float total = 0.0f;
    // Start AT the data floor, not above it: octaves coarser than one
    // native sample are the real data's job, and synthesizing there
    // double-counts relief that is already in the elevation field
    // (which is what made the first spectral build look like bark).
    double wave = nativeKm;
    for (int level = 0; level < 16 && wave >= 3.0 * pixKm; level++)
    {
        // 0 where the DEM still resolves this wavelength, 1 below its
        // effective (Nyquist-ish) floor at ~1.5x the native pixel.
        float fade = (float)std::clamp(
            (nativeKm * 4.0 - wave) / (nativeKm * 2.5),
            0.0, 1.0);
        if (fade > 0.0f)
        {
            // Continue the ground's OWN measured power law downward:
            // relief at wavelength w = (relief at the native scale) x
            // (w / native)^H. Rough walls stay rough, maria stay calm,
            // and the fine scales carry the energy the real spectrum
            // says they should — which hand-tuned constants never did.
            float amp = roughM *
                        std::pow((float)(wave / nativeKm), hurst) * 0.30f;
            float band = amp *
                         DetailNoise(u, v, wave, 0x51u + (uint32_t)level);
            // Hummocky ground: half-rectified noise reads as soft
            // mounds and rock lumps scattered on the plain, not as
            // symmetric static.
            float lumpN = DetailNoise(u, v, wave * 0.7,
                                      0xB00Bu + (uint32_t)level);
            band += 0.3f * amp * std::max(0.0f, lumpN);
            // Craters above a ~30 m floor — smaller ones read as noise
            // speckle, not landforms. NOTE this gate used to be 0.12 km,
            // which no octave on a 59 m SLDEM overlay ever satisfies
            // (wave starts AT nativeKm = 0.059 and halves): Tycho and
            // Imbrium — the only high-res ground we have — were
            // generating no synthetic craters at all.
            if (wave >= 0.03)
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

    // No smoothing pass and no unsharp compensation: the Catmull-Rom
    // sampler is slope-continuous, so an upsampled window has no
    // interpolation lattice to hide — and nothing gets blurred.

    // FRACTAL reconstruction: on top of the CATROM base, add a
    // stochastic residual between the measured points whose amplitude
    // follows the LOCAL measured relief — rough ground gets rough
    // infill, smooth ground stays smooth. Classic conditioned
    // subdivision, expressed as banded noise below the native scale.
    if (g_interp == LolaInterp::FRACTAL)
    {
        double dMs = spanKm * 1000.0 / (res - 1);
        std::vector<float> resid((size_t)res * res, 0.0f);
        for (int j = 0; j < res; j++)
        {
            int jm = std::max(0, j - 1), jp = std::min(res - 1, j + 1);
            for (int i = 0; i < res; i++)
            {
                int im = std::max(0, i - 1), ip = std::min(res - 1, i + 1);
                size_t k = (size_t)j * res + i;
                double gx = (out.elevationM[(size_t)j * res + ip] -
                             out.elevationM[(size_t)j * res + im]) /
                            (dMs * (ip - im));
                double gy = (out.elevationM[(size_t)jp * res + i] -
                             out.elevationM[(size_t)jm * res + i]) /
                            (dMs * (jp - jm));
                // Height swing of the real ground over one native cell.
                float localRelief = (float)(std::hypot(gx, gy) *
                                            nativeYKm * 1000.0);
                float amp = 0.16f * localRelief;
                double wave = nativeYKm / 1.5;
                float total = 0.0f;
                for (int oct = 0; oct < 5 && wave >= 2.0 * spanKm / res;
                     oct++)
                {
                    total += amp * DetailNoise(gu.empty() ? i * dMs : gu[k],
                                               gu.empty() ? j * dMs : gv[k],
                                               wave, 0xF2AC7A1u + oct);
                    amp *= 0.55f;
                    wave *= 0.5;
                }
                resid[k] = total;
            }
        }
        for (size_t k = 0; k < resid.size(); k++)
        {
            out.elevationM[k] += resid[k];
        }
    }

    // Synthesize sub-floor detail on top of the (smoothed) real ground.
    // Runs after the blur so it is not smoothed away; the real slope of
    // the smoothed field roughens the synthesis on crater walls.
    if (detailStrength > 0.0f)
    {
        float pixKm = (float)(spanKm / res);

        // --- Measure the real ground's spectrum in this window ---
        // Lag of one native sample, in output pixels. Everything below
        // this is the synthesis's to own; at and above it the data
        // speaks, and we make the synthesis continue what it says.
        int lag = std::max(1, (int)std::lround(nativeYKm / pixKm));

        // Global Hurst exponent: RMS height difference grows as L^H, so
        // H = log2( rms(2L) / rms(L) ). Sampled sparsely — this is one
        // number for the window.
        double s1 = 0.0, s2 = 0.0;
        int n1 = 0, n2 = 0;
        // Measure at 4x and 8x the native sample, NOT 1x and 2x: a
        // stereo DEM rolls off approaching its own grid, so the data
        // reaches only ~0.74/0.80 of its true power law at 1x/2x.
        // Fitting there reads H too steep (~0.90 vs the true ~0.77)
        // AND deflates the amplitude anchor — together about half the
        // energy the fine octaves should carry.
        int lagA = 4 * lag, lagB = 8 * lag;
        for (int j = 0; j < res; j += 4)
        {
            for (int i = 0; i + lagB < res; i += 4)
            {
                size_t k = (size_t)j * res + i;
                double d1 = out.elevationM[k + lagA] - out.elevationM[k];
                double d2 = out.elevationM[k + lagB] - out.elevationM[k];
                s1 += d1 * d1; n1++;
                s2 += d2 * d2; n2++;
            }
        }
        float hurst = 0.75f;
        if (n1 > 8 && n2 > 8 && s1 > 1e-9)
        {
            double r1 = std::sqrt(s1 / n1), r2 = std::sqrt(s2 / n2);
            if (r1 > 1e-6 && r2 > r1 * 1.001)
            {
                hurst = (float)(std::log2(r2 / r1));
            }
        }
        // Real terrain sits well inside this range; clamp so a noisy
        // or near-flat window cannot produce a runaway exponent.
        hurst = std::clamp(hurst, 0.35f, 1.0f);

        // Local roughness: RMS relief the real data carries over one
        // native sample AT EACH POINT, so a crater wall and the mare
        // beside it get different synthetic amplitudes.
        // Sampled at 4x the native lag (clear of the roll-off) and then
        // walked back down the fitted power law to the native scale,
        // which removes the ~0.74 anchor deflation.
        std::vector<float> rough((size_t)res * res, 0.0f);
        const float roughToNative = std::pow(0.25f, hurst);
        for (int j = 0; j < res; j++)
        {
            int jm = std::max(0, j - lagA), jp = std::min(res - 1, j + lagA);
            for (int i = 0; i < res; i++)
            {
                int im = std::max(0, i - lagA), ip = std::min(res - 1, i + lagA);
                size_t k = (size_t)j * res + i;
                float h = out.elevationM[k];
                float mad = 0.25f *
                    (std::fabs(out.elevationM[(size_t)j * res + ip] - h) +
                     std::fabs(h - out.elevationM[(size_t)j * res + im]) +
                     std::fabs(out.elevationM[(size_t)jp * res + i] - h) +
                     std::fabs(h - out.elevationM[(size_t)jm * res + i]));
                // MAD -> RMS for gaussian-ish, then 4x lag -> native.
                rough[k] = 1.25f * mad * roughToNative;
            }
        }
        // Smooth the amplitude field itself, or its own graininess
        // modulates the synthesis and reads as blotching.
        GaussianBlur(rough, res, res, (float)lagA, (float)lagA);

        std::vector<float> detail((size_t)res * res);
        for (int j = 0; j < res; j++)
        {
            for (int i = 0; i < res; i++)
            {
                size_t k = (size_t)j * res + i;
                detail[k] = SynthesizeDetail(
                    gu[k], gv[k], pixKm, (float)nativeYKm,
                    detailStrength, rough[k], hurst);
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
