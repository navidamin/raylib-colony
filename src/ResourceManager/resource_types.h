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
    Ti,
    Al,
    Ca,
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

// Single source of truth for all per-resource metadata
struct ResourceDescriptor
{
    ResourceType type;
    ResourceCategory category;
    const char* name;
    Color color;
    std::vector<std::string> subtypes;  // Empty for SINGULAR
};

inline const std::vector<ResourceDescriptor>& GetResourceDescriptors()
{
    static const std::vector<ResourceDescriptor> descriptors = {
        // Tier 1 - Raw singular
        {ResourceType::ENERGY,   ResourceCategory::SINGULAR, "ENERGY",   {128, 128, 128, 255}, {}},
        {ResourceType::H2,       ResourceCategory::SINGULAR, "H2",       {150, 150, 255, 255}, {}},
        {ResourceType::O2,       ResourceCategory::SINGULAR, "O2",       {255, 150, 150, 255}, {}},
        {ResourceType::C,        ResourceCategory::SINGULAR, "C",        {100, 100, 100, 255}, {}},
        {ResourceType::Fe,       ResourceCategory::SINGULAR, "Fe",       {139, 69, 19, 255},   {}},
        {ResourceType::Si,       ResourceCategory::SINGULAR, "Si",       {144, 180, 148, 255}, {}},
        {ResourceType::Ti,       ResourceCategory::SINGULAR, "Ti",       {180, 160, 200, 255}, {}},
        {ResourceType::Al,       ResourceCategory::SINGULAR, "Al",       {200, 200, 220, 255}, {}},
        {ResourceType::Ca,       ResourceCategory::SINGULAR, "Ca",       {220, 210, 190, 255}, {}},
        // Tier 2 - Processed singular
        {ResourceType::WATER,    ResourceCategory::SINGULAR, "WATER",    {128, 128, 128, 255}, {}},
        {ResourceType::FOOD,     ResourceCategory::SINGULAR, "FOOD",     {128, 128, 128, 255}, {}},
        {ResourceType::BIOFUEL,  ResourceCategory::SINGULAR, "BIOFUEL",  {128, 128, 128, 255}, {}},
        // Tier 3 - Abstract singular
        {ResourceType::SCIENCE,  ResourceCategory::SINGULAR, "SCIENCE",  {128, 128, 128, 255}, {}},
        {ResourceType::MANPOWER, ResourceCategory::SINGULAR, "MANPOWER", {128, 128, 128, 255}, {}},
        // Tier 2/3 - Manufactured typed
        {ResourceType::MACHINERY,              ResourceCategory::TYPED, "MACHINERY",              {180, 180, 180, 255}, {"HeavyDrill", "Conveyor", "Assembler"}},
        {ResourceType::ELECTRONICS,            ResourceCategory::TYPED, "ELECTRONICS",            {0, 200, 200, 255},   {"Sensor", "Controller", "Computer"}},
        {ResourceType::ALLOYS,                 ResourceCategory::TYPED, "ALLOYS",                 {200, 150, 50, 255},  {"Steel", "Bronze", "Aluminum", "Titanium"}},
        {ResourceType::CONSTRUCTION_MATERIALS, ResourceCategory::TYPED, "CONSTRUCTION_MATERIALS", {150, 100, 70, 255},  {"Beam", "Panel", "Pipe", "Cable"}},
    };
    return descriptors;
}

inline const ResourceDescriptor& GetResourceDescriptor(ResourceType type)
{
    const auto& descriptors = GetResourceDescriptors();
    for (const auto& desc : descriptors)
    {
        if (desc.type == type) return desc;
    }
    // Fallback - should never happen with valid enum values
    static const ResourceDescriptor unknown = {
        ResourceType::ENERGY, ResourceCategory::SINGULAR, "UNKNOWN", {128, 128, 128, 255}, {}
    };
    return unknown;
}

// Convert ResourceType to string
inline const char* ResourceTypeToString(ResourceType type)
{
    return GetResourceDescriptor(type).name;
}

// Get resource category
inline ResourceCategory GetResourceCategory(ResourceType type)
{
    return GetResourceDescriptor(type).category;
}

// Stream operator overload
inline std::ostream& operator<<(std::ostream& os, const ResourceType& type)
{
    os << ResourceTypeToString(type);
    return os;
}

// Helper functions for ResourceType
namespace ResourceUtils {
    inline std::string GetResourceName(ResourceType type)
    {
        return GetResourceDescriptor(type).name;
    }

    inline Color GetResourceColor(ResourceType type)
    {
        return GetResourceDescriptor(type).color;
    }

    inline std::vector<std::string> GetSubtypes(ResourceType type)
    {
        return GetResourceDescriptor(type).subtypes;
    }

    // Check if subtype is valid for a resource type
    inline bool IsValidSubtype(ResourceType type, const std::string& subtype)
    {
        const auto& subtypes = GetSubtypes(type);
        for (const auto& s : subtypes)
        {
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
