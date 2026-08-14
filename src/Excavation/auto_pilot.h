#pragma once

#include "excavation_types.h"

class ProspectingGrid;
class SampleTray;
class SiteView;
class EstimateEngine;

// Decides where to dig, with what, and how hard.
//
// Pure logic and stateless: it takes the world and returns a decision, without
// applying it. That is what lets the panel show what the automation WOULD do,
// and what lets the tests check its judgement without digging anything.
//
// The whole ladder turns on one question -- how to treat ground you have not
// surveyed:
//
//   BASIC    scores a spot by the BOTTOM of its range. An unsurveyed spot has
//            a bottom near zero, so Basic simply never works one. Safe, and
//            blind to everything it has not been shown.
//
//   TRAINED  scores by the middle of the range plus a slice of its width, so a
//            wide unknown reads as worth investigating rather than worth
//            avoiding. This is the level that gambles -- and on ground nobody
//            has surveyed, gambling is the only way to find anything.
//
//   EXPERT   scores like Trained, and additionally reports where resolving the
//            uncertainty would most change its own decision. It cannot survey
//            (that is prospecting's module), but it can say where to look.
//
// The player's own choices are always better than any of them: automation
// carries an efficiency penalty that shrinks with level but never vanishes.
class AutoPilot
{
public:
    // Highest level this module tier can run.
    static AiLevel MaxLevelForTier(int tier);

    // What this level costs against a player making the same call.
    static float EfficiencyFor(AiLevel level);

    static const char* LevelName(AiLevel level);
    static const char* LevelDescription(AiLevel level);

    // Decide this tick. `currentDepth` is where the operation is working now;
    // the decision may move it deeper when a layer is worked out.
    // `currentX/Y` is the spot being worked now. The decision stays on it until
    // the face is spent -- see EXC_AI_ABANDON_BELOW for why that is a "work it
    // out" rule rather than a margin. Pass -1 when there is no current spot.
    AutoDecision Decide(const ProspectingGrid& grid, const SampleTray& tray,
                        const SiteView& site, const EstimateEngine& estimator,
                        const DigSite& worked,
                        AiLevel level, int tier,
                        ResourceType target, DepthLayer currentDepth,
                        int currentX = -1, int currentY = -1) const;

private:
    // Score one spot under a level's attitude to uncertainty.
    float ScoreSpot(const ProspectingGrid& grid, const SampleTray& tray,
                    const SiteView& site, const EstimateEngine& estimator,
                    const DigSite& worked,
                    AiLevel level, int x, int y, DepthLayer depth,
                    ResourceType target) const;

    // Best machine for this ground, given how well it is understood.
    MachineId ChooseMachine(AiLevel level, int tier, DepthLayer depth,
                            float confidence) const;
};
