#include "sampling_engine.h"
#include <algorithm>
#include <cmath>

SamplingEngine::SamplingEngine(int tier)
    : tier(std::clamp(tier, 0, 3))
{
}

bool SamplingEngine::CanDrill(DepthLayer depth) const
{
    return IsLayerAccessible(tier, depth);
}

float SamplingEngine::GetDrillCost(DepthLayer depth) const
{
    int layerIdx = static_cast<int>(depth);
    if (layerIdx < 0 || layerIdx > 3) return -1.0f;
    return DRILL_ENERGY_COST[tier][layerIdx];
}

bool SamplingEngine::CollectSample(ProspectingGrid& grid, SampleTray& tray,
                                    int subX, int subY, DepthLayer depth)
{
    if (!CanDrill(depth))
        return false;

    if (tray.IsFull())
        return false;

    int size = grid.GetGridSize();
    if (subX < 0 || subX >= size || subY < 0 || subY >= size)
        return false;

    // Out-of-reach sub-cells exist and hold real data, but the drill cannot
    // get to them until a higher tier extends range.
    if (!grid.IsInReach(subX, subY))
        return false;

    Sample sample = CreateSample(grid, subX, subY, depth);

    if (!tray.AddSample(sample))
        return false;

    int assignedId = tray.GetSampleByIndex(tray.GetCount() - 1)->id;

    SubCell& cell = grid.GetSubCellMut(subX, subY);
    cell.sampleIds.push_back(assignedId);

    return true;
}

void SamplingEngine::SetTier(int newTier)
{
    tier = std::clamp(newTier, 0, 3);
}

int SamplingEngine::GetTier() const { return tier; }

Sample SamplingEngine::CreateSample(const ProspectingGrid& grid,
                                     int subX, int subY, DepthLayer depth) const
{
    Sample s;
    s.subCellX = subX;
    s.subCellY = subY;
    s.depthLayer = depth;
    s.trueComposition = grid.GetGroundTruth(subX, subY, depth);
    s.richness = CalculateRichnessFromQuantity(grid.GetQuantity(subX, subY, depth));
    s.state = SampleState::IN_TRAY;
    s.visual = AssignCrystalVisual(s, grid.GetParentGridX(), grid.GetParentGridY());
    return s;
}

CrystalVisual SamplingEngine::AssignCrystalVisual(const Sample& sample,
                                                    int parentGridX, int parentGridY)
{
    CrystalVisual v;

    uint32_t seed = HashVisual(sample.subCellX, sample.subCellY,
                                static_cast<int>(sample.depthLayer),
                                parentGridX, parentGridY);

    // Shape family: 70% primary (depth-based), 30% random
    seed = LCG(seed);
    float familyRoll = (seed % 1000) / 1000.0f;
    if (familyRoll < CRYSTAL_PRIMARY_FAMILY_CHANCE)
    {
        v.shapeFamily = GetPrimaryShapeFamily(sample.depthLayer);
    }
    else
    {
        seed = LCG(seed);
        v.shapeFamily = static_cast<ShapeFamily>(seed % 4);
    }

    // Template index: random 0-4
    seed = LCG(seed);
    v.templateIndex = seed % CRYSTAL_TEMPLATES_PER_FAMILY;

    // Size from richness
    v.sizeLevel = GetSizeLevel(sample.richness);

    // Glow from confidence (starts at 0 — no analysis done yet)
    v.glowLevel = GetGlowLevel(sample.GetAggregateConfidence());

    // Color from dominant element
    ResourceType dominant = GetDominantElement(sample.trueComposition);
    v.elementColor = GetElementColor(dominant);

    return v;
}

float SamplingEngine::CalculateRichnessFromQuantity(float totalQuantity)
{
    return std::clamp(totalQuantity / RICHNESS_NORMALIZATION, 0.0f, 1.0f);
}

ResourceType SamplingEngine::GetDominantElement(const std::map<ResourceType, float>& composition)
{
    ResourceType dominant = ResourceType::Fe;
    float maxAbundance = -1.0f;

    for (const auto& [type, abundance] : composition)
    {
        if (abundance > maxAbundance)
        {
            maxAbundance = abundance;
            dominant = type;
        }
    }
    return dominant;
}

uint32_t SamplingEngine::HashVisual(int subX, int subY, int depth, int px, int py)
{
    uint32_t h = 2166136261u;
    h ^= static_cast<uint32_t>(subX);   h *= 16777619u;
    h ^= static_cast<uint32_t>(subY);   h *= 16777619u;
    h ^= static_cast<uint32_t>(depth);  h *= 16777619u;
    h ^= static_cast<uint32_t>(px);     h *= 16777619u;
    h ^= static_cast<uint32_t>(py);     h *= 16777619u;
    return h;
}

uint32_t SamplingEngine::LCG(uint32_t seed)
{
    return seed * 1664525u + 1013904223u;
}
