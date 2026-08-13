#include "estimate_engine.h"
#include "excavation_constants.h"
#include "site_view.h"
#include "prospecting_grid.h"
#include "sample_tray.h"

#include <algorithm>

uint32_t EstimateEngine::HashSeed(int parentGridX, int parentGridY,
                                  int x, int y, int depth, int resourceIdx)
{
    uint32_t h = 2166136261u;
    h ^= static_cast<uint32_t>(parentGridX); h *= 16777619u;
    h ^= static_cast<uint32_t>(parentGridY); h *= 16777619u;
    h ^= static_cast<uint32_t>(x);           h *= 16777619u;
    h ^= static_cast<uint32_t>(y);           h *= 16777619u;
    h ^= static_cast<uint32_t>(depth);       h *= 16777619u;
    h ^= static_cast<uint32_t>(resourceIdx); h *= 16777619u;
    return h;
}

float EstimateEngine::StableOffset(int parentGridX, int parentGridY,
                                   int x, int y, DepthLayer depth,
                                   ResourceType target)
{
    uint32_t h = HashSeed(parentGridX, parentGridY, x, y,
                          static_cast<int>(depth), static_cast<int>(target));

    // 0-1, then mapped to -1..+1. |offset| <= 1 is what guarantees the range
    // always contains the true value (see EstimateAt).
    float unit = static_cast<float>(h % 100000u) / 100000.0f;
    return unit * 2.0f - 1.0f;
}

SpotEstimate EstimateEngine::EstimateAt(float trueValue, float confidence,
                                        int parentGridX, int parentGridY,
                                        int x, int y, DepthLayer depth,
                                        ResourceType target) const
{
    SpotEstimate estimate;
    estimate.confidence = std::clamp(confidence, 0.0f, 1.0f);

    // How wide the uncertainty is. Shrinks to nothing at full confidence,
    // which is Rule 2.
    estimate.halfWidth = trueValue * EXC_MAX_ESTIMATE_SPREAD *
                         (1.0f - estimate.confidence);

    // Where inside that uncertainty the reading happens to land. Stable per
    // spot, so re-reading a spot never changes the answer (Rule 1).
    float offset = StableOffset(parentGridX, parentGridY, x, y, depth, target);

    estimate.shown = trueValue + offset * estimate.halfWidth;
    estimate.low   = std::max(0.0f, estimate.shown - estimate.halfWidth);
    estimate.high  = estimate.shown + estimate.halfWidth;

    // Because |offset| <= 1, trueValue always lies within [shown - halfWidth,
    // shown + halfWidth]. The instrument is imprecise, never wrong.

    estimate.isCertain = estimate.confidence >= EXC_CERTAIN_CONFIDENCE;
    if (estimate.isCertain)
    {
        estimate.shown = trueValue;
        estimate.low = trueValue;
        estimate.high = trueValue;
        estimate.halfWidth = 0.0f;
    }

    return estimate;
}

SpotEstimate EstimateEngine::Estimate(const ProspectingGrid& grid,
                                      const SampleTray& tray,
                                      const SiteView& site,
                                      int x, int y, DepthLayer depth,
                                      ResourceType target) const
{
    float trueValue = site.GetTargetYield(grid, x, y, depth, target);
    float confidence = site.GetConfidence(grid, tray, x, y, depth);

    return EstimateAt(trueValue, confidence,
                      grid.GetParentGridX(), grid.GetParentGridY(),
                      x, y, depth, target);
}
