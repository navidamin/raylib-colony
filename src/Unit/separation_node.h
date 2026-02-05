#ifndef SEPARATION_NODE_H
#define SEPARATION_NODE_H

#include <string>
#include <map>
#include <vector>
#include "resource_types.h"

// Types of separation processes
enum class SeparationNodeType {
    SIZE_SORT,      // Sieve-based particle size sorting
    MAGNETIC,       // Magnetic separation (Fe, Ti extraction)
    ELECTROSTATIC,  // Electrostatic separation (ilmenite, glass)
    THERMAL,        // Thermal processing (volatile release)
    CHEMICAL,       // Chemical leaching
    MRE,            // Molten Regolith Electrolysis
    DIRECT_OUTPUT   // No processing, passthrough (Tier 0)
};

struct SeparationNode {
    SeparationNodeType type = SeparationNodeType::DIRECT_OUTPUT;
    std::string name;
    float efficiency = 0.5f;       // 0-1, how well it separates
    float wear = 0.0f;            // 0-1, accumulated wear
    float energyConsumption = 0.0f; // kW required
    float temperature = 20.0f;     // Operating temperature (for thermal nodes)
    bool isActive = true;

    // Input: what this node accepts (resource type -> fraction consumed)
    std::map<ResourceType, float> inputRatios;

    // Output: what this node produces (resource type -> yield fraction)
    std::map<ResourceType, float> outputRatios;

    // Waste output
    float wasteRatio = 0.0f;       // Fraction lost to waste

    // Process input regolith and return outputs
    std::map<ResourceType, float> Process(const std::map<ResourceType, float>& input, float deltaTime) const
    {
        std::map<ResourceType, float> output;

        if (!isActive) return output;

        float effectiveEfficiency = efficiency * (1.0f - wear * 0.5f);

        for (const auto& [outType, ratio] : outputRatios)
        {
            float totalInput = 0.0f;
            for (const auto& [inType, inAmount] : input)
            {
                if (inputRatios.count(inType) > 0)
                {
                    totalInput += inAmount * inputRatios.at(inType);
                }
            }
            output[outType] = totalInput * ratio * effectiveEfficiency * deltaTime;
        }

        return output;
    }
};

// Predefined node configurations
namespace SeparationNodes {

    inline SeparationNode CreateDirectOutput()
    {
        SeparationNode node;
        node.type = SeparationNodeType::DIRECT_OUTPUT;
        node.name = "Direct Output";
        node.efficiency = 1.0f;
        node.energyConsumption = 0.0f;
        // Pass through all raw resources
        node.inputRatios = {
            {ResourceType::Fe, 1.0f}, {ResourceType::Ti, 1.0f},
            {ResourceType::Si, 1.0f}, {ResourceType::Al, 1.0f},
            {ResourceType::Ca, 1.0f}, {ResourceType::H2, 1.0f},
            {ResourceType::O2, 1.0f}, {ResourceType::C, 1.0f}
        };
        node.outputRatios = node.inputRatios;
        node.wasteRatio = 0.0f;
        return node;
    }

    inline SeparationNode CreateSizeSort()
    {
        SeparationNode node;
        node.type = SeparationNodeType::SIZE_SORT;
        node.name = "Size Sort";
        node.efficiency = 0.92f;
        node.energyConsumption = 15.0f;
        node.inputRatios = {
            {ResourceType::Fe, 1.0f}, {ResourceType::Ti, 1.0f},
            {ResourceType::Si, 1.0f}, {ResourceType::Al, 1.0f},
            {ResourceType::Ca, 1.0f}
        };
        node.outputRatios = node.inputRatios;
        node.wasteRatio = 0.10f;
        return node;
    }

    inline SeparationNode CreateMagnetic()
    {
        SeparationNode node;
        node.type = SeparationNodeType::MAGNETIC;
        node.name = "Magnetic Separator";
        node.efficiency = 0.88f;
        node.energyConsumption = 30.0f;
        node.inputRatios = {
            {ResourceType::Fe, 1.0f}, {ResourceType::Ti, 0.8f}
        };
        node.outputRatios = {
            {ResourceType::Fe, 0.90f}, {ResourceType::Ti, 0.70f}
        };
        node.wasteRatio = 0.08f;
        return node;
    }

    inline SeparationNode CreateElectrostatic()
    {
        SeparationNode node;
        node.type = SeparationNodeType::ELECTROSTATIC;
        node.name = "Electrostatic Separator";
        node.efficiency = 0.85f;
        node.energyConsumption = 40.0f;
        node.inputRatios = {
            {ResourceType::Si, 1.0f}, {ResourceType::Al, 0.9f},
            {ResourceType::Ca, 0.7f}
        };
        node.outputRatios = {
            {ResourceType::Si, 0.82f}, {ResourceType::Al, 0.75f},
            {ResourceType::Ca, 0.60f}
        };
        node.wasteRatio = 0.08f;
        return node;
    }

    inline SeparationNode CreateThermal()
    {
        SeparationNode node;
        node.type = SeparationNodeType::THERMAL;
        node.name = "Thermal Processor";
        node.efficiency = 0.82f;
        node.energyConsumption = 65.0f;
        node.temperature = 700.0f;
        node.inputRatios = {
            {ResourceType::H2, 1.0f}, {ResourceType::O2, 1.0f}
        };
        node.outputRatios = {
            {ResourceType::H2, 0.92f}, {ResourceType::O2, 0.85f},
            {ResourceType::WATER, 0.35f}
        };
        node.wasteRatio = 0.05f;
        return node;
    }

    inline SeparationNode CreateMRE()
    {
        SeparationNode node;
        node.type = SeparationNodeType::MRE;
        node.name = "Molten Regolith Electrolysis";
        node.efficiency = 0.90f;
        node.energyConsumption = 130.0f;
        node.temperature = 1600.0f;
        node.inputRatios = {
            {ResourceType::Fe, 1.0f}, {ResourceType::Ti, 1.0f},
            {ResourceType::Si, 1.0f}, {ResourceType::Al, 1.0f},
            {ResourceType::O2, 1.0f}
        };
        node.outputRatios = {
            {ResourceType::Fe, 0.95f}, {ResourceType::Ti, 0.92f},
            {ResourceType::Si, 0.88f}, {ResourceType::Al, 0.85f},
            {ResourceType::O2, 0.75f}
        };
        node.wasteRatio = 0.03f;
        return node;
    }
}

#endif // SEPARATION_NODE_H
