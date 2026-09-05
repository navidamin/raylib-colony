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

SpotEstimate EstimateEngine::EstimateAt(float trueValue, float cellMean,
                                        float confidence,
                                        int parentGridX, int parentGridY,
                                        int x, int y, DepthLayer depth,
                                        ResourceType target) const
{
    SpotEstimate estimate;
    estimate.confidence = std::clamp(confidence, 0.0f, 1.0f);
    float known = estimate.confidence;

    if (cellMean <= 0.0f) cellMean = trueValue;

    // What is knowable WITHOUT surveying is roughly what this ground averages,
    // and the span any one spot could fall in -- the generator clamps a spot to
    // between 0.3x and 2.0x the average. Knowing nothing means being able to
    // say only "somewhere in that span".
    float floorValue = cellMean * EXC_SPOT_MIN_FACTOR;
    float ceilValue  = cellMean * EXC_SPOT_MAX_FACTOR;

    // Knowledge pulls both ends of the span onto the truth. At full confidence
    // they meet there and the gamble is gone, which is Rule 2.
    estimate.low  = floorValue + (trueValue - floorValue) * known;
    estimate.high = ceilValue  + (trueValue - ceilValue)  * known;
    if (estimate.low < 0.0f) estimate.low = 0.0f;

    // The reading inside that span. Stable per spot, so re-reading never
    // changes the answer (Rule 1) -- but at low confidence it says far more
    // about the instrument's quirk than about the ground.
    float offset = StableOffset(parentGridX, parentGridY, x, y, depth, target);
    float blindReading = cellMean + offset * cellMean * EXC_MAX_ESTIMATE_SPREAD;
    estimate.shown = blindReading + (trueValue - blindReading) * known;
    estimate.shown = std::clamp(estimate.shown, estimate.low, estimate.high);

    // The span is derived from the average over REACHABLE spots, but this can
    // be asked about any spot on the lattice -- and at tier 0 the average comes
    // from four cells while the question may be about one far outside them. So
    // widen the range rather than assume: the instrument may be vague, but it
    // must never rule out the truth.
    if (trueValue < estimate.low) estimate.low = trueValue;
    if (trueValue > estimate.high) estimate.high = trueValue;
    estimate.shown = std::clamp(estimate.shown, estimate.low, estimate.high);

    estimate.halfWidth = (estimate.high - estimate.low) * 0.5f;

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
    float cellMean = site.GetCellMeanYield(grid, depth, target);
    float confidence = site.GetConfidence(grid, tray, x, y, depth);

    return EstimateAt(trueValue, cellMean, confidence,
                      grid.GetParentGridX(), grid.GetParentGridY(),
                      x, y, depth, target);
}
