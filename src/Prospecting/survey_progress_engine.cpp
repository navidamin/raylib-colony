#include "survey_progress_engine.h"
#include <algorithm>

CellSurveyResult SurveyProgressEngine::Calculate(const ProspectingGrid& grid,
                                                   const SampleTray& tray)
{
    CellSurveyResult result;
    result.sweepConfidence = ComputeSweepComponent(grid);
    result.sampleConfidence = ComputeSampleComponent(grid, tray);
    result.testingConfidence = ComputeTestingComponent(grid, tray);

    result.surveyProgress = std::clamp(
        SURVEY_SWEEP_WEIGHT * result.sweepConfidence
      + SURVEY_SAMPLE_WEIGHT * result.sampleConfidence
      + SURVEY_TESTING_WEIGHT * result.testingConfidence,
        0.0f, 1.0f);

    return result;
}

bool SurveyProgressEngine::QualifiesAsMarkedSite(float surveyProgress)
{
    return surveyProgress >= MARKED_SITE_THRESHOLD;
}

float SurveyProgressEngine::ComputeSweepComponent(const ProspectingGrid& grid)
{
    int size = grid.GetGridSize();
    int total = size * size;
    if (total == 0) return 0.0f;

    // Sweeping is not the only way to learn what is in the ground -- digging a
    // layer observes it directly. A cell counts as known by whichever route got
    // further, so a player who never surveys still bootstraps their own
    // extraction efficiency, just the expensive way.
    float sum = 0.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            sum += std::max(grid.GetSubCell(x, y).aggregateConfidence,
                            grid.GetExcavatedKnowledge(x, y));

    return sum / total;
}

float SurveyProgressEngine::ComputeSampleComponent(const ProspectingGrid& grid,
                                                     const SampleTray& tray)
{
    int size = grid.GetGridSize();
    int total = size * size;
    if (total == 0) return 0.0f;

    int sampledCells = 0;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            if (!grid.GetSubCell(x, y).sampleIds.empty())
                sampledCells++;

    float coverageTarget = total * SURVEY_SAMPLE_COVERAGE_TARGET;
    float coverage = std::min(1.0f, sampledCells / std::max(1.0f, coverageTarget));

    auto samples = CollectGridSamples(grid, tray);
    if (samples.empty()) return 0.0f;

    float avgRichness = 0.0f;
    for (const auto* s : samples)
        avgRichness += s->richness;
    avgRichness /= samples.size();

    return coverage * std::clamp(avgRichness * 2.0f, 0.0f, 1.0f);
}

float SurveyProgressEngine::ComputeTestingComponent(const ProspectingGrid& grid,
                                                      const SampleTray& tray)
{
    auto samples = CollectGridSamples(grid, tray);

    std::vector<const Sample*> analyzed;
    for (const auto* s : samples)
        if (!s->analysisHistory.empty())
            analyzed.push_back(s);

    if (analyzed.empty()) return 0.0f;

    float sum = 0.0f;
    for (const auto* s : analyzed)
        sum += s->GetAggregateConfidence();

    return sum / analyzed.size();
}

std::vector<const Sample*> SurveyProgressEngine::CollectGridSamples(
    const ProspectingGrid& grid, const SampleTray& tray)
{
    std::vector<const Sample*> result;
    int size = grid.GetGridSize();

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            const SubCell& cell = grid.GetSubCell(x, y);
            for (int id : cell.sampleIds)
            {
                const Sample* s = tray.GetSampleById(id);
                if (s) result.push_back(s);
            }
        }
    }
    return result;
}
