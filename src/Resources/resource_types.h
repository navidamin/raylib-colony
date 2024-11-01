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
    MANPOWER
};

// Helper functions for ResourceType
class ResourceUtils {
public:
    static std::string GetResourceName(ResourceType type) {
        switch (type) {
            case ResourceType::ENERGY:
                return "Energy";
            case ResourceType::H2:
                return "Hydrogen";
            case ResourceType::O2:
                return "Oxygen";
            case ResourceType::C:
                return "Carbon";
            case ResourceType::Fe:
                return "Iron";
            case ResourceType::Si:
                return "Silicon";
            case ResourceType::WATER:
                return "Water";
            case ResourceType::FOOD:
                return "Food";
            case ResourceType::SCIENCE:
                return "Science";
            case ResourceType::MANPOWER:
                return "Manpower";
            default:
                throw std::runtime_error("Unknown resource type");
        }
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
        default:                     return "UNKNOWN";
    }
}

// Then, create the stream operator overload
inline std::ostream& operator<<(std::ostream& os, const ResourceType& type) {
    os << ResourceTypeToString(type);
    return os;
}



#endif // RESOURCE_TYPES_H
