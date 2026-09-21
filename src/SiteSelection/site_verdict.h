#ifndef SITE_VERDICT_H
#define SITE_VERDICT_H

// Can a base be built on this exact ground?
//
// The verdict is measured, never amplified: it reads TerrainBuildability,
// which LolaDem::EvaluateSite derives from real elevation alone. The
// thresholds live in site_selection_constants.h. Pure: no GL, no game
// state. Shared by the game and lunar_map.

#include "lola_dem.h"

struct PlacementVerdict
{
    bool allowed = false;
    const char* reason = "";     // the blocking limit, named, or the go-ahead
};

PlacementVerdict JudgeSite(const TerrainBuildability& b);

// Ground statistics over a sub-square of a real elevation window: what
// the level cards report at the navigation rungs. Everything measured.
struct GroundStats
{
    float meanSlope = 0.0f;
    float maxSlope = 0.0f;
    float buildableFrac = 0.0f;    // slope under the mean-slope gate
    float reliefM = 0.0f;
};

// offX/offY are km east/north of the window centre; sizeKm the side of
// the square measured.
GroundStats CursorGroundStats(const LolaWindow& window,
                              double offXKm, double offYKm, double sizeKm);

#endif // SITE_VERDICT_H
