#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include "prospecting_types.h"
#include "resource_manager.h"

class SampleTray;

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

    // Excavation reports what it has dug. This is the ONLY way the worked
    // state is written, and it is called by excavation -- prospecting never
    // reaches into excavation.
    void RecordExcavation(int subX, int subY, DepthLayer depth, float fraction);

    // Fraction of a sub-cell's depth column that has been dug out. Digging is
    // direct observation, so this counts toward how well the cell is known.
    float GetExcavatedKnowledge(int subX, int subY) const;

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

// What is known about one spot at ONE depth.
//
// The grid keeps a single aggregateConfidence per sub-cell, so this
// reconstructs the per-depth view from sweep evidence (attenuated by depth,
// and only for layers the swept band penetrated) combined with sample
// evidence (samples taken at this exact spot and depth). A dug layer is
// direct observation and returns 1.0 -- per depth, so digging the surface
// says nothing about what lies under it.
//
// This lives in prospecting because every input is prospecting state.
// Excavation's SiteView::GetConfidence delegates here rather than keeping its
// own copy; if confidence is ever stored per depth directly, this is still
// the only function to change.
float GetDepthConfidence(const ProspectingGrid& grid, const SampleTray& tray,
                         int x, int y, DepthLayer depth);

// Yield of one resource at a spot: absolute quantity x composition fraction.
// The product is the number worth choosing between spots on -- quantity alone
// is much flatter, because each resource clusters separately. Naming it here
// keeps the units trap (module-architecture.md Part II) in one place.
float GetSubCellYield(const ProspectingGrid& grid, int x, int y,
                      DepthLayer depth, ResourceType type);

// Tonnage of one resource split by how well it is known. Summed over every
// sub-cell within reach at `tier`, across every depth that tier can see.
struct ClassSplit
{
    float measured = 0.0f;
    float indicated = 0.0f;
    float inferred = 0.0f;
    float unclassified = 0.0f;

    float Total() const;
    float Committable() const;      // measured + indicated
    float Get(ResourceClass cls) const;
};

ClassSplit GetClassSplit(const ProspectingGrid& grid, const SampleTray& tray,
                         ResourceType type, int tier);

