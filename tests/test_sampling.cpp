#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "sampling_engine.h"

TEST_CASE("depth is priced, never gated", "[sampling]")
{
    // All four layers are drillable from tier 0. Depth stays meaningful
    // through the per-metre price, not through a wall -- see
    // docs/design/prospecting/progression-design.md.
    for (int tier = 0; tier <= 3; tier++)
    {
        SamplingEngine engine(tier);
        REQUIRE(engine.CanDrill(DepthLayer::SURFACE));
        REQUIRE(engine.CanDrill(DepthLayer::SHALLOW));
        REQUIRE(engine.CanDrill(DepthLayer::MID));
        REQUIRE(engine.CanDrill(DepthLayer::DEEP));
    }
}

TEST_CASE("a hole is priced per metre of column, tier-independent", "[sampling]")
{
    // Thickness x rate, summed down the column: 12*1.2, +22*1.9, +34*2.8,
    // +52*4.0. No tier discount, ever -- that is what stops "bank energy and
    // drill after the upgrade" from being correct.
    SamplingEngine t0(0);
    SamplingEngine t3(3);
    REQUIRE_THAT(t0.GetDrillCost(DepthLayer::SURFACE), Catch::Matchers::WithinAbs(14.4f, 0.01f));
    REQUIRE_THAT(t0.GetDrillCost(DepthLayer::SHALLOW), Catch::Matchers::WithinAbs(56.2f, 0.01f));
    REQUIRE_THAT(t0.GetDrillCost(DepthLayer::MID),     Catch::Matchers::WithinAbs(151.4f, 0.01f));
    REQUIRE_THAT(t0.GetDrillCost(DepthLayer::DEEP),    Catch::Matchers::WithinAbs(359.4f, 0.01f));
    for (int d = 0; d < 4; d++)
    {
        REQUIRE_THAT(t3.GetDrillCost(static_cast<DepthLayer>(d)),
                     Catch::Matchers::WithinAbs(
                         t0.GetDrillCost(static_cast<DepthLayer>(d)), 0.001f));
    }
}

TEST_CASE("SamplingEngine CollectSample adds to tray", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SampleTray tray(2);
    SamplingEngine engine(2);

    REQUIRE(tray.IsEmpty());
    // (3,3) is the centre 2x2, which every tier can reach. (0,0) is a corner
    // and is out of reach until T3 -- the lattice is fixed and tier is a ring.
    REQUIRE(engine.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    REQUIRE(tray.GetCount() == 1);
}

TEST_CASE("a full specimen shelf never blocks knowledge", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(0, 5, 5, rm);
    SampleTray tray(0);          // capacity 4
    SamplingEngine engine(0);

    const int spots[4][2] = { {3,3}, {4,3}, {3,4}, {4,4} };
    for (int i = 0; i < 4; i++)
        REQUIRE(engine.CollectSample(grid, tray, spots[i][0], spots[i][1],
                                     DepthLayer::SURFACE));
    REQUIRE(tray.IsFull());

    // Drilling still works: the assay lives on the grid, the tray is a shelf
    // of physical rocks, and a full shelf must never un-know ground.
    REQUIRE(engine.CollectSample(grid, tray, 2, 2, DepthLayer::SURFACE));
    REQUIRE(grid.GetSubCell(2, 2).HasCore(0));
    REQUIRE(tray.GetCount() == 4);          // the specimen was discarded
}

TEST_CASE("an auger hole cores the whole column", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(0, 5, 5, rm);
    SampleTray tray(3);
    SamplingEngine engine(0);

    // Deep drilling from tier 0: legal, and it recovers everything on the
    // way down -- a MID hole knows SURFACE and SHALLOW too.
    REQUIRE(engine.CollectSample(grid, tray, 3, 3, DepthLayer::MID));
    REQUIRE(grid.GetSubCell(3, 3).HasCore(0));
    REQUIRE(grid.GetSubCell(3, 3).HasCore(1));
    REQUIRE(grid.GetSubCell(3, 3).HasCore(2));
    REQUIRE_FALSE(grid.GetSubCell(3, 3).HasCore(3));   // below the target
}

TEST_CASE("SamplingEngine CollectSample fails on out-of-bounds", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SamplingEngine engine(1);

    REQUIRE_FALSE(engine.CollectSample(grid, tray, -1, 0, DepthLayer::SURFACE));
    REQUIRE_FALSE(engine.CollectSample(grid, tray, 10, 0, DepthLayer::SURFACE));
    REQUIRE(tray.IsEmpty());
}

TEST_CASE("Collected sample has ground truth composition", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SampleTray tray(2);
    SamplingEngine engine(2);

    engine.CollectSample(grid, tray, 1, 1, DepthLayer::SURFACE);
    const Sample* s = tray.GetSampleByIndex(0);
    REQUIRE(s != nullptr);

    auto groundTruth = grid.GetGroundTruth(1, 1, DepthLayer::SURFACE);
    for (const auto& [type, expected] : groundTruth)
    {
        auto it = s->trueComposition.find(type);
        REQUIRE(it != s->trueComposition.end());
        REQUIRE_THAT(it->second, Catch::Matchers::WithinAbs(expected, 0.001f));
    }
}

TEST_CASE("A recovered core comes out of the ground assayed", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SamplingEngine engine(1);

    REQUIRE(engine.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    const Sample* s = tray.GetSampleByIndex(0);
    REQUIRE(s != nullptr);

    // A core is rock you are holding. The lab stage used to gate this, and it
    // modelled the wrong uncertainty: analytical precision is a percent or
    // two, while the uncertainty BETWEEN holes is total.
    REQUIRE_FALSE(s->elementConfidence.empty());
    for (const auto& [type, abundance] : s->trueComposition)
    {
        REQUIRE(s->elementConfidence.at(type) == 1.0f);
    }
    REQUIRE_THAT(s->GetAggregateConfidence(), Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

TEST_CASE("Collected sample richness is in valid range", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SampleTray tray(2);
    SamplingEngine engine(2);

    int size = grid.GetGridSize();
    for (int y = 0; y < size && !tray.IsFull(); y++)
        for (int x = 0; x < size && !tray.IsFull(); x++)
            engine.CollectSample(grid, tray, x, y, DepthLayer::SURFACE);

    for (int i = 0; i < tray.GetCount(); i++)
    {
        float r = tray.GetSampleByIndex(i)->richness;
        REQUIRE(r >= 0.0f);
        REQUIRE(r <= 1.0f);
    }
}

TEST_CASE("Collected sample records correct position and depth", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 10, 12, rm);
    SampleTray tray(3);
    SamplingEngine engine(3);

    engine.CollectSample(grid, tray, 3, 4, DepthLayer::MID);
    const Sample* s = tray.GetSampleByIndex(0);

    REQUIRE(s->subCellX == 3);
    REQUIRE(s->subCellY == 4);
    REQUIRE(s->depthLayer == DepthLayer::MID);
    REQUIRE(s->state == SampleState::IN_TRAY);
}

TEST_CASE("Collected sample registered in sub-cell sampleIds", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SampleTray tray(2);
    SamplingEngine engine(2);

    REQUIRE(grid.GetSubCell(1, 1).sampleIds.empty());

    engine.CollectSample(grid, tray, 1, 1, DepthLayer::SURFACE);
    REQUIRE(grid.GetSubCell(1, 1).sampleIds.size() == 1);

    engine.CollectSample(grid, tray, 1, 1, DepthLayer::SHALLOW);
    REQUIRE(grid.GetSubCell(1, 1).sampleIds.size() == 2);
}

TEST_CASE("Crystal visual glow reflects a fully known core", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SamplingEngine engine(1);

    REQUIRE(engine.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    // Glow is derived from confidence, and a recovered core is fully known, so
    // it now pins at the top. The channel no longer varies -- worth
    // repurposing (richness? depth?) rather than leaving it constant.
    REQUIRE(tray.GetSampleByIndex(0)->visual.glowLevel == 4);
}

TEST_CASE("Crystal visual size matches richness", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SampleTray tray(2);
    SamplingEngine engine(2);

    REQUIRE(engine.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    const Sample* s = tray.GetSampleByIndex(0);
    REQUIRE(s != nullptr);

    REQUIRE(s->visual.sizeLevel == GetSizeLevel(s->richness));
}

TEST_CASE("Crystal visual template index is valid", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 5, 5, rm);
    SampleTray tray(3);
    SamplingEngine engine(3);

    int size = grid.GetGridSize();
    for (int y = 0; y < size && !tray.IsFull(); y++)
        for (int x = 0; x < size && !tray.IsFull(); x++)
            engine.CollectSample(grid, tray, x, y, DepthLayer::SURFACE);

    for (int i = 0; i < tray.GetCount(); i++)
    {
        int idx = tray.GetSampleByIndex(i)->visual.templateIndex;
        REQUIRE(idx >= 0);
        REQUIRE(idx < CRYSTAL_TEMPLATES_PER_FAMILY);
    }
}

TEST_CASE("Crystal visual shape family favors primary 70%", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    SamplingEngine engine(3);
    int primaryCount = 0;
    int totalCount = 0;

    for (int px = 0; px < 10; px++)
    {
        for (int py = 0; py < 10; py++)
        {
            ProspectingGrid grid(3, px, py, rm);
            SampleTray tray(3);

            int size = grid.GetGridSize();
            for (int y = 0; y < size && !tray.IsFull(); y++)
            {
                for (int x = 0; x < size && !tray.IsFull(); x++)
                {
                    engine.CollectSample(grid, tray, x, y, DepthLayer::SURFACE);
                }
            }

            ShapeFamily primary = GetPrimaryShapeFamily(DepthLayer::SURFACE);
            for (int i = 0; i < tray.GetCount(); i++)
            {
                if (tray.GetSampleByIndex(i)->visual.shapeFamily == primary)
                    primaryCount++;
                totalCount++;
            }
        }
    }

    float primaryRatio = static_cast<float>(primaryCount) / totalCount;
    REQUIRE(primaryRatio > 0.55f);
    REQUIRE(primaryRatio < 0.85f);
}

TEST_CASE("Crystal visual element color matches dominant element", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SampleTray tray(2);
    SamplingEngine engine(2);

    REQUIRE(engine.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    const Sample* s = tray.GetSampleByIndex(0);
    REQUIRE(s != nullptr);

    ResourceType dominant = SamplingEngine::GetDominantElement(s->trueComposition);
    Color expected = GetElementColor(dominant);
    REQUIRE(s->visual.elementColor.r == expected.r);
    REQUIRE(s->visual.elementColor.g == expected.g);
    REQUIRE(s->visual.elementColor.b == expected.b);
}

TEST_CASE("CalculateRichnessFromQuantity normalizes correctly", "[sampling]")
{
    // Richness is derived from the absolute QUANTITY in the ground, not from
    // a composition map -- composition entries are fractions summing to ~1, so
    // feeding them in made every sample read equally rich. The old signature
    // took the map and this test went with it.
    REQUIRE_THAT(SamplingEngine::CalculateRichnessFromQuantity(0.0f),
                 Catch::Matchers::WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(SamplingEngine::CalculateRichnessFromQuantity(RICHNESS_NORMALIZATION * 0.5f),
                 Catch::Matchers::WithinAbs(0.5f, 0.001f));

    REQUIRE_THAT(SamplingEngine::CalculateRichnessFromQuantity(RICHNESS_NORMALIZATION),
                 Catch::Matchers::WithinAbs(1.0f, 0.001f));

    // Clamped, so an unusually rich cell cannot report more than full.
    REQUIRE_THAT(SamplingEngine::CalculateRichnessFromQuantity(RICHNESS_NORMALIZATION * 10.0f),
                 Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

TEST_CASE("GetDominantElement returns highest abundance", "[sampling]")
{
    std::map<ResourceType, float> comp = {
        {ResourceType::Fe, 0.30f},
        {ResourceType::Ti, 0.60f},
        {ResourceType::Si, 0.10f}
    };
    REQUIRE(SamplingEngine::GetDominantElement(comp) == ResourceType::Ti);
}

TEST_CASE("SamplingEngine SetTier clamps range", "[sampling]")
{
    SamplingEngine engine(0);
    engine.SetTier(5);
    REQUIRE(engine.GetTier() == 3);
    engine.SetTier(-2);
    REQUIRE(engine.GetTier() == 0);
}
