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
    parentSectPosition(position)
{
    SetInitialParameters();
    InitializeModules();
    InitializeStorage();
}

Unit::~Unit() {
    // Cleanup if necessary
}

void Unit::Start() {
    status = "active";
    std::cout << "Unit " << unit_type << " started." << std::endl;
}

void Unit::Stop() {
    status = "inactive";
    std::cout << "Unit " << unit_type << " stopped." << std::endl;
}

void Unit::Upgrade(int level) {
    // TODO: Implement upgrade logic
    std::cout << "Unit " << unit_type << " upgraded to level " << level << std::endl;
}

std::map<std::string, float> Unit::CalculateConsumption() const {
    // TODO: Implement consumption calculation
    std::cout << "Unit " << unit_type << " consumption calculated." << std::endl;
    return consumption;
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
    // Left section for resource stats
    Rectangle statsPanel = { 0, 0, 600, (float)GetScreenHeight() };
    DrawRectangleRec(statsPanel, Fade(RAYWHITE, 0.9f));

    // Draw all resource stats
    DrawResourceStats();

    // Right transparent panel for other info
    Rectangle rightPanel = { (float)GetScreenWidth() - 300, 0, 300, (float)GetScreenHeight() };
    DrawRectangleRec(rightPanel, Fade(GRAY, 0.5f));

    // Draw additional UI elements in right panel
    DrawText(("Unit Type: " + unit_type).c_str(),
            (float)GetScreenWidth() - 280, 10, 20, BLACK);
}

void Unit::SetInitialParameters() {
    if (unit_type == "Extraction") {
        parameters["H2ExtractionRate"] = .2;
        parameters["O2ExtractionRate"] = .1;
        parameters["CExtractionRate"] = .1;
        parameters["FeExtractionRate"] = .3;
        parameters["SiExtractionRate"] = .01;
        parameters["ResourceFocus"] = 1;
        parameters["EnergyConsumption"] = 5;
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
    basicModule.efficiency = parameters["Efficiency"];

    if (unit_type == "Extraction") {
        basicModule.consumptionRates[ResourceType::ENERGY] = parameters["EnergyConsumption"];
        // Set production rates based on extraction parameters
        basicModule.productionRates[ResourceType::H2] = parameters["H2ExtractionRate"];
        basicModule.productionRates[ResourceType::O2] = parameters["O2ExtractionRate"];
        basicModule.productionRates[ResourceType::C]  = parameters["CExtractionRate"];
        basicModule.productionRates[ResourceType::Fe] = parameters["FeExtractionRate"];
        basicModule.productionRates[ResourceType::Si] = parameters["SiExtractionRate"];    }
    else if (unit_type == "Farming") {
        basicModule.consumptionRates[ResourceType::H2] = parameters["WaterConsumption"];
        basicModule.consumptionRates[ResourceType::O2] = 1.5f;
        basicModule.consumptionRates[ResourceType::C] = 1.0f;
        basicModule.productionRates[ResourceType::FOOD] = parameters["FoodProductionRate"];
        basicModule.productionRates[ResourceType::WATER] = 2.0f;
    }
    else if (unit_type == "Energy") {
        basicModule.productionRates[ResourceType::ENERGY] = parameters["EnergyOutput"];
    }

    modules.push_back(basicModule);
    activeModule = &modules[0];
}

bool Unit::UpgradeModule(int moduleIndex) {
    if (moduleIndex >= modules.size() || modules[moduleIndex].level >= 5) {
        return false;
    }

    UnitModule& module = modules[moduleIndex];

    // Check if we have required resources for upgrade
    const auto& costs = module.upgradeCosts[module.level + 1];
    // TODO: Check if we have enough resources

    module.level++;

    // Update efficiency and rates based on level
    float levelMultiplier = 1.0f + (module.level - 1) * 0.2f;

    // Consumption rates decrease with level
    for (auto& [type, rate] : module.consumptionRates) {
        rate = rate * (2.0f - levelMultiplier);
    }

    // Production rates increase with level
    for (auto& [type, rate] : module.productionRates) {
        rate = rate * levelMultiplier;
    }

    return true;
}

void Unit::ProcessModuleEffects(float deltaTime, ResourceManager& resourceManager) {
    if (!IsActive() || !activeModule) return;

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

    // Consume resources
    for (const auto& [type, rate] : activeModule->consumptionRates) {
        resourceStorage[type] -= rate * deltaTime;
    }

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

void Unit::DrawResourceStats() {
    float startX = 20.0f;
    float startY = 100.0f;
    float rowHeight = 25.0f;
    float colWidth = 150.0f;
    int fontSize = 20;
    Color darkGreen = {34, 139, 34, 255};  // Forest Green

    // First draw unit and module info
    DrawText(TextFormat("Unit Type: %s", unit_type.c_str()),
             startX, startY, fontSize + 5, BLACK);
    startY += rowHeight * 1.5f;

    if (activeModule) {
        DrawText(TextFormat("Active Module: %s (Level %d)",
                activeModule->name.c_str(),
                activeModule->level),
                startX, startY, fontSize, BLACK);
        startY += rowHeight;

        DrawText(TextFormat("Status: %s", status.c_str()),
                startX, startY, fontSize,
                status == "active" ? darkGreen : GRAY);
        startY += rowHeight;

        DrawText(TextFormat("Efficiency: %.1f%%", activeModule->efficiency * 100.0f),
                startX, startY, fontSize, BLACK);
        startY += rowHeight * 2;  // Extra space before rates

        // Column headers for rates
        DrawText("Resource", startX, startY, fontSize, BLACK);
        DrawText("Production", startX + colWidth, startY, fontSize, BLACK);
        DrawText("Consumption", startX + colWidth * 2, startY, fontSize, BLACK);

        startY += rowHeight;

        // List of all resource types with their names
        std::vector<std::pair<ResourceType, const char*>> resources = {
            {ResourceType::ENERGY, "Energy"},
            {ResourceType::H2, "Hydrogen"},
            {ResourceType::O2, "Oxygen"},
            {ResourceType::C, "Carbon"},
            {ResourceType::Fe, "Iron"},
            {ResourceType::Si, "Silicon"},
            {ResourceType::WATER, "Water"},
            {ResourceType::FOOD, "Food"},
            {ResourceType::SCIENCE, "Science"},
            {ResourceType::MANPOWER, "Manpower"}
        };

        // Draw rates only for resources that have production or consumption
        for (const auto& [type, name] : resources) {
            bool hasActivity = false;
            float prodRate = activeModule->productionRates[type];
            float consRate = activeModule->consumptionRates[type];

            if (prodRate > 0 || consRate > 0) {
                hasActivity = true;
                DrawText(name, startX, startY, fontSize, BLACK);

                // Production rate
                if (prodRate > 0) {
                    DrawText(TextFormat("+%.2f/s", prodRate),
                            startX + colWidth, startY, fontSize, darkGreen);
                } else {
                    DrawText("-", startX + colWidth, startY, fontSize, GRAY);
                }

                // Consumption rate
                if (consRate > 0) {
                    DrawText(TextFormat("-%.2f/s", consRate),
                            startX + colWidth * 2, startY, fontSize, RED);
                } else {
                    DrawText("-", startX + colWidth * 2, startY, fontSize, GRAY);
                }

                if (hasActivity) {
                    startY += rowHeight;
                }
            }
        }
    } else {
        DrawText("No active module", startX, startY, fontSize, GRAY);
    }

    // Add Debug Storage Info section
    startY += rowHeight * 2;  // Add some space before debug section

    DrawText("Debug Info", startX, startY, fontSize, RED);
    startY += rowHeight * 1.2f;

    bool hasStoredResources = false;
    std::vector<std::pair<ResourceType, const char*>> resources = {
        {ResourceType::ENERGY, "Energy"},
        {ResourceType::H2, "Hydrogen"},
        {ResourceType::O2, "Oxygen"},
        {ResourceType::C, "Carbon"},
        {ResourceType::Fe, "Iron"},
        {ResourceType::Si, "Silicon"},
        {ResourceType::WATER, "Water"},
        {ResourceType::FOOD, "Food"},
        {ResourceType::SCIENCE, "Science"},
        {ResourceType::MANPOWER, "Manpower"}
    };

    // Check for any stored resources
    for (const auto& [type, name] : resources) {
        float stored = GetStoredResource(type);
        if (stored > 0) {
            hasStoredResources = true;
            DrawText(TextFormat("%s: %.2f", name, stored),
                    startX, startY, fontSize, BLACK);
            startY += rowHeight;
        }
    }

    // If no resources are stored, show empty message
    if (!hasStoredResources) {
        DrawText("Unit storage is empty", startX, startY, fontSize, GRAY);
    }
}
