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
    // One implementation, owned by prospecting -- every input is prospecting
    // state, and a second copy here would be free to drift. The weights it
    // uses are the same numbers EXC_*_CONFIDENCE_WEIGHT now alias.
    return ::GetDepthConfidence(grid, tray, x, y, depth);
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
