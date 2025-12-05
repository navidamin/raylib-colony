#ifndef RESOURCE_TYPES_H
#define RESOURCE_TYPES_H

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include "raylib.h"

// Resource categories: singular (countable) vs typed (has subtypes)
enum class ResourceCategory {
    SINGULAR,   // Simple resources: H2, O2, ENERGY, etc.
    TYPED       // Complex resources with subtypes: MACHINERY, ELECTRONICS, etc.
};

enum class ResourceType {
    // Singular resources (Tier 1 - Raw)
    ENERGY,
    H2,
    O2,
    C,
    Fe,
    Si,
    // Singular resources (Tier 2 - Processed)
    WATER,
    FOOD,
    BIOFUEL,
    // Singular resources (Tier 3 - Abstract)
    SCIENCE,
    MANPOWER,
    // Typed resources (Tier 2/3 - Manufactured)
    MACHINERY,
    ELECTRONICS,
    ALLOYS,
    CONSTRUCTION_MATERIALS
};

// Subtypes for typed resources
enum class MachinerySubtype {
    HEAVY_DRILL,
    CONVEYOR,
    ASSEMBLER
};

enum class ElectronicsSubtype {
    SENSOR,
    CONTROLLER,
    COMPUTER
};

enum class AlloysSubtype {
    STEEL,
    BRONZE,
    ALUMINUM,
    TITANIUM
};

enum class ConstructionSubtype {
    BEAM,
    PANEL,
    PIPE,
    CABLE
};

// Typed resource instance (no quality system)
struct TypedResource {
    ResourceType baseType;      // MACHINERY, ELECTRONICS, ALLOYS, or CONSTRUCTION_MATERIALS
    std::string subType;        // String name of subtype (e.g., "HeavyDrill", "Steel")
    float efficiency = 1.0f;    // Production efficiency modifier (1.0 = 100%)

    // Constructor
    TypedResource(ResourceType base, const std::string& sub, float eff = 1.0f)
        : baseType(base), subType(sub), efficiency(eff) {}

    // Default constructor
    TypedResource() : baseType(ResourceType::MACHINERY), subType(""), efficiency(1.0f) {}
};

// Convert ResourceType to string
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
        case ResourceType::MACHINERY: return "MACHINERY";
        case ResourceType::ELECTRONICS: return "ELECTRONICS";
        case ResourceType::ALLOYS:   return "ALLOYS";
        case ResourceType::CONSTRUCTION_MATERIALS: return "CONSTRUCTION_MATERIALS";
        default:                     return "UNKNOWN";
    }
}

// Get resource category
inline ResourceCategory GetResourceCategory(ResourceType type) {
    switch (type) {
        case ResourceType::MACHINERY:
        case ResourceType::ELECTRONICS:
        case ResourceType::ALLOYS:
        case ResourceType::CONSTRUCTION_MATERIALS:
            return ResourceCategory::TYPED;
        default:
            return ResourceCategory::SINGULAR;
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
            {ResourceType::BIOFUEL, "BIOFUEL"},
            {ResourceType::MACHINERY, "MACHINERY"},
            {ResourceType::ELECTRONICS, "ELECTRONICS"},
            {ResourceType::ALLOYS, "ALLOYS"},
            {ResourceType::CONSTRUCTION_MATERIALS, "CONSTRUCTION_MATERIALS"}
        };
        return names.at(type);
    }

    inline Color GetResourceColor(ResourceType type) {
        static const std::map<ResourceType, Color> colors = {
            {ResourceType::H2, {150, 150, 255, 255}},       // Light Blue
            {ResourceType::O2, {255, 150, 150, 255}},       // Light Red
            {ResourceType::C, {100, 100, 100, 255}},        // Dark Gray
            {ResourceType::Fe, {139, 69, 19, 255}},         // Brown
            {ResourceType::Si, {144, 180, 148, 255}},       // Greenish Gray
            {ResourceType::MACHINERY, {180, 180, 180, 255}},     // Silver
            {ResourceType::ELECTRONICS, {0, 200, 200, 255}},     // Cyan
            {ResourceType::ALLOYS, {200, 150, 50, 255}},         // Gold
            {ResourceType::CONSTRUCTION_MATERIALS, {150, 100, 70, 255}} // Tan
        };
        auto it = colors.find(type);
        if (it != colors.end()) {
            return it->second;
        }
        return {128, 128, 128, 255}; // Default gray
    }

    // Get valid subtypes for a typed resource category
    inline std::vector<std::string> GetSubtypes(ResourceType type) {
        switch (type) {
            case ResourceType::MACHINERY:
                return {"HeavyDrill", "Conveyor", "Assembler"};
            case ResourceType::ELECTRONICS:
                return {"Sensor", "Controller", "Computer"};
            case ResourceType::ALLOYS:
                return {"Steel", "Bronze", "Aluminum", "Titanium"};
            case ResourceType::CONSTRUCTION_MATERIALS:
                return {"Beam", "Panel", "Pipe", "Cable"};
            default:
                return {};
        }
    }

    // Check if subtype is valid for a resource type
    inline bool IsValidSubtype(ResourceType type, const std::string& subtype) {
        auto subtypes = GetSubtypes(type);
        for (const auto& s : subtypes) {
            if (s == subtype) return true;
        }
        return false;
    }
}

struct ResourceRate {
    ResourceType type;
    float baseRate;
    float currentRate;
    float efficiencyMultiplier;
};


#endif // RESOURCE_TYPES_H
