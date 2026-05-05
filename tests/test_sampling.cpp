#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "sampling_engine.h"

TEST_CASE("SamplingEngine CanDrill respects tier gating", "[sampling]")
{
    SamplingEngine t0(0);
    REQUIRE(t0.CanDrill(DepthLayer::SURFACE));
    REQUIRE_FALSE(t0.CanDrill(DepthLayer::SHALLOW));
    REQUIRE_FALSE(t0.CanDrill(DepthLayer::MID));
    REQUIRE_FALSE(t0.CanDrill(DepthLayer::DEEP));

    SamplingEngine t1(1);
    REQUIRE(t1.CanDrill(DepthLayer::SURFACE));
    REQUIRE(t1.CanDrill(DepthLayer::SHALLOW));
    REQUIRE_FALSE(t1.CanDrill(DepthLayer::MID));

    SamplingEngine t2(2);
    REQUIRE(t2.CanDrill(DepthLayer::MID));
    REQUIRE_FALSE(t2.CanDrill(DepthLayer::DEEP));

    SamplingEngine t3(3);
    REQUIRE(t3.CanDrill(DepthLayer::DEEP));
}

TEST_CASE("SamplingEngine GetDrillCost matches constants", "[sampling]")
{
    SamplingEngine t0(0);
    REQUIRE_THAT(t0.GetDrillCost(DepthLayer::SURFACE), Catch::Matchers::WithinAbs(15.0f, 0.01f));
    REQUIRE_THAT(t0.GetDrillCost(DepthLayer::SHALLOW), Catch::Matchers::WithinAbs(-1.0f, 0.01f));

    SamplingEngine t3(3);
    REQUIRE_THAT(t3.GetDrillCost(DepthLayer::SURFACE), Catch::Matchers::WithinAbs(8.0f, 0.01f));
    REQUIRE_THAT(t3.GetDrillCost(DepthLayer::SHALLOW), Catch::Matchers::WithinAbs(20.0f, 0.01f));
    REQUIRE_THAT(t3.GetDrillCost(DepthLayer::MID), Catch::Matchers::WithinAbs(35.0f, 0.01f));
    REQUIRE_THAT(t3.GetDrillCost(DepthLayer::DEEP), Catch::Matchers::WithinAbs(75.0f, 0.01f));
}

TEST_CASE("SamplingEngine CollectSample adds to tray", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SampleTray tray(2);
    SamplingEngine engine(2);

    REQUIRE(tray.IsEmpty());
    REQUIRE(engine.CollectSample(grid, tray, 0, 0, DepthLayer::SURFACE));
    REQUIRE(tray.GetCount() == 1);
}

TEST_CASE("SamplingEngine CollectSample fails on full tray", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(0, 5, 5, rm);
    SampleTray tray(0);
    SamplingEngine engine(0);

    for (int i = 0; i < 4; i++)
        REQUIRE(engine.CollectSample(grid, tray, i % 3, 0, DepthLayer::SURFACE));

    REQUIRE(tray.IsFull());
    REQUIRE_FALSE(engine.CollectSample(grid, tray, 0, 0, DepthLayer::SURFACE));
}

TEST_CASE("SamplingEngine CollectSample fails on inaccessible depth", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(0, 5, 5, rm);
    SampleTray tray(0);
    SamplingEngine engine(0);

    REQUIRE_FALSE(engine.CollectSample(grid, tray, 0, 0, DepthLayer::SHALLOW));
    REQUIRE(tray.IsEmpty());
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

TEST_CASE("Collected sample starts with zero confidence", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SamplingEngine engine(1);

    engine.CollectSample(grid, tray, 0, 0, DepthLayer::SURFACE);
    const Sample* s = tray.GetSampleByIndex(0);

    REQUIRE(s->elementConfidence.empty());
    REQUIRE_THAT(s->GetAggregateConfidence(), Catch::Matchers::WithinAbs(0.0f, 0.001f));
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

TEST_CASE("Crystal visual glow starts at 0", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SamplingEngine engine(1);

    engine.CollectSample(grid, tray, 0, 0, DepthLayer::SURFACE);
    REQUIRE(tray.GetSampleByIndex(0)->visual.glowLevel == 0);
}

TEST_CASE("Crystal visual size matches richness", "[sampling]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SampleTray tray(2);
    SamplingEngine engine(2);

    engine.CollectSample(grid, tray, 0, 0, DepthLayer::SURFACE);
    const Sample* s = tray.GetSampleByIndex(0);

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

    engine.CollectSample(grid, tray, 0, 0, DepthLayer::SURFACE);
    const Sample* s = tray.GetSampleByIndex(0);

    ResourceType dominant = SamplingEngine::GetDominantElement(s->trueComposition);
    Color expected = GetElementColor(dominant);
    REQUIRE(s->visual.elementColor.r == expected.r);
    REQUIRE(s->visual.elementColor.g == expected.g);
    REQUIRE(s->visual.elementColor.b == expected.b);
}

TEST_CASE("CalculateRichness normalizes correctly", "[sampling]")
{
    std::map<ResourceType, float> empty;
    REQUIRE_THAT(SamplingEngine::CalculateRichness(empty),
                 Catch::Matchers::WithinAbs(0.0f, 0.001f));

    std::map<ResourceType, float> moderate = {
        {ResourceType::Fe, 0.5f}, {ResourceType::Si, 0.5f}
    };
    float expected = std::min(1.0f, 1.0f / RICHNESS_NORMALIZATION);
    REQUIRE_THAT(SamplingEngine::CalculateRichness(moderate),
                 Catch::Matchers::WithinAbs(expected, 0.001f));
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
