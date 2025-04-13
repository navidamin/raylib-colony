#ifndef RESOURCE_TYPES_H
#define RESOURCE_TYPES_H

#include <string>
#include <map>

enum class ResourceType {
    ENERGY,
    H2,
    O2,
    C,
    Fe,
    Si,
    WATER,
    FOOD,
    SCIENCE,
    MANPOWER,
    BIOFUEL
};

// First, create a function to convert ResourceType to string
inline const char* ResourceTypeToString(ResourceType type) {
    switch (type) {
        case ResourceType::ENERGY:   return "ENERGY";
        case ResourceType::SCIENCE:  return "SCIENCE";
        case ResourceType::MANPOWER: return "MANPOWER";
        case ResourceType::H2:       return "H2";
        case ResourceType::O2:       return "O2";
        case ResourceType::C:        return "C";
        case ResourceType::Fe:       return "Fe";
        case ResourceType::Si:       return "Si";
        case ResourceType::WATER:    return "WATER";
        case ResourceType::FOOD:     return "FOOD";
        case ResourceType::BIOFUEL:  return "BIOFUEL";

        default:                     return "UNKNOWN";

    }
}

// Then, create the stream operator overload
inline std::ostream& operator<<(std::ostream& os, const ResourceType& type) {
    os << ResourceTypeToString(type);
    return os;
}




// Helper functions for ResourceType
namespace ResourceUtils {
    inline std::string GetResourceName(ResourceType type) {
        static const std::map<ResourceType, std::string> names = {
            {ResourceType::ENERGY, "ENERGY"},
            {ResourceType::SCIENCE, "SCIENCE"},
            {ResourceType::MANPOWER, "MANPOWER"},
            {ResourceType::WATER, "WATER"},
            {ResourceType::FOOD, "FOOD"},

            {ResourceType::H2, "H2"},
            {ResourceType::O2, "O2"},
            {ResourceType::C, "C"},
            {ResourceType::Fe, "Fe"},
            {ResourceType::Si, "Si"},
            {ResourceType::BIOFUEL, "BIOFUEL"}
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

struct ResourceRate {
    ResourceType type;
    float baseRate;
    float currentRate;
    float efficiencyMultiplier;
};


#endif // RESOURCE_TYPES_H
