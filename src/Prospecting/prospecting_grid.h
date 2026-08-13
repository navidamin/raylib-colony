#pragma once

#include <cstdint>
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

    // Composition fractions (0-1, sum to ~1 across elements) -- what a sample
    // "is made of". ResourceManager stores absolute quantities; those are
    // normalized here so the whole prospecting chain speaks in fractions.
    std::map<ResourceType, float> GetGroundTruth(int subX, int subY, DepthLayer depth) const;

    // Absolute deposit quantity for a sub-cell layer -- how *much* is there.
    // Drives sweep signal strength and sample richness.
    float GetQuantity(int subX, int subY, DepthLayer depth) const;

    float GetTotalRichness(int subX, int subY) const;

    void RecordSweep(int frequencyBand, float energyCost, float timestamp);
    const std::vector<SweepRecord>& GetSweepHistory() const;
    bool HasSweptFrequency(int frequencyBand) const;

    // Tier only changes instrument reach; the lattice is fixed, so survey
    // data and sample links survive an upgrade.
    void ResizeForTier(int newTier);

    // Reach helpers, forwarded with this grid's tier applied.
    bool IsInReach(int subX, int subY) const;
    int GetReach() const;

private:
    int tier;
    int gridSize;
    int parentGridX;
    int parentGridY;
    ResourceManager& resourceManager;

    std::vector<std::vector<SubCell>> cells;
    std::vector<SweepRecord> sweepHistory;

    // Pre-computed sub-cell resource distribution per depth layer
    // subCellResources[depth][y][x] = element composition fractions (0-1)
    std::vector<std::vector<std::map<ResourceType, float>>> subCellResources[4];

    // subCellQuantities[depth][y][x] = total absolute deposit quantity
    std::vector<std::vector<float>> subCellQuantities[4];

    void AllocateGrid();
    void GenerateSubCellDistribution();
    void GenerateLayerDistribution(DepthLayer depth,
                                    const std::vector<std::pair<ResourceType, float>>& parentResources);

    static uint32_t HashSeed(int px, int py, int depth, int resourceIdx);
    static uint32_t LCG(uint32_t seed);
};
