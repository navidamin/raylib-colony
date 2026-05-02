#pragma once

#include "prospecting_types.h"
#include "prospecting_constants.h"
#include <vector>
#include <string>

struct LabPreset
{
    std::string name;
    SeparationMethod separation;
    std::vector<AnalysisTool> tools;
    int requiredTier;
};

class LabEngine
{
public:
    LabEngine(int tier = 0);

    bool CanApplyTool(const Sample& sample, AnalysisTool tool) const;
    static float GetToolCost(AnalysisTool tool);
    bool ApplyTool(Sample& sample, AnalysisTool tool, float gameTime,
                   ResourceType fireAssayTarget = ResourceType::Fe);

    bool CanApplySeparation(const Sample& sample, SeparationMethod method) const;
    static float GetSeparationCost(SeparationMethod method);
    bool ApplySeparation(Sample& sample, SeparationMethod method, float gameTime);

    bool CanApplyPreset(const Sample& sample, int presetIndex) const;
    bool ApplyPreset(Sample& sample, int presetIndex, float gameTime);

    static const std::vector<LabPreset>& GetPresets();

    void SetTier(int tier);
    int GetTier() const;

    static bool IsHeavyElement(ResourceType type);
    static bool IsLightElement(ResourceType type);
    static bool IsFerromagnetic(ResourceType type);

private:
    int tier;

    bool IsToolAvailable(AnalysisTool tool) const;
    bool IsSeparationAvailable(SeparationMethod method) const;

    std::map<ResourceType, float> GenerateToolConfidence(
        const Sample& sample, AnalysisTool tool,
        ResourceType fireAssayTarget) const;

    std::map<ResourceType, float> GenerateSeparationConfidence(
        const Sample& sample, SeparationMethod method) const;

    static void AccumulateConfidence(Sample& sample,
                                      const std::map<ResourceType, float>& gains);

    static void UpdateVisualGlow(Sample& sample);

    float InterpolateConfidence(float minConf, float maxConf,
                                 float richness) const;
};
