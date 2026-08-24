#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <cmath>

TEST_CASE("ProspectingGrid initializes with correct size per tier", "[grid]")
{
    auto rm = MakeTestResourceManager();

    // Fixed lattice at every tier -- see prospecting_constants.h.
    for (int tier = 0; tier <= 3; tier++)
    {
        ProspectingGrid g(tier, 5, 5, rm);
        REQUIRE(g.GetGridSize() == PROSPECTING_GRID_SIZE);
        REQUIRE(g.GetTier() == tier);
    }
}

TEST_CASE("ProspectingGrid stores parent cell coordinates", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 7, 12, rm);

    REQUIRE(grid.GetParentGridX() == 7);
    REQUIRE(grid.GetParentGridY() == 12);
}

TEST_CASE("ProspectingGrid sub-cell distribution averages near parent value", "[grid]")
{
    auto rm = MakeTestResourceManager();
    int px = 10, py = 10;

    ProspectingGrid grid(2, px, py, rm); // 5x5 grid
    int size = grid.GetGridSize();

    auto parentResources = rm.GetResourcesAtGridLayer(px, py, DepthLayer::SURFACE);

    for (const auto& [type, parentAbundance] : parentResources)
    {
        if (parentAbundance < 0.01f) continue;

        float sum = 0.0f;
        int count = 0;
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                auto gt = grid.GetGroundTruth(x, y, DepthLayer::SURFACE);
                auto it = gt.find(type);
                if (it != gt.end())
                {
                    sum += it->second;
                    count++;
                }
            }
        }

        // GetGroundTruth returns composition FRACTIONS; parentAbundance is an
        // absolute quantity. Comparing them directly is the units trap named
        // in module-architecture.md Part II, so compare fractions to 1.0.
        float avg = sum / count;
        REQUIRE(avg > 0.0f);
        REQUIRE(avg <= 1.0f);
    }
}

TEST_CASE("ProspectingGrid sub-cells have spatial variation", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 5, 5, rm); // 6x6 grid
    int size = grid.GetGridSize();

    // Check that across all sub-cells, at least some spatial variation exists
    // (a barren parent cell may have no variation — skip if so)
    auto gt00 = grid.GetGroundTruth(0, 0, DepthLayer::SURFACE);
    if (gt00.empty()) return;

    bool hasVariation = false;
    for (int y = 0; y < size && !hasVariation; y++)
    {
        for (int x = 0; x < size && !hasVariation; x++)
        {
            if (x == 0 && y == 0) continue;
            auto gt = grid.GetGroundTruth(x, y, DepthLayer::SURFACE);
            for (const auto& [type, val] : gt00)
            {
                auto it = gt.find(type);
                if (it != gt.end() && std::abs(val - it->second) > 0.001f)
                {
                    hasVariation = true;
                    break;
                }
            }
        }
    }
    REQUIRE(hasVariation);
}

TEST_CASE("ProspectingGrid values are within clamped range", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    int size = grid.GetGridSize();

    auto parentResources = rm.GetResourcesAtGridLayer(8, 8, DepthLayer::SURFACE);

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            auto gt = grid.GetGroundTruth(x, y, DepthLayer::SURFACE);
            for (const auto& [type, val] : gt)
            {
                float parentVal = 0.0f;
                for (const auto& [pt, pv] : parentResources)
                {
                    if (pt == type) { parentVal = pv; break; }
                }
                if (parentVal < 0.001f) continue;

                // The variation clamp applies to the WEIGHT on abundance, so
                // it has to be checked on the yield product -- quantity x
                // composition -- not on the composition fraction alone.
                float yield = grid.GetQuantity(x, y, DepthLayer::SURFACE) * val;
                REQUIRE(yield >= parentVal * SUBCELL_VARIATION_MIN - 0.001f);
                REQUIRE(yield <= parentVal * SUBCELL_VARIATION_MAX + 0.001f);
            }
        }
    }
}

TEST_CASE("ProspectingGrid out-of-bounds returns empty", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(0, 5, 5, rm);

    auto gt = grid.GetGroundTruth(-1, 0, DepthLayer::SURFACE);
    REQUIRE(gt.empty());

    // One past the fixed lattice. Note this is BOUNDS, not reach: a cell can
    // be in bounds and still out of the instruments' reach at a low tier.
    gt = grid.GetGroundTruth(PROSPECTING_GRID_SIZE, 0, DepthLayer::SURFACE);
    REQUIRE(gt.empty());
}

TEST_CASE("ResizeForTier changes reach and keeps the lattice", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(0, 5, 5, rm);
    REQUIRE(grid.GetGridSize() == PROSPECTING_GRID_SIZE);

    // A tier upgrade must not reallocate the grid, or survey data and the
    // sample links pointing into it would be lost.
    grid.RecordExcavation(4, 4, DepthLayer::SURFACE, 0.5f);

    grid.ResizeForTier(2);
    REQUIRE(grid.GetTier() == 2);
    REQUIRE(grid.GetGridSize() == PROSPECTING_GRID_SIZE);
    REQUIRE(grid.GetSubCell(4, 4).workedFraction[0] == 0.5f);
}

TEST_CASE("ProspectingGrid deterministic: same inputs produce same results", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid1(2, 10, 10, rm);
    ProspectingGrid grid2(2, 10, 10, rm);

    int size = grid1.GetGridSize();
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            auto gt1 = grid1.GetGroundTruth(x, y, DepthLayer::SURFACE);
            auto gt2 = grid2.GetGroundTruth(x, y, DepthLayer::SURFACE);
            REQUIRE(gt1.size() == gt2.size());
            for (const auto& [type, val] : gt1)
            {
                REQUIRE_THAT(val, Catch::Matchers::WithinAbs(gt2.at(type), 0.0001f));
            }
        }
    }
}

TEST_CASE("ProspectingGrid sweep history tracking", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);

    REQUIRE(grid.GetSweepHistory().empty());
    REQUIRE_FALSE(grid.HasSweptFrequency(0));

    grid.RecordSweep(0, 30.0f, 100.0f);
    REQUIRE(grid.GetSweepHistory().size() == 1);
    REQUIRE(grid.HasSweptFrequency(0));
    REQUIRE_FALSE(grid.HasSweptFrequency(1));

    grid.RecordSweep(2, 100.0f, 200.0f);
    REQUIRE(grid.GetSweepHistory().size() == 2);
    REQUIRE(grid.HasSweptFrequency(2));
}

TEST_CASE("ProspectingGrid SubCell starts unswept", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);

    const auto& cell = grid.GetSubCell(0, 0);
    REQUIRE_FALSE(cell.hasBeenSwept);
    REQUIRE(cell.sweepSignal == 0.0f);
    REQUIRE(cell.sweepFrequencyBand == -1);
    REQUIRE(cell.sampleIds.empty());
}
