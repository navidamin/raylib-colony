#include "prospecting_grid.h"
#include "sample_tray.h"
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

bool ProspectingGrid::IsInReach(int subX, int subY) const
{
    return IsSubCellInReach(subX, subY, tier);
}

int ProspectingGrid::GetReach() const
{
    return GetReachForTier(tier);
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

void ProspectingGrid::RecordExcavation(int subX, int subY, DepthLayer depth,
                                       float fraction)
{
    if (subX < 0 || subX >= gridSize || subY < 0 || subY >= gridSize) return;

    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return;
    if (fraction <= 0.0f) return;

    SubCell& cell = cells[subY][subX];
    float worked = cell.workedFraction[d] + fraction;
    cell.workedFraction[d] = worked > 1.0f ? 1.0f : worked;
}

float ProspectingGrid::GetExcavatedKnowledge(int subX, int subY) const
{
    if (subX < 0 || subX >= gridSize || subY < 0 || subY >= gridSize) return 0.0f;

    const SubCell& cell = cells[subY][subX];

    // Each layer dug is a quarter of the column observed directly.
    int dug = 0;
    for (int d = 0; d < 4; d++)
    {
        if (cell.HasBeenDug(d)) dug++;
    }
    return static_cast<float>(dug) / 4.0f;
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
    // The lattice is fixed, so a tier change only extends reach. Nothing is
    // reallocated -- which is what preserves sweep data, confidence, and the
    // sub-cell links held by collected samples across an upgrade. (The
    // previous size-changing grid wiped all of that on every tier-up.)
    tier = newTier;
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

// ---------------------------------------------------------------------------
// Per-depth confidence, classification and roll-up
//
// Moved here from Excavation/site_view.cpp unchanged: every input is
// prospecting state, and two copies of this arithmetic is exactly the drift
// the single-threshold rule exists to prevent.
// ---------------------------------------------------------------------------

float GetDepthConfidence(const ProspectingGrid& grid, const SampleTray& tray,
                         int x, int y, DepthLayer depth)
{
    int size = grid.GetGridSize();
    if (x < 0 || x >= size || y < 0 || y >= size) return 0.0f;

    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return 0.0f;

    const SubCell& cell = grid.GetSubCell(x, y);

    // --- Direct observation ---
    if (cell.HasBeenDug(d)) return 1.0f;

    // --- Sweep evidence ---
    float sweepConfidence = 0.0f;
    if (cell.hasBeenSwept && cell.sweepFrequencyBand >= 0)
    {
        int band = cell.sweepFrequencyBand;
        if (band < 0) band = 0;
        if (band > SWEEP_FREQUENCY_BANDS - 1) band = SWEEP_FREQUENCY_BANDS - 1;

        if (d < SWEEP_DEPTH_PENETRATION[band])
        {
            float attenuation = 1.0f / (1.0f + d * SWEEP_DEPTH_ATTENUATION);
            sweepConfidence = cell.aggregateConfidence * attenuation *
                              PROSPECT_SWEEP_CONFIDENCE_WEIGHT;
        }
    }

    // --- Sample evidence ---
    float sampleConfidence = 0.0f;
    const std::vector<Sample>& samples = tray.GetSamples();
    for (const Sample& sample : samples)
    {
        if (sample.subCellX != x || sample.subCellY != y) continue;
        if (sample.depthLayer != depth) continue;

        float aggregate = sample.GetAggregateConfidence();
        float weighted = aggregate * PROSPECT_SAMPLE_CONFIDENCE_WEIGHT;
        if (weighted > sampleConfidence) sampleConfidence = weighted;
    }

    // Independent evidence combines rather than replaces.
    float combined = 1.0f - (1.0f - sweepConfidence) * (1.0f - sampleConfidence);
    if (combined < 0.0f) combined = 0.0f;
    if (combined > 1.0f) combined = 1.0f;
    return combined;
}

float GetSubCellYield(const ProspectingGrid& grid, int x, int y,
                      DepthLayer depth, ResourceType type)
{
    // quantity (absolute) x composition (fraction). Neither alone is a yield.
    float quantity = grid.GetQuantity(x, y, depth);
    if (quantity <= 0.0f) return 0.0f;

    std::map<ResourceType, float> composition = grid.GetGroundTruth(x, y, depth);
    auto it = composition.find(type);
    if (it == composition.end()) return 0.0f;

    return quantity * it->second;
}

float ClassSplit::Total() const
{
    return measured + indicated + inferred + unclassified;
}

float ClassSplit::Committable() const
{
    return measured + indicated;
}

float ClassSplit::Get(ResourceClass cls) const
{
    switch (cls)
    {
        case ResourceClass::MEASURED:  return measured;
        case ResourceClass::INDICATED: return indicated;
        case ResourceClass::INFERRED:  return inferred;
        default:                       return unclassified;
    }
}

ClassSplit GetClassSplit(const ProspectingGrid& grid, const SampleTray& tray,
                         ResourceType type, int tier)
{
    ClassSplit split;

    if (tier < 0) tier = 0;
    if (tier > 3) tier = 3;

    int size = grid.GetGridSize();
    int maxDepth = MAX_DEPTH_PER_TIER[tier];

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            // Ground the instruments cannot reach is not part of the statement.
            if (!IsSubCellInReach(x, y, tier)) continue;

            for (int d = 0; d < maxDepth; d++)
            {
                DepthLayer depth = static_cast<DepthLayer>(d);

                float yield = GetSubCellYield(grid, x, y, depth, type);
                if (yield <= 0.0f) continue;

                float confidence = GetDepthConfidence(grid, tray, x, y, depth);

                switch (GetResourceClass(confidence))
                {
                    case ResourceClass::MEASURED:  split.measured     += yield; break;
                    case ResourceClass::INDICATED: split.indicated    += yield; break;
                    case ResourceClass::INFERRED:  split.inferred     += yield; break;
                    default:                       split.unclassified += yield; break;
                }
            }
        }
    }

    return split;
}
