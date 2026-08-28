#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "sweep_engine.h"
#include "sampling_engine.h"
#include "lab_engine.h"

// --- Full pipeline: grid → sweep → sample → lab ---

TEST_CASE("Full pipeline: sweep grid, collect sample, analyze in lab", "[integration]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SweepEngine sweep(1);
    SamplingEngine sampler(1);
    LabEngine lab(1);

    // Step 1: Sweep the grid at band 0
    REQUIRE(sweep.CanSweep(grid, 0));
    SweepResult result = sweep.ExecuteSweep(grid, 0, 100.0f);
    REQUIRE(result.cellsSwept > 0);

    // Every cell within the tier's reach should now be swept
    for (int y = 0; y < grid.GetGridSize(); y++)
        for (int x = 0; x < grid.GetGridSize(); x++)
            if (grid.IsInReach(x, y))
                REQUIRE(grid.GetSubCell(x, y).hasBeenSwept);

    // Step 2: Find a reachable cell with signal and collect a sample
    auto [bestX, bestY] = InReachCoord(1);
    float bestSignal = 0.0f;
    for (int y = 0; y < grid.GetGridSize(); y++)
    {
        for (int x = 0; x < grid.GetGridSize(); x++)
        {
            if (!grid.IsInReach(x, y)) continue;
            float sig = grid.GetSubCell(x, y).sweepSignal;
            if (sig > bestSignal)
            {
                bestSignal = sig;
                bestX = x;
                bestY = y;
            }
        }
    }

    REQUIRE(sampler.CollectSample(grid, tray, bestX, bestY, DepthLayer::SURFACE));
    REQUIRE(tray.GetCount() == 1);

    Sample* sample = tray.GetSampleByIndex(0);
    REQUIRE(sample != nullptr);
    REQUIRE(sample->state == SampleState::IN_TRAY);

    // Step 3: Analyze in lab
    REQUIRE(lab.CanApplyTool(*sample, AnalysisTool::XRF));
    REQUIRE(lab.ApplyTool(*sample, AnalysisTool::XRF, 200.0f));
    REQUIRE(sample->state == SampleState::PROCESSING);
    REQUIRE(sample->analysisHistory.size() == 1);

    // If the sample has heavy elements, XRF should have produced confidence
    for (const auto& [type, abundance] : sample->trueComposition)
    {
        if (abundance > 0.01f && LabEngine::IsHeavyElement(type))
            REQUIRE(sample->elementConfidence[type] > 0.0f);
    }
}

TEST_CASE("Sweep data persists through sampling and lab stages", "[integration]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 3, 3, rm);
    SampleTray tray(2);
    SweepEngine sweep(2);
    SamplingEngine sampler(2);

    // Sweep two bands
    sweep.ExecuteSweep(grid, 0, 100.0f);
    sweep.ExecuteSweep(grid, 1, 200.0f);

    // Record pre-sample sweep state
    float sweepSignalBefore = grid.GetSubCell(3, 3).sweepSignal;
    float sweepConfBefore = grid.GetSubCell(3, 3).aggregateConfidence;

    // Collect a sample
    sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE);

    // Sweep data should be unchanged by sampling
    REQUIRE(grid.GetSubCell(3, 3).sweepSignal == sweepSignalBefore);
    REQUIRE(grid.GetSubCell(3, 3).aggregateConfidence == sweepConfBefore);
    REQUIRE(grid.GetSubCell(3, 3).hasBeenSwept);
}

TEST_CASE("Sample composition from grid matches lab analysis targets", "[integration]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SampleTray tray(2);
    SamplingEngine sampler(2);
    LabEngine lab(2);

    sampler.CollectSample(grid, tray, 4, 4, DepthLayer::SURFACE);
    Sample* sample = tray.GetSampleByIndex(0);
    REQUIRE(sample != nullptr);

    // LIBS covers all elements — every element in trueComposition should get confidence
    lab.ApplyTool(*sample, AnalysisTool::LIBS_PULSE, 100.0f);

    for (const auto& [type, abundance] : sample->trueComposition)
    {
        if (abundance > 0.01f)
        {
            REQUIRE(sample->elementConfidence.count(type) > 0);
            REQUIRE(sample->elementConfidence[type] > 0.0f);
        }
    }
}

TEST_CASE("Multiple samples from same grid have consistent ground truth", "[integration]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 10, 10, rm);
    SampleTray tray(1);
    SamplingEngine sampler(1);

    // Collect two samples from the same sub-cell at the same depth
    sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE);
    sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE);
    REQUIRE(tray.GetCount() == 2);

    Sample* s1 = tray.GetSampleByIndex(0);
    Sample* s2 = tray.GetSampleByIndex(1);

    // Same location, same depth → same composition
    REQUIRE(s1->trueComposition.size() == s2->trueComposition.size());
    for (const auto& [type, abundance] : s1->trueComposition)
    {
        REQUIRE(s2->trueComposition.count(type) > 0);
        REQUIRE_THAT(s2->trueComposition[type],
                     Catch::Matchers::WithinAbs(abundance, 0.001f));
    }
}

// --- Tier progression ---

TEST_CASE("Tier upgrade unlocks new capabilities across all engines", "[integration]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(0, 5, 5, rm);
    SweepEngine sweep(0);
    SamplingEngine sampler(0);
    LabEngine lab(0);

    // Tier 0: band 0 sweep only, surface-only drilling, visual only
    REQUIRE(sweep.CanSweep(grid, 0));
    REQUIRE_FALSE(sweep.CanSweep(grid, 1));
    REQUIRE(sampler.CanDrill(DepthLayer::SURFACE));
    REQUIRE_FALSE(sampler.CanDrill(DepthLayer::SHALLOW));
    REQUIRE(lab.CanApplyTool(MakeDummySample(), AnalysisTool::VISUAL_INSPECTION));
    REQUIRE_FALSE(lab.CanApplyTool(MakeDummySample(), AnalysisTool::XRF));

    // Upgrade to tier 1
    sweep.SetTier(1);
    sampler.SetTier(1);
    lab.SetTier(1);
    grid.ResizeForTier(1);

    REQUIRE(sweep.CanSweep(grid, 0));
    REQUIRE(sampler.CanDrill(DepthLayer::SHALLOW));
    REQUIRE(lab.CanApplyTool(MakeDummySample(), AnalysisTool::XRF));
    REQUIRE_FALSE(lab.CanApplyTool(MakeDummySample(), AnalysisTool::LIBS_PULSE));

    // Upgrade to tier 2
    sweep.SetTier(2);
    sampler.SetTier(2);
    lab.SetTier(2);
    grid.ResizeForTier(2);

    REQUIRE(sweep.CanSweep(grid, 1));
    REQUIRE(sampler.CanDrill(DepthLayer::MID));
    REQUIRE(lab.CanApplyTool(MakeDummySample(), AnalysisTool::LIBS_PULSE));
    REQUIRE_FALSE(lab.CanApplyTool(MakeDummySample(), AnalysisTool::FIRE_ASSAY));

    // Upgrade to tier 3
    sweep.SetTier(3);
    sampler.SetTier(3);
    lab.SetTier(3);
    grid.ResizeForTier(3);

    REQUIRE(sweep.CanSweep(grid, 3));
    REQUIRE(sampler.CanDrill(DepthLayer::DEEP));
    REQUIRE(lab.CanApplyTool(MakeDummySample(), AnalysisTool::FIRE_ASSAY));
}

// --- Energy budget awareness ---

TEST_CASE("Pipeline energy costs are queryable at each stage", "[integration]")
{
    SweepEngine sweep(2);
    SamplingEngine sampler(2);

    float sweepCost = sweep.GetSweepCost(0);
    float drillCost = sampler.GetDrillCost(DepthLayer::SURFACE);
    float toolCost = LabEngine::GetToolCost(AnalysisTool::XRF);
    float sepCost = LabEngine::GetSeparationCost(SeparationMethod::MAGNETIC);

    REQUIRE(sweepCost > 0.0f);
    REQUIRE(drillCost > 0.0f);
    REQUIRE(toolCost > 0.0f);
    REQUIRE(sepCost > 0.0f);

    float totalPipeline = sweepCost + drillCost + sepCost + toolCost;
    REQUIRE(totalPipeline > 0.0f);
}

// --- Sub-cell registration ---

TEST_CASE("Collected samples are registered in sub-cell sampleIds", "[integration]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 4, 4, rm);
    SampleTray tray(1);
    SamplingEngine sampler(1);

    REQUIRE(grid.GetSubCell(4, 4).sampleIds.empty());

    sampler.CollectSample(grid, tray, 4, 4, DepthLayer::SURFACE);

    REQUIRE(grid.GetSubCell(4, 4).sampleIds.size() == 1);

    int sampleId = grid.GetSubCell(4, 4).sampleIds[0];
    Sample* sample = tray.GetSampleById(sampleId);
    REQUIRE(sample != nullptr);
    REQUIRE(sample->subCellX == 4);
    REQUIRE(sample->subCellY == 4);
}

// --- Fire assay end-to-end ---

TEST_CASE("Fire assay on real sample sets 100% confidence and COMPLETED", "[integration]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 6, 6, rm);
    SampleTray tray(3);
    SamplingEngine sampler(3);
    LabEngine lab(3);

    sampler.CollectSample(grid, tray, 2, 2, DepthLayer::SURFACE);
    Sample* sample = tray.GetSampleByIndex(0);

    // Partial analysis first
    lab.ApplyTool(*sample, AnalysisTool::XRF, 100.0f);
    REQUIRE(sample->state == SampleState::PROCESSING);

    // Fire assay on Fe
    lab.ApplyTool(*sample, AnalysisTool::FIRE_ASSAY, 200.0f, ResourceType::Fe);
    REQUIRE(sample->state == SampleState::COMPLETED);
    REQUIRE_THAT(sample->elementConfidence[ResourceType::Fe],
                 Catch::Matchers::WithinAbs(1.0f, 0.001f));

    // No more operations allowed
    REQUIRE_FALSE(lab.CanApplyTool(*sample, AnalysisTool::VISUAL_INSPECTION));
    REQUIRE_FALSE(lab.CanApplySeparation(*sample, SeparationMethod::MAGNETIC));
}

// --- Separation + analysis combination ---

TEST_CASE("Separation before analysis boosts confidence for targeted elements", "[integration]")
{
    Sample withSep = MakeFullCompositionSample();
    Sample withoutSep = MakeFullCompositionSample();
    LabEngine lab(1);

    // Sample 1: magnetic separation + XRF
    lab.ApplySeparation(withSep, SeparationMethod::MAGNETIC, 100.0f);
    lab.ApplyTool(withSep, AnalysisTool::XRF, 101.0f);

    // Sample 2: XRF only
    lab.ApplyTool(withoutSep, AnalysisTool::XRF, 100.0f);

    // Fe confidence should be higher with magnetic separation (probabilistic accumulation)
    REQUIRE(withSep.elementConfidence[ResourceType::Fe]
          > withoutSep.elementConfidence[ResourceType::Fe]);
}

// --- Calibration affects sweep quality ---

TEST_CASE("Degraded calibration reduces sweep confidence gain", "[integration]")
{
    ResourceManager rm = MakeTestResourceManager();

    // Both sweeps run at tier 3 so reach is identical and calibration is the
    // only difference between them.
    ProspectingGrid grid1(3, 5, 5, rm);
    SweepEngine sweep1(3);
    sweep1.ExecuteSweep(grid1, 0, 100.0f);

    float freshConf = 0.0f;
    int sweptCells = 0;
    int size = grid1.GetGridSize();
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            if (!grid1.IsInReach(x, y)) continue;
            freshConf += grid1.GetSubCell(x, y).aggregateConfidence;
            sweptCells++;
        }
    }
    REQUIRE(sweptCells > 0);
    freshConf /= sweptCells;

    // Degrade calibration by sweeping many times on different grids
    SweepEngine sweep2(3);
    for (int i = 0; i < 20; i++)
    {
        ProspectingGrid tempGrid(3, i, i, rm);
        sweep2.ExecuteSweep(tempGrid, 0, float(i * 100));
    }

    // Now sweep2 has degraded calibration
    REQUIRE(sweep2.GetCalibrationQuality() < 1.0f);

    ProspectingGrid grid2(3, 5, 5, rm);
    sweep2.ExecuteSweep(grid2, 0, 3000.0f);

    float degradedConf = 0.0f;
    int degradedCells = 0;
    int size2 = grid2.GetGridSize();
    for (int y = 0; y < size2; y++)
    {
        for (int x = 0; x < size2; x++)
        {
            if (!grid2.IsInReach(x, y)) continue;
            degradedConf += grid2.GetSubCell(x, y).aggregateConfidence;
            degradedCells++;
        }
    }
    REQUIRE(degradedCells == sweptCells);
    degradedConf /= degradedCells;

    REQUIRE(degradedConf < freshConf);
}
