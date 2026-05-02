#include "prospecting_types.h"

ShapeFamily GetPrimaryShapeFamily(DepthLayer layer)
{
    switch (layer)
    {
        case DepthLayer::SURFACE: return ShapeFamily::ANGULAR_CHUNKS;
        case DepthLayer::SHALLOW: return ShapeFamily::ROUNDED_NODULES;
        case DepthLayer::MID:     return ShapeFamily::LAYERED_SLABS;
        case DepthLayer::DEEP:    return ShapeFamily::CRYSTALLINE_SHARDS;
        default:                  return ShapeFamily::ANGULAR_CHUNKS;
    }
}

ConfidenceLevel GetConfidenceLevel(float confidence)
{
    if (confidence <= CONFIDENCE_THRESHOLD_LOW)      return ConfidenceLevel::VERY_LOW;
    if (confidence <= CONFIDENCE_THRESHOLD_MODERATE)  return ConfidenceLevel::LOW;
    if (confidence <= CONFIDENCE_THRESHOLD_HIGH)      return ConfidenceLevel::MODERATE;
    if (confidence <= CONFIDENCE_THRESHOLD_CERTAIN)   return ConfidenceLevel::HIGH;
    return ConfidenceLevel::CERTAIN;
}

int GetGlowLevel(float confidence)
{
    return static_cast<int>(GetConfidenceLevel(confidence));
}

int GetSizeLevel(float richness)
{
    if (richness < 0.25f) return 1;
    if (richness < 0.50f) return 2;
    if (richness < 0.75f) return 3;
    return 4;
}

Color GetElementColor(ResourceType element)
{
    switch (element)
    {
        case ResourceType::Fe:    return Color{ 181, 70, 60, 255 };
        case ResourceType::Ti:    return Color{ 160, 176, 192, 255 };
        case ResourceType::Si:    return Color{ 212, 168, 80, 255 };
        case ResourceType::Al:    return Color{ 192, 192, 200, 255 };
        case ResourceType::Ca:    return Color{ 232, 220, 192, 255 };
        case ResourceType::WATER: return Color{ 68, 136, 204, 255 };
        case ResourceType::H2:    return Color{ 136, 204, 238, 255 };
        case ResourceType::O2:    return Color{ 85, 170, 153, 255 };
        case ResourceType::C:     return Color{ 64, 64, 64, 255 };
        default:                  return Color{ 128, 128, 128, 255 };
    }
}

static const DepthLayerInfo depthLayerInfoTable[] = {
    { "Regolith",          "Fine-grained surface soil, impact-gardened",      "Fe, Si, Al, Ca" },
    { "Megaregolith",      "Coarse fragmented rock, partially sintered",      "Ti, higher metal grades" },
    { "Fractured Bedrock",  "Cracked coherent rock, hydrothermal alteration", "H2O, mineral veins" },
    { "Intact Bedrock",    "Solid unweathered rock, deep formations",         "He-3, rare minerals" },
};

const DepthLayerInfo& GetDepthLayerInfo(DepthLayer layer)
{
    int idx = static_cast<int>(layer);
    if (idx < 0 || idx > 3) idx = 0;
    return depthLayerInfoTable[idx];
}

bool IsLayerAccessible(int tier, DepthLayer layer)
{
    int layerIdx = static_cast<int>(layer);
    return layerIdx < MAX_DEPTH_PER_TIER[tier];
}

int GetGridSizeForTier(int tier)
{
    if (tier < 0) tier = 0;
    if (tier > 3) tier = 3;
    return PROSPECTING_GRID_SIZE[tier];
}

int GetTrayCapacityForTier(int tier)
{
    if (tier < 0) tier = 0;
    if (tier > 3) tier = 3;
    return TRAY_BASE_CAPACITY[tier];
}

bool Sample::IsElementRevealed(ResourceType type) const
{
    auto it = elementConfidence.find(type);
    return it != elementConfidence.end() && it->second > 0.0f;
}

float Sample::GetRevealedValue(ResourceType type) const
{
    auto confIt = elementConfidence.find(type);
    if (confIt == elementConfidence.end() || confIt->second <= 0.0f)
        return 0.0f;

    auto trueIt = trueComposition.find(type);
    if (trueIt == trueComposition.end())
        return 0.0f;

    return trueIt->second;
}

float Sample::GetAggregateConfidence() const
{
    float weightedSum = 0.0f;
    float totalWeight = 0.0f;

    for (const auto& [type, abundance] : trueComposition)
    {
        if (abundance < CELL_CONFIDENCE_MIN_ABUNDANCE)
            continue;

        float conf = 0.0f;
        auto it = elementConfidence.find(type);
        if (it != elementConfidence.end())
            conf = it->second;

        weightedSum += conf * abundance;
        totalWeight += abundance;
    }

    if (totalWeight <= 0.0f) return 0.0f;
    return weightedSum / totalWeight;
}
