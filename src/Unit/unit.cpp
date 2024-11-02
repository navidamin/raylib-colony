#include "unit.h"
#include <iostream>
#include <cmath>

Unit::Unit(std::string type, Vector2 &position, ResourceManager &resource, TimeManager &time) :
    unit_type(type),
    status("inactive"),
    energy_cost(0),
    isUnderConstruction(false),
    productionCycleTime(0),
    resourceManager(resource),
    timeManager(time),
    parentSectPosition(position),
    isInModuleView(false),
    selectedModuleIndex(-1),
    lastClickTime(0),
    lastClickedModule(-1),
    showingStats(false)
{
    SetInitialParameters();
    InitializeModules();
    InitializeStorage();
}

Unit::~Unit() {
    // Cleanup if necessary
}

void Unit::Start() {
    // Only start if we have an active module

        status = "active";
        std::cout << "Unit " << unit_type << " started." << std::endl;

}

void Unit::Stop() {
    status = "inactive";
    std::cout << "Unit " << unit_type << " stopped." << std::endl;
}

void Unit::UpdateUnitStatus() {
    // Check if any module is active
    bool anyModuleActive = false;
    activeModule = nullptr;  // Reset active module pointer

    for (auto& module : modules) {
        if (module.isActive) {
            anyModuleActive = true;
            activeModule = &module;  // Set active module to the active one
            break;
        }
    }

    // Update unit status based on module state
    status = anyModuleActive ? "active" : "inactive";
}

void Unit::Upgrade(int level) {
    // TODO: Implement upgrade logic
    std::cout << "Unit " << unit_type << " upgraded to level " << level << std::endl;
}

void Unit::CalculateConsumption() {
    if (!activeModule) {
        std::cout << "No active module, skipping consumption calculation" << std::endl;
        return;
    }

    // Debug output before calculation
    std::cout << "\nStarting consumption calculation for " << unit_type << std::endl;
    std::cout << "Production rates:" << std::endl;
    for (const auto& [res, rate] : activeModule->productionRates) {
        std::cout << "Resource " << static_cast<int>(res) << ": " << rate << std::endl;
    }

    std::cout << "Production costs map size: " << productionCosts.size() << std::endl;

    // Clear existing consumption rates
    activeModule->consumptionRates.clear();

    // For each production rate
    for (const auto& [producedResource, productionRate] : activeModule->productionRates) {
        // Skip if production rate is 0
        if (productionRate <= 0) {
            std::cout << "Skipping resource " << static_cast<int>(producedResource)
                     << " due to zero production rate" << std::endl;
            continue;
        }

        std::cout << "Checking costs for resource " << static_cast<int>(producedResource) << std::endl;

        // Safely check if this resource has associated costs
        if (productionCosts.count(producedResource) == 0) {
            std::cout << "No production costs defined for resource "
                     << static_cast<int>(producedResource) << std::endl;
            continue;
        }

        // Get the cost map for this resource
        const auto& resourceCosts = productionCosts.at(producedResource);
        std::cout << "Found " << resourceCosts.size() << " cost entries for resource "
                 << static_cast<int>(producedResource) << std::endl;

        // For each resource consumed in production
        for (const auto& [consumedResource, rate] : resourceCosts) {
            // Calculate consumption based on production rate
            float consumption = productionRate * rate;

            std::cout << "Calculated consumption for resource "
                     << static_cast<int>(consumedResource)
                     << ": " << consumption << "/s" << std::endl;

            // Add to module's consumption rates
            activeModule->consumptionRates[consumedResource] += consumption;
        }
    }

    // Debug output final consumption rates
    std::cout << "Final consumption rates:" << std::endl;
    for (const auto& [resource, rate] : activeModule->consumptionRates) {
        try {
            std::string resourceName = ResourceUtils::GetResourceName(resource);
            std::cout << resourceName << ": " << rate << "/s" << std::endl;
        } catch (...) {
            std::cout << "Resource " << static_cast<int>(resource) << ": " << rate << "/s" << std::endl;
        }
    }
}

std::map<std::string, float> Unit::CalculateProduction() const {
    // TODO: Implement production calculation
    std::cout << "Unit " << unit_type << " production calculated." << std::endl;
    return production;
}

void Unit::DisplayStats() const {
    std::cout << "Unit Type: " << unit_type << ", Status: " << status << std::endl;
    for (const auto& param : parameters) {
        std::cout << param.first << ": " << param.second << std::endl;
    }
}

void Unit::Update(float deltaTime) {
    ProcessModuleEffects(deltaTime, resourceManager);

}

void Unit::DrawInSectView(Vector2 corePosition, float coreRadius, int index) {
    float angle = (index * 45.0f) * DEG2RAD;  // 8 units evenly spaced (360/8 = 45 degrees)
    float radius = coreRadius * 1.2f;  // Distance from the core

    Vector2 unitPosition = {
        corePosition.x + radius * cosf(angle),
        corePosition.y + radius * sinf(angle)
    };

    // Draw the unit circle
    bool isBuilt = (status == "active");
    float unitRadius = 30;
    DrawCircleV(unitPosition, unitRadius, isBuilt ? BLUE : BLANK);
    DrawCircleLines(unitPosition.x, unitPosition.y, unitRadius, GREEN);
}

void Unit::DrawInUnitView() {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    // Draw main background
    DrawRectangle(0, 0, screenWidth, screenHeight, RAYWHITE);

    // Draw UI sections
    DrawTopBar();
    DrawBottomBar();

    // Draw three-panel layout
    if (isInModuleView) {
        DrawModuleList();
        DrawModuleDetails();
        DrawControlPanel();


    } else {
        DrawModuleList();
        DrawResourcePanel();
        DrawControlPanel();

    }

    UpdateMessage(GetFrameTime());
}

void Unit::SetInitialParameters() {
    if (unit_type == "Extraction") {
        parameters["H2ExtractionRate"] = .1;
        parameters["O2ExtractionRate"] = .1;
        parameters["CExtractionRate"] = .1;
        parameters["FeExtractionRate"] = .1;
        parameters["SiExtractionRate"] = .01;
        parameters["ResourceFocus"] = 1;
        parameters["EnergyConsumption"] = 1;
        parameters["WearAndTear"] = 0.2;
        parameters["Efficiency"] = 0.8;
        parameters["StorageCapacity"] = 100;
        parameters["BreakdownChance"] = 0.02;
    } else if (unit_type == "Farming") {
        parameters["FoodProductionRate"] = 10;
        parameters["WaterConsumption"] = 3;
        parameters["EnergyConsumption"] = 2;
        parameters["FertilityLevel"] = 0.75;
        parameters["StorageCapacity"] = 200;
        parameters["GrowthBoost"] = 1.1;
        parameters["CropFocus"] = 1; // 1 could represent "Grain"
    } else if (unit_type == "Energy") {
        parameters["EnergyOutput"] = 15;
        parameters["EnergySource"] = 1; // 1 could represent "Solar"
        parameters["StorageCapacity"] = 500;
        parameters["Efficiency"] = 0.9;
        parameters["FuelConsumption"] = 2;
        parameters["WeatherImpact"] = -0.2;
        parameters["MaintenanceCost"] = 0.1; // 1 Fe per 10 minutes
    } else if (unit_type == "Manufacture") {
        parameters["ProductionRate"] = 1;
        parameters["BlueprintsUnlocked"] = 1; // 1 could represent "Tools"
        parameters["EnergyConsumption"] = 5;
        parameters["MaterialConsumption"] = 5; // 3 Fe + 2 Si
        parameters["ProductStorage"] = 100;
        parameters["ProductionEfficiency"] = 0.85;
        parameters["UpgradeEffect"] = 0.1;
    } else if (unit_type == "Construction") {
        parameters["BuildSpeed"] = 0.2; // 1 structure per 5 minutes
        parameters["RepairEfficiency"] = 0.9;
        parameters["EnergyConsumption"] = 4;
        parameters["MaterialConsumption"] = 5;
        parameters["MaintenanceCost"] = 0.2; // 2 Fe per 10 minutes
        parameters["ConstructionRange"] = 2;
    } else if (unit_type == "Transport") {
        parameters["TransportCapacity"] = 50;
        parameters["Speed"] = 10;
        parameters["EnergyConsumption"] = 5;
        parameters["FuelConsumption"] = 3;
        parameters["Efficiency"] = 0.8;
        parameters["RoadConstructionSpeed"] = 1.0/24; // 1 km per day
        parameters["UpgradeEffect"] = 0.1;
    } else if (unit_type == "Research") {
        parameters["ResearchPointsPerTick"] = 5;
        parameters["EnergyConsumption"] = 10;
        parameters["RareMetalConsumption"] = 1;
        parameters["FocusArea"] = 1; // 1 could represent "Manufacturing"
        parameters["ResearchSpeedMultiplier"] = 1.0;
        parameters["BreakthroughChance"] = 0.05;
        parameters["UpgradeEffect"] = 0.2;
    } else if (unit_type == "Communication") {
        parameters["TradeCapacity"] = 100;
        parameters["ExchangeRate"] = 1;
        parameters["EnergyConsumption"] = 3;
        parameters["GoodsConsumption"] = 2;
        parameters["TradeEfficiency"] = 0.9;
        parameters["UpgradeEffect"] = 0.05;
    }
}


void Unit::ProcessFarming(float deltaTime) {
    if (!IsActive()) return;

    // Get relevant parameters
    float productionRate = parameters["FoodProductionRate"];
    float fertility = parameters["FertilityLevel"];
    float growthBoost = parameters["GrowthBoost"];
    float waterConsumption = parameters["WaterConsumption"];

    // Calculate food production for this time step
    float foodProduced = productionRate * fertility * growthBoost * deltaTime;

    // Consume water
    float waterNeeded = waterConsumption * deltaTime;
    // TODO: Check if enough water is available and subtract it

    // Reduce fertility over time (soil degradation)
    parameters["FertilityLevel"] = std::max(0.2f, fertility - (0.01f * deltaTime));

    // TODO: Add produced food to storage
    // This will need to interface with your resource management system
}

void Unit::ProcessEnergy(float deltaTime) {
    if (!IsActive()) return;

    // Get relevant parameters
    float energyOutput = parameters["EnergyOutput"];
    float efficiency = parameters["Efficiency"];
    float weatherImpact = parameters["WeatherImpact"];
    float fuelConsumption = parameters["FuelConsumption"];

    // Calculate actual energy production for this time step
    float energyProduced = energyOutput * efficiency * (1.0f + weatherImpact) * deltaTime;

    // Consume fuel if needed
    float fuelNeeded = fuelConsumption * deltaTime;
    // TODO: Check if enough fuel is available and subtract it

    // Apply maintenance degradation
    float maintenanceCost = parameters["MaintenanceCost"];
    parameters["Efficiency"] = std::max(0.2f, efficiency - (maintenanceCost * deltaTime));

    // TODO: Add produced energy to storage
    // This will need to interface with your resource management system
}

void Unit::UpdateConstruction(float deltaTime) {
    if (!IsUnderConstruction()) return;

    float buildTime = parameters["BuildTime"];
    float currentProgress = parameters["ConstructionProgress"];

    // Update construction progress
    currentProgress += deltaTime;
    parameters["ConstructionProgress"] = currentProgress;

    // Check if construction is complete
    if (currentProgress >= buildTime) {
        OnConstructionComplete();
    }
}

void Unit::OnConstructionComplete() {
    SetStatus("active");
    parameters["ConstructionProgress"] = parameters["BuildTime"];

    // Initialize operational parameters based on unit type
    if (unit_type == "Extraction") {
        parameters["Efficiency"] = 0.8f;  // Start at 80% efficiency
    } else if (unit_type == "Farming") {
        parameters["FertilityLevel"] = 0.75f;  // Start with 75% fertility
    } else if (unit_type == "Energy") {
        parameters["Efficiency"] = 0.9f;  // Start at 90% efficiency
    }

    std::cout << "Construction complete for " << unit_type << " unit!" << std::endl;
}

void Unit::InitializeModules() {
    UnitModule basicModule;
    basicModule.name = "Basic " + unit_type;
    basicModule.level = 1;
    basicModule.isBuilt = true;  // Changed from the original
    basicModule.isActive = true;
    basicModule.efficiency = parameters["Efficiency"];
    basicModule.description = "Basic module for " + unit_type;

    // Initialize maps with empty maps (this ensures they exist)
    basicModule.consumptionRates = std::map<ResourceType, float>();
    basicModule.productionRates = std::map<ResourceType, float>();
    basicModule.maxProductionRates = std::map<ResourceType, float>();

    // Initialize production costs based on unit type
    if (unit_type == "Extraction") {
        productionCosts = EXTRACTION_PRODUCTION_COSTS;
    }
    else if (unit_type == "Farming") {
        productionCosts = FARMING_PRODUCTION_COSTS;
    }

    // Initialize upgrade costs for all levels (1-5)
    for (int level = 1; level <= 5; level++) {
        // Initialize empty maps for each level
        basicModule.upgradeCosts[level] = std::map<ResourceType, float>();
        basicModule.enhancements[level] = std::map<std::string, float>();

        // Add some example costs and enhancements
        if (level > 1) {  // Levels 2-5 have upgrade costs
            basicModule.upgradeCosts[level][ResourceType::ENERGY] = 10.0f * level;
            basicModule.upgradeCosts[level][ResourceType::Fe] = 0.1f * level;

            basicModule.enhancements[level]["efficiency"] = 1.0f + (level * 0.1f);
            basicModule.enhancements[level]["production"] = 1.0f + (level * 0.2f);
        }
    }

    // Set type-specific rates
    if (unit_type == "Extraction") {
        basicModule.maxProductionRates[ResourceType::H2] = parameters["H2ExtractionRate"];
        basicModule.maxProductionRates[ResourceType::O2] = parameters["O2ExtractionRate"];
        basicModule.maxProductionRates[ResourceType::C]  = parameters["CExtractionRate"];
        basicModule.maxProductionRates[ResourceType::Fe] = parameters["FeExtractionRate"];
        basicModule.maxProductionRates[ResourceType::Si] = parameters["SiExtractionRate"];

        // Initialize production rates to maximum
        basicModule.productionRates = basicModule.maxProductionRates;

        // Set consumption rates
        basicModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"];
    }
    else if (unit_type == "Farming") {
            // Set maximum production rates for farming
            basicModule.maxProductionRates[ResourceType::FOOD] = parameters["FoodProductionRate"];

            // Initialize production rates to maximum
            basicModule.productionRates = basicModule.maxProductionRates;

            // Set consumption rates
            basicModule.consumptionRates[ResourceType::WATER] = parameters["WaterConsumption"];
            basicModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"];
    }
    // ... other unit types ...

    modules.push_back(basicModule);
    activeModule = &modules[0];

    // Initialize future modules
    InitializeFutureModules();

    // Update unit status based on module states
    UpdateUnitStatus();

    // Initialize production/consumption rates if we have an active module
    if (activeModule) {
        CalculateConsumption();
    }
}


bool Unit::UpgradeModule(int moduleIndex) {
    if (moduleIndex >= modules.size() || modules[moduleIndex].level >= 5) {
        return false;
    }

    UnitModule& module = modules[moduleIndex];

    // Check if we have required resources for upgrade
    const auto& costs = module.upgradeCosts[module.level + 1];
    // TODO: Check if we have enough resources

    for (auto& [resource, cost] : costs) {
        ConsumeResource(resource, cost);
    }
    module.level++;

    // Update efficiency and rates based on level
    float levelMultiplier = 1.0f + (module.level - 1) * 0.2f;

    // Consumption rates decrease with level
    for (auto& [type, rate] : module.consumptionRates) {
        rate = rate * (2.0f - levelMultiplier);
    }

    // Update maximum production rates
    for (auto& [type, rate] : module.maxProductionRates) {
        float baseRate = rate / (1.0f + (module.level - 2) * 0.2f);  // Get original base rate
        rate = baseRate * levelMultiplier;  // Apply new level multiplier
    }

    // Update actual production rates to maintain same proportion of max
    for (auto& [type, rate] : module.productionRates) {
        if (module.maxProductionRates.count(type) > 0) {
            float proportion = rate / module.maxProductionRates[type];
            rate = module.maxProductionRates[type] * proportion;
        }
    }

    // Recalculate consumption rates if this is the active module
    if (&module == activeModule) {
        CalculateConsumption();
    }

    ShowMessage(TextFormat("Module upgraded to level %d - Production set to maximum", module.level));
    return true;
}

void Unit::ProcessModuleEffects(float deltaTime, ResourceManager& resourceManager) {
    if (!IsActive() || !activeModule) return;

    CalculateConsumption();  // Update consumption rates when module is activated

    // Check if we have enough resources for consumption
    bool canProcess = true;
    for (const auto& [type, rate] : activeModule->consumptionRates) {
        float required = rate * deltaTime;
        if (resourceStorage[type] < required) {
            canProcess = false;
            std::cout << "Not enough resource " << static_cast<int>(type)
                     << " for processing. Required: " << required
                     << ", Available: " << resourceStorage[type] << std::endl;
            break;
        }
    }

    if (!canProcess) return;
    std::cout << "\nIn the PrcoessModule" << std::endl;

    /*
    // Consume resources
    for (const auto& [type, rate] : activeModule->consumptionRates) {
        resourceStorage[type] -= rate * deltaTime;
    }*/

    // Handle production based on unit type
    if (unit_type == "Extraction") {
        ProcessExtraction(deltaTime, resourceManager);
    }
    else {
        // Normal production for other unit types
        for (const auto& [type, rate] : activeModule->productionRates) {
            resourceStorage[type] += rate * deltaTime;
        }
    }
}

void Unit::ProcessExtraction(float deltaTime, ResourceManager& resourceManager) {
    Vector2 gridPos = WorldToGrid(parentSectPosition);
    int gridX = static_cast<int>(gridPos.x);
    int gridY = static_cast<int>(gridPos.y);

    // Get available resources at this location
    auto availableResources = resourceManager.GetResourcesAtGrid(gridX, gridY);

    int worldX = static_cast<int>(parentSectPosition.x);
    int worldY = static_cast<int>(parentSectPosition.y);

    float efficiency = activeModule->efficiency;
    float levelMultiplier = 1.0f + (activeModule->level - 1) * 0.2f;

    // Map for base extraction rates
    std::map<ResourceType, float> extractionRates = {
        {ResourceType::H2, parameters["H2ExtractionRate"]},
        {ResourceType::O2, parameters["O2ExtractionRate"]},
        {ResourceType::C,  parameters["CExtractionRate"]},
        {ResourceType::Fe, parameters["FeExtractionRate"]},
        {ResourceType::Si, parameters["SiExtractionRate"]}
    };

    // For Debugging resource variation at Sect
    //resourceManager.DisplayResourceGrid(parentSectPosition);

    // Debug print extraction rates
    std::cout << "\nExtraction rates for each resource:" << std::endl;
    for (const auto& [type, rate] : extractionRates) {
        std::cout << "Resource " << static_cast<int>(type) << " rate: " << rate << std::endl;
    }

    // Process each available resource
    for (const auto& [resourceType, abundance] : availableResources) {

        std::cout << "resourceType:" << resourceType << "\t abundance:" << abundance << std::endl;

        if (abundance < 0.1f) {
            std::cout << "Resource " << static_cast<int>(resourceType)
                     << " abundance too low: " << abundance << std::endl;
            continue;
        }

        // Get the specific extraction rate for this resource type
        float baseRate = extractionRates[resourceType];
        float extractionAmount = baseRate * efficiency * levelMultiplier * abundance * deltaTime;

        // Deplete the resource from the planet
        resourceManager.UpdateResourceDepletion(gridX, gridY, resourceType, extractionAmount);

        // Add the extracted resource to storage
        resourceStorage[resourceType] += extractionAmount;


        // Debug print for all resources
        std::cout << "Resource " << static_cast<int>(resourceType)
                 << " - Extracted: " << extractionAmount
                 << " (Rate: " << baseRate
                 << ", Efficiency: " << efficiency
                 << ", Level Mult: " << levelMultiplier
                 << ", Abundance: " << abundance << ")" << std::endl;

        // Debug print for Fe
        if (resourceType == ResourceType::Fe) {
            std::cout << "Fe Extraction - Amount: " << extractionAmount
                     << ", Current Storage: " << resourceStorage[ResourceType::Fe] << std::endl;
        }
    }
}

// Add getters/setters for resource storage
float Unit::GetStoredResource(ResourceType type) const {
    auto it = resourceStorage.find(type);
    return it != resourceStorage.end() ? it->second : 0.0f;
}

void Unit::AddResource(ResourceType type, float amount) {
    resourceStorage[type] += amount;
}

bool Unit::ConsumeResource(ResourceType type, float amount) {
    if (resourceStorage[type] >= amount) {
        resourceStorage[type] -= amount;
        return true;
    }
    return false;
}


// function for the sect to recollect generated resources at the end of the day
void Unit::DischargeAllResources(std::map<ResourceType, float>& collected) {
    // First, calculate daily consumption needs for each resource
    std::map<ResourceType, float> dailyNeeds;

    if (activeModule && IsActive()) {
        // Calculate 24 hours worth of consumption for each resource
        float dayInSeconds = TICKS_PER_DAY * TICK_DURATION;  // or however your time system works

        // Get consumption rates from active module
        for (const auto& [type, rate] : activeModule->consumptionRates) {
            dailyNeeds[type] = rate * dayInSeconds * 1.5f;  // Keep 1.5x safety margin
            std::cout << "Unit " << unit_type << " needs to keep "
                     << dailyNeeds[type] << " of resource "
                     << type << " for next day" << std::endl;
        }
    }

    // Now process each resource
    for (auto& [type, amount] : resourceStorage) {
        if (amount > 0) {
            float reserveAmount = dailyNeeds[type];  // Will be 0 if not in dailyNeeds
            float excessAmount = amount - reserveAmount;

            // Only discharge if we have excess
            if (excessAmount > 0) {
                collected[type] += excessAmount;
                amount = reserveAmount;  // Keep the reserve amount

                // Fixed ShowMessage call
                try {
                    std::string resourceName = ResourceUtils::GetResourceName(type);
                    ShowMessage(TextFormat("Unit discharged %.2f of %s",
                                         excessAmount, resourceName.c_str()));
                } catch (...) {
                    ShowMessage(TextFormat("Unit discharged %.2f of resource %d",
                                         excessAmount, static_cast<int>(type)));
                }

                std::cout << "Unit " << unit_type
                         << " discharged " << excessAmount
                         << " of resource " << static_cast<int>(type)
                         << " (keeping " << reserveAmount << " in reserve)" << std::endl;


            } else {
                std::cout << "Unit " << unit_type
                         << " keeping all " << amount
                         << " of resource " << static_cast<int>(type)
                         << " for next day's operations" << std::endl;
            }
        }
    }
}

void Unit::InitializeFutureModules() {
    // Enhanced Module - Level 1 stats but better efficiency
    UnitModule enhancedModule;
    enhancedModule.name = "Enhanced " + unit_type;
    enhancedModule.level = 1;
    enhancedModule.isBuilt = false;
    enhancedModule.isActive = false;
    enhancedModule.efficiency = parameters["Efficiency"] * 1.2f;  // 20% better efficiency
    enhancedModule.description = "Enhanced efficiency module for " + unit_type +
                                "\nProvides better resource utilization.";

    // Advanced Module - Higher base rates but more energy consumption
    UnitModule advancedModule;
    advancedModule.name = "Advanced " + unit_type;
    advancedModule.level = 1;
    advancedModule.isBuilt = false;
    advancedModule.isActive = false;
    advancedModule.efficiency = parameters["Efficiency"] * 1.1f;
    advancedModule.description = "Advanced module for " + unit_type +
                                "\nHigher production rates with increased energy cost.";

    // Automated Module - Less energy consumption but lower base efficiency
    UnitModule automatedModule;
    automatedModule.name = "Automated " + unit_type;
    automatedModule.level = 1;
    automatedModule.isBuilt = false;
    automatedModule.isActive = false;
    automatedModule.efficiency = parameters["Efficiency"] * 0.9f;  // Lower base efficiency
    automatedModule.description = "Automated module for " + unit_type +
                                "\nLower energy consumption but requires maintenance.";

    // Deep-Core Module - Highest rates but highest consumption
    UnitModule deepCoreModule;
    deepCoreModule.name = "Deep-Core " + unit_type;
    deepCoreModule.level = 1;
    deepCoreModule.isBuilt = false;
    deepCoreModule.isActive = false;
    deepCoreModule.efficiency = parameters["Efficiency"] * 1.3f;
    deepCoreModule.description = "Deep-Core module for " + unit_type +
                                "\nHighest production potential with maximum resource cost.";

    // Initialize production and consumption rates based on unit type
    if (unit_type == "Extraction") {
        // Enhanced Module
        enhancedModule.maxProductionRates = {
            {ResourceType::H2, parameters["H2ExtractionRate"] * 1.2f},
            {ResourceType::O2, parameters["O2ExtractionRate"] * 1.2f},
            {ResourceType::C,  parameters["CExtractionRate"] * 1.2f},
            {ResourceType::Fe, parameters["FeExtractionRate"] * 1.2f},
            {ResourceType::Si, parameters["SiExtractionRate"] * 1.2f}
        };
        enhancedModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"] * 1.1f;

        // Advanced Module
        advancedModule.maxProductionRates = {
            {ResourceType::H2, parameters["H2ExtractionRate"] * 1.5f},
            {ResourceType::O2, parameters["O2ExtractionRate"] * 1.5f},
            {ResourceType::C,  parameters["CExtractionRate"] * 1.5f},
            {ResourceType::Fe, parameters["FeExtractionRate"] * 1.5f},
            {ResourceType::Si, parameters["SiExtractionRate"] * 1.5f}
        };
        advancedModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"] * 1.4f;

        // Automated Module
        automatedModule.maxProductionRates = {
            {ResourceType::H2, parameters["H2ExtractionRate"] * 1.1f},
            {ResourceType::O2, parameters["O2ExtractionRate"] * 1.1f},
            {ResourceType::C,  parameters["CExtractionRate"] * 1.1f},
            {ResourceType::Fe, parameters["FeExtractionRate"] * 1.1f},
            {ResourceType::Si, parameters["SiExtractionRate"] * 1.1f}
        };
        automatedModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"] * 0.8f;

        // Deep-Core Module
        deepCoreModule.maxProductionRates = {
            {ResourceType::H2, parameters["H2ExtractionRate"] * 2.0f},
            {ResourceType::O2, parameters["O2ExtractionRate"] * 2.0f},
            {ResourceType::C,  parameters["CExtractionRate"] * 2.0f},
            {ResourceType::Fe, parameters["FeExtractionRate"] * 2.0f},
            {ResourceType::Si, parameters["SiExtractionRate"] * 2.0f}
        };
        deepCoreModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"] * 2.0f;
    }
    else if (unit_type == "Farming") {
        // Enhanced Module
        enhancedModule.maxProductionRates[ResourceType::FOOD] = parameters["FoodProductionRate"] * 1.2f;
        enhancedModule.consumptionRates[ResourceType::WATER] = parameters["WaterConsumption"] * 1.1f;
        enhancedModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"] * 1.1f;

        // Advanced Module
        advancedModule.maxProductionRates[ResourceType::FOOD] = parameters["FoodProductionRate"] * 1.5f;
        advancedModule.consumptionRates[ResourceType::WATER] = parameters["WaterConsumption"] * 1.4f;
        advancedModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"] * 1.4f;

        // Automated Module
        automatedModule.maxProductionRates[ResourceType::FOOD] = parameters["FoodProductionRate"] * 1.1f;
        automatedModule.consumptionRates[ResourceType::WATER] = parameters["WaterConsumption"] * 0.9f;
        automatedModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"] * 0.8f;

        // Deep-Core Module
        deepCoreModule.maxProductionRates[ResourceType::FOOD] = parameters["FoodProductionRate"] * 2.0f;
        deepCoreModule.consumptionRates[ResourceType::WATER] = parameters["WaterConsumption"] * 1.8f;
        deepCoreModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"] * 2.0f;
    }
    // ... Add other unit types here ...

    // Initialize production rates to max for all modules
    enhancedModule.productionRates = enhancedModule.maxProductionRates;
    advancedModule.productionRates = advancedModule.maxProductionRates;
    automatedModule.productionRates = automatedModule.maxProductionRates;
    deepCoreModule.productionRates = deepCoreModule.maxProductionRates;

    // Set upgrade costs for all modules (example costs, adjust as needed)
    for (int level = 1; level <= 5; level++) {
        float levelMultiplier = level * 1.5f;  // Costs increase with each level

        // Enhanced Module costs
        enhancedModule.upgradeCosts[level][ResourceType::ENERGY] = 20.0f * levelMultiplier;
        enhancedModule.upgradeCosts[level][ResourceType::Fe] = 10.0f * levelMultiplier;
        enhancedModule.enhancements[level]["efficiency"] = 1.0f + (level * 0.12f);
        enhancedModule.enhancements[level]["production"] = 1.0f + (level * 0.15f);

        // Advanced Module costs
        advancedModule.upgradeCosts[level][ResourceType::ENERGY] = 30.0f * levelMultiplier;
        advancedModule.upgradeCosts[level][ResourceType::Fe] = 15.0f * levelMultiplier;
        advancedModule.upgradeCosts[level][ResourceType::Si] = 5.0f * levelMultiplier;
        advancedModule.enhancements[level]["efficiency"] = 1.0f + (level * 0.15f);
        advancedModule.enhancements[level]["production"] = 1.0f + (level * 0.2f);

        // Automated Module costs
        automatedModule.upgradeCosts[level][ResourceType::ENERGY] = 15.0f * levelMultiplier;
        automatedModule.upgradeCosts[level][ResourceType::Fe] = 8.0f * levelMultiplier;
        automatedModule.upgradeCosts[level][ResourceType::Si] = 3.0f * levelMultiplier;
        automatedModule.enhancements[level]["efficiency"] = 1.0f + (level * 0.1f);
        automatedModule.enhancements[level]["production"] = 1.0f + (level * 0.12f);

        // Deep-Core Module costs
        deepCoreModule.upgradeCosts[level][ResourceType::ENERGY] = 40.0f * levelMultiplier;
        deepCoreModule.upgradeCosts[level][ResourceType::Fe] = 20.0f * levelMultiplier;
        deepCoreModule.upgradeCosts[level][ResourceType::Si] = 10.0f * levelMultiplier;
        deepCoreModule.enhancements[level]["efficiency"] = 1.0f + (level * 0.2f);
        deepCoreModule.enhancements[level]["production"] = 1.0f + (level * 0.25f);
    }

    // Add all modules to the modules vector
    modules.push_back(enhancedModule);
    modules.push_back(advancedModule);
    modules.push_back(automatedModule);
    modules.push_back(deepCoreModule);
}

void Unit::InitializeStorage() {
    resourceStorage[ResourceType::ENERGY] = INITIAL_UNIT_ENERGY;
    resourceStorage[ResourceType::FOOD] = INITIAL_UNIT_FOOD;
    resourceStorage[ResourceType::WATER] = INITIAL_UNIT_WATER;
    resourceStorage[ResourceType::SCIENCE] = INITIAL_UNIT_SCIENCE;
    resourceStorage[ResourceType::MANPOWER] = INITIAL_UNIT_MANPOWER;

    // Initialize extraction resources
    resourceStorage[ResourceType::H2] = 0.0f;
    resourceStorage[ResourceType::O2] = 0.0f;
    resourceStorage[ResourceType::C] = 0.0f;
    resourceStorage[ResourceType::Fe] = 0.0f;
    resourceStorage[ResourceType::Si] = 0.0f;

    // Debug print to verify initialization
    std::cout << "Unit storage initialized with values:" << std::endl;
    for (const auto& [type, amount] : resourceStorage) {
        std::cout << "Resource " << static_cast<int>(type) << ": " << amount << std::endl;
    }

}
void Unit::UpdateStorage(){
    // Implement if needed
}

Vector2 Unit::WorldToGrid(Vector2 worldPos) const {
    // Convert to grid coordinates
    return {
        std::floor(worldPos.x / (SECT_CORE_RADIUS * 2.0f)),
        std::floor(worldPos.y / (SECT_CORE_RADIUS * 2.0f))
    };
}

