#include "lab_engine.h"
#include <algorithm>

LabEngine::LabEngine(int tier)
    : tier(std::clamp(tier, 0, 3))
{
}

bool LabEngine::IsToolAvailable(AnalysisTool tool) const
{
    switch (tool)
    {
        case AnalysisTool::VISUAL_INSPECTION:      return tier >= 0;
        case AnalysisTool::XRF:                    return tier >= 1;
        case AnalysisTool::OPTICAL_MICROSCOPY:     return tier >= 1;
        case AnalysisTool::MAGNETIC_SUSCEPTIBILITY: return tier >= 1;
        case AnalysisTool::LIBS_PULSE:             return tier >= 2;
        case AnalysisTool::FIRE_ASSAY:             return tier >= 3;
        default:                                    return false;
    }
}

bool LabEngine::IsSeparationAvailable(SeparationMethod method) const
{
    switch (method)
    {
        case SeparationMethod::NONE:                return true;
        case SeparationMethod::MAGNETIC:            return tier >= 1;
        case SeparationMethod::HEAVY_MINERAL:       return tier >= 2;
        case SeparationMethod::VOLATILE_EXTRACTION: return tier >= 2;
        default:                                     return false;
    }
}

bool LabEngine::CanApplyTool(const Sample& sample, AnalysisTool tool) const
{
    if (sample.state == SampleState::COMPLETED)
        return false;
    return IsToolAvailable(tool);
}

bool LabEngine::ApplyTool(Sample& sample, AnalysisTool tool, float gameTime,
                            ResourceType fireAssayTarget)
{
    if (!CanApplyTool(sample, tool))
        return false;

    auto gains = GenerateToolConfidence(sample, tool, fireAssayTarget);
    AccumulateConfidence(sample, gains);

    ProcessingStep step;
    step.tool = tool;
    step.timestamp = gameTime;
    step.confidenceAdded = gains;
    sample.analysisHistory.push_back(step);

    if (tool == AnalysisTool::FIRE_ASSAY)
        sample.state = SampleState::COMPLETED;

    UpdateVisualGlow(sample);
    return true;
}

bool LabEngine::CanApplySeparation(const Sample& sample, SeparationMethod method) const
{
    if (sample.state == SampleState::COMPLETED)
        return false;
    if (sample.separationApplied != SeparationMethod::NONE)
        return false;
    if (method == SeparationMethod::NONE)
        return false;
    return IsSeparationAvailable(method);
}

bool LabEngine::ApplySeparation(Sample& sample, SeparationMethod method,
                                  float gameTime)
{
    if (!CanApplySeparation(sample, method))
        return false;

    auto gains = GenerateSeparationConfidence(sample, method);
    AccumulateConfidence(sample, gains);
    sample.separationApplied = method;

    UpdateVisualGlow(sample);
    return true;
}

bool LabEngine::CanApplyPreset(const Sample& sample, int presetIndex) const
{
    const auto& presets = GetPresets();
    if (presetIndex < 0 || presetIndex >= static_cast<int>(presets.size()))
        return false;

    const auto& preset = presets[presetIndex];
    if (tier < preset.requiredTier)
        return false;
    if (sample.state == SampleState::COMPLETED)
        return false;

    return true;
}

bool LabEngine::ApplyPreset(Sample& sample, int presetIndex, float gameTime)
{
    if (!CanApplyPreset(sample, presetIndex))
        return false;

    const auto& preset = GetPresets()[presetIndex];

    if (preset.separation != SeparationMethod::NONE)
        ApplySeparation(sample, preset.separation, gameTime);

    for (const auto& tool : preset.tools)
    {
        if (CanApplyTool(sample, tool))
            ApplyTool(sample, tool, gameTime);
    }

    return true;
}

const std::vector<LabPreset>& LabEngine::GetPresets()
{
    static const std::vector<LabPreset> presets = {
        { "Quick Survey",
          SeparationMethod::NONE,
          { AnalysisTool::VISUAL_INSPECTION },
          0 },
        { "Structural",
          SeparationMethod::MAGNETIC,
          { AnalysisTool::XRF },
          1 },
        { "Life Support",
          SeparationMethod::VOLATILE_EXTRACTION,
          { AnalysisTool::LIBS_PULSE },
          2 },
        { "Strategic",
          SeparationMethod::HEAVY_MINERAL,
          { AnalysisTool::FIRE_ASSAY },
          3 },
    };
    return presets;
}

void LabEngine::SetTier(int newTier)
{
    tier = std::clamp(newTier, 0, 3);
}

int LabEngine::GetTier() const { return tier; }

bool LabEngine::IsHeavyElement(ResourceType type)
{
    return type == ResourceType::Fe || type == ResourceType::Si ||
           type == ResourceType::Ti || type == ResourceType::Al ||
           type == ResourceType::Ca;
}

bool LabEngine::IsLightElement(ResourceType type)
{
    return type == ResourceType::H2 || type == ResourceType::O2 ||
           type == ResourceType::C  || type == ResourceType::WATER;
}

bool LabEngine::IsFerromagnetic(ResourceType type)
{
    return type == ResourceType::Fe || type == ResourceType::Ti;
}

float LabEngine::InterpolateConfidence(float minConf, float maxConf,
                                        float richness) const
{
    return minConf + (maxConf - minConf) * richness;
}

std::map<ResourceType, float> LabEngine::GenerateToolConfidence(
    const Sample& sample, AnalysisTool tool, ResourceType fireAssayTarget) const
{
    std::map<ResourceType, float> gains;
    float richness = sample.richness;

    switch (tool)
    {
        case AnalysisTool::VISUAL_INSPECTION:
        case AnalysisTool::OPTICAL_MICROSCOPY:
        {
            float conf = InterpolateConfidence(
                CONFIDENCE_VISUAL_MIN, CONFIDENCE_VISUAL_MAX, richness);
            for (const auto& [type, abundance] : sample.trueComposition)
            {
                if (abundance > 0.01f)
                    gains[type] = conf;
            }
            break;
        }
        case AnalysisTool::XRF:
        {
            float conf = InterpolateConfidence(
                CONFIDENCE_XRF_MIN, CONFIDENCE_XRF_MAX, richness);
            for (const auto& [type, abundance] : sample.trueComposition)
            {
                if (abundance > 0.01f && IsHeavyElement(type))
                    gains[type] = conf;
            }
            break;
        }
        case AnalysisTool::LIBS_PULSE:
        {
            float conf = InterpolateConfidence(
                CONFIDENCE_LIBS_MIN, CONFIDENCE_LIBS_MAX, richness);
            for (const auto& [type, abundance] : sample.trueComposition)
            {
                if (abundance > 0.01f)
                    gains[type] = conf;
            }
            break;
        }
        case AnalysisTool::FIRE_ASSAY:
        {
            gains[fireAssayTarget] = CONFIDENCE_FIRE_ASSAY;
            break;
        }
        case AnalysisTool::MAGNETIC_SUSCEPTIBILITY:
        {
            float conf = InterpolateConfidence(0.10f, 0.20f, richness);
            for (const auto& [type, abundance] : sample.trueComposition)
            {
                if (abundance > 0.01f && IsFerromagnetic(type))
                    gains[type] = conf;
            }
            break;
        }
    }
    return gains;
}

std::map<ResourceType, float> LabEngine::GenerateSeparationConfidence(
    const Sample& sample, SeparationMethod method) const
{
    std::map<ResourceType, float> gains;

    switch (method)
    {
        case SeparationMethod::MAGNETIC:
            for (const auto& [type, abundance] : sample.trueComposition)
            {
                if (abundance > 0.01f && IsFerromagnetic(type))
                    gains[type] = 0.15f;
            }
            break;
        case SeparationMethod::HEAVY_MINERAL:
            for (const auto& [type, abundance] : sample.trueComposition)
            {
                if (abundance > 0.01f)
                    gains[type] = 0.08f;
            }
            break;
        case SeparationMethod::VOLATILE_EXTRACTION:
            for (const auto& [type, abundance] : sample.trueComposition)
            {
                if (abundance > 0.01f && IsLightElement(type))
                    gains[type] = 0.15f;
            }
            break;
        default:
            break;
    }
    return gains;
}

void LabEngine::AccumulateConfidence(Sample& sample,
                                      const std::map<ResourceType, float>& gains)
{
    for (const auto& [type, gain] : gains)
    {
        float existing = 0.0f;
        auto it = sample.elementConfidence.find(type);
        if (it != sample.elementConfidence.end())
            existing = it->second;

        // Probabilistic: conf = 1 - (1 - existing)(1 - gain)
        float combined = 1.0f - (1.0f - existing) * (1.0f - gain);
        sample.elementConfidence[type] = std::min(combined, 1.0f);
    }
}

void LabEngine::UpdateVisualGlow(Sample& sample)
{
    sample.visual.glowLevel = GetGlowLevel(sample.GetAggregateConfidence());
}
