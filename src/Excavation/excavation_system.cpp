#include "excavation_system.h"
#include "excavation_constants.h"
#include "prospecting_system.h"

#include <algorithm>

ExcavationSystem::ExcavationSystem(int tier)
    : tier(std::clamp(tier, 0, 3)),
      site(std::clamp(tier, 0, 3))
{
    // Default to the centre of the lattice, which is in reach at every tier
    // because the rings are centred and nest.
    int centre = PROSPECTING_GRID_SIZE / 2;
    selectedSpotX = centre;
    selectedSpotY = centre;
}

void ExcavationSystem::SetTier(int newTier)
{
    tier = std::clamp(newTier, 0, 3);
    site.SetTier(tier);

    // A tier change only widens reach, so an existing selection stays valid.
    // Clamp anyway -- a future downgrade path would otherwise strand it.
    EnsureSelectionInReach();
}

int ExcavationSystem::GetTier() const
{
    return tier;
}

SiteView& ExcavationSystem::GetSite()
{
    return site;
}

const SiteView& ExcavationSystem::GetSite() const
{
    return site;
}

SpotView ExcavationSystem::DescribeSelected(const ProspectingSystem& prospecting) const
{
    return site.Describe(prospecting.GetGrid(), prospecting.GetTray(),
                         selectedSpotX, selectedSpotY,
                         selectedDepth, targetResource);
}

void ExcavationSystem::SelectBestReachableSpot(const ProspectingSystem& prospecting)
{
    int bestX = selectedSpotX;
    int bestY = selectedSpotY;

    if (site.FindBestReachableSpot(prospecting.GetGrid(), selectedDepth,
                                   targetResource, bestX, bestY))
    {
        selectedSpotX = bestX;
        selectedSpotY = bestY;
    }
}

void ExcavationSystem::EnsureSelectionInReach()
{
    if (site.IsInReach(selectedSpotX, selectedSpotY)) return;

    int centre = PROSPECTING_GRID_SIZE / 2;
    selectedSpotX = centre;
    selectedSpotY = centre;
}
