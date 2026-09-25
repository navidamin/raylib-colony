// domeforge_util.h — the helpers dome-forge-engine.js exports as `util` and the
// base module reuses, so noise and blending match exactly across both files.
//
// Three places where JS and C disagree, reproduced here on purpose:
//   Math.round        rounds halves UP (-2.5 -> -2); C round() goes away from zero
//   Uint8ClampedArray rounds halves to EVEN on store; a plain (uint8_t) cast truncates
//   Math.imul / >>>   32-bit wraparound and unsigned shifts; done on uint32_t
#ifndef DOMEFORGE_UTIL_H
#define DOMEFORGE_UTIL_H

#include "domeforge.h"

#include <cmath>
#include <cstdint>

namespace DomeForgeUtil
{
    constexpr double PI = 3.14159265358979323846;
    constexpr double DEG = PI / 180.0;
    const double SQ3 = std::sqrt(3.0);

    struct V3
    {
        double x, y, z;
    };

    inline double Clamp(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
    inline double Lerp(double a, double b, double t) { return a + (b - a) * t; }
    inline double Sstep(double a, double b, double x)
    {
        const double t = Clamp((x - a) / (b - a), 0.0, 1.0);
        return t * t * (3 - 2 * t);
    }
    inline double JsRound(double x) { return std::floor(x + 0.5); }
    inline double Hypot3(double x, double y, double z) { return std::sqrt(x * x + y * y + z * z); }

    // Uint8ClampedArray store: clamp to 0..255, round half to even, NaN -> 0.
    inline uint8_t ToByte(double v)
    {
        if (!(v > 0)) return 0;
        if (v >= 255) return 255;
        return (uint8_t)std::nearbyint(v);
    }

    inline double HashInt(int32_t x, int32_t y, int32_t s)
    {
        uint32_t h = ((uint32_t)x * 0x27d4eb2du) ^ ((uint32_t)y * 0x165667b1u) ^
                     (((uint32_t)s + 0x5bd1e995u) * 0x9e3779b1u);
        h ^= h >> 15;
        h *= 0x2c1b3c6du;
        h ^= h >> 12;
        h *= 0x297a2d39u;
        h ^= h >> 15;
        return h / 4294967296.0;
    }

    inline V3 Norm3(double x, double y, double z)
    {
        double l = Hypot3(x, y, z);
        if (l == 0) l = 1;
        return {x / l, y / l, z / l};
    }

    inline V3 DirFromAzEl(double az, double el)
    {
        const double ca = std::cos(az * DEG), sa = std::sin(az * DEG), ce = std::cos(el * DEG), se = std::sin(el * DEG);
        return Norm3(ca * ce, sa * ce, se);
    }

    // smooth minimum: two shapes merge with a concave fillet of radius ~k instead of a sharp crease
    inline double Smin(double a, double b, double k)
    {
        if (k <= 0) return std::fmin(a, b);
        const double h = std::fmax(k - std::fabs(a - b), 0.0) / k;
        return std::fmin(a, b) - h * h * k * 0.25;
    }

    inline double RoundedRectSDF(double x, double y, double hw, double hh, double rc)
    {
        const double qx = std::fabs(x) - hw + rc, qy = std::fabs(y) - hh + rc;
        return std::hypot(std::fmax(qx, 0.0), std::fmax(qy, 0.0)) + std::fmin(std::fmax(qx, qy), 0.0) - rc;
    }

    // jittered-grid patch noise: every point belongs to the nearest of a set of scattered seeds,
    // and each seed carries its own random shade -> irregular patches of different greys
    inline double PatchNoise(double x, double y, double cell, int seed, double soft)
    {
        const double gx = std::floor(x / cell), gy = std::floor(y / cell);
        double d1 = 1e9, d2 = 1e9, v1 = 0.5, v2 = 0.5;
        for (int j = -1; j <= 1; j++)
        {
            for (int i = -1; i <= 1; i++)
            {
                const int cx = (int)gx + i, cy = (int)gy + j;
                const double jx = (cx + HashInt(cx, cy, seed)) * cell, jy = (cy + HashInt(cx, cy, seed + 1)) * cell;
                const double d = (x - jx) * (x - jx) + (y - jy) * (y - jy);
                if (d < d1)
                {
                    d2 = d1;
                    v2 = v1;
                    d1 = d;
                    v1 = HashInt(cx, cy, seed + 2);
                }
                else if (d < d2)
                {
                    d2 = d;
                    v2 = HashInt(cx, cy, seed + 2);
                }
            }
        }
        // soften the border between two patches
        const double w = Sstep(0, soft, (std::sqrt(d2) - std::sqrt(d1)) / cell);
        return Lerp((v1 + v2) * 0.5, v1, w);
    }
}

#endif // DOMEFORGE_UTIL_H
