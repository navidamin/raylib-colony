#include "prospecting_system.h"

ProspectingSystem::ProspectingSystem(int tier, int parentGridX, int parentGridY,
                                     ResourceManager& resourceManager)
    : tier(tier)
    , resourceManager(resourceManager)
    , grid(tier, parentGridX, parentGridY, resourceManager)
    , tray(tier)
    , sweep(tier)
    , sampler(tier)
    , lab(tier)
{
    // Start on a cell the instruments can actually reach, so the cell readout
    // is meaningful before the player clicks anything.
    int centre = GetGridSizeForTier(tier) / 2;
    selectedCellX = centre;
    selectedCellY = centre;
}

float ProspectingSystem::GetSurveyProgress() const
{
    EnsureCache();
    return cachedResult.surveyProgress;
}

bool ProspectingSystem::IsMarkedSite() const
{
    return SurveyProgressEngine::QualifiesAsMarkedSite(GetSurveyProgress());
}

void ProspectingSystem::SetTier(int newTier)
{
    tier = newTier;
    grid.ResizeForTier(newTier);
    tray.SetTier(newTier);
    sweep.SetTier(newTier);
    sampler.SetTier(newTier);
    lab.SetTier(newTier);
    InvalidateCache();
}

int ProspectingSystem::GetTier() const
{
    return tier;
}

ProspectingGrid& ProspectingSystem::GetGrid()
{
    InvalidateCache();
    return grid;
}

const ProspectingGrid& ProspectingSystem::GetGrid() const
{
    return grid;
}

SampleTray& ProspectingSystem::GetTray()
{
    InvalidateCache();
    return tray;
}

const SampleTray& ProspectingSystem::GetTray() const
{
    return tray;
}

SweepEngine& ProspectingSystem::GetSweep()
{
    InvalidateCache();
    return sweep;
}

const SweepEngine& ProspectingSystem::GetSweep() const
{
    return sweep;
}

SamplingEngine& ProspectingSystem::GetSampler()
{
    InvalidateCache();
    return sampler;
}

const SamplingEngine& ProspectingSystem::GetSampler() const
{
    return sampler;
}

LabEngine& ProspectingSystem::GetLab()
{
    InvalidateCache();
    return lab;
}

const LabEngine& ProspectingSystem::GetLab() const
{
    return lab;
}

void ProspectingSystem::InvalidateCache()
{
    cacheValid = false;
}

void ProspectingSystem::EnsureCache() const
{
    if (!cacheValid)
    {
        cachedResult = SurveyProgressEngine::Calculate(grid, tray);
        cacheValid = true;
    }
}
