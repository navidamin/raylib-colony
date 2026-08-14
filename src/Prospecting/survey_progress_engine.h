#pragma once

#include "prospecting_grid.h"
#include "sample_tray.h"
#include "prospecting_constants.h"
#include <vector>

struct CellSurveyResult
{
    float sweepConfidence = 0.0f;
    float sampleConfidence = 0.0f;
    float testingConfidence = 0.0f;
    float surveyProgress = 0.0f;
};

class SurveyProgressEngine
{
public:
    static CellSurveyResult Calculate(const ProspectingGrid& grid,
                                       const SampleTray& tray);

    static bool QualifiesAsMarkedSite(float surveyProgress);

    static float ComputeSweepComponent(const ProspectingGrid& grid);
    static float ComputeSampleComponent(const ProspectingGrid& grid,
                                         const SampleTray& tray);
    static float ComputeTestingComponent(const ProspectingGrid& grid,
                                          const SampleTray& tray);

private:
    static std::vector<const Sample*> CollectGridSamples(
        const ProspectingGrid& grid, const SampleTray& tray);
};
