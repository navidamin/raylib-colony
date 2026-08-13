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

int main()
{
    printf("\n=== excavation tests ===\n\n");

    TestOffsetIsStable();
    TestEstimateSurvivesRebuild();
    TestRangeContainsTruth();
    TestConfidenceClosesTheRange();
    TestYieldUsesQuantityTimesComposition();
    TestReachRings();

    printf("\n%d checks, %d failures\n\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
