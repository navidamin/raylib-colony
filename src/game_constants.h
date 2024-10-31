#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

#include <string>
#include <map>
#include <iostream>


const float SECT_CORE_RADIUS = 50.0f;
const int PLANET_SIZE = 20;  // 20x20 grid of possible sect locations
const float PLANET_WIDTH = PLANET_SIZE * SECT_CORE_RADIUS * 2.0f;  // Total width of planet
const float PLANET_HEIGHT = PLANET_SIZE * SECT_CORE_RADIUS * 2.0f; // Total height of planet

const float TICK_DURATION = 1.0f;
// TICKS_PER_DAY
static const int TICKS_PER_DAY = 10;  // 60 seconds = 1 day

const float INITIAL_UNIT_ENERGY = 1000.0f;
const float INITIAL_UNIT_FOOD = 1000.0f;
const float INITIAL_UNIT_WATER= 1000.0f;
const float INITIAL_UNIT_MANPOWER = 100.0f;
const float INITIAL_UNIT_SCIENCE = 20.0f;

enum class ResourceType {
    ENERGY,
    SCIENCE,
    MANPOWER,
    H2,
    O2,
    C,
    Fe,
    Si,
    WATER,
    FOOD
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

struct ResourceRate {
    ResourceType type;
    float baseRate;
    float currentRate;
    float efficiencyMultiplier;
};

#endif // GAME_CONSTANTS_H
