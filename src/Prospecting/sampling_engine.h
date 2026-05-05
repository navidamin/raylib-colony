#pragma once

#include <cstdint>
#include "prospecting_grid.h"
#include "sample_tray.h"
#include "prospecting_constants.h"

class SamplingEngine
{
public:
    SamplingEngine(int tier = 0);

    bool CanDrill(DepthLayer depth) const;
    float GetDrillCost(DepthLayer depth) const;

    bool CollectSample(ProspectingGrid& grid, SampleTray& tray,
                       int subX, int subY, DepthLayer depth);

    void SetTier(int tier);
    int GetTier() const;

    static CrystalVisual AssignCrystalVisual(const Sample& sample,
                                              int parentGridX, int parentGridY);
    static float CalculateRichness(const std::map<ResourceType, float>& composition);
    static ResourceType GetDominantElement(const std::map<ResourceType, float>& composition);

private:
    int tier;

    Sample CreateSample(const ProspectingGrid& grid,
                         int subX, int subY, DepthLayer depth) const;

    static uint32_t HashVisual(int subX, int subY, int depth, int px, int py);
    static uint32_t LCG(uint32_t seed);
};
