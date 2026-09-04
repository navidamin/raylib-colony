#include <cmath>
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

void ExcavationSystem::UpdatePlateLight(int hoveredLayer, int activeLayer, float dt)
{
    // Exponential approach, framerate-independent: the same wall-clock rise
    // whether the panel runs at 30 or 144. Lit: the plate under the pointer,
    // and the plate being worked -- the depth you are digging is live whether
    // or not you are pointing at it. Either may be -1.
    float k = 1.0f - std::exp(-std::max(dt, 0.0f) / PLATE_LIGHT_TAU_S);
    for (int L = 0; L < 4; L++)
    {
        float target = (L == hoveredLayer || L == activeLayer)
                     ? PLATE_LIGHT_FULL : PLATE_REST_LIGHT[L];
        plateLight[L] += (target - plateLight[L]) * k;
    }
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


void ExcavationSystem::SelectBestReachableSpot(const ProspectingSystem& prospecting)
{
    const ProspectingGrid& grid = prospecting.GetGrid();

    // Weighted by what is LEFT, not by what was originally there. Without that
    // this keeps returning a spot that has already been dug out -- which had
    // the operation grinding away at dead ground and producing nothing, while
    // reporting itself as working the richest spot on the lattice.
    float bestScore = -1.0f;
    int bestX = -1;
    int bestY = -1;

    for (int y = 0; y < grid.GetGridSize(); y++)
    {
        for (int x = 0; x < grid.GetGridSize(); x++)
        {
            if (!site.IsInReach(x, y)) continue;

            float left = worked.Remaining(x, y, selectedDepth);
            if (left <= 0.0f) continue;

            float score = site.GetTargetYield(grid, x, y, selectedDepth,
                                              targetResource) * left;
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

    // This layer is worked out. Go deeper if anything can.
    int next = static_cast<int>(selectedDepth) + 1;
    if (next < 4 && site.CanWorkDepth(static_cast<DepthLayer>(next)))
    {
        selectedDepth = static_cast<DepthLayer>(next);
        SelectBestReachableSpot(prospecting);
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

AiLevel ExcavationSystem::MaxAiLevel() const
{
    return AutoPilot::MaxLevelForTier(tier);
}

AutoDecision ExcavationSystem::PreviewAuto(const ProspectingSystem& prospecting) const
{
    return autoPilot.Decide(prospecting.GetGrid(), prospecting.GetTray(), site,
                            estimator, worked, aiLevel, tier,
                            targetResource, selectedDepth,
                            selectedSpotX, selectedSpotY);
}

void ExcavationSystem::SyncToGround(const ProspectingSystem& prospecting)
{
    EnsureTargetPresent(prospecting);

    if (aiLevel != AiLevel::OFF)
    {
        // The automation drives spot, depth, machine and pace. The power cap
        // stays the player's -- it is a standing constraint on the operation
        // rather than part of running it.
        lastDecision = PreviewAuto(prospecting);
        if (lastDecision.valid)
        {
            selectedSpotX = lastDecision.spotX;
            selectedSpotY = lastDecision.spotY;
            selectedDepth = lastDecision.depth;
            activeMachine = lastDecision.machine;
            pace = lastDecision.pace;
        }
        return;
    }

    lastDecision = AutoDecision();

    // Manual: the player picks the spot, but a worked-out one still advances,
    // so walking away never stalls the unit entirely.
    if (autoMachine)
    {
        SelectAutoMachine(prospecting);
    }
}

void ExcavationSystem::EnsureTargetPresent(const ProspectingSystem& prospecting)
{
    const ProspectingGrid& grid = prospecting.GetGrid();
    auto composition = grid.GetGroundTruth(selectedSpotX, selectedSpotY, selectedDepth);

    auto it = composition.find(targetResource);
    if (it != composition.end() && it->second > 0.02f) return;

    // Fall back to whatever this spot is mostly made of.
    ResourceType best = targetResource;
    float bestFraction = 0.0f;
    for (const auto& [type, fraction] : composition)
    {
        if (fraction > bestFraction)
        {
            bestFraction = fraction;
            best = type;
        }
    }
    targetResource = best;
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

DigResult ExcavationSystem::Dig(ProspectingSystem& prospecting,
                                int machineCount, float externalMultiplier,
                                float deltaTime)
{
    SyncToGround(prospecting);

    // A machine that cannot reach this depth digs nothing at all, which would
    // silently stall an unattended unit. Fall back to the deepest layer the
    // active machine can actually work.
    if (!DigEngine::CanMachineWorkDepth(activeMachine, selectedDepth))
    {
        int deepest = DigEngine::GetMachine(activeMachine).maxDepthLayers - 1;
        selectedDepth = static_cast<DepthLayer>(std::clamp(deepest, 0, 3));
    }

    // Automation costs efficiency against a player making the same calls.
    float aiEfficiency = AutoPilot::EfficiencyFor(
        aiLevel == AiLevel::OFF ? AiLevel::OFF
                                : std::min(aiLevel, MaxAiLevel()));

    lastResult = digger.Dig(prospecting.GetGrid(), site, worked,
                            selectedSpotX, selectedSpotY, selectedDepth,
                            targetResource, activeMachine, machineCount,
                            pace, powerCap, externalMultiplier * aiEfficiency,
                            deltaTime);

    worked.Take(selectedSpotX, selectedSpotY, selectedDepth,
                lastResult.depletionFraction);

    // Tell prospecting what was dug. A dug layer is observed directly, so the
    // spot stops being a guess -- and it reads differently from a surveyed one,
    // because it also says how much has been taken out.
    if (lastResult.depletionFraction > 0.0f)
    {
        prospecting.GetGrid().RecordExcavation(selectedSpotX, selectedSpotY,
                                               selectedDepth,
                                               lastResult.depletionFraction);
    }

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
