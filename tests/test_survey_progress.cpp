#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "survey_progress_engine.h"
#include "sweep_engine.h"
#include "sampling_engine.h"
#include "lab_engine.h"

// --- Component isolation ---

TEST_CASE("Unswept, unsampled grid has zero progress", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);

    CellSurveyResult result = SurveyProgressEngine::Calculate(grid, tray);
    REQUIRE(result.sweepConfidence == 0.0f);
    REQUIRE(result.sampleConfidence == 0.0f);
    REQUIRE(result.testingConfidence == 0.0f);
    REQUIRE(result.surveyProgress == 0.0f);
}

TEST_CASE("Sweep-only produces sweep component only", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SweepEngine sweep(1);

    sweep.ExecuteSweep(grid, 0, 100.0f);

    CellSurveyResult result = SurveyProgressEngine::Calculate(grid, tray);
    REQUIRE(result.sweepConfidence > 0.0f);
    REQUIRE(result.sampleConfidence == 0.0f);
    REQUIRE(result.testingConfidence == 0.0f);
    REQUIRE(result.surveyProgress > 0.0f);
    REQUIRE_THAT(result.surveyProgress,
                 Catch::Matchers::WithinAbs(
                     SURVEY_SWEEP_WEIGHT * result.sweepConfidence, 0.001f));
}

TEST_CASE("Sample-only produces sample component only", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SamplingEngine sampler(1);

    REQUIRE(sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));

    CellSurveyResult result = SurveyProgressEngine::Calculate(grid, tray);
    REQUIRE(result.sweepConfidence == 0.0f);
    REQUIRE(result.sampleConfidence > 0.0f);
    REQUIRE(result.testingConfidence == 0.0f);
    REQUIRE_THAT(result.surveyProgress,
                 Catch::Matchers::WithinAbs(
                     SURVEY_SAMPLE_WEIGHT * result.sampleConfidence, 0.001f));
}

TEST_CASE("Testing component requires analyzed samples", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SamplingEngine sampler(1);
    LabEngine lab(1);

    REQUIRE(sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    Sample* sample = tray.GetSampleByIndex(0);

    // Before lab analysis
    float beforeTesting = SurveyProgressEngine::ComputeTestingComponent(grid, tray);
    REQUIRE(beforeTesting == 0.0f);

    // After lab analysis — testing component increases if sample has significant elements
    lab.ApplyTool(*sample, AnalysisTool::LIBS_PULSE, 100.0f);
    float afterTesting = SurveyProgressEngine::ComputeTestingComponent(grid, tray);
    REQUIRE(afterTesting >= beforeTesting);
    if (sample->GetAggregateConfidence() > 0.0f)
        REQUIRE(afterTesting > 0.0f);
}

// --- Weighted formula ---

TEST_CASE("Survey progress is weighted sum of components", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 5, 5, rm);
    SampleTray tray(2);
    SweepEngine sweep(2);
    SamplingEngine sampler(2);
    LabEngine lab(2);

    sweep.ExecuteSweep(grid, 0, 100.0f);
    REQUIRE(sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    lab.ApplyTool(*tray.GetSampleByIndex(0), AnalysisTool::XRF, 200.0f);

    CellSurveyResult result = SurveyProgressEngine::Calculate(grid, tray);

    float expected = SURVEY_SWEEP_WEIGHT * result.sweepConfidence
                   + SURVEY_SAMPLE_WEIGHT * result.sampleConfidence
                   + SURVEY_TESTING_WEIGHT * result.testingConfidence;
    expected = std::clamp(expected, 0.0f, 1.0f);

    REQUIRE_THAT(result.surveyProgress,
                 Catch::Matchers::WithinAbs(expected, 0.001f));
}

TEST_CASE("Weights sum to 1.0", "[survey]")
{
    float sum = SURVEY_SWEEP_WEIGHT + SURVEY_SAMPLE_WEIGHT + SURVEY_TESTING_WEIGHT;
    REQUIRE_THAT(sum, Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

// --- Progressive improvement ---

TEST_CASE("Sweep and sampling stages increase survey progress", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 5, 5, rm);
    SampleTray tray(2);
    SweepEngine sweep(2);
    SamplingEngine sampler(2);

    float p0 = SurveyProgressEngine::Calculate(grid, tray).surveyProgress;
    REQUIRE(p0 == 0.0f);

    sweep.ExecuteSweep(grid, 0, 100.0f);
    float p1 = SurveyProgressEngine::Calculate(grid, tray).surveyProgress;
    REQUIRE(p1 > p0);

    REQUIRE(sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    float p2 = SurveyProgressEngine::Calculate(grid, tray).surveyProgress;
    REQUIRE(p2 > p1);
}

TEST_CASE("Lab analysis adds testing component to progress", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 5, 5, rm);
    SampleTray tray(2);
    SamplingEngine sampler(2);
    LabEngine lab(2);

    REQUIRE(sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    Sample* sample = tray.GetSampleByIndex(0);

    float beforeLab = SurveyProgressEngine::Calculate(grid, tray).testingConfidence;
    REQUIRE(beforeLab == 0.0f);

    lab.ApplyTool(*sample, AnalysisTool::LIBS_PULSE, 200.0f);

    float afterLab = SurveyProgressEngine::Calculate(grid, tray).testingConfidence;
    // Testing confidence increases if sample has elements above min abundance
    REQUIRE(afterLab >= beforeLab);

    // Verify aggregate confidence was actually computed from the sample
    if (sample->GetAggregateConfidence() > 0.0f)
        REQUIRE(afterLab > 0.0f);
}

TEST_CASE("More sweeps increase sweep component", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 5, 5, rm);
    SampleTray tray(2);
    SweepEngine sweep(2);

    sweep.ExecuteSweep(grid, 0, 100.0f);
    float after1 = SurveyProgressEngine::ComputeSweepComponent(grid);

    sweep.ExecuteSweep(grid, 1, 200.0f);
    float after2 = SurveyProgressEngine::ComputeSweepComponent(grid);

    REQUIRE(after2 >= after1);
}

TEST_CASE("More samples increase sample component", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SamplingEngine sampler(1);

    REQUIRE(sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    float after1 = SurveyProgressEngine::ComputeSampleComponent(grid, tray);

    REQUIRE(sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    float after2 = SurveyProgressEngine::ComputeSampleComponent(grid, tray);

    // Coverage increases; combined product should not decrease
    REQUIRE(after2 >= after1);
}

TEST_CASE("Better lab analysis increases testing component", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 5, 5, rm);
    SampleTray tray(2);
    SamplingEngine sampler(2);
    LabEngine lab(2);

    REQUIRE(sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    Sample* sample = tray.GetSampleByIndex(0);

    lab.ApplyTool(*sample, AnalysisTool::VISUAL_INSPECTION, 100.0f);
    float afterVisual = SurveyProgressEngine::ComputeTestingComponent(grid, tray);

    lab.ApplyTool(*sample, AnalysisTool::LIBS_PULSE, 200.0f);
    float afterLIBS = SurveyProgressEngine::ComputeTestingComponent(grid, tray);

    REQUIRE(afterLIBS > afterVisual);
}

// --- Marked site qualification ---

TEST_CASE("Marked site qualification threshold", "[survey]")
{
    REQUIRE_FALSE(SurveyProgressEngine::QualifiesAsMarkedSite(0.0f));
    REQUIRE_FALSE(SurveyProgressEngine::QualifiesAsMarkedSite(0.59f));
    REQUIRE(SurveyProgressEngine::QualifiesAsMarkedSite(0.60f));
    REQUIRE(SurveyProgressEngine::QualifiesAsMarkedSite(1.0f));
}

// --- Sweep component details ---

TEST_CASE("Sweep component averages all sub-cell confidences", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SweepEngine sweep(1);

    sweep.ExecuteSweep(grid, 0, 100.0f);

    int size = grid.GetGridSize();
    float manualSum = 0.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            manualSum += grid.GetSubCell(x, y).aggregateConfidence;
    float expected = manualSum / (size * size);

    float computed = SurveyProgressEngine::ComputeSweepComponent(grid);
    REQUIRE_THAT(computed, Catch::Matchers::WithinAbs(expected, 0.001f));
}

// --- Sample component coverage mechanics ---

TEST_CASE("Sample coverage caps at target fraction", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SampleTray tray(1);
    SamplingEngine sampler(1);

    // Grid is 4x4 = 16 cells, target = 25% = 4 cells
    // Sample all 4 cells needed for full coverage
    int gridSize = grid.GetGridSize();
    int targetCells = static_cast<int>(gridSize * gridSize * SURVEY_SAMPLE_COVERAGE_TARGET);
    if (targetCells < 1) targetCells = 1;

    for (int i = 0; i < targetCells && i < gridSize * gridSize; i++)
    {
        int x = i % gridSize;
        int y = i / gridSize;
        sampler.CollectSample(grid, tray, x, y, DepthLayer::SURFACE);
    }

    float atTarget = SurveyProgressEngine::ComputeSampleComponent(grid, tray);

    // Sampling one more shouldn't increase coverage factor (already at 1.0)
    int nextX = targetCells % gridSize;
    int nextY = targetCells / gridSize;
    if (nextY < gridSize)
    {
        sampler.CollectSample(grid, tray, nextX, nextY, DepthLayer::SURFACE);
        float overTarget = SurveyProgressEngine::ComputeSampleComponent(grid, tray);
        // Should be similar — coverage capped, but richness might change slightly
        REQUIRE_THAT(overTarget,
                     Catch::Matchers::WithinAbs(atTarget, 0.1f));
    }
}

// --- Empty tray edge cases ---

TEST_CASE("Removed samples don't count toward testing component", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 5, 5, rm);
    SampleTray tray(2);
    SamplingEngine sampler(2);
    LabEngine lab(2);

    // Collect and analyze with LIBS for guaranteed confidence gain
    REQUIRE(sampler.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));
    lab.ApplyTool(*tray.GetSampleByIndex(0), AnalysisTool::LIBS_PULSE, 100.0f);

    float testingBefore = SurveyProgressEngine::ComputeTestingComponent(grid, tray);

    int sampleId = tray.GetSampleByIndex(0)->id;
    tray.RemoveSample(sampleId);

    float testingAfter = SurveyProgressEngine::ComputeTestingComponent(grid, tray);
    REQUIRE(testingAfter == 0.0f);
    REQUIRE(testingAfter <= testingBefore);
}

// --- Survey progress clamping ---

TEST_CASE("Survey progress is clamped to 0-1", "[survey]")
{
    ResourceManager rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 5, 5, rm);
    SampleTray tray(3);
    SweepEngine sweep(3);
    SamplingEngine sampler(3);
    LabEngine lab(3);

    // Do everything: multiple sweeps, many samples, heavy analysis
    sweep.ExecuteSweep(grid, 0, 100.0f);
    sweep.ExecuteSweep(grid, 1, 200.0f);
    sweep.ExecuteSweep(grid, 2, 300.0f);
    sweep.ExecuteSweep(grid, 3, 400.0f);

    int size = grid.GetGridSize();
    for (int y = 0; y < size && tray.GetCount() < tray.GetCapacity(); y++)
        for (int x = 0; x < size && tray.GetCount() < tray.GetCapacity(); x++)
            sampler.CollectSample(grid, tray, x, y, DepthLayer::SURFACE);

    for (int i = 0; i < tray.GetCount(); i++)
    {
        Sample* s = tray.GetSampleByIndex(i);
        lab.ApplyTool(*s, AnalysisTool::LIBS_PULSE, float(500 + i));
        lab.ApplyTool(*s, AnalysisTool::XRF, float(600 + i));
    }

    CellSurveyResult result = SurveyProgressEngine::Calculate(grid, tray);
    REQUIRE(result.surveyProgress >= 0.0f);
    REQUIRE(result.surveyProgress <= 1.0f);
}
