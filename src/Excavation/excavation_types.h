#pragma once

#include <map>
#include "prospecting_types.h"
#include "resource_types.h"

// What excavation sees at one spot (sub-cell) at one depth.
//
// Units are named at this boundary on purpose. `quantity` is an ABSOLUTE
// deposit amount (hundreds to thousands); `composition` is FRACTIONS that sum
// to ~1. Mixing the two is the most expensive bug this codebase has had --
// see docs/guides/module-architecture.md Part II §2. `targetYield` is the
// product, and is the only one of the three worth optimising against.
struct SpotView
{
    int x = 0;
    int y = 0;
    DepthLayer depth = DepthLayer::SURFACE;

    // Access
    bool inReach = false;         // within THIS module's tier reach
    int  tierRequired = -1;       // lowest excavation tier that reaches it
    bool depthAccessible = false; // within this tier's depth ceiling

    // Ground truth
    float quantity = 0.0f;                      // absolute -- how much is there
    std::map<ResourceType, float> composition;  // fractions -- what it is made of
    float targetYield = 0.0f;                   // quantity * composition[target]

    // Knowledge
    float confidence = 0.0f;      // 0-1, for this spot AND this depth
    bool  hasBeenSwept = false;
    int   samplesHere = 0;        // samples taken at this exact spot and depth
};
