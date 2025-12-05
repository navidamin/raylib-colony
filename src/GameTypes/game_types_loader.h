#ifndef GAME_TYPES_LOADER_H
#define GAME_TYPES_LOADER_H

#include <string>
#include <map>
#include <vector>
#include "resource_types.h"
#include "game_enums.h"

// Forward declarations for TOML types
namespace toml {
    inline namespace v3 {
        class table;
    }
}

// Definitions for game types loaded from TOML

struct ResourceTypeDefinition {
    ResourceType type;
    std::string name;
    std::string displayName;
    std::string color;  // Hex color string
    std::string category;  // "raw", "processed", "manufactured", "meta"
    float baseValue;

    // For typed resources
    bool isTyped;
    std::vector<std::string> variants;
    std::map<ResourceType, float> baseCost;
    std::string assetPrefix;
};

struct ModuleDefinition {
    std::string name;
    std::string displayName;
    std::string description;
    std::string assetFile;  // e.g., "H2Extractor.png" (relative to unit's asset_directory)
    std::map<ResourceType, float> baseProduction;
    std::map<ResourceType, float> baseConsumption;
    float baseEfficiency;
    float upgradeCostMultiplier;
    int maxLevel;

    // Level-based enhancements
    std::map<int, std::map<std::string, float>> levelEnhancements;
};

struct UnitTypeDefinition {
    UnitType type;
    std::string name;
    std::string displayName;
    std::map<ResourceType, float> baseCost;
    float constructionTime;
    std::vector<std::string> moduleNames;
    std::string assetDirectory;   // e.g., "assets/units/Extraction/"
    std::string unitAssetFile;    // e.g., "unit.png" - shown in SECT VIEW
    std::string strategyClass;

    // Helper to get full unit asset path (for sect view)
    std::string GetUnitAssetPath() const {
        return assetDirectory + unitAssetFile;
    }

    // Helper to get full module asset path (for unit view)
    std::string GetModuleAssetPath(const std::string& moduleAssetFile) const {
        return assetDirectory + moduleAssetFile;
    }
};

class GameTypesLoader {
public:
    // Singleton access
    static GameTypesLoader& Instance();

    // Delete copy constructor and assignment operator
    GameTypesLoader(const GameTypesLoader&) = delete;
    GameTypesLoader& operator=(const GameTypesLoader&) = delete;

    // Load game types from TOML file
    bool LoadFromFile(const std::string& filepath);

    // Getters for definitions
    ResourceTypeDefinition GetResourceTypeDef(const std::string& typeName) const;
    UnitTypeDefinition GetUnitTypeDef(const std::string& typeName) const;
    ModuleDefinition GetModuleDef(const std::string& moduleName) const;

    // Check if definitions exist
    bool HasResourceType(const std::string& typeName) const;
    bool HasUnitType(const std::string& typeName) const;
    bool HasModule(const std::string& moduleName) const;

    // Get all definitions (for iteration)
    const std::map<std::string, ResourceTypeDefinition>& GetAllResourceTypes() const;
    const std::map<std::string, UnitTypeDefinition>& GetAllUnitTypes() const;
    const std::map<std::string, ModuleDefinition>& GetAllModules() const;

    // Check if loader has been initialized
    bool IsLoaded() const { return isLoaded; }

private:
    GameTypesLoader();
    ~GameTypesLoader();

    // Loading functions
    void LoadResourceTypes(const toml::table& data);
    void LoadUnitTypes(const toml::table& data);
    void LoadModules(const toml::table& data);

    // Helper functions
    ResourceType StringToResourceType(const std::string& str) const;
    UnitType StringToUnitType(const std::string& str) const;

    // Storage for definitions
    std::map<std::string, ResourceTypeDefinition> resourceTypeDefinitions;
    std::map<std::string, UnitTypeDefinition> unitTypeDefinitions;
    std::map<std::string, ModuleDefinition> moduleDefinitions;

    bool isLoaded;
};

#endif // GAME_TYPES_LOADER_H
