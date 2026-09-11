#include "survey_ground.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

/* The generator's PRNG and noise, ported from the prototype EXACTLY. These
   are not "a hash and some fbm": they are THE hash and THE fbm the block's
   look was tuned against, and a different one -- even a better one -- gives
   different ground. Kept bit-identical, which is why the shifts are spelled
   out on uint32_t rather than left to float arithmetic. */
namespace
{
    constexpr uint32_t SEED_SHARED = 0x51edu;
    constexpr uint32_t SEED_OWN    = 0x9e37u;
    constexpr uint32_t SEED_CRATER = 0x6d2bu;
    constexpr uint32_t SEED_ERR    = 0x3c9du;

    uint32_t Mix(uint32_t a)
    {
        a ^= a >> 16; a *= 0x7feb352du;
        a ^= a >> 15; a *= 0x846ca68bu;
        a ^= a >> 16;
        return a;
    }

    float Hash2(uint32_t x, uint32_t y, uint32_t seed)
    {
        const uint32_t v = x * 374761393u + y * 668265263u + seed;
        return static_cast<float>(Mix(v) / 4294967295.0);
    }

    float Smooth(float t)
    {
        return t * t * (3.0f - 2.0f * t);
    }

    // Value noise on a lattice of `period` cells that WRAPS, so the block has
    // no seam where the noise domain restarts.
    float ValueNoise(float u, float v, int period, uint32_t seed)
    {
        const float fx = u * period, fy = v * period;
        int x0 = static_cast<int>(std::floor(fx));
        int y0 = static_cast<int>(std::floor(fy));
        const float tx = Smooth(fx - x0), ty = Smooth(fy - y0);
        const int x1 = ((x0 + 1) % period + period) % period;
        const int y1 = ((y0 + 1) % period + period) % period;
        x0 = ((x0 % period) + period) % period;
        y0 = ((y0 % period) + period) % period;
        const float a = Hash2(x0, y0, seed), b = Hash2(x1, y0, seed);
        const float c = Hash2(x0, y1, seed), d = Hash2(x1, y1, seed);
        const float ab = a + (b - a) * tx, cd = c + (d - c) * tx;
        return ab + (cd - ab) * ty;
    }

    float Fbm(float u, float v, int basePeriod, int octaves, uint32_t seed)
    {
        float sum = 0.0f, amp = 1.0f, norm = 0.0f;
        int period = basePeriod;
        for (int o = 0; o < octaves; o++)
        {
            sum += ValueNoise(u, v, period, seed + static_cast<uint32_t>(o) * 7919u) * amp;
            norm += amp; amp *= 0.5f; period *= 2;
        }
        return sum / norm;
    }

    /* How wrong an interface is allowed to be before any of it is known.
       Scaled by the thinner of the two beds it separates, and drawn from a
       fixed hash, so the wrongness is a property of THIS BLOCK rather than
       of this frame -- drill the same ground twice and it is wrong the same
       way, which is what makes re-fitting it feel like learning. */
    struct ErrParams
    {
        float bulk, tilt, tca, tsa, amp;
        int p1, p2;
        uint32_t s1, s2;
    };

    ErrParams MakeErrParams(int L, const float* edge)
    {
        const float span = std::max(2.0f, std::min(edge[L] - edge[L - 1], edge[L + 1] - edge[L]));
        auto h = [L](int k) { return Hash2(static_cast<uint32_t>(L * 37 + k), 11u, SEED_ERR); };
        const float ta = h(2) * 6.2832f;
        ErrParams e;
        e.bulk = (h(1) - 0.5f) * 2.0f * 0.30f * span;
        e.tilt = (0.15f + h(3) * 0.45f) * span;
        e.tca = std::cos(ta); e.tsa = std::sin(ta);
        e.amp = (0.22f + h(4) * 0.30f) * span;
        e.p1 = 2 + static_cast<int>(std::lround(h(5) * 2.0f));
        e.p2 = 5 + static_cast<int>(std::lround(h(6) * 4.0f));
        e.s1 = SEED_ERR + static_cast<uint32_t>(L) * 7919u;
        e.s2 = SEED_ERR + static_cast<uint32_t>(L) * 104729u + 17u;
        return e;
    }
}

SurveyGround::SurveyGround()
{
    SurveyKnowledge empty;
    Build(empty, {});
}

void SurveyGround::BuildCraters()
{
    craters.clear();
    for (int k = 0; k < SURVEY_CRATER_N; k++)
    {
        auto h = [k](int i) { return Hash2(static_cast<uint32_t>(k * 31 + i), 3u, SEED_CRATER); };
        float x = 0.0f, y = 0.0f;
        for (int t = 0; t < 14; t++)
        {
            x = 0.10f + h(1 + t * 5) * 0.80f;
            y = 0.10f + h(2 + t * 5) * 0.80f;
            if (std::hypot(x - 0.5f, y - 0.5f) > 0.17f) break;
        }
        craters.push_back({ x, y,
                            (0.028f + h(3) * 0.042f) * SURVEY_CRATER_SIZE,
                            0.55f + h(4) * 0.9f });
    }
}

// Summed, so two that overlap merge the way two that overlap would.
float SurveyGround::Bowl(float u, float v) const
{
    float s = 0.0f;
    for (const Crater& c : craters)
    {
        const float r = std::hypot(u - c.x, v - c.y) / std::max(c.r, 0.006f);
        if (r > 2.0f) continue;
        const float floorTerm = r < 1.0f ? -(1.0f - r * r) : 0.0f;
        const float rim = SURVEY_CRATER_RIM * std::exp(-((r - 1.0f) * (r - 1.0f)) / (2.0f * 0.20f * 0.20f));
        s += (floorTerm + rim) * c.d;
    }
    return s;
}

/* Which interfaces a crater reaches: it was cut into interface k, so below is
   undisturbed and above is the same bowl progressively filled in -- which is
   what "buried crater" means. */
float SurveyGround::CraterAmp(int interfaceIndex) const
{
    const int k = SURVEY_CRATER_LAYER;
    if (interfaceIndex > k) return 0.0f;
    return SURVEY_CRATER_DEPTH * std::pow(SURVEY_CRATER_INFILL,
                                          static_cast<float>(k - interfaceIndex));
}

void SurveyGround::Build(const SurveyKnowledge& knowledge, const std::vector<SurveyScour>& scours)
{
    lattice = SURVEY_LATTICE;
    const int stride = Stride();
    BuildCraters();

    edgeM[0] = 0.0f;
    edgeM[1] = SURVEY_T0;
    edgeM[2] = SURVEY_T0 + SURVEY_T1;
    edgeM[3] = SURVEY_T0 + SURVEY_T1 + SURVEY_T2;
    edgeM[4] = edgeM[3] + SURVEY_T3;
    columnM = edgeM[4];

    const float az = SURVEY_DIP_AZ_DEG * PI / 180.0f;
    const float ca = std::cos(az), sa = std::sin(az);
    const int period = std::max(1, static_cast<int>(std::lround(SURVEY_FEATURE)));
    const int oct = std::max(1, SURVEY_DETAIL);
    const float ky = 1.0f - 0.85f * SURVEY_GRAIN;   // squash the noise domain: mottle becomes ridges

    for (int L = 0; L <= SURVEY_BEDS; L++)
    {
        surf[L].assign(static_cast<size_t>(stride) * stride, 0.0f);
        const float amp = SURVEY_RELIEF_M * std::pow(SURVEY_DAMP, static_cast<float>(L));
        /* Topography is a surface fact. Beds inherit some of it near the top
           and less with depth; the BASE inherits none -- letting the terrain
           warp it turns a block diagram into a drape. */
        const float realW = (L == SURVEY_BEDS) ? 0.0f : 1.0f;
        const float dip = SURVEY_DIP_M * (1.0f + SURVEY_DIP_SPREAD * L);
        const float cr = CraterAmp(L);
        const bool hasErr = (L >= 1 && L <= SURVEY_BEDS - 1);
        const ErrParams ep = hasErr ? MakeErrParams(L, edgeM) : ErrParams{};

        for (int j = 0; j <= lattice; j++)
        {
            for (int i = 0; i <= lattice; i++)
            {
                const float u = static_cast<float>(i) / lattice;
                const float v = static_cast<float>(j) / lattice;
                const float vv = 0.5f + (v - 0.5f) * ky;
                const float shared = (Fbm(u, vv, period, oct, SEED_SHARED) - 0.5f) * realW;
                const float own = Fbm(u, vv, period, oct,
                                      SEED_OWN + static_cast<uint32_t>(L) * 7919u) - 0.5f;
                float hgt = amp * 2.0f * (SURVEY_CONFORM * shared + (1.0f - SURVEY_CONFORM) * own);
                hgt += dip * ((u - 0.5f) * ca + (v - 0.5f) * sa);
                if (cr != 0.0f) hgt += cr * Bowl(u, v);
                float z = edgeM[L] - hgt;               // hgt is up-positive, z is depth
                if (hasErr)
                {
                    const float e = ep.bulk
                                  + ep.tilt * ((u - 0.5f) * ep.tca + (v - 0.5f) * ep.tsa)
                                  + ep.amp * ((Fbm(u, v, ep.p1, 2, ep.s1) - 0.5f) * 1.30f
                                            + (Fbm(u, v, ep.p2, 2, ep.s2) - 0.5f) * 0.70f);
                    z += e * (1.0f - SurveyKnowledge::Confidence(
                                        knowledge.KnowAt(static_cast<float>(i),
                                                         static_cast<float>(j), edgeM[L])));
                }
                surf[L][static_cast<size_t>(j) * stride + i] = z;
            }
        }
    }

    /* Denudation. The scour is not a decal on the surface, it IS the surface:
       a bowl driven into interface 0 with the spoil heaped in a ring outside
       it. Because it deforms the height field the block already draws, the
       shading, the silhouette and the walls all pick it up for free. */
    for (const SurveyScour& s : scours)
    {
        if (s.progress <= 0.001f) continue;
        for (int j = 0; j <= lattice; j++)
        {
            for (int i = 0; i <= lattice; i++)
            {
                const float dd = std::hypot(i - s.i, j - s.j);
                if (dd > SURVEY_SCOUR_R * 2.1f) continue;
                const float u = dd / SURVEY_SCOUR_R;
                const float bowl = u < 1.0f ? (1.0f - u * u) * (1.0f - u * u) : 0.0f;
                const float sg = (dd - SURVEY_SCOUR_R * 1.25f) / (SURVEY_SCOUR_R * 0.62f);
                const float ring = std::exp(-sg * sg * 0.5f);
                surf[0][static_cast<size_t>(j) * stride + i] +=
                    (bowl * SURVEY_SCOUR_DEEP_M - ring * SURVEY_SCOUR_RIM_M) * s.progress;
            }
        }
    }

    /* No interface may pass through the one below it. A deep crater bottoming
       out on a harder unit is the interesting case this keeps legal: the
       floor flattens against the bed beneath instead of turning the block
       inside out. */
    for (int L = 1; L <= SURVEY_BEDS; L++)
    {
        const float minTh = std::max(0.4f, (edgeM[L] - edgeM[L - 1]) * 0.12f);
        for (size_t k = 0; k < surf[L].size(); k++)
            surf[L][k] = std::max(surf[L][k], surf[L - 1][k] + minTh);
    }
}

float SurveyGround::SampleDepth(int interfaceIndex, float u, float v) const
{
    const int stride = Stride();
    const float fx = std::min(std::max(u, 0.0f), 1.0f) * lattice;
    const float fz = std::min(std::max(v, 0.0f), 1.0f) * lattice;
    const int i0 = std::min(lattice - 1, static_cast<int>(fx));
    const int j0 = std::min(lattice - 1, static_cast<int>(fz));
    const float tx = fx - i0, tz = fz - j0;
    const std::vector<float>& d = surf[interfaceIndex];
    auto at = [&](int i, int j) { return d[static_cast<size_t>(j) * stride + i]; };
    const float a = at(i0, j0) + (at(i0 + 1, j0) - at(i0, j0)) * tx;
    const float b = at(i0, j0 + 1) + (at(i0 + 1, j0 + 1) - at(i0, j0 + 1)) * tx;
    return a + (b - a) * tz;
}
