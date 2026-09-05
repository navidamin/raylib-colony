#pragma once

#include <cstdint>
#include "excavation_types.h"

class ProspectingGrid;
class SampleTray;
class SiteView;

// What the player is told is in a spot, as opposed to what is actually there.
//
// The range always CONTAINS the true value -- the instrument is imprecise, not
// lying. What moves is where inside that range the point estimate sits, and how
// wide the range is.
struct SpotEstimate
{
    float shown = 0.0f;       // the point estimate the player reads
    float low = 0.0f;         // range floor (never below zero)
    float high = 0.0f;        // range ceiling
    float halfWidth = 0.0f;   // how wide the uncertainty is, in absolute terms
    float confidence = 0.0f;  // 0-1, for this spot AND depth
    bool  isCertain = false;  // range has closed -- no gamble left here
};

// Turns ground truth into what the player is shown.
//
// Pure logic and stateless. Rules 1 and 2 of the design live here:
//
//   Rule 1  Truth is fixed; only knowledge changes. The bias applied to a spot
//           is a stable hash of its coordinates, so the same spot always reads
//           the same wrong number until it is surveyed further. Never rand(),
//           never re-rolled per tick -- otherwise the gamble is a slot machine
//           rather than a hidden fact.
//
//   Rule 2  Confidence 1.0 means no gamble. The range closes onto the truth,
//           and excavation becomes pure optimisation. That is the reward for
//           surveying, not a failure of the design.
class EstimateEngine
{
public:
    // What the player is shown for one resource at one spot and depth.
    SpotEstimate Estimate(const ProspectingGrid& grid, const SampleTray& tray,
                          const SiteView& site,
                          int x, int y, DepthLayer depth,
                          ResourceType target) const;

    // Estimate built from a confidence and a cell average supplied by the
    // caller. Used by tests and by the AI, which needs to ask "what would this
    // read at confidence X".
    //
    // `cellMean` matters more than it looks. Blurring around the SPOT's own
    // value leaves a rich spot reading rich even unsurveyed -- measured, a
    // zero-confidence estimate built that way already captured 95% of the best
    // spot's value, so surveying could add almost nothing and the module's
    // central claim was false. Blurring toward the cell average instead means
    // an unsurveyed spot reads like average ground, whatever it really holds.
    SpotEstimate EstimateAt(float trueValue, float cellMean, float confidence,
                            int parentGridX, int parentGridY,
                            int x, int y, DepthLayer depth,
                            ResourceType target) const;

    // Stable bias in [-1, +1] for a spot. Same inputs always give the same
    // value, in this run and in any other run of the same world.
    static float StableOffset(int parentGridX, int parentGridY,
                              int x, int y, DepthLayer depth,
                              ResourceType target);

private:
    // FNV-1a, matching the pattern ProspectingGrid::HashSeed uses so the two
    // modules derive stable values the same way.
    static uint32_t HashSeed(int parentGridX, int parentGridY,
                             int x, int y, int depth, int resourceIdx);
};
