#ifndef RESOURCE_TYPES_H
#define RESOURCE_TYPES_H

#include <string>
#include <map>

enum class ResourceType {
    H2,     // Hydrogen
    O2,     // Oxygen
    C,      // Carbon
    Fe,     // Iron
    Si      // Silicon
};

// Helper functions for ResourceType
namespace ResourceUtils {
    inline std::string GetResourceName(ResourceType type) {
        static const std::map<ResourceType, std::string> names = {
            {ResourceType::H2, "H₂"},
            {ResourceType::O2, "O₂"},
            {ResourceType::C, "C"},
            {ResourceType::Fe, "Fe"},
            {ResourceType::Si, "Si"}
        };
        return names.at(type);
    }

    inline Color GetResourceColor(ResourceType type) {
        static const std::map<ResourceType, Color> colors = {
            {ResourceType::H2, {150, 150, 255, 255}},  // Light Blue
            {ResourceType::O2, {255, 150, 150, 255}},  // Light Red
            {ResourceType::C, {100, 100, 100, 255}},   // Dark Gray
            {ResourceType::Fe, {139, 69, 19, 255}},    // Brown
            {ResourceType::Si, {144, 180, 148, 255}} // Greenish Gray Hex: #90B494
        };
        return colors.at(type);
    }
}

#endif // RESOURCE_TYPES_H
