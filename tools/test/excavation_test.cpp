// Asserts the excavation invariants that are easy to break silently.
//
// There is no test framework in this repo, so this is a plain executable with
// a Check() helper. It exits non-zero on the first failure, which is enough to
// wire into CI later.
//
//   cmake --build build --target colony_test && ./build/src/colony_test
//
// What is worth testing here is not arithmetic -- it is the two design rules
// that stop the gamble becoming a slot machine:
//
//   Rule 1  A spot's estimate is STABLE. Reading it again, or rebuilding the
//           world from the same seed, must give the same answer. If this ever
//           regresses to rand() the module still "works" and quietly stops
//           being a game about information.
//   Rule 2  Confidence 1.0 closes the range onto the truth.
//
// Plus the units trap the architecture guide calls this codebase's most
// expensive bug: yield must be quantity x composition, not either alone.

#include "resource_manager.h"
#include "prospecting_grid.h"
#include "sample_tray.h"
#include "site_view.h"
#include "estimate_engine.h"
#include "dig_engine.h"
#include "survey_progress_engine.h"
#include "game_constants.h"

#include <cstdio>
#include <cmath>
#include <iostream>
#include <streambuf>

static const unsigned int TEST_MAP_SEED = 20260813u;

// ResourceManager narrates its generation to stdout, which buries the test
// results. Mute it for the duration of a scope.
class Quiet
{
public:
    Quiet() : saved(std::cout.rdbuf()) { std::cout.rdbuf(nullptr); }
    ~Quiet() { std::cout.rdbuf(saved); }
private:
    std::streambuf* saved;
};

// Builds a world at the shared test seed, quietly.
static void BuildWorld(ResourceManager& rm)
{
    Quiet quiet;
    rm.GenerateResourceMap(TEST_MAP_SEED);
}

static int failures = 0;
static int checks = 0;

static void Check(bool condition, const char* what)
{
    checks++;
    if (!condition)
    {
        failures++;
        printf("  FAIL  %s\n", what);
    }
}

static bool Near(float a, float b, float tolerance = 1e-4f)
{
    return std::fabs(a - b) <= tolerance * std::max(1.0f, std::fabs(a));
}

// ---------------------------------------------------------------------------

static void TestOffsetIsStable()
{
    printf("Rule 1 -- the bias on a spot is stable\n");

    float first = EstimateEngine::StableOffset(5, 5, 3, 4, DepthLayer::MID,
                                               ResourceType::Fe);

    for (int i = 0; i < 1000; i++)
    {
        float again = EstimateEngine::StableOffset(5, 5, 3, 4, DepthLayer::MID,
                                                   ResourceType::Fe);
        if (!Near(first, again))
        {
            Check(false, "offset changed between calls");
            return;
        }
    }
    Check(true, "offset identical across 1000 calls");

    // Different spot, depth, resource and parent cell must all differ --
    // otherwise the whole lattice reads the same wrong number.
    Check(!Near(first, EstimateEngine::StableOffset(5, 5, 4, 4, DepthLayer::MID, ResourceType::Fe)),
          "offset differs by sub-cell");
    Check(!Near(first, EstimateEngine::StableOffset(5, 5, 3, 4, DepthLayer::DEEP, ResourceType::Fe)),
          "offset differs by depth");
    Check(!Near(first, EstimateEngine::StableOffset(5, 5, 3, 4, DepthLayer::MID, ResourceType::Si)),
          "offset differs by resource");
    Check(!Near(first, EstimateEngine::StableOffset(6, 5, 3, 4, DepthLayer::MID, ResourceType::Fe)),
          "offset differs by parent cell");

    Check(first >= -1.0f && first <= 1.0f, "offset stays within -1..+1");
}

static void TestEstimateSurvivesRebuild()
{
    printf("Rule 1 -- estimates survive a rebuild of the world\n");

    EstimateEngine engine;
    SiteView site(3);
    SampleTray tray(3);

    // No save system exists yet, so the meaningful equivalent of save/load is
    // rebuilding the world from the same seed and re-reading the same spot.
    ResourceManager rmA(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rmA);
    ProspectingGrid gridA(3, 5, 5, rmA);

    ResourceManager rmB(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rmB);
    ProspectingGrid gridB(3, 5, 5, rmB);

    bool allMatch = true;
    for (int y = 0; y < gridA.GetGridSize(); y++)
    {
        for (int x = 0; x < gridA.GetGridSize(); x++)
        {
            SpotEstimate a = engine.Estimate(gridA, tray, site, x, y,
                                             DepthLayer::SURFACE, ResourceType::C);
            SpotEstimate b = engine.Estimate(gridB, tray, site, x, y,
                                             DepthLayer::SURFACE, ResourceType::C);
            if (!Near(a.shown, b.shown) || !Near(a.low, b.low) || !Near(a.high, b.high))
            {
                allMatch = false;
            }
        }
    }
    Check(allMatch, "all 64 spots read identically after a rebuild");
}

static void TestRangeContainsTruth()
{
    printf("The instrument is imprecise, never wrong\n");

    EstimateEngine engine;
    bool alwaysContains = true;
    bool everWrong = false;

    // Sweep confidence and a spread of true values, including the extremes.
    for (int c = 0; c <= 10; c++)
    {
        float confidence = c / 10.0f;
        for (int x = 0; x < 8; x++)
        {
            for (int y = 0; y < 8; y++)
            {
                float truth = 100.0f + x * 137.0f + y * 29.0f;
                SpotEstimate e = engine.EstimateAt(truth, confidence, 5, 5, x, y,
                                                   DepthLayer::SURFACE, ResourceType::Fe);

                if (truth < e.low - 1e-3f || truth > e.high + 1e-3f)
                {
                    alwaysContains = false;
                }
                if (confidence < 0.9f && std::fabs(e.shown - truth) > 1e-3f)
                {
                    everWrong = true;   // the point estimate should be biased
                }
            }
        }
    }
    Check(alwaysContains, "true value always lies inside the shown range");
    Check(everWrong, "the point estimate is biased when confidence is low");
}

static void TestConfidenceClosesTheRange()
{
    printf("Rule 2 -- confidence closes the range onto the truth\n");

    EstimateEngine engine;
    const float truth = 1234.0f;

    SpotEstimate blind = engine.EstimateAt(truth, 0.0f, 5, 5, 2, 3,
                                           DepthLayer::SURFACE, ResourceType::Fe);
    SpotEstimate half = engine.EstimateAt(truth, 0.5f, 5, 5, 2, 3,
                                          DepthLayer::SURFACE, ResourceType::Fe);
    SpotEstimate known = engine.EstimateAt(truth, 1.0f, 5, 5, 2, 3,
                                           DepthLayer::SURFACE, ResourceType::Fe);

    Check(blind.halfWidth > half.halfWidth, "range narrows as confidence rises");
    Check(half.halfWidth > known.halfWidth, "range keeps narrowing");
    Check(Near(known.halfWidth, 0.0f), "range is closed at full confidence");
    Check(Near(known.shown, truth), "estimate equals truth at full confidence");
    Check(known.isCertain, "spot reads as certain at full confidence");
    Check(!blind.isCertain, "an unsurveyed spot does not read as certain");

    // A blind reading should be wrong by a meaningful amount, or the gamble is
    // cosmetic.
    Check(std::fabs(blind.shown - truth) > truth * 0.01f,
          "a blind reading is meaningfully wrong");
    Check(blind.low >= 0.0f, "range floor never goes negative");
}

static void TestYieldUsesQuantityTimesComposition()
{
    printf("The units trap -- yield is quantity x composition\n");

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rm);
    ProspectingGrid grid(3, 5, 5, rm);
    SiteView site(3);

    bool matchesProduct = true;
    bool anyNonZero = false;
    bool differsFromQuantity = false;

    for (int y = 0; y < grid.GetGridSize(); y++)
    {
        for (int x = 0; x < grid.GetGridSize(); x++)
        {
            float quantity = grid.GetQuantity(x, y, DepthLayer::SURFACE);
            auto composition = grid.GetGroundTruth(x, y, DepthLayer::SURFACE);

            for (const auto& [type, fraction] : composition)
            {
                float yield = site.GetTargetYield(grid, x, y, DepthLayer::SURFACE, type);
                if (!Near(yield, quantity * fraction)) matchesProduct = false;
                if (yield > 0.0f) anyNonZero = true;
                if (std::fabs(yield - quantity) > 1.0f) differsFromQuantity = true;
            }
        }
    }

    Check(matchesProduct, "yield equals quantity x composition fraction");
    Check(anyNonZero, "yields are not all zero");
    Check(differsFromQuantity, "yield is not just the raw quantity");

    // Composition really is fractions, not quantities -- the bug the guide warns about.
    auto composition = grid.GetGroundTruth(3, 3, DepthLayer::SURFACE);
    float sum = 0.0f;
    for (const auto& [type, fraction] : composition) sum += fraction;
    Check(Near(sum, 1.0f, 1e-2f), "composition fractions sum to 1");
}

static void TestReachRings()
{
    printf("Reach rings nest and come from this module's tier\n");

    // Centred, nesting, and a strict superset as tier rises.
    for (int tier = 0; tier < 3; tier++)
    {
        SiteView lower(tier);
        SiteView higher(tier + 1);
        bool nests = true;
        for (int y = 0; y < 8; y++)
        {
            for (int x = 0; x < 8; x++)
            {
                if (lower.IsInReach(x, y) && !higher.IsInReach(x, y)) nests = false;
            }
        }
        Check(nests, "every tier's reach contains the one below it");
    }

    SiteView t0(0);
    SiteView t3(3);
    Check(t0.GetReach() == 2 && t3.GetReach() == 8, "reach runs 2x2 to 8x8");
    Check(t0.IsInReach(3, 3) && t0.IsInReach(4, 4), "tier 0 reaches the centre");
    Check(!t0.IsInReach(0, 0) && !t0.IsInReach(7, 7), "tier 0 does not reach the corners");
    Check(t3.IsInReach(0, 0) && t3.IsInReach(7, 7), "tier 3 reaches the corners");

    Check(t0.CanWorkDepth(DepthLayer::SURFACE), "tier 0 can work the surface");
    Check(!t0.CanWorkDepth(DepthLayer::DEEP), "tier 0 cannot work the deep layer");
    Check(t3.CanWorkDepth(DepthLayer::DEEP), "tier 3 can work the deep layer");
}

static void TestPrecisionBlendsNeighbours()
{
    printf("Precision -- a blunt machine averages the spot with its neighbours\n");

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rm);
    ProspectingGrid grid(3, 5, 5, rm);
    SiteView site(3);
    DigEngine engine;

    // Find a spot whose composition genuinely differs from its neighbours,
    // otherwise blending is unobservable and the test proves nothing.
    int tx = -1, ty = -1;
    float biggestGap = 0.0f;
    for (int y = 1; y < 7; y++)
    {
        for (int x = 1; x < 7; x++)
        {
            auto exact = engine.BlendedComposition(grid, site, x, y, DepthLayer::SURFACE, 1.0f);
            auto blunt = engine.BlendedComposition(grid, site, x, y, DepthLayer::SURFACE, 0.45f);
            float gap = std::fabs(exact[ResourceType::C] - blunt[ResourceType::C]);
            if (gap > biggestGap) { biggestGap = gap; tx = x; ty = y; }
        }
    }

    Check(tx >= 0 && biggestGap > 1e-4f, "blending visibly changes the mix somewhere");

    auto exact = engine.BlendedComposition(grid, site, tx, ty, DepthLayer::SURFACE, 1.0f);
    auto blunt = engine.BlendedComposition(grid, site, tx, ty, DepthLayer::SURFACE, 0.45f);

    float exactSum = 0.0f, bluntSum = 0.0f;
    for (const auto& [t, f] : exact) exactSum += f;
    for (const auto& [t, f] : blunt) bluntSum += f;

    Check(Near(exactSum, 1.0f, 1e-2f), "a precise machine still reads as fractions");
    Check(Near(bluntSum, 1.0f, 1e-2f), "blending conserves the fraction total");

    // Precision 1.0 must be exactly the ground truth -- no drift.
    auto truth = grid.GetGroundTruth(tx, ty, DepthLayer::SURFACE);
    bool matchesTruth = true;
    for (const auto& [t, f] : truth)
    {
        if (!Near(exact[t], f)) matchesTruth = false;
    }
    Check(matchesTruth, "full precision digs exactly the aimed spot");
}

static void TestSelectivityCleansTheMix()
{
    printf("Selectivity -- a choosy machine hands on a better mix, less of it\n");

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rm);
    ProspectingGrid grid(3, 5, 5, rm);
    SiteView site(3);
    DigSite worked;
    DigEngine engine;

    const ResourceType target = ResourceType::C;
    const int x = 4, y = 4;

    // Auger is very selective and slow; wheel is indiscriminate and fast.
    DigResult choosy = engine.Dig(grid, site, worked, x, y, DepthLayer::SURFACE, target,
                                  MachineId::AUGER, 1, 0.6f, 0.0f, 1.0f, 1.0f);
    DigResult blunt = engine.Dig(grid, site, worked, x, y, DepthLayer::SURFACE, target,
                                 MachineId::BUCKET_WHEEL, 1, 1.8f, 0.0f, 1.0f, 1.0f);

    float choosyShare = choosy.totalMass > 0.0f ? choosy.targetMass / choosy.totalMass : 0.0f;
    float bluntShare = blunt.totalMass > 0.0f ? blunt.targetMass / blunt.totalMass : 0.0f;

    Check(choosy.totalMass > 0.0f && blunt.totalMass > 0.0f, "both machines dig something");
    Check(choosyShare > bluntShare, "the choosy machine hands on a cleaner mix");
    Check(blunt.totalMass > choosy.totalMass, "the blunt machine moves more total mass");

    // Pace should cost selectivity: the same machine, pushed, gets dirtier.
    DigResult gentle = engine.Dig(grid, site, worked, x, y, DepthLayer::SURFACE, target,
                                  MachineId::BUCKET_DRUM, 1, 0.2f, 0.0f, 1.0f, 1.0f);
    DigResult hard = engine.Dig(grid, site, worked, x, y, DepthLayer::SURFACE, target,
                                MachineId::BUCKET_DRUM, 1, 1.0f, 0.0f, 1.0f, 1.0f);

    float gentleShare = gentle.totalMass > 0.0f ? gentle.targetMass / gentle.totalMass : 0.0f;
    float hardShare = hard.totalMass > 0.0f ? hard.targetMass / hard.totalMass : 0.0f;

    Check(gentleShare > hardShare, "pushing the pace costs selectivity");
    Check(hard.targetMass > gentle.targetMass, "pushing the pace still yields more target");
}

static void TestPowerCapThrottles()
{
    printf("The power cap throttles pace rather than being ignored\n");

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rm);
    ProspectingGrid grid(3, 5, 5, rm);
    SiteView site(3);
    DigSite worked;
    DigEngine engine;

    DigResult uncapped = engine.Dig(grid, site, worked, 4, 4, DepthLayer::SURFACE,
                                    ResourceType::C, MachineId::BUCKET_DRUM, 1,
                                    1.0f, 0.0f, 1.0f, 1.0f);
    DigResult capped = engine.Dig(grid, site, worked, 4, 4, DepthLayer::SURFACE,
                                  ResourceType::C, MachineId::BUCKET_DRUM, 1,
                                  1.0f, 1.4f, 1.0f, 1.0f);

    Check(!uncapped.throttledByPower, "an uncapped dig runs at the set pace");
    Check(capped.throttledByPower, "a tight cap reports throttling");
    Check(capped.effectivePace < uncapped.effectivePace, "the cap lowers the pace");
    Check(capped.totalMass < uncapped.totalMass, "throttling reduces output");
    Check(capped.powerDraw <= 1.4f + 1e-3f, "draw stays within the cap");
}

static void TestReachAndDepthAreEnforced()
{
    printf("A dig outside reach or beyond the machine produces nothing\n");

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rm);
    ProspectingGrid grid(3, 5, 5, rm);
    DigSite worked;
    DigEngine engine;

    SiteView t0(0);   // reaches only the central 2x2, surface only
    DigResult outside = engine.Dig(grid, t0, worked, 0, 0, DepthLayer::SURFACE,
                                   ResourceType::C, MachineId::SCOOP, 1,
                                   1.0f, 0.0f, 1.0f, 1.0f);
    Check(outside.totalMass == 0.0f, "a spot outside reach yields nothing");

    DigResult inside = engine.Dig(grid, t0, worked, 4, 4, DepthLayer::SURFACE,
                                  ResourceType::C, MachineId::SCOOP, 1,
                                  1.0f, 0.0f, 1.0f, 1.0f);
    Check(inside.totalMass > 0.0f, "a spot inside reach yields material");

    SiteView t3(3);
    DigResult tooDeep = engine.Dig(grid, t3, worked, 4, 4, DepthLayer::DEEP,
                                   ResourceType::C, MachineId::SCOOP, 1,
                                   1.0f, 0.0f, 1.0f, 1.0f);
    Check(tooDeep.totalMass == 0.0f, "a surface machine cannot reach the deep layer");

    DigResult hammer = engine.Dig(grid, t3, worked, 4, 4, DepthLayer::DEEP,
                                  ResourceType::C, MachineId::PERCUSSIVE, 1,
                                  1.0f, 0.0f, 1.0f, 1.0f);
    Check(hammer.totalMass > 0.0f, "the hammer reaches the deep layer");
}

static void TestSpotsDeplete()
{
    printf("Spots deplete and eventually run out\n");

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rm);
    ProspectingGrid grid(3, 5, 5, rm);
    SiteView site(3);
    DigSite worked;
    DigEngine engine;

    Check(Near(worked.Remaining(4, 4, DepthLayer::SURFACE), 1.0f), "a fresh spot is untouched");

    int ticks = 0;
    while (!worked.IsExhausted(4, 4, DepthLayer::SURFACE) && ticks < 100000)
    {
        DigResult r = engine.Dig(grid, site, worked, 4, 4, DepthLayer::SURFACE,
                                 ResourceType::C, MachineId::BUCKET_WHEEL, 1,
                                 1.8f, 0.0f, 1.0f, 1.0f);
        worked.Take(4, 4, DepthLayer::SURFACE, r.depletionFraction);
        ticks++;
    }

    Check(worked.IsExhausted(4, 4, DepthLayer::SURFACE), "a worked spot runs out");
    Check(ticks > 20 && ticks < 200, "a spot lasts a sensible number of ticks");
    printf("        (spot exhausted after %d ticks at full pace)\n", ticks);

    DigResult after = engine.Dig(grid, site, worked, 4, 4, DepthLayer::SURFACE,
                                 ResourceType::C, MachineId::BUCKET_WHEEL, 1,
                                 1.8f, 0.0f, 1.0f, 1.0f);
    Check(after.totalMass == 0.0f, "an exhausted spot yields nothing");
    Check(after.spotExhausted, "an exhausted spot says so");

    Check(Near(worked.Remaining(3, 3, DepthLayer::SURFACE), 1.0f),
          "digging one spot does not deplete its neighbour");
}

static void TestDiggingWritesBackToProspecting()
{
    printf("Rule 5 -- digging reports back, per depth\n");

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rm);
    ProspectingGrid grid(3, 5, 5, rm);
    SampleTray tray(3);
    SiteView site(3);
    DigEngine engine;
    DigSite worked;

    const int x = 4, y = 4;

    Check(!grid.GetSubCell(x, y).HasBeenDug(0), "a fresh spot has not been dug");
    Check(Near(site.GetConfidence(grid, tray, x, y, DepthLayer::SURFACE), 0.0f),
          "an unsurveyed, undug spot has no confidence");
    Check(Near(grid.GetExcavatedKnowledge(x, y), 0.0f), "no dug knowledge yet");

    DigResult r = engine.Dig(grid, site, worked, x, y, DepthLayer::SURFACE,
                             ResourceType::C, MachineId::BUCKET_DRUM, 1,
                             1.0f, 0.0f, 1.0f, 1.0f);
    grid.RecordExcavation(x, y, DepthLayer::SURFACE, r.depletionFraction);

    Check(grid.GetSubCell(x, y).HasBeenDug(0), "digging marks the layer worked");
    Check(Near(site.GetConfidence(grid, tray, x, y, DepthLayer::SURFACE), 1.0f),
          "a dug layer is known for certain");

    // The point of per-depth confidence: digging the surface says nothing
    // about what is underneath it.
    Check(Near(site.GetConfidence(grid, tray, x, y, DepthLayer::MID), 0.0f),
          "digging the surface reveals nothing deeper");
    Check(!grid.GetSubCell(x, y).HasBeenDug(2), "the mid layer is still undug");

    // And nothing about the neighbours.
    Check(!grid.GetSubCell(x + 1, y).HasBeenDug(0), "digging one spot does not reveal its neighbour");

    Check(Near(grid.GetExcavatedKnowledge(x, y), 0.25f),
          "one dug layer is a quarter of the column");

    for (int d = 1; d < 4; d++)
    {
        grid.RecordExcavation(x, y, static_cast<DepthLayer>(d), 0.5f);
    }
    Check(Near(grid.GetExcavatedKnowledge(x, y), 1.0f),
          "digging every layer knows the whole column");
}

static void TestBlindDiggingBootstrapsSurvey()
{
    printf("Rule 3 -- digging blind slowly bootstraps survey progress\n");

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    BuildWorld(rm);
    ProspectingGrid grid(3, 5, 5, rm);
    SampleTray tray(3);

    CellSurveyResult before = SurveyProgressEngine::Calculate(grid, tray);
    Check(Near(before.surveyProgress, 0.0f, 1e-2f), "an untouched cell has no survey progress");

    // Dig out a patch without ever sweeping or sampling it.
    for (int y = 2; y < 6; y++)
    {
        for (int x = 2; x < 6; x++)
        {
            grid.RecordExcavation(x, y, DepthLayer::SURFACE, 1.0f);
        }
    }

    CellSurveyResult after = SurveyProgressEngine::Calculate(grid, tray);
    Check(after.surveyProgress > before.surveyProgress,
          "digging raises survey progress without any surveying");
    printf("        (survey progress %.3f -> %.3f from digging alone)\n",
           before.surveyProgress, after.surveyProgress);
}

int main()
{
    printf("\n=== excavation tests ===\n\n");

    TestOffsetIsStable();
    TestEstimateSurvivesRebuild();
    TestRangeContainsTruth();
    TestConfidenceClosesTheRange();
    TestYieldUsesQuantityTimesComposition();
    TestReachRings();
    TestPrecisionBlendsNeighbours();
    TestSelectivityCleansTheMix();
    TestPowerCapThrottles();
    TestReachAndDepthAreEnforced();
    TestSpotsDeplete();
    TestDiggingWritesBackToProspecting();
    TestBlindDiggingBootstrapsSurvey();

    printf("\n%d checks, %d failures\n\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
