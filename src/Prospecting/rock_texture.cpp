#include "rock_texture.h"

#include <algorithm>
#include <cmath>

namespace RockTexture
{

namespace
{

// --- deterministic noise ---------------------------------------------------

// One stream per layer per feature, so adding a feature to one stratum cannot
// re-roll another's grain.
inline unsigned Mix(unsigned a)
{
    a ^= a >> 16; a *= 0x7feb352du;
    a ^= a >> 15; a *= 0x846ca68bu;
    a ^= a >> 16;
    return a;
}
inline float Hash2(int x, int y, unsigned seed)
{
    return static_cast<float>(Mix(static_cast<unsigned>(x) * 374761393u +
                                  static_cast<unsigned>(y) * 668265263u + seed))
           / 4294967295.0f;
}
inline float Smooth(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

// Value noise on a lattice of `period` cells that WRAPS: the lattice index is
// taken mod period, so column size-1 and column 0 are neighbours by
// construction. Tiling a band down a strip of any height shows no seam.
float ValueNoise(float u, float v, int period, unsigned seed)
{
    float fx = u * period, fy = v * period;
    int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
    float tx = Smooth(fx - x0), ty = Smooth(fy - y0);
    int x1 = (x0 + 1) % period, y1 = (y0 + 1) % period;
    x0 = ((x0 % period) + period) % period;
    y0 = ((y0 % period) + period) % period;
    float a = Hash2(x0, y0, seed), b = Hash2(x1, y0, seed);
    float c = Hash2(x0, y1, seed), d = Hash2(x1, y1, seed);
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

// Octaves, each doubling the lattice: broad mottle first, detail on top.
float Fbm(float u, float v, int basePeriod, int octaves, unsigned seed)
{
    float sum = 0.0f, amp = 1.0f, norm = 0.0f;
    int period = basePeriod;
    for (int o = 0; o < octaves; o++)
    {
        sum += ValueNoise(u, v, period, seed + o * 7919u) * amp;
        norm += amp;
        amp *= 0.5f;
        period *= 2;
    }
    return sum / norm;   // 0..1
}

// --- a canvas of luminance deltas ------------------------------------------

struct Canvas
{
    int size = 0;
    std::vector<float> lum;    // signed delta around the eventual mean
    std::vector<float> ice;    // 0..1, the only hue any stratum carries

    explicit Canvas(int s) : size(s), lum(s * s, 0.0f), ice(s * s, 0.0f) {}

    int Wrap(int a) const
    {
        a %= size;
        return a < 0 ? a + size : a;
    }
    void Add(int x, int y, float d)
    {
        lum[Wrap(y) * size + Wrap(x)] += d;
    }
    void AddIce(int x, int y, float t)
    {
        float& c = ice[Wrap(y) * size + Wrap(x)];
        c = std::min(1.0f, c + t);
    }
    // A round clast/vug/bubble: dark core, light rim on the lower-right, which
    // is the section 3 lighting (implied upper-left key) applied at grain
    // scale. Wrapping is handled by Add.
    void Disc(float cx, float cy, float r, float core, float rim)
    {
        int ri = static_cast<int>(std::ceil(r)) + 1;
        for (int dy = -ri; dy <= ri; dy++)
            for (int dx = -ri; dx <= ri; dx++)
            {
                float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (d > r + 1.0f) continue;
                if (d <= r)
                {
                    Add(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, core);
                }
                else if (dx + dy > 0)
                {
                    Add(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, rim);
                }
            }
    }
};

// A fracture: a random walk that crosses the tile and wraps off its edges.
// Straight lines read as drawn, not as broken rock, so the heading jitters.
void Fracture(Canvas& c, unsigned seed, float dark, float halo, float iceAmt,
              float lengthMul, float jitter)
{
    float x = Hash2(0, 0, seed) * c.size;
    float y = Hash2(1, 0, seed) * c.size;
    float ang = Hash2(2, 0, seed) * 6.2832f;
    int steps = static_cast<int>(c.size * lengthMul);
    for (int i = 0; i < steps; i++)
    {
        ang += (Hash2(i, 3, seed) - 0.5f) * jitter;
        x += std::cos(ang);
        y += std::sin(ang);
        int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
        c.Add(xi, yi, dark);
        // one-sided halo: the open face of the fracture catches the key light
        c.Add(xi + (std::sin(ang) > 0.0f ? 1 : -1), yi, halo);
        if (iceAmt > 0.0f)
        {
            c.AddIce(xi, yi, iceAmt);
            c.AddIce(xi, yi + 1, iceAmt * 0.5f);
        }
    }
}

// --- the four strata --------------------------------------------------------
//
// Each layer is a different STRUCTURE, not the same noise re-tinted: what
// separates regolith from basalt on screen is how the grain is organised.

// 0 REGOLITH -- impact-gardened soil. No hard structure: the finest grain of
// the four, sorted into broad mottled patches, with faint bedding laminae
// (it is DEPOSITED material, and in section that is what separates it from
// the lava below) and scattered angular grit sitting on top.
void BuildRegolith(Canvas& c, unsigned seed)
{
    const int N = c.size;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
        {
            float u = static_cast<float>(x) / N, v = static_cast<float>(y) / N;
            float broad = Fbm(u, v, 4, 3, seed) - 0.5f;              // patchiness
            // laminae: horizontal bands warped by noise so they read as
            // settled beds, not as printed lines. Whole periods only, or the
            // tile would not wrap.
            // Two beat frequencies and a heavier warp: one sine alone came
            // out as corduroy, evenly spaced in a way soil never is.
            float warp = (Fbm(u, v, 3, 2, seed + 55u) - 0.5f) * 0.22f;
            float bed = std::sin((v + warp) * 6.2832f * 6.0f) * 0.6f
                      + std::sin((v + warp * 1.7f) * 6.2832f * 11.0f) * 0.4f;
            float fine = Hash2(x, y, seed + 101u) - 0.5f;            // sand
            c.lum[y * N + x] = broad * 22.0f + bed * 6.0f + fine * 14.0f;
        }
    // grit: 1-2 px clasts, lit on top, shadowed under
    for (int i = 0; i < 430; i++)
    {
        int x = static_cast<int>(Hash2(i, 11, seed) * N);
        int y = static_cast<int>(Hash2(i, 12, seed) * N);
        float b = 15.0f + Hash2(i, 13, seed) * 16.0f;
        c.Add(x, y, b);
        if (Hash2(i, 14, seed) > 0.5f) c.Add(x + 1, y, b * 0.7f);
        c.Add(x, y + 1, -b * 0.6f);
    }
    // pebbles: the few fragments big enough to have a lit side
    for (int i = 0; i < 34; i++)
    {
        c.Disc(Hash2(i, 15, seed) * N, Hash2(i, 16, seed) * N,
               1.1f + Hash2(i, 17, seed) * 1.5f, 11.0f, -9.0f);
    }
}

// 1 MEGAREGOLITH -- coarse fragmented rock. Poorly sorted angular blocks with
// dark seams between them: a breccia reads as CELLS, so it is built from
// wrapped Voronoi rather than from noise. Two generations of it -- big blocks
// that are themselves broken into smaller ones -- because a single cell size
// reads as a pattern, and poor sorting is the whole character of the rock.
void BuildMegaregolith(Canvas& c, unsigned seed)
{
    const int N = c.size;

    auto voronoi = [&](int count, unsigned vs, float tone, float seamW, float seamD)
    {
        std::vector<float> sx(count), sy(count), tn(count);
        for (int s = 0; s < count; s++)
        {
            sx[s] = Hash2(s, 20, vs) * N;
            sy[s] = Hash2(s, 21, vs) * N;
            tn[s] = (Hash2(s, 22, vs) - 0.5f) * tone;
        }
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++)
            {
                float d1 = 1e9f, d2 = 1e9f;
                int hit = 0;
                for (int s = 0; s < count; s++)
                {
                    // toroidal distance, so blocks continue across the edges
                    float dx = std::fabs(sx[s] - x), dy = std::fabs(sy[s] - y);
                    dx = std::min(dx, N - dx); dy = std::min(dy, N - dy);
                    float d = dx * dx + dy * dy;
                    if (d < d1) { d2 = d1; d1 = d; hit = s; }
                    else if (d < d2) { d2 = d; }
                }
                float edge = std::sqrt(d2) - std::sqrt(d1);       // 0 on a seam
                float add = tn[hit];
                if (edge < seamW) add -= (seamW - edge) * seamD;
                c.lum[y * N + x] += add;
            }
    };

    voronoi(20, seed,          26.0f, 1.9f, 13.0f);   // the big blocks
    voronoi(58, seed + 991u,   11.0f, 1.2f,  9.0f);   // broken into smaller

    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
        {
            float u = static_cast<float>(x) / N, v = static_cast<float>(y) / N;
            c.lum[y * N + x] += (Fbm(u, v, 8, 2, seed) - 0.5f) * 11.0f
                              + (Hash2(x, y, seed + 202u) - 0.5f) * 8.0f;
        }
    // the coarse fragments themselves, sitting proud of the matrix
    for (int i = 0; i < 54; i++)
    {
        float cx = Hash2(i, 23, seed) * N, cy = Hash2(i, 24, seed) * N;
        float r = 1.6f + Hash2(i, 25, seed) * 2.8f;
        c.Disc(cx, cy, r, 9.0f, -12.0f);
    }
}

// 2 FRACTURED -- coherent rock cut by fractures, some ice-filled. Big calm
// slabs, so the low frequency dominates and the fractures are the detail that
// carries: a few long straight ones that define the slabs, and shorter
// wandering ones between. The ice is the one hue in the whole set, and the
// core log's legend already names it.
void BuildFractured(Canvas& c, unsigned seed)
{
    const int N = c.size;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
        {
            float u = static_cast<float>(x) / N, v = static_cast<float>(y) / N;
            float slab = Fbm(u, v, 3, 3, seed) - 0.5f;
            float fine = Hash2(x, y, seed + 303u) - 0.5f;
            c.lum[y * N + x] = slab * 24.0f + fine * 7.0f;
        }
    // the master joints: long, nearly straight, they cut the slabs
    for (int f = 0; f < 4; f++)
    {
        Fracture(c, seed + f * 7717u, -26.0f, 10.0f,
                 f == 0 ? 0.9f : 0.0f, 1.8f, 0.10f);
    }
    // and the branching ones between them
    for (int f = 0; f < 9; f++)
    {
        bool icy = (f % 4) == 0;
        Fracture(c, seed + f * 5701u, -19.0f, 7.0f, icy ? 0.8f : 0.0f, 1.1f, 0.42f);
    }
    // rubble in the fracture zone
    for (int i = 0; i < 130; i++)
    {
        int x = static_cast<int>(Hash2(i, 31, seed) * N);
        int y = static_cast<int>(Hash2(i, 32, seed) * N);
        c.Add(x, y, 11.0f);
        c.Add(x, y + 1, -8.0f);
    }
}

// 3 BASALT -- dense lava. Almost featureless by design (it is the hardest,
// most uniform rock in the column), carried by vesicles: gas bubbles frozen
// in, dark with a lit lower rim. Columnar joints run the long axis, with a
// few horizontal cooling cracks across them.
void BuildBasalt(Canvas& c, unsigned seed)
{
    const int N = c.size;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
        {
            float u = static_cast<float>(x) / N, v = static_cast<float>(y) / N;
            c.lum[y * N + x] = (Fbm(u, v, 4, 2, seed) - 0.5f) * 13.0f
                             + (Hash2(x, y, seed + 404u) - 0.5f) * 6.0f;
        }
    // columnar jointing: near-vertical, low contrast, one lit face
    for (int j = 0; j < 6; j++)
    {
        float x = Hash2(j, 40, seed) * N;
        for (int y = 0; y < N; y++)
        {
            x += (Hash2(y, 41, seed + j * 13u) - 0.5f) * 0.5f;
            c.Add(static_cast<int>(x), y, -11.0f);
            c.Add(static_cast<int>(x) + 1, y, 5.0f);
        }
    }
    // cooling cracks across the columns
    for (int h = 0; h < 5; h++)
    {
        float y = Hash2(h, 45, seed) * N;
        float x0 = Hash2(h, 46, seed) * N;
        int len = static_cast<int>(N * (0.25f + Hash2(h, 47, seed) * 0.4f));
        for (int i = 0; i < len; i++)
        {
            y += (Hash2(i, 48, seed + h * 31u) - 0.5f) * 0.35f;
            c.Add(static_cast<int>(x0) + i, static_cast<int>(y), -9.0f);
        }
    }
    // vesicles
    for (int i = 0; i < 240; i++)
    {
        float cx = Hash2(i, 42, seed) * N, cy = Hash2(i, 43, seed) * N;
        float r = 0.8f + Hash2(i, 44, seed) * 1.9f;
        c.Disc(cx, cy, r, -20.0f, 13.0f);
    }
}

}   // namespace

void Generate(int layer, int size, unsigned char* dst)
{
    if (size <= 0 || dst == nullptr) return;
    layer = std::max(0, std::min(3, layer));

    Canvas c(size);
    const unsigned seed = 0x9e37u + static_cast<unsigned>(layer) * 0x51edu;
    switch (layer)
    {
        case 0:  BuildRegolith(c, seed);     break;
        case 1:  BuildMegaregolith(c, seed); break;
        case 2:  BuildFractured(c, seed);    break;
        default: BuildBasalt(c, seed);       break;
    }

    // Centre on 128 EXACTLY. This is the contract that lets a textured
    // surface keep the tone its flat fill had: the caller multiplies by
    // 2 * tex/255, so a mean of 128 is a mean of x1.0. Without it, every
    // stratum colour would need re-tuning per texture.
    double sum = 0.0;
    for (float v : c.lum) sum += v;
    float shift = 128.0f - static_cast<float>(sum / c.lum.size());

    for (int i = 0; i < size * size; i++)
    {
        float g = std::clamp(c.lum[i] + shift, 60.0f, 205.0f);
        float t = std::min(1.0f, c.ice[i]);
        dst[i * 4 + 0] = static_cast<unsigned char>(std::clamp(g - 16.0f * t, 0.0f, 255.0f));
        dst[i * 4 + 1] = static_cast<unsigned char>(std::clamp(g +  2.0f * t, 0.0f, 255.0f));
        dst[i * 4 + 2] = static_cast<unsigned char>(std::clamp(g + 22.0f * t, 0.0f, 255.0f));
        dst[i * 4 + 3] = 255;
    }
}

std::vector<unsigned char> Generate(int layer, int size)
{
    std::vector<unsigned char> out(static_cast<size_t>(size) * size * 4, 0);
    Generate(layer, size, out.data());
    return out;
}

}   // namespace RockTexture
