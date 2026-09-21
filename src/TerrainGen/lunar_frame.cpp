#include "lunar_frame.h"

#include <algorithm>
#include <cmath>

namespace
{

const double LUNAR_DEG2RAD = 3.14159265358979323846 / 180.0;

double CosLatFloored(double latDeg)
{
    return std::max(LUNAR_COS_LAT_FLOOR, std::cos(latDeg * LUNAR_DEG2RAD));
}

double WrapLon(double lonDeg)
{
    while (lonDeg > 180.0) lonDeg -= 360.0;
    while (lonDeg < -180.0) lonDeg += 360.0;
    return lonDeg;
}

} // namespace

Vector2 LunarOffsetKm(const LunarPoint& centre, const LunarPoint& p)
{
    double dLon = WrapLon(p.lonDeg - centre.lonDeg);
    double dLat = p.latDeg - centre.latDeg;
    Vector2 out;
    out.x = (float)(dLon * LUNAR_KM_PER_DEG * CosLatFloored(centre.latDeg));
    out.y = (float)(dLat * LUNAR_KM_PER_DEG);
    return out;
}

LunarPoint LunarOffsetPoint(const LunarPoint& centre, double eastKm, double northKm)
{
    LunarPoint p;
    p.latDeg = std::clamp(centre.latDeg + northKm / LUNAR_KM_PER_DEG, -90.0, 90.0);
    p.lonDeg = WrapLon(centre.lonDeg +
                       eastKm / (LUNAR_KM_PER_DEG * CosLatFloored(centre.latDeg)));
    return p;
}

double LunarDistanceKm(const LunarPoint& a, const LunarPoint& b)
{
    double la1 = a.latDeg * LUNAR_DEG2RAD, la2 = b.latDeg * LUNAR_DEG2RAD;
    double c = std::sin(la1) * std::sin(la2) +
               std::cos(la1) * std::cos(la2) *
               std::cos((b.lonDeg - a.lonDeg) * LUNAR_DEG2RAD);
    c = std::clamp(c, -1.0, 1.0);
    return std::acos(c) * (180.0 / 3.14159265358979323846) * LUNAR_KM_PER_DEG;
}

LunarKey LunarQuantise(const LunarPoint& p, double stepDeg)
{
    LunarKey k;
    k.lat = std::llround(p.latDeg / stepDeg);
    k.lon = std::llround(WrapLon(p.lonDeg) / stepDeg);
    return k;
}

Vector2 LocalFrame::ToLocal(const LunarPoint& p) const
{
    Vector2 km = LunarOffsetKm(centre, p);
    return Vector2{ km.x * LOCAL_UNITS_PER_KM, -km.y * LOCAL_UNITS_PER_KM };
}

LunarPoint LocalFrame::FromLocal(Vector2 local) const
{
    return LunarOffsetPoint(centre, local.x / LOCAL_UNITS_PER_KM,
                            -local.y / LOCAL_UNITS_PER_KM);
}
