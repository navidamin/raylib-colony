#pragma once

#include <vector>
#include <map>
#include "prospecting_types.h"
#include "resource_manager.h"

class ProspectingGrid
{
public:
    ProspectingGrid(int tier, int parentGridX, int parentGridY, ResourceManager& resourceManager);

    int GetGridSize() const;
    int GetTier() const;
    int GetParentGridX() const;
    int GetParentGridY() const;

    const SubCell& GetSubCell(int x, int y) const;
    SubCell& GetSubCellMut(int x, int y);

    std::map<ResourceType, float> GetGroundTruth(int subX, int subY, DepthLayer depth) const;
    float GetTotalRichness(int subX, int subY) const;

    void RecordSweep(int frequencyBand, float energyCost, float timestamp);
    const std::vector<SweepRecord>& GetSweepHistory() const;
    bool HasSweptFrequency(int frequencyBand) const;

    void ResizeForTier(int newTier);

private:
    int tier;
    int gridSize;
    int parentGridX;
    int parentGridY;
    ResourceManager& resourceManager;

    std::vector<std::vector<SubCell>> cells;
    std::vector<SweepRecord> sweepHistory;

    // Pre-computed sub-cell resource distribution per depth layer
    // subCellResources[depth][y][x] = element abundances
    std::vector<std::vector<std::map<ResourceType, float>>> subCellResources[4];

    void AllocateGrid();
    void GenerateSubCellDistribution();
    void GenerateLayerDistribution(DepthLayer depth,
                                    const std::vector<std::pair<ResourceType, float>>& parentResources);

    static uint32_t HashSeed(int px, int py, int depth, int resourceIdx);
    static uint32_t LCG(uint32_t seed);
};
