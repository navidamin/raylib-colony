#include "game_types_loader.h"
#include <toml++/toml.h>
#include <iostream>
#include <fstream>

GameTypesLoader::GameTypesLoader() : isLoaded(false) {
}

GameTypesLoader::~GameTypesLoader() {
}

GameTypesLoader& GameTypesLoader::Instance() {
    static GameTypesLoader instance;
    return instance;
}

bool GameTypesLoader::LoadFromFile(const std::string& filepath) {
    try {
        std::cout << "Loading game types from: " << filepath << std::endl;

        // Parse TOML file
        toml::table config = toml::parse_file(filepath);

        // Load each section
        if (config.contains("resource_types")) {
            const toml::table* resourceTypesTable = config["resource_types"].as_table();
            if (resourceTypesTable) {
                LoadResourceTypes(*resourceTypesTable);
            }
        }

        if (config.contains("unit_types")) {
            const toml::table* unitTypesTable = config["unit_types"].as_table();
            if (unitTypesTable) {
                LoadUnitTypes(*unitTypesTable);
            }
        }

        if (config.contains("modules")) {
            const toml::table* modulesTable = config["modules"].as_table();
            if (modulesTable) {
                LoadModules(*modulesTable);
            }
        }

        isLoaded = true;
        std::cout << "Successfully loaded game types:" << std::endl;
        std::cout << "  - Resources: " << resourceTypeDefinitions.size() << std::endl;
        std::cout << "  - Units: " << unitTypeDefinitions.size() << std::endl;
        std::cout << "  - Modules: " << moduleDefinitions.size() << std::endl;

        return true;
    }
    catch (const toml::parse_error& err) {
        std::cerr << "TOML Parse Error: " << err << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading game types: " << e.what() << std::endl;
        return false;
    }
}

void GameTypesLoader::LoadResourceTypes(const toml::table& data) {
    // Load singular resources
    if (data.contains("singular")) {
        const toml::table* singular = data["singular"].as_table();
        if (singular) {
            for (const auto& [key, value] : *singular) {
                std::string resourceName = std::string(key.str());
                const toml::table* resourceData = value.as_table();

                if (resourceData) {
                    ResourceTypeDefinition def;
                    def.name = resourceName;
                    def.type = StringToResourceType(resourceName);
                    def.displayName = resourceData->at_path("display_name").value_or(resourceName);
                    def.color = resourceData->at_path("color").value_or("#FFFFFF");
                    def.category = resourceData->at_path("category").value_or("unknown");
                    def.baseValue = resourceData->at_path("base_value").value_or(1.0);
                    def.isTyped = false;

                    resourceTypeDefinitions[resourceName] = def;
                }
            }
        }
    }

    // Load typed resources
    if (data.contains("typed")) {
        const toml::table* typed = data["typed"].as_table();
        if (typed) {
            for (const auto& [key, value] : *typed) {
                std::string resourceName = std::string(key.str());
                const toml::table* resourceData = value.as_table();

                if (resourceData) {
                    ResourceTypeDefinition def;
                    def.name = resourceName;
                    def.type = StringToResourceType(resourceName);
                    def.displayName = resourceData->at_path("display_name").value_or(resourceName);
                    def.isTyped = true;
                    def.assetPrefix = resourceData->at_path("asset_prefix").value_or("");

                    // Load variants
                    if (resourceData->contains("variants")) {
                        const toml::array* variants = resourceData->at("variants").as_array();
                        if (variants) {
                            for (const auto& variant : *variants) {
                                def.variants.push_back(variant.value_or(""));
                            }
                        }
                    }

                    resourceTypeDefinitions[resourceName] = def;
                }
            }
        }
    }
}

void GameTypesLoader::LoadUnitTypes(const toml::table& data) {
    for (const auto& [key, value] : data) {
        std::string unitName = std::string(key.str());
        const toml::table* unitData = value.as_table();

        if (unitData) {
            UnitTypeDefinition def;
            def.name = unitName;
            def.type = StringToUnitType(unitName);
            def.displayName = unitData->at_path("display_name").value_or(unitName);
            def.constructionTime = unitData->at_path("construction_time").value_or(60.0);
            def.assetDirectory = unitData->at_path("asset_directory").value_or("");
            def.unitAssetFile = unitData->at_path("unit_asset_file").value_or("unit.png");
            def.strategyClass = unitData->at_path("strategy_class").value_or("");

            // Load base cost
            if (unitData->contains("base_cost")) {
                const toml::table* costs = unitData->at("base_cost").as_table();
                if (costs) {
                    for (const auto& [resource, amount] : *costs) {
                        std::string resourceName = std::string(resource.str());
                        ResourceType resType = StringToResourceType(resourceName);
                        def.baseCost[resType] = amount.value_or(0.0);
                    }
                }
            }

            // Load module names
            if (unitData->contains("modules")) {
                const toml::array* modules = unitData->at("modules").as_array();
                if (modules) {
                    for (const auto& mod : *modules) {
                        def.moduleNames.push_back(mod.value_or(""));
                    }
                }
            }

            unitTypeDefinitions[unitName] = def;
        }
    }
}

void GameTypesLoader::LoadModules(const toml::table& data) {
    for (const auto& [key, value] : data) {
        std::string moduleName = std::string(key.str());
        const toml::table* moduleData = value.as_table();

        if (moduleData) {
            ModuleDefinition def;
            def.name = moduleName;
            def.displayName = moduleData->at_path("display_name").value_or(moduleName);
            def.description = moduleData->at_path("description").value_or("");
            def.assetFile = moduleData->at_path("asset_file").value_or("");
            def.baseEfficiency = moduleData->at_path("base_efficiency").value_or(1.0);
            def.upgradeCostMultiplier = moduleData->at_path("upgrade_cost_multiplier").value_or(1.5);
            def.maxLevel = static_cast<int>(moduleData->at_path("max_level").value_or(10));

            // Load production rates
            if (moduleData->contains("production")) {
                const toml::table* production = moduleData->at("production").as_table();
                if (production) {
                    for (const auto& [resource, rate] : *production) {
                        std::string resourceName = std::string(resource.str());
                        ResourceType resType = StringToResourceType(resourceName);
                        def.baseProduction[resType] = rate.value_or(0.0);
                    }
                }
            }

            // Load consumption rates
            if (moduleData->contains("consumption")) {
                const toml::table* consumption = moduleData->at("consumption").as_table();
                if (consumption) {
                    for (const auto& [resource, rate] : *consumption) {
                        std::string resourceName = std::string(resource.str());
                        ResourceType resType = StringToResourceType(resourceName);
                        def.baseConsumption[resType] = rate.value_or(0.0);
                    }
                }
            }

            moduleDefinitions[moduleName] = def;
        }
    }
}

ResourceType GameTypesLoader::StringToResourceType(const std::string& str) const {
    // Map string names to ResourceType enum values
    static const std::map<std::string, ResourceType> mapping = {
        {"ENERGY", ResourceType::ENERGY},
        {"H2", ResourceType::H2},
        {"O2", ResourceType::O2},
        {"C", ResourceType::C},
        {"Fe", ResourceType::Fe},
        {"Si", ResourceType::Si},
        {"WATER", ResourceType::WATER},
        {"FOOD", ResourceType::FOOD},
        {"SCIENCE", ResourceType::SCIENCE},
        {"MANPOWER", ResourceType::MANPOWER},
        {"BIOFUEL", ResourceType::BIOFUEL}
    };

    auto it = mapping.find(str);
    if (it != mapping.end()) {
        return it->second;
    }

    std::cerr << "Warning: Unknown resource type '" << str << "', defaulting to ENERGY" << std::endl;
    return ResourceType::ENERGY;
}

UnitType GameTypesLoader::StringToUnitType(const std::string& str) const {
    // Map string names to UnitType enum values
    static const std::map<std::string, UnitType> mapping = {
        {"Extraction", UnitType::Extraction},
        {"Farming", UnitType::Farming},
        {"Energy", UnitType::Energy},
        {"Construction", UnitType::Construction},
        {"Transport", UnitType::Transport},
        {"Manufacture", UnitType::Manufacture},
        {"Research", UnitType::Research},
        {"Commerce", UnitType::Commerce}
    };

    auto it = mapping.find(str);
    if (it != mapping.end()) {
        return it->second;
    }

    std::cerr << "Warning: Unknown unit type '" << str << "', defaulting to Extraction" << std::endl;
    return UnitType::Extraction;
}

ResourceTypeDefinition GameTypesLoader::GetResourceTypeDef(const std::string& typeName) const {
    auto it = resourceTypeDefinitions.find(typeName);
    if (it != resourceTypeDefinitions.end()) {
        return it->second;
    }
    std::cerr << "Error: Resource type definition not found: " << typeName << std::endl;
    return ResourceTypeDefinition();
}

UnitTypeDefinition GameTypesLoader::GetUnitTypeDef(const std::string& typeName) const {
    auto it = unitTypeDefinitions.find(typeName);
    if (it != unitTypeDefinitions.end()) {
        return it->second;
    }
    std::cerr << "Error: Unit type definition not found: " << typeName << std::endl;
    return UnitTypeDefinition();
}

ModuleDefinition GameTypesLoader::GetModuleDef(const std::string& moduleName) const {
    auto it = moduleDefinitions.find(moduleName);
    if (it != moduleDefinitions.end()) {
        return it->second;
    }
    std::cerr << "Error: Module definition not found: " << moduleName << std::endl;
    return ModuleDefinition();
}

bool GameTypesLoader::HasResourceType(const std::string& typeName) const {
    return resourceTypeDefinitions.find(typeName) != resourceTypeDefinitions.end();
}

bool GameTypesLoader::HasUnitType(const std::string& typeName) const {
    return unitTypeDefinitions.find(typeName) != unitTypeDefinitions.end();
}

bool GameTypesLoader::HasModule(const std::string& moduleName) const {
    return moduleDefinitions.find(moduleName) != moduleDefinitions.end();
}

const std::map<std::string, ResourceTypeDefinition>& GameTypesLoader::GetAllResourceTypes() const {
    return resourceTypeDefinitions;
}

const std::map<std::string, UnitTypeDefinition>& GameTypesLoader::GetAllUnitTypes() const {
    return unitTypeDefinitions;
}

const std::map<std::string, ModuleDefinition>& GameTypesLoader::GetAllModules() const {
    return moduleDefinitions;
}
