#ifndef LUNAR_REGIONS_H
#define LUNAR_REGIONS_H

#include <string>
#include <vector>

// The named regions of the Moon, loaded from src/assets/planet/zones.json.
//
// That file was already in the repo, carrying 73 real features with
// coordinates, sizes, rock types and (where anyone has measured them)
// compositions -- but no C++ ever read it. The instrument's regions came
// from a 19-entry table hand-transcribed into lunarmap_main.cpp, all of
// them on the near side. So a globe you can turn all the way round found
// nothing to name on the half it had just made reachable.
//
// Landing sites are in the file too (Apollo 11, Chang'e 4 ...) with
// diameters of a kilometre or less. They are not regions: at orbital
// scale they are invisible and unpickable, and "which economy is this
// ground" is not a question a flag answers. They are excluded here.

struct LunarRegion
{
    std::string name;
    std::string featureType;     // mare | crater | basin
    std::string dominantRock;    // may be empty
    double latDeg = 0.0;
    double lonDeg = 0.0;
    double radiusKm = 0.0;       // half of the file's diameter_km
    // Composition where the dataset carries it. Negative means nobody
    // has put a number on this one, and the caller should derive it from
    // the terrane and the ground type instead of inventing one.
    float fePct = -1.0f;
    float tiPct = -1.0f;
    float thPpm = -1.0f;
};

// Parsed once on first call. Empty if the asset is missing or malformed,
// which the caller should treat as "fall back to whatever you have".
const std::vector<LunarRegion>& GetLunarRegions();

// Great-circle distance from a point to a region's centre, in km.
double LunarRegionDistanceKm(const LunarRegion& region,
                             double latDeg, double lonDeg);

// Index of the SMALLEST region containing the point, or -1 for unnamed
// ground. Smallest wins so that a crater inside a basin inside a mare
// still names itself: the specific answer beats the general one.
int LunarRegionAt(double latDeg, double lonDeg);

#endif // LUNAR_REGIONS_H
