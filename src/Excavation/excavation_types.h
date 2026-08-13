#pragma once

#include <map>
#include <array>
#include <string>
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

// ---------------------------------------------------------------------------
// Machines
// ---------------------------------------------------------------------------

enum class MachineId
{
    SCOOP,
    BUCKET_WHEEL,
    BUCKET_DRUM,
    PERCUSSIVE,
    AUGER,
    PNEUMATIC,
    COUNT
};

// One machine's character. All six differ on every axis, so no machine is a
// strict upgrade on another -- which is what makes the choice a choice.
struct Machine
{
    MachineId id = MachineId::SCOOP;
    const char* displayName = "Scoop";

    int   maxDepthLayers = 1;   // how many depth layers it can reach (1-4)

    // How tightly it digs the spot you aimed at. Below 1.0 it also pulls up
    // the neighbours, averaging your chosen spot with them -- which is what
    // throws away a survey you paid for.
    float precision = 1.0f;

    float paceCeiling = 1.0f;   // how hard it can be pushed
    float powerFloor = 1.0f;    // baseline draw, before pace
    float wearRate = 1.0f;      // relative wear per unit of work

    // How much surrounding waste it leaves in the ground rather than loading.
    // Higher means a cleaner mix handed to beneficiation, not a bigger number.
    float selectivity = 0.0f;

    int   requiredTier = 0;
    const char* requiredTech = "";
};

// ---------------------------------------------------------------------------
// The site's worked state
// ---------------------------------------------------------------------------

// How much of each spot is left. Lives on the facade, not in the engine --
// engines stay pure. Indexed [depth][y][x].
struct DigSite
{
    static constexpr int GRID = 8;
    static constexpr int DEPTHS = 4;

    std::array<float, GRID * GRID * DEPTHS> remaining;

    DigSite() { remaining.fill(1.0f); }

    static int Index(int x, int y, DepthLayer depth)
    {
        return (static_cast<int>(depth) * GRID * GRID) + (y * GRID) + x;
    }

    float Remaining(int x, int y, DepthLayer depth) const
    {
        if (x < 0 || x >= GRID || y < 0 || y >= GRID) return 0.0f;
        return remaining[Index(x, y, depth)];
    }

    void Take(int x, int y, DepthLayer depth, float fraction)
    {
        if (x < 0 || x >= GRID || y < 0 || y >= GRID) return;
        float& left = remaining[Index(x, y, depth)];
        left = left - fraction < 0.0f ? 0.0f : left - fraction;
    }

    bool IsExhausted(int x, int y, DepthLayer depth) const
    {
        return Remaining(x, y, depth) <= 0.0f;
    }
};

// ---------------------------------------------------------------------------
// One tick of digging
// ---------------------------------------------------------------------------

// What a tick of work produced. `yield` IS the composition handed onward --
// how dirty the dig was needs no separate field, because a dirtier dig simply
// carries more of everything that is not the target.
struct DigResult
{
    std::map<ResourceType, float> yield;

    float totalMass = 0.0f;     // everything loaded, target and waste alike
    float targetMass = 0.0f;    // just the resource being aimed at
    float powerDraw = 0.0f;
    float wearDelta = 0.0f;

    // Fraction of the spot this tick used up. The ENGINE does not apply it --
    // the caller does, so the engine stays pure and can be run speculatively
    // (which is what the AI needs to compare spots without digging them).
    float depletionFraction = 0.0f;

    bool  throttledByPower = false;  // the power cap held the pace back
    bool  spotExhausted = false;     // the spot ran out during this tick
    float effectivePace = 0.0f;      // after the power cap was applied
};
