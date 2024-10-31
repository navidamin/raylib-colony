#include "unit.h"
#include <iostream>
#include <cmath>

Unit::Unit(std::string type) :
    unit_type(type),
    status("inactive"),
    energy_cost(0),
    isUnderConstruction(false),
    productionCycleTime(0)
{
    SetInitialParameters();
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

void Unit::Update() {
    // TODO: Implement unit update logic
    std::cout << "Unit " << unit_type << " updated." << std::endl;
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
    // Left control panel (rectangle)
    //Rectangle controlPanel = { 0, 0, 300, (float)GetScreenHeight() };
    //DrawRectangleRec(controlPanel, GRAY);

    // Right transparent panel
    Rectangle transparentPanel = { (float)GetScreenWidth() - 300, 0, 300, (float)GetScreenHeight() };
    DrawRectangleRec(transparentPanel, Fade(GRAY, 0.5f));

    // Draw additional UI elements inside the control panel (e.g., unit stats)
    DrawText(("Unit Type: " + unit_type).c_str(), (float)GetScreenWidth() - 280, 10, 20, BLACK);
    // Add more UI elements as needed
}

void Unit::SetInitialParameters() {
    if (unit_type == "Extraction") {
        parameters["ExtractionRate"] = 10;
        parameters["ResourceFocus"] = 1; // 1 could represent "Iron"
        parameters["EnergyConsumption"] = 5;
        parameters["WearAndTear"] = 0.2; // 1 Fe per 5 minutes
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

void Unit::ProcessExtraction(float deltaTime) {
    if (!IsActive()) return;

    // Get relevant parameters
    float extractionRate = parameters["ExtractionRate"];
    float efficiency = parameters["Efficiency"];
    float wearAndTear = parameters["WearAndTear"];

    // Calculate actual resources extracted in this time step
    float resourcesExtracted = extractionRate * efficiency * deltaTime;

    // Apply wear and tear
    parameters["Efficiency"] = std::max(0.1f, efficiency - (wearAndTear * deltaTime));

    /*
    // Check for random breakdowns
    float breakdownChance = parameters["BreakdownChance"];
    if (GetRandomValue(0, 100) < breakdownChance * 100) {
        SetStatus("inactive");
        std::cout << "Unit " << unit_type << " broke down!" << std::endl;
    }*/

    // TODO: Add extracted resources to storage
    // This will need to interface with your resource management system
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
