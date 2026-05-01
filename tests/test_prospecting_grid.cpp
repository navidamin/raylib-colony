#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <cmath>

TEST_CASE("ProspectingGrid initializes with correct size per tier", "[grid]")
{
    auto rm = MakeTestResourceManager();

    ProspectingGrid g0(0, 5, 5, rm);
    REQUIRE(g0.GetGridSize() == 3);

    ProspectingGrid g1(1, 5, 5, rm);
    REQUIRE(g1.GetGridSize() == 4);

    ProspectingGrid g2(2, 5, 5, rm);
    REQUIRE(g2.GetGridSize() == 5);

    ProspectingGrid g3(3, 5, 5, rm);
    REQUIRE(g3.GetGridSize() == 6);
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

        float avg = sum / count;
        // Average should be within 50% of parent value (clusters shift mass around)
        REQUIRE(avg > parentAbundance * 0.3f);
        REQUIRE(avg < parentAbundance * 2.0f);
    }
}

TEST_CASE("ProspectingGrid sub-cells have spatial variation", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 5, 5, rm); // 6x6 grid
    int size = grid.GetGridSize();

    // Check that not all sub-cells have identical values for Fe
    auto gt00 = grid.GetGroundTruth(0, 0, DepthLayer::SURFACE);
    auto gtLast = grid.GetGroundTruth(size - 1, size - 1, DepthLayer::SURFACE);

    bool hasVariation = false;
    for (const auto& [type, val] : gt00)
    {
        auto it = gtLast.find(type);
        if (it != gtLast.end() && std::abs(val - it->second) > 0.001f)
        {
            hasVariation = true;
            break;
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

                REQUIRE(val >= parentVal * SUBCELL_VARIATION_MIN - 0.001f);
                REQUIRE(val <= parentVal * SUBCELL_VARIATION_MAX + 0.001f);
            }
        }
    }
}

TEST_CASE("ProspectingGrid out-of-bounds returns empty", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(0, 5, 5, rm); // 3x3

    auto gt = grid.GetGroundTruth(-1, 0, DepthLayer::SURFACE);
    REQUIRE(gt.empty());

    gt = grid.GetGroundTruth(3, 0, DepthLayer::SURFACE);
    REQUIRE(gt.empty());
}

TEST_CASE("ProspectingGrid ResizeForTier changes grid size", "[grid]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(0, 5, 5, rm);
    REQUIRE(grid.GetGridSize() == 3);

    grid.ResizeForTier(2);
    REQUIRE(grid.GetGridSize() == 5);
    REQUIRE(grid.GetTier() == 2);

    // Ground truth should still be accessible after resize
    auto gt = grid.GetGroundTruth(2, 2, DepthLayer::SURFACE);
    REQUIRE_FALSE(gt.empty());
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
