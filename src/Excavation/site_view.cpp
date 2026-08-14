#include "site_view.h"
#include "excavation_constants.h"
#include "prospecting_grid.h"
#include "sample_tray.h"
#include "prospecting_constants.h"

#include <algorithm>

SiteView::SiteView(int tier)
    : tier(std::clamp(tier, 0, 3))
{
}

void SiteView::SetTier(int newTier)
{
    tier = std::clamp(newTier, 0, 3);
}

int SiteView::GetTier() const
{
    return tier;
}

int SiteView::GetGridSize() const
{
    return PROSPECTING_GRID_SIZE;
}

int SiteView::GetReach() const
{
    return GetReachForTier(tier);
}

bool SiteView::IsInReach(int x, int y) const
{
    return IsSubCellInReach(x, y, tier);
}

bool SiteView::CanWorkDepth(DepthLayer depth) const
{
    int d = static_cast<int>(depth);
    return d >= 0 && d < EXC_MAX_DEPTH_PER_TIER[tier];
}

float SiteView::GetConfidence(const ProspectingGrid& grid, const SampleTray& tray,
                              int x, int y, DepthLayer depth) const
{
    int size = grid.GetGridSize();
    if (x < 0 || x >= size || y < 0 || y >= size) return 0.0f;

    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return 0.0f;

    const SubCell& cell = grid.GetSubCell(x, y);

    // --- Direct observation ---
    // A layer that has been dug was seen with its own eyes. Note this is per
    // DEPTH: digging the surface says nothing about what lies under it, which
    // is why the deep layers stay a bet long after the surface is mapped.
    if (cell.HasBeenDug(d)) return 1.0f;

    // --- Sweep evidence ---
    // A sweep only reveals the layers its frequency band penetrated, and what
    // it does reveal weakens with depth by the same attenuation the sweep
    // engine itself uses.
    float sweepConfidence = 0.0f;
    if (cell.hasBeenSwept && cell.sweepFrequencyBand >= 0)
    {
        int band = std::clamp(cell.sweepFrequencyBand, 0, SWEEP_FREQUENCY_BANDS - 1);
        if (d < SWEEP_DEPTH_PENETRATION[band])
        {
            float attenuation = 1.0f / (1.0f + d * SWEEP_DEPTH_ATTENUATION);
            sweepConfidence = cell.aggregateConfidence * attenuation *
                              EXC_SWEEP_CONFIDENCE_WEIGHT;
        }
    }

    // --- Sample evidence ---
    // Samples are direct evidence, but only for the exact spot and depth they
    // were taken at.
    float sampleConfidence = 0.0f;
    const std::vector<Sample>& samples = tray.GetSamples();
    for (const Sample& sample : samples)
    {
        if (sample.subCellX != x || sample.subCellY != y) continue;
        if (sample.depthLayer != depth) continue;

        float aggregate = sample.GetAggregateConfidence();
        sampleConfidence = std::max(sampleConfidence,
                                    aggregate * EXC_SAMPLE_CONFIDENCE_WEIGHT);
    }

    // Independent evidence combines rather than replaces.
    float combined = 1.0f - (1.0f - sweepConfidence) * (1.0f - sampleConfidence);
    return std::clamp(combined, 0.0f, 1.0f);
}

float SiteView::GetTargetYield(const ProspectingGrid& grid,
                               int x, int y, DepthLayer depth,
                               ResourceType target) const
{
    // Absolute deposit amount at this spot and depth.
    float quantity = grid.GetQuantity(x, y, depth);

    // Composition fractions (0-1, sum to ~1) -- what the deposit is made of.
    std::map<ResourceType, float> composition = grid.GetGroundTruth(x, y, depth);

    auto it = composition.find(target);
    if (it == composition.end()) return 0.0f;

    return quantity * it->second;
}

SpotView SiteView::Describe(const ProspectingGrid& grid, const SampleTray& tray,
                            int x, int y, DepthLayer depth,
                            ResourceType target) const
{
    SpotView view;
    view.x = x;
    view.y = y;
    view.depth = depth;

    view.inReach = IsInReach(x, y);
    view.tierRequired = TierRequiredForSubCell(x, y);
    view.depthAccessible = CanWorkDepth(depth);

    int size = grid.GetGridSize();
    if (x < 0 || x >= size || y < 0 || y >= size) return view;

    view.quantity = grid.GetQuantity(x, y, depth);
    view.composition = grid.GetGroundTruth(x, y, depth);
    view.targetYield = GetTargetYield(grid, x, y, depth, target);

    view.confidence = GetConfidence(grid, tray, x, y, depth);
    view.hasBeenSwept = grid.GetSubCell(x, y).hasBeenSwept;

    const std::vector<Sample>& samples = tray.GetSamples();
    for (const Sample& sample : samples)
    {
        if (sample.subCellX == x && sample.subCellY == y &&
            sample.depthLayer == depth)
        {
            view.samplesHere++;
        }
    }

    return view;
}

float SiteView::GetCellMeanYield(const ProspectingGrid& grid, DepthLayer depth,
                                 ResourceType target) const
{
    int size = grid.GetGridSize();
    float sum = 0.0f;
    int count = 0;

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            if (!IsInReach(x, y)) continue;
            sum += GetTargetYield(grid, x, y, depth, target);
            count++;
        }
    }

    return count > 0 ? sum / static_cast<float>(count) : 0.0f;
}

bool SiteView::FindBestReachableSpot(const ProspectingGrid& grid,
                                     DepthLayer depth, ResourceType target,
                                     int& outX, int& outY) const
{
    if (!CanWorkDepth(depth)) return false;

    int size = grid.GetGridSize();
    float bestYield = -1.0f;
    bool found = false;

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            if (!IsInReach(x, y)) continue;

            float yield = GetTargetYield(grid, x, y, depth, target);
            if (yield > bestYield)
            {
                bestYield = yield;
                outX = x;
                outY = y;
                found = true;
            }
        }
    }

    return found;
}
