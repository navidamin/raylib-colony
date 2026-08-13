#include "prospecting_grid.h"
#include <cmath>
#include <algorithm>

ProspectingGrid::ProspectingGrid(int tier, int parentGridX, int parentGridY,
                                  ResourceManager& resourceManager)
    : tier(tier)
    , gridSize(GetGridSizeForTier(tier))
    , parentGridX(parentGridX)
    , parentGridY(parentGridY)
    , resourceManager(resourceManager)
{
    AllocateGrid();
    GenerateSubCellDistribution();
}

int ProspectingGrid::GetGridSize() const { return gridSize; }
int ProspectingGrid::GetTier() const { return tier; }
int ProspectingGrid::GetParentGridX() const { return parentGridX; }
int ProspectingGrid::GetParentGridY() const { return parentGridY; }

const SubCell& ProspectingGrid::GetSubCell(int x, int y) const
{
    return cells[y][x];
}

SubCell& ProspectingGrid::GetSubCellMut(int x, int y)
{
    return cells[y][x];
}

std::map<ResourceType, float> ProspectingGrid::GetGroundTruth(int subX, int subY,
                                                               DepthLayer depth) const
{
    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return {};
    if (subY < 0 || subY >= gridSize || subX < 0 || subX >= gridSize) return {};
    return subCellResources[d][subY][subX];
}

float ProspectingGrid::GetQuantity(int subX, int subY, DepthLayer depth) const
{
    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return 0.0f;
    if (subY < 0 || subY >= gridSize || subX < 0 || subX >= gridSize) return 0.0f;
    return subCellQuantities[d][subY][subX];
}

float ProspectingGrid::GetTotalRichness(int subX, int subY) const
{
    float total = 0.0f;
    int maxDepth = MAX_DEPTH_PER_TIER[tier];

    for (int d = 0; d < maxDepth; d++)
    {
        total += GetQuantity(subX, subY, static_cast<DepthLayer>(d));
    }
    return total;
}

void ProspectingGrid::RecordSweep(int frequencyBand, float energyCost, float timestamp)
{
    sweepHistory.push_back({ frequencyBand, energyCost, timestamp });
}

const std::vector<SweepRecord>& ProspectingGrid::GetSweepHistory() const
{
    return sweepHistory;
}

bool ProspectingGrid::HasSweptFrequency(int frequencyBand) const
{
    for (const auto& record : sweepHistory)
    {
        if (record.frequencyBand == frequencyBand) return true;
    }
    return false;
}

void ProspectingGrid::ResizeForTier(int newTier)
{
    tier = newTier;
    gridSize = GetGridSizeForTier(newTier);
    AllocateGrid();
    GenerateSubCellDistribution();
}

void ProspectingGrid::AllocateGrid()
{
    cells.assign(gridSize, std::vector<SubCell>(gridSize));

    for (int d = 0; d < 4; d++)
    {
        subCellResources[d].assign(gridSize,
            std::vector<std::map<ResourceType, float>>(gridSize));
        subCellQuantities[d].assign(gridSize, std::vector<float>(gridSize, 0.0f));
    }
}

void ProspectingGrid::GenerateSubCellDistribution()
{
    int maxDepth = MAX_DEPTH_PER_TIER[tier];

    for (int d = 0; d < maxDepth; d++)
    {
        DepthLayer depth = static_cast<DepthLayer>(d);
        auto parentResources = resourceManager.GetResourcesAtGridLayer(
            parentGridX, parentGridY, depth);
        GenerateLayerDistribution(depth, parentResources);
    }

    // ResourceManager works in absolute quantities (hundreds to thousands per
    // cell); the prospecting chain works in composition fractions. Record the
    // absolute total per sub-cell, then normalize the per-element values into
    // fractions that sum to 1.
    for (int d = 0; d < maxDepth; d++)
    {
        for (int y = 0; y < gridSize; y++)
        {
            for (int x = 0; x < gridSize; x++)
            {
                auto& cellResources = subCellResources[d][y][x];

                float total = 0.0f;
                for (const auto& [type, quantity] : cellResources)
                {
                    total += quantity;
                }
                subCellQuantities[d][y][x] = total;

                if (total > 0.0f)
                {
                    for (auto& [type, quantity] : cellResources)
                    {
                        quantity /= total;
                    }
                }
            }
        }
    }
}

void ProspectingGrid::GenerateLayerDistribution(
    DepthLayer depth,
    const std::vector<std::pair<ResourceType, float>>& parentResources)
{
    int d = static_cast<int>(depth);
    int resourceIdx = 0;

    for (const auto& [type, abundance] : parentResources)
    {
        if (abundance < 0.001f)
        {
            resourceIdx++;
            continue;
        }

        uint32_t seed = HashSeed(parentGridX, parentGridY, d, resourceIdx);

        // Generate 1-2 hot-spot cluster centers for this resource
        seed = LCG(seed);
        int numClusters = 1 + (seed % 2);

        float clusterX[2], clusterY[2], clusterRadius[2];
        for (int c = 0; c < numClusters; c++)
        {
            seed = LCG(seed);
            clusterX[c] = (seed % 1000) / 1000.0f * gridSize;
            seed = LCG(seed);
            clusterY[c] = (seed % 1000) / 1000.0f * gridSize;
            seed = LCG(seed);
            clusterRadius[c] = 0.8f + (seed % 1000) / 1000.0f * 1.5f;
        }

        // Compute spatial weights via gaussian falloff from cluster centers
        float weights[PROSPECTING_MAX_GRID_SIZE][PROSPECTING_MAX_GRID_SIZE];
        float totalWeight = 0.0f;

        for (int y = 0; y < gridSize; y++)
        {
            for (int x = 0; x < gridSize; x++)
            {
                float maxInfluence = 0.1f;

                for (int c = 0; c < numClusters; c++)
                {
                    float dx = (x + 0.5f) - clusterX[c];
                    float dy = (y + 0.5f) - clusterY[c];
                    float distSq = dx*dx + dy*dy;
                    float sigma = clusterRadius[c];
                    float influence = expf(-distSq / (2.0f * sigma*sigma));
                    if (influence > maxInfluence)
                        maxInfluence = influence;
                }

                weights[y][x] = maxInfluence;
                totalWeight += maxInfluence;
            }
        }

        // Normalize so mean weight = 1.0, then clamp to variation range
        float avgWeight = totalWeight / (gridSize*gridSize);
        if (avgWeight < 0.001f) avgWeight = 0.001f;

        for (int y = 0; y < gridSize; y++)
        {
            for (int x = 0; x < gridSize; x++)
            {
                float w = weights[y][x] / avgWeight;
                if (w < SUBCELL_VARIATION_MIN) w = SUBCELL_VARIATION_MIN;
                if (w > SUBCELL_VARIATION_MAX) w = SUBCELL_VARIATION_MAX;
                subCellResources[d][y][x][type] = abundance * w;
            }
        }

        resourceIdx++;
    }
}

uint32_t ProspectingGrid::HashSeed(int px, int py, int depth, int resourceIdx)
{
    uint32_t h = 2166136261u;
    h ^= static_cast<uint32_t>(px);
    h *= 16777619u;
    h ^= static_cast<uint32_t>(py);
    h *= 16777619u;
    h ^= static_cast<uint32_t>(depth);
    h *= 16777619u;
    h ^= static_cast<uint32_t>(resourceIdx);
    h *= 16777619u;
    return h;
}

uint32_t ProspectingGrid::LCG(uint32_t seed)
{
    return seed * 1664525u + 1013904223u;
}
