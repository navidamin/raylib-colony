// Real lunar elevation from the LOLA LDEM_16 model. See lola_dem.h.

#include "lola_dem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

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

bool LolaDem::Load(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        std::fprintf(stderr,
                     "LolaDem: %s missing — run the fetch-dem workflow "
                     "(push a change to data/lola/REQUEST) and pull.\n",
                     path.c_str());
        return false;
    }
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
        width = height = 0;
        return false;
    }
    if (rowsPerStrip == 0) rowsPerStrip = (uint32_t)height;

    raw.assign((size_t)width * height, 0);
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
            width = height = 0;
            return false;
        }
        const uint8_t* src = file.data() + offset;
        uint16_t* dst = raw.data() + (size_t)row * width;
        for (uint32_t k = 0; k < rows * (uint32_t)width; k++)
        {
            dst[k] = ReadU16(src + 2 * k);
        }
        row += rows;
    }
    return true;
}

float LolaDem::Sample(int x, int y) const
{
    // Longitude wraps, latitude clamps at the poles.
    x = ((x % width) + width) % width;
    y = std::clamp(y, 0, height - 1);
    return Decode(raw[(size_t)y * width + x]);
}

float LolaDem::ElevationM(double latDeg, double lonDeg) const
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
    double c = std::max(0.2, std::cos(midLat * M_PI / 180.0));
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
                (float)(std::atan(std::hypot(gx, gy)) * 180.0 / M_PI);
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

// Regional windows sample through an azimuthal equidistant projection
// centred on the pick: every output pixel is a true ground offset in
// km, so windows stay square-in-km at any latitude — including the
// poles, where an equirectangular crop degenerates into a smear.
LolaWindow LolaDem::Window(double latDeg, double lonDeg, double spanKm,
                           int res) const
{
    LolaWindow out;
    if (!IsLoaded() || res < 2) return out;
    out.resolution = res;
    out.latDeg = latDeg;
    out.lonDeg = lonDeg;
    out.spanKm = spanKm;
    out.elevationM.resize((size_t)res * res);
    out.slopeDeg.resize((size_t)res * res);

    const double DEG = M_PI / 180.0;
    double lat0 = latDeg * DEG;
    double lon0 = lonDeg * DEG;
    double halfM = spanKm * 1000.0 / 2.0;
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
        }
    }

    // Anti-lattice smoothing, matched to how far the output oversamples
    // the DEM's native spacing (per axis: columns shrink with cos lat).
    double outKm = spanKm / res;
    double nativeYKm = LOLA_M_PER_DEG / (width / 360.0) / 1000.0;
    double nativeXKm = nativeYKm * std::max(0.05, std::cos(lat0));
    float upX = (float)(nativeXKm / outKm);
    float upY = (float)(nativeYKm / outKm);
    GaussianBlur(out.elevationM, res, res,
                 (upX > 1.5f) ? 0.6f * upX : 0.0f,
                 (upY > 1.5f) ? 0.6f * upY : 0.0f);

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
