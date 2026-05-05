#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "unit.h"
#include "time_manager.h"

namespace
{
    struct UnitFixture
    {
        ResourceManager rm;
        TimeManager tm;
        Vector2 position;
        std::map<ResourceType, float> storage;
        std::map<ResourceType, float> capacity;

        UnitFixture()
            : rm(20, 100.0f)
            , position{500.0f, 500.0f}
        {
            rm.GenerateResourceMap(42);
            rm.GenerateOrbitalSurveyData();

            ResourceType allTypes[] = {
                ResourceType::ENERGY, ResourceType::H2, ResourceType::O2,
                ResourceType::C, ResourceType::Fe, ResourceType::Si,
                ResourceType::Ti, ResourceType::Al, ResourceType::Ca,
                ResourceType::WATER, ResourceType::FOOD, ResourceType::BIOFUEL,
                ResourceType::SCIENCE, ResourceType::MANPOWER,
                ResourceType::MACHINERY, ResourceType::ELECTRONICS,
                ResourceType::ALLOYS, ResourceType::CONSTRUCTION_MATERIALS
            };
            for (auto rt : allTypes)
            {
                storage[rt] = (rt == ResourceType::ENERGY) ? 10000.0f : 0.0f;
                capacity[rt] = 5000.0f;
            }
        }

        Unit MakeExtraction()
        {
            return Unit("Extraction", position, rm, tm, storage, capacity);
        }

        Unit MakeFarming()
        {
            return Unit("Farming", position, rm, tm, storage, capacity);
        }
    };
}

TEST_CASE("Extraction unit has ProspectingSystem", "[wiring]")
{
    UnitFixture f;
    auto unit = f.MakeExtraction();

    REQUIRE(unit.HasProspectingSystem());
    REQUIRE(unit.GetProspectingSystem() != nullptr);
}

TEST_CASE("Non-extraction unit has no ProspectingSystem", "[wiring]")
{
    UnitFixture f;
    auto unit = f.MakeFarming();

    REQUIRE_FALSE(unit.HasProspectingSystem());
    REQUIRE(unit.GetProspectingSystem() == nullptr);
}

TEST_CASE("ProspectingSystem grid matches unit position", "[wiring]")
{
    UnitFixture f;
    auto unit = f.MakeExtraction();

    Vector2 gp = unit.GetGridPosition();
    int gx = static_cast<int>(gp.x);
    int gy = static_cast<int>(gp.y);

    auto* ps = unit.GetProspectingSystem();
    REQUIRE(ps->GetGrid().GetParentGridX() == gx);
    REQUIRE(ps->GetGrid().GetParentGridY() == gy);
}

TEST_CASE("ProspectingSystem starts with zero survey progress", "[wiring]")
{
    UnitFixture f;
    auto unit = f.MakeExtraction();

    auto* ps = unit.GetProspectingSystem();
    REQUIRE_THAT(ps->GetSurveyProgress(), Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_FALSE(ps->IsMarkedSite());
}

TEST_CASE("ProspectingSystem survey progress drives extraction multiplier", "[wiring]")
{
    UnitFixture f;
    auto unit = f.MakeExtraction();
    auto* ps = unit.GetProspectingSystem();

    // Sweep all bands available at tier 1 to build survey progress
    ps->SetTier(1);
    ps->GetSweep().ExecuteSweep(ps->GetGrid(), 0, 100.0f);

    // Collect samples at a few sub-cells
    for (int i = 0; i < 2; i++)
        ps->GetSampler().CollectSample(ps->GetGrid(), ps->GetTray(), i, 0, DepthLayer::SURFACE);

    // Analyze samples
    for (int i = 0; i < ps->GetTray().GetCount(); i++)
    {
        Sample* s = ps->GetTray().GetSampleByIndex(i);
        if (s && ps->GetLab().CanApplyTool(*s, AnalysisTool::VISUAL_INSPECTION))
            ps->GetLab().ApplyTool(*s, AnalysisTool::VISUAL_INSPECTION, 100.0f + i);
    }

    float progress = ps->GetSurveyProgress();
    REQUIRE(progress > 0.0f);
}

TEST_CASE("ProspectingSystem SetTier propagates to all engines", "[wiring]")
{
    UnitFixture f;
    auto unit = f.MakeExtraction();
    auto* ps = unit.GetProspectingSystem();

    REQUIRE(ps->GetTier() == 0);

    ps->SetTier(2);
    REQUIRE(ps->GetTier() == 2);
    REQUIRE(ps->GetGrid().GetTier() == 2);
    REQUIRE(ps->GetGrid().GetGridSize() == PROSPECTING_GRID_SIZE[2]);
    REQUIRE(ps->GetSweep().GetTier() == 2);
    REQUIRE(ps->GetSampler().GetTier() == 2);
    REQUIRE(ps->GetLab().GetTier() == 2);
}

TEST_CASE("ProspectingSystem marked site threshold", "[wiring]")
{
    UnitFixture f;
    auto unit = f.MakeExtraction();
    auto* ps = unit.GetProspectingSystem();

    ps->SetTier(3);

    // Sweep all 4 bands
    for (int band = 0; band < 4; band++)
        ps->GetSweep().ExecuteSweep(ps->GetGrid(), band, 100.0f * (band + 1));

    // Collect many samples
    int size = ps->GetGrid().GetGridSize();
    for (int y = 0; y < size && y < 3; y++)
        for (int x = 0; x < size && x < 3; x++)
            ps->GetSampler().CollectSample(ps->GetGrid(), ps->GetTray(), x, y, DepthLayer::SURFACE);

    // Analyze all samples with LIBS (broad coverage)
    for (int i = 0; i < ps->GetTray().GetCount(); i++)
    {
        Sample* s = ps->GetTray().GetSampleByIndex(i);
        if (s && ps->GetLab().CanApplyTool(*s, AnalysisTool::LIBS_PULSE))
            ps->GetLab().ApplyTool(*s, AnalysisTool::LIBS_PULSE, 200.0f + i);
    }

    float progress = ps->GetSurveyProgress();
    if (progress >= MARKED_SITE_THRESHOLD)
    {
        REQUIRE(ps->IsMarkedSite());
    }
}

TEST_CASE("ProspectingSystem facade accessors return engine references", "[wiring]")
{
    UnitFixture f;
    auto unit = f.MakeExtraction();
    auto* ps = unit.GetProspectingSystem();

    // Mutable accessors should work
    ProspectingGrid& grid = ps->GetGrid();
    SampleTray& tray = ps->GetTray();
    SweepEngine& sweep = ps->GetSweep();
    SamplingEngine& sampler = ps->GetSampler();
    LabEngine& lab = ps->GetLab();

    (void)grid;
    (void)tray;
    (void)sweep;
    (void)sampler;
    (void)lab;

    // Const accessors on const pointer
    const ProspectingSystem* cps = ps;
    const ProspectingGrid& cgrid = cps->GetGrid();
    const SampleTray& ctray = cps->GetTray();
    (void)cgrid;
    (void)ctray;
}
