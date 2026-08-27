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

    // Priced per metre of column, never per hole and never discounted by
    // anything -- the whole way depth stays meaningful without a gate.
    return DrillEnergyToDepth(layerIdx);
}

bool SamplingEngine::CollectSample(ProspectingGrid& grid, SampleTray& tray,
                                    int subX, int subY, DepthLayer depth)
{
    if (!CanDrill(depth))
        return false;

    int size = grid.GetGridSize();
    if (subX < 0 || subX >= size || subY < 0 || subY >= size)
        return false;

    if (!grid.IsInReach(subX, subY))
        return false;

    // A VERTICAL HOLE, not a teleported point sample. The auger cores
    // everything between the surface and its target -- drilling to MID
    // recovers (and therefore knows) SURFACE and SHALLOW on the way down.
    // Vertical drilling is the degenerate case of the designed line hole
    // (dip 90), which is what makes this an extension rather than a rewrite.
    int targetDepth = static_cast<int>(depth);
    for (int d = 0; d <= targetDepth; d++)
    {
        grid.RecordCore(subX, subY, static_cast<DepthLayer>(d));
    }

    // The specimen is best-effort. Knowledge lives on the grid; the tray is a
    // shelf of physical rocks, and a full shelf must never un-know ground.
    // One specimen per hole -- the deepest interval, the interesting one.
    AddSpecimen(grid, tray, subX, subY, depth);

    return true;
}

// The tray half of a hole, on its own so LINE holes can use it too: create
// the physical specimen and remember which cell it came from. Never cores --
// knowledge is the caller's job -- and never blocks on a full shelf.
bool SamplingEngine::AddSpecimen(ProspectingGrid& grid, SampleTray& tray,
                                  int subX, int subY, DepthLayer depth)
{
    if (tray.IsFull())
        return false;

    Sample sample = CreateSample(grid, subX, subY, depth);
    if (!tray.AddSample(sample))
        return false;

    int assignedId = tray.GetSampleByIndex(tray.GetCount() - 1)->id;
    grid.GetSubCellMut(subX, subY).sampleIds.push_back(assignedId);
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

    // A recovered core is rock you are holding. Its composition is KNOWN --
    // there is nothing further for the player to decide about reading it, and
    // analytical precision is a percent or two while the uncertainty BETWEEN
    // holes is total. The lab stage used to gate this; it modelled the small
    // uncertainty and made the drill look like it might not tell you what you
    // had just pulled out. See docs/design/prospecting/block-model-design.md.
    for (const auto& [type, abundance] : s.trueComposition)
    {
        s.elementConfidence[type] = 1.0f;
    }

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
