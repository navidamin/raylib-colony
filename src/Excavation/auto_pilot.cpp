#include "auto_pilot.h"
#include "excavation_constants.h"
#include "site_view.h"
#include "estimate_engine.h"
#include "dig_engine.h"
#include "prospecting_grid.h"
#include "sample_tray.h"

#include <algorithm>

AiLevel AutoPilot::MaxLevelForTier(int tier)
{
    tier = std::clamp(tier, 0, 3);
    return EXC_AI_MAX_LEVEL_PER_TIER[tier];
}

float AutoPilot::EfficiencyFor(AiLevel level)
{
    int index = std::clamp(static_cast<int>(level), 0,
                           static_cast<int>(AiLevel::COUNT) - 1);
    return EXC_AI_EFFICIENCY[index];
}

const char* AutoPilot::LevelName(AiLevel level)
{
    switch (level)
    {
        case AiLevel::OFF:     return "MANUAL";
        case AiLevel::BASIC:   return "BASIC";
        case AiLevel::TRAINED: return "TRAINED";
        case AiLevel::EXPERT:  return "EXPERT";
        default:               return "MANUAL";
    }
}

const char* AutoPilot::LevelDescription(AiLevel level)
{
    switch (level)
    {
        case AiLevel::OFF:     return "you choose the spot and the pace";
        case AiLevel::BASIC:   return "picks by the worst case -- cautious";
        case AiLevel::TRAINED: return "picks by the upside -- explores unknown ground";
        case AiLevel::EXPERT:  return "explores, and says where surveying would pay";
        default:               return "";
    }
}

float AutoPilot::ScoreSpot(const ProspectingGrid& grid, const SampleTray& tray,
                           const SiteView& site, const EstimateEngine& estimator,
                           const DigSite& worked,
                           AiLevel level, int x, int y, DepthLayer depth,
                           ResourceType target) const
{
    if (!site.IsInReach(x, y)) return -1.0f;

    float left = worked.Remaining(x, y, depth);
    if (left <= 0.0f) return -1.0f;

    SpotEstimate estimate = estimator.Estimate(grid, tray, site, x, y, depth, target);

    float value = 0.0f;
    if (level == AiLevel::BASIC)
    {
        // The bottom of the range: what the spot is guaranteed to hold
        // whatever the truth turns out to be. Basic optimises the worst case,
        // so wide uncertainty counts against a spot -- a wide range has a low
        // floor however promising its middle looks.
        value = estimate.low;
    }
    else
    {
        // The middle, plus a slice of the width. A wide unknown scores ABOVE a
        // narrow known of the same midpoint, so uncertainty attracts rather
        // than repels -- which is the only way to find anything on ground
        // nobody has surveyed.
        value = estimate.shown + EXC_AI_EXPLORE_BONUS * estimate.halfWidth;
    }

    // A nearly-worked-out spot is worth less than a fresh one holding the same.
    return value * left;
}

MachineId AutoPilot::ChooseMachine(AiLevel level, int tier, DepthLayer depth,
                                   float confidence) const
{
    MachineId best = MachineId::SCOOP;
    float bestScore = -1.0f;

    for (int i = 0; i < EXC_MACHINE_TABLE_SIZE; i++)
    {
        MachineId id = static_cast<MachineId>(i);
        if (!DigEngine::IsMachineAvailable(id, tier)) continue;
        if (!DigEngine::CanMachineWorkDepth(id, depth)) continue;

        const Machine& machine = DigEngine::GetMachine(id);

        // Aiming is worth exactly what the survey is worth: on ground you
        // understand, precision collects the payoff; on ground you do not, it
        // buys nothing and volume is what finds things.
        float aimValue = (machine.precision + machine.selectivity) * confidence;
        float volumeValue = machine.paceCeiling * (1.0f - confidence * 0.5f);
        float score = aimValue + volumeValue - machine.wearRate * 0.15f;

        // Basic will not run a machine that eats itself quickly -- it optimises
        // for not going wrong rather than for going well.
        if (level == AiLevel::BASIC) score -= machine.wearRate * 0.35f;

        if (score > bestScore)
        {
            bestScore = score;
            best = id;
        }
    }

    return best;
}

AutoDecision AutoPilot::Decide(const ProspectingGrid& grid, const SampleTray& tray,
                               const SiteView& site, const EstimateEngine& estimator,
                               const DigSite& worked,
                               AiLevel level, int tier,
                               ResourceType target, DepthLayer currentDepth) const
{
    AutoDecision decision;
    decision.depth = currentDepth;
    decision.efficiency = EfficiencyFor(level);

    if (level == AiLevel::OFF) return decision;

    // Cap the level at what this tier can actually run, so a saved higher
    // setting cannot outrun a downgraded module.
    AiLevel capped = level;
    if (static_cast<int>(capped) > static_cast<int>(MaxLevelForTier(tier)))
    {
        capped = MaxLevelForTier(tier);
        decision.efficiency = EfficiencyFor(capped);
    }

    int gridSize = grid.GetGridSize();

    // --- Where to dig ---
    // Search from the current layer downward: a worked-out layer should push
    // the operation deeper rather than stall it.
    float bestScore = -1.0f;
    for (int d = static_cast<int>(currentDepth); d < 4; d++)
    {
        DepthLayer depth = static_cast<DepthLayer>(d);
        if (!site.CanWorkDepth(depth)) break;

        for (int y = 0; y < gridSize; y++)
        {
            for (int x = 0; x < gridSize; x++)
            {
                float score = ScoreSpot(grid, tray, site, estimator, worked,
                                        capped, x, y, depth, target);
                if (score > bestScore)
                {
                    bestScore = score;
                    decision.spotX = x;
                    decision.spotY = y;
                    decision.depth = depth;
                }
            }
        }

        if (bestScore > 0.0f) break;   // found something workable at this depth
    }

    if (decision.spotX < 0)
    {
        // Nothing scored above zero -- every reachable spot is worked out, or
        // holds none of the target. Fall back to the centre so the unit keeps
        // producing rather than sitting idle.
        int centre = gridSize / 2;
        decision.spotX = centre;
        decision.spotY = centre;
    }

    decision.valid = true;

    // --- What to dig it with ---
    float confidence = site.GetConfidence(grid, tray, decision.spotX, decision.spotY,
                                          decision.depth);
    decision.machine = ChooseMachine(capped, tier, decision.depth, confidence);

    // --- How hard ---
    const Machine& machine = DigEngine::GetMachine(decision.machine);
    if (capped == AiLevel::BASIC)
    {
        decision.pace = machine.paceCeiling * EXC_AI_BASIC_PACE;
    }
    else
    {
        // Push where the ground is understood, ease off where it is not:
        // pace costs selectivity, and being indiscriminate is most expensive
        // when you cannot tell what you are picking up.
        float fraction = EXC_AI_PACE_FLOOR + (1.0f - EXC_AI_PACE_FLOOR) * confidence;
        decision.pace = machine.paceCeiling * fraction;
    }

    // --- Where knowing more would pay ---
    if (capped == AiLevel::EXPERT)
    {
        // The spot whose uncertainty could most improve on the best decision
        // currently available. Not the widest range in the cell -- the widest
        // range that could actually beat what is being worked.
        float bestUpside = 0.0f;
        for (int y = 0; y < gridSize; y++)
        {
            for (int x = 0; x < gridSize; x++)
            {
                if (!site.IsInReach(x, y)) continue;
                if (worked.Remaining(x, y, decision.depth) <= 0.0f) continue;

                SpotEstimate e = estimator.Estimate(grid, tray, site, x, y,
                                                    decision.depth, target);
                if (e.isCertain) continue;

                float upside = e.high - e.shown;
                if (upside > bestUpside)
                {
                    bestUpside = upside;
                    decision.surveyHintX = x;
                    decision.surveyHintY = y;
                }
            }
        }

        decision.surveyGain = bestUpside;

        // Only raise it when it would actually change something.
        if (bestScore > 0.0f && bestUpside < bestScore * EXC_AI_SURVEY_HINT_THRESHOLD)
        {
            decision.surveyHintX = -1;
            decision.surveyHintY = -1;
            decision.surveyGain = 0.0f;
        }
    }

    return decision;
}
