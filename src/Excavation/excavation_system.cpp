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

const EstimateEngine& ExcavationSystem::GetEstimator() const
{
    return estimator;
}

SpotEstimate ExcavationSystem::EstimateSelected(const ProspectingSystem& prospecting) const
{
    return estimator.Estimate(prospecting.GetGrid(), prospecting.GetTray(), site,
                              selectedSpotX, selectedSpotY,
                              selectedDepth, targetResource);
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

const DigSite& ExcavationSystem::GetWorked() const
{
    return worked;
}

const DigEngine& ExcavationSystem::GetDigger() const
{
    return digger;
}

const DigResult& ExcavationSystem::GetLastResult() const
{
    return lastResult;
}

bool ExcavationSystem::IsMachineAvailable(MachineId id) const
{
    return DigEngine::IsMachineAvailable(id, tier);
}

const Machine& ExcavationSystem::GetActiveMachine() const
{
    return DigEngine::GetMachine(activeMachine);
}

void ExcavationSystem::SelectAutoMachine(const ProspectingSystem& prospecting)
{
    // Pick by what the ground and the survey actually call for, rather than by
    // a fixed "best machine per tier" list -- otherwise AUTO would make the
    // machine bay pointless at every tier.
    //
    // Well-surveyed ground rewards precision, because you know which spot you
    // want. Unsurveyed ground rewards reach and volume, because you do not --
    // covering ground beats aiming when you cannot aim.
    float confidence = site.GetConfidence(prospecting.GetGrid(), prospecting.GetTray(),
                                          selectedSpotX, selectedSpotY, selectedDepth);

    MachineId best = MachineId::SCOOP;
    float bestScore = -1.0f;

    for (int i = 0; i < EXC_MACHINE_TABLE_SIZE; i++)
    {
        MachineId id = static_cast<MachineId>(i);
        if (!IsMachineAvailable(id)) continue;
        if (!DigEngine::CanMachineWorkDepth(id, selectedDepth)) continue;

        const Machine& machine = DigEngine::GetMachine(id);

        // Aiming is worth what the survey is worth; volume always counts.
        float aimValue = (machine.precision + machine.selectivity) * confidence;
        float volumeValue = machine.paceCeiling * (1.0f - confidence * 0.5f);
        float score = aimValue + volumeValue - machine.wearRate * 0.15f;

        if (score > bestScore)
        {
            bestScore = score;
            best = id;
        }
    }

    activeMachine = best;
}

DigResult ExcavationSystem::Dig(const ProspectingSystem& prospecting,
                                int machineCount, float externalMultiplier,
                                float deltaTime)
{
    if (autoMachine)
    {
        SelectAutoMachine(prospecting);
    }

    // A machine that cannot reach this depth digs nothing at all, which would
    // silently stall an unattended unit. Fall back to the deepest layer the
    // active machine can actually work.
    if (!DigEngine::CanMachineWorkDepth(activeMachine, selectedDepth))
    {
        int deepest = DigEngine::GetMachine(activeMachine).maxDepthLayers - 1;
        selectedDepth = static_cast<DepthLayer>(std::clamp(deepest, 0, 3));
    }

    lastResult = digger.Dig(prospecting.GetGrid(), site, worked,
                            selectedSpotX, selectedSpotY, selectedDepth,
                            targetResource, activeMachine, machineCount,
                            pace, powerCap, externalMultiplier, deltaTime);

    worked.Take(selectedSpotX, selectedSpotY, selectedDepth,
                lastResult.depletionFraction);

    // When a spot runs dry, move on rather than stalling. The player can always
    // override; this only stops an unattended unit producing nothing forever.
    if (worked.IsExhausted(selectedSpotX, selectedSpotY, selectedDepth))
    {
        MoveToNextSpot(prospecting);
    }

    return lastResult;
}

void ExcavationSystem::MoveToNextSpot(const ProspectingSystem& prospecting)
{
    const ProspectingGrid& grid = prospecting.GetGrid();

    // Best remaining spot this tier can reach, by KNOWN value -- what the
    // player has been told, not the truth. Digging blind should stay blind.
    float bestScore = -1.0f;
    int bestX = -1;
    int bestY = -1;

    for (int y = 0; y < grid.GetGridSize(); y++)
    {
        for (int x = 0; x < grid.GetGridSize(); x++)
        {
            if (!site.IsInReach(x, y)) continue;
            if (worked.IsExhausted(x, y, selectedDepth)) continue;

            SpotEstimate estimate = estimator.Estimate(grid, prospecting.GetTray(),
                                                       site, x, y, selectedDepth,
                                                       targetResource);
            float score = estimate.shown * worked.Remaining(x, y, selectedDepth);
            if (score > bestScore)
            {
                bestScore = score;
                bestX = x;
                bestY = y;
            }
        }
    }

    if (bestX >= 0)
    {
        selectedSpotX = bestX;
        selectedSpotY = bestY;
        return;
    }

    // This whole layer is worked out -- go deeper if anything can.
    int next = static_cast<int>(selectedDepth) + 1;
    if (next < 4 && site.CanWorkDepth(static_cast<DepthLayer>(next)))
    {
        selectedDepth = static_cast<DepthLayer>(next);
        MoveToNextSpot(prospecting);
    }
}
