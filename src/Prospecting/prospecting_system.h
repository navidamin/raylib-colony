#pragma once

#include "prospecting_grid.h"
#include "sample_tray.h"
#include "sweep_engine.h"
#include "sampling_engine.h"
#include "lab_engine.h"
#include "survey_progress_engine.h"

enum class ProspectingTab { SWEEP, SAMPLES, LAB };

class ProspectingSystem
{
public:
    ProspectingSystem(int tier, int parentGridX, int parentGridY,
                      ResourceManager& resourceManager);

    float GetSurveyProgress() const;
    bool IsMarkedSite() const;

    void SetTier(int tier);
    int GetTier() const;

    ProspectingGrid& GetGrid();
    const ProspectingGrid& GetGrid() const;
    SampleTray& GetTray();
    const SampleTray& GetTray() const;
    SweepEngine& GetSweep();
    const SweepEngine& GetSweep() const;
    SamplingEngine& GetSampler();
    const SamplingEngine& GetSampler() const;
    LabEngine& GetLab();
    const LabEngine& GetLab() const;

    // UI state
    ProspectingTab activeTab = ProspectingTab::SWEEP;
    int selectedCellX = -1;
    int selectedCellY = -1;
    int selectedSampleIndex = -1;
    int selectedFrequencyBand = 0;
    DepthLayer selectedDepth = DepthLayer::SURFACE;
    float gameTime = 0.0f;

private:
    int tier;
    ResourceManager& resourceManager;

    ProspectingGrid grid;
    SampleTray tray;
    SweepEngine sweep;
    SamplingEngine sampler;
    LabEngine lab;

    mutable CellSurveyResult cachedResult;
    mutable bool cacheValid = false;

    void InvalidateCache();
    void EnsureCache() const;
};
