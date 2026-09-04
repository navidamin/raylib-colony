#include "dig_engine.h"
#include "excavation_constants.h"
#include "site_view.h"
#include "prospecting_grid.h"

#include <algorithm>

const Machine& DigEngine::GetMachine(MachineId id)
{
    int index = static_cast<int>(id);
    if (index < 0 || index >= EXC_MACHINE_TABLE_SIZE)
    {
        return EXC_MACHINES[0];
    }
    return EXC_MACHINES[index];
}

bool DigEngine::IsMachineAvailable(MachineId id, int tier)
{
    // Tier only. The tech that unlocks a machine is the same tech that unlocks
    // the module tier carrying it (the EXCAVATION module's tierDependencies),
    // so checking UnlockRegistry here would gate the same thing twice -- and it
    // made every machine unavailable in the preview and playtest harnesses,
    // which reach high tiers through DebugUpgradeModuleTier precisely because
    // that bypasses tech. requiredTech is kept on the table as documentation of
    // which tech the tier depends on.
    return tier >= GetMachine(id).requiredTier;
}

bool DigEngine::CanMachineWorkDepth(MachineId id, DepthLayer depth)
{
    int d = static_cast<int>(depth);
    return d >= 0 && d < GetMachine(id).maxDepthLayers;
}

std::map<ResourceType, float> DigEngine::BlendedComposition(const ProspectingGrid& grid,
                                                            const SiteView& site,
                                                            int x, int y,
                                                            DepthLayer depth,
                                                            float precision) const
{
    std::map<ResourceType, float> blended = grid.GetGroundTruth(x, y, depth);

    // A perfectly precise machine takes only what you aimed at.
    if (precision >= 0.999f) return blended;

    // Everything else also bites into the neighbours. Only spots that are
    // themselves in reach count -- the machine cannot dig outside the sect's
    // ring just because it is sloppy.
    float spill = 1.0f - precision;
    int gridSize = grid.GetGridSize();

    std::map<ResourceType, float> neighbourSum;
    int neighbours = 0;

    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0) continue;

            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= gridSize || ny < 0 || ny >= gridSize) continue;
            if (!site.IsInReach(nx, ny)) continue;

            for (const auto& [type, fraction] : grid.GetGroundTruth(nx, ny, depth))
            {
                neighbourSum[type] += fraction;
            }
            neighbours++;
        }
    }

    if (neighbours == 0) return blended;

    // Weighted average of the aimed spot and the mean of its neighbours.
    for (auto& [type, fraction] : blended)
    {
        fraction *= precision;
    }
    for (const auto& [type, sum] : neighbourSum)
    {
        blended[type] += spill * (sum / static_cast<float>(neighbours));
    }

    return blended;
}

DigResult DigEngine::Dig(const ProspectingGrid& grid, const SiteView& site,
                         const DigSite& worked,
                         int x, int y, DepthLayer depth,
                         ResourceType target,
                         MachineId machineId, int machineCount,
                         float pace, float powerCap,
                         float externalMultiplier, float deltaTime) const
{
    DigResult result;

    // Stamped before every early return: a tick that dug nothing still dug
    // nothing OVER AN INTERVAL, and a reader averaging rates needs that zero
    // to weigh the same as a productive tick of the same length.
    result.dtSeconds = std::max(0.0f, deltaTime);

    const Machine& machine = GetMachine(machineId);

    // --- Can this dig happen at all? ---
    if (!site.IsInReach(x, y)) return result;
    if (!site.CanWorkDepth(depth)) return result;
    if (!CanMachineWorkDepth(machineId, depth)) return result;

    float left = worked.Remaining(x, y, depth);
    if (left <= 0.0f)
    {
        result.spotExhausted = true;
        return result;
    }

    machineCount = std::max(1, machineCount);
    pace = std::clamp(pace, 0.0f, machine.paceCeiling);

    // --- Power, and the cap the player set ---
    // The floor is unavoidable; pace is what the cap can throttle.
    float floorDraw = machine.powerFloor * machineCount;
    float wantedDraw = floorDraw + pace * EXC_POWER_PER_PACE * machineCount;

    float effectivePace = pace;
    if (powerCap > 0.0f && wantedDraw > powerCap)
    {
        float spare = powerCap - floorDraw;
        if (spare <= 0.0f)
        {
            // The cap will not even cover idling, so nothing gets dug.
            result.powerDraw = std::min(floorDraw, powerCap);
            result.throttledByPower = true;
            return result;
        }
        effectivePace = spare / (EXC_POWER_PER_PACE * machineCount);
        effectivePace = std::clamp(effectivePace, 0.0f, machine.paceCeiling);
        result.throttledByPower = true;
    }

    result.effectivePace = effectivePace;
    result.powerDraw = floorDraw + effectivePace * EXC_POWER_PER_PACE * machineCount;

    if (effectivePace <= 0.0f) return result;

    // --- How much ground gets moved ---
    float hardness = EXC_DEPTH_HARDNESS[std::clamp(static_cast<int>(depth), 0, 3)];

    float movedMass = EXC_BASE_DIG_MASS
                    * effectivePace
                    * hardness
                    * machineCount
                    * externalMultiplier
                    * deltaTime;

    // A nearly-empty spot gives up less, but the taper is floored: without a
    // floor, depletion approaches zero as the spot empties and it never
    // actually runs out.
    float taper = std::max(EXC_MIN_TAPER, std::min(1.0f, left * 4.0f));
    movedMass *= taper;
    if (movedMass <= 0.0f) return result;

    // --- What is in it ---
    std::map<ResourceType, float> composition =
        BlendedComposition(grid, site, x, y, depth, machine.precision);

    // Pushing the pace costs selectivity -- no time to be choosy.
    float paceFraction = machine.paceCeiling > 0.0f
                       ? effectivePace / machine.paceCeiling : 0.0f;
    float effectiveSelectivity = machine.selectivity *
                                 (1.0f - paceFraction * EXC_PACE_SELECTIVITY_PENALTY);
    effectiveSelectivity = std::clamp(effectiveSelectivity, 0.0f, 1.0f);

    float wasteKept = 1.0f - effectiveSelectivity * EXC_MAX_WASTE_REJECTION;

    // Target comes up whole; waste is partly left behind. A selective machine
    // therefore moves LESS total mass but hands on a better mix -- which is the
    // whole trade, expressed in the composition rather than a purity number.
    for (const auto& [type, fraction] : composition)
    {
        float mass = movedMass * fraction;
        if (type != target) mass *= wasteKept;
        if (mass <= 0.0f) continue;

        result.yield[type] += mass;
        result.totalMass += mass;
        if (type == target) result.targetMass += mass;
    }

    // --- Wear and depletion ---
    result.wearDelta = machine.wearRate * effectivePace * machineCount *
                       deltaTime * 0.001f;

    // Depletion tracks everything disturbed, including waste left in place --
    // the ground is dug over either way. The caller applies it.
    result.depletionFraction = movedMass * EXC_DEPLETION_PER_MASS;
    result.spotExhausted = (left - result.depletionFraction) <= 0.0f;

    return result;
}
