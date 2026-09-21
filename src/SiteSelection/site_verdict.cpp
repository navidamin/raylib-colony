#include "site_verdict.h"

#include "site_selection_constants.h"

#include <algorithm>

PlacementVerdict JudgeSite(const TerrainBuildability& b)
{
    PlacementVerdict v;
    if (b.meanSlopeDeg > SITE_MAX_MEAN_SLOPE_DEG) { v.reason = "TOO STEEP (mean slope)"; return v; }
    if (b.maxSlopeDeg > SITE_MAX_PEAK_SLOPE_DEG)  { v.reason = "TOO STEEP (local face)"; return v; }
    if (b.roughnessM > SITE_MAX_ROUGHNESS_M)      { v.reason = "GROUND TOO BROKEN";      return v; }
    if (b.reliefM > SITE_MAX_RELIEF_M)            { v.reason = "RELIEF TOO GREAT";       return v; }
    if (b.isPsr)                                  { v.reason = "PERMANENT SHADOW";       return v; }
    v.allowed = true;
    v.reason = "SITE OK - BUILD ALLOWED";
    return v;
}

GroundStats CursorGroundStats(const LolaWindow& window,
                              double offXKm, double offYKm, double sizeKm)
{
    GroundStats g;
    int res = window.resolution;
    if (res <= 0 || window.spanKm <= 0.0) return g;
    if (window.slopeDeg.size() < (size_t)res * res ||
        window.elevationM.size() < (size_t)res * res) return g;
    double half = sizeKm / window.spanKm * res * 0.5;
    int cx = (int)(res * 0.5 + offXKm / window.spanKm * res);
    int cy = (int)(res * 0.5 - offYKm / window.spanKm * res);
    int x0 = std::max(0, (int)(cx - half)), x1 = std::min(res - 1, (int)(cx + half));
    int y0 = std::max(0, (int)(cy - half)), y1 = std::min(res - 1, (int)(cy + half));
    if (x1 <= x0 || y1 <= y0) return g;
    double sum = 0.0;
    int n = 0, buildable = 0;
    float lo = 1e9f, hi = -1e9f;
    for (int y = y0; y <= y1; y++)
    {
        for (int x = x0; x <= x1; x++)
        {
            float s = window.slopeDeg[(size_t)y * res + x];
            sum += s;
            n++;
            if (s > g.maxSlope) g.maxSlope = s;
            if (s < SITE_MAX_MEAN_SLOPE_DEG) buildable++;
            float e = window.elevationM[(size_t)y * res + x];
            if (e < lo) lo = e;
            if (e > hi) hi = e;
        }
    }
    g.meanSlope = (float)(sum / n);
    g.buildableFrac = (float)buildable / (float)n;
    g.reliefM = hi - lo;
    return g;
}
