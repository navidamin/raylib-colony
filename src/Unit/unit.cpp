#include "unit.h"
#include "unlock_registry.h"
#include <iostream>
#include <cmath>

Unit::Unit(std::string type, Vector2 &position, ResourceManager &resource,
           TimeManager &time, std::map<ResourceType, float> &storage,
           std::map<ResourceType, float> &capacity) :
    unit_type(type),
    status("inactive"),
    energy_cost(0),
    isUnderConstruction(false),
    productionCycleTime(0),
    resourceManager(resource),
    timeManager(time),
    resourceStorage(storage),
    storageCapacity(capacity),
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
    // Clear and rebuild active module indices
    activeModuleIndices.clear();

    for (size_t i = 0; i < modules.size(); i++) {
        if (modules[i].isActive) {
            activeModuleIndices.insert(i);
        }
    }

    // Update unit status based on module state
    status = !activeModuleIndices.empty() ? "active" : "inactive";
}

void Unit::Upgrade(int level) {
    // TODO: Implement upgrade logic
    std::cout << "Unit " << unit_type << " upgraded to level " << level << std::endl;
}

void Unit::CalculateConsumption() {
    if (activeModuleIndices.empty()) {
        std::cout << "No active modules, skipping consumption calculation" << std::endl;
        return;
    }

    // Calculate consumption for each active module
    for (int moduleIndex : activeModuleIndices) {
        UnitModule& module = modules[moduleIndex];

        // Clear existing consumption rates for this module
        module.consumptionRates.clear();

        // For each production rate in this module
        for (const auto& [producedResource, productionRate] : module.productionRates) {
            // Safely check if this resource has associated costs
            if (productionCosts.count(producedResource) == 0) {
                continue;
            }

            // Get the cost map for this resource
            const auto& resourceCosts = productionCosts.at(producedResource);

            // For each resource consumed in production
            for (const auto& [consumedResource, rate] : resourceCosts) {
                // Calculate consumption based on production rate
                float consumption = productionRate * rate;

                // Add to module's consumption rates
                module.consumptionRates[consumedResource] += consumption;
            }
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
        parameters["H2ExtractionRate"] = DEFAULT_H2ExtractionRate;
        parameters["O2ExtractionRate"] = DEFAULT_O2ExtractionRate;
        parameters["CExtractionRate"] = DEFAULT_CExtractionRate;
        parameters["FeExtractionRate"] = DEFAULT_FeExtractionRate;
        parameters["SiExtractionRate"] = DEFAULT_SiExtractionRate;
        parameters["ResourceFocus"] = DEFAULT_ResourceFocus;
        parameters["EnergyConsumption"] = DEFAULT_EnergyConsumption;
        parameters["WearAndTear"] = DEFAULT_WearAndTear;
        parameters["Efficiency"] = DEFAULT_Efficiency;
        parameters["StorageCapacity"] = DEFAULT_StorageCapacity;
        parameters["BreakdownChance"] = DEFAULT_BreakdownChance;
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
    if (unit_type == "Extraction")
    {
        InitializeExtractionModules();
    }
    else if (unit_type == "Farming")
    {
        InitializeFarmingModules();
    }
    else if (unit_type == "Energy")
    {
        InitializeEnergyModules();
    }
    else if (unit_type == "Manufacture")
    {
        InitializeManufactureModules();
    }
    else if (unit_type == "Research")
    {
        InitializeResearchModules();
    }
    else
    {
        // Generic fallback for other unit types
        InitializeGenericModules();
    }

    // Update unit status based on module states
    UpdateUnitStatus();

    // Initialize production/consumption rates if we have active modules
    if (!activeModuleIndices.empty())
    {
        CalculateConsumption();
    }
}

void Unit::InitializeExtractionModules() {
    productionCosts = EXTRACTION_PRODUCTION_COSTS;

    // Initialize first excavator (Tier 0: 1 excavator)
    Excavator firstExcavator;
    firstExcavator.id = 0;
    firstExcavator.gridPos = WorldToGrid(parentSectPosition);
    firstExcavator.method = "scoop";
    firstExcavator.depth = 0.0f;
    firstExcavator.rate = 30.0f;
    firstExcavator.wear = 0.0f;
    excavators.push_back(firstExcavator);

    // Module 0: PROSPECTING - surveys and identifies resources
    {
        UnitModule mod;
        mod.name = "Prospecting";
        mod.moduleType = "PROSPECTING";
        mod.tier = 0;
        mod.level = 1;
        mod.isBuilt = true;
        mod.isActive = true;
        mod.efficiency = 0.6f;  // Tier 0: visual estimation, 40% noise
        mod.energyRequired = 10.0f;
        mod.description = "Visual estimation of surface resources.\n~40% accuracy at Tier 0.";
        mod.tierDependencies = {"Spectroscopy"};  // Required for tier 1
        mod.consumptionRates[ResourceType::ENERGY] = 0.2f;
        modules.push_back(mod);
        activeModuleIndices.insert(0);
    }

    // Module 1: EXCAVATION - collects regolith
    {
        UnitModule mod;
        mod.name = "Excavation";
        mod.moduleType = "EXCAVATION";
        mod.tier = 0;
        mod.level = 1;
        mod.isBuilt = true;
        mod.isActive = true;
        mod.efficiency = parameters.count("Efficiency") ? parameters["Efficiency"] : 0.75f;
        mod.energyRequired = 30.0f;
        mod.description = "Manual scoop extraction.\n1 excavator, max 10cm depth, 30 kg/hr.";
        mod.tierDependencies = {"MechanizedDrilling"};
        // Tier 0: basic extraction rates
        mod.maxProductionRates[ResourceType::H2] = parameters["H2ExtractionRate"] * 0.5f;
        mod.maxProductionRates[ResourceType::O2] = parameters["O2ExtractionRate"] * 0.5f;
        mod.maxProductionRates[ResourceType::C]  = parameters["CExtractionRate"] * 0.5f;
        mod.maxProductionRates[ResourceType::Fe] = parameters["FeExtractionRate"] * 0.5f;
        mod.maxProductionRates[ResourceType::Si] = parameters["SiExtractionRate"] * 0.5f;
        mod.productionRates = mod.maxProductionRates;
        mod.consumptionRates[ResourceType::ENERGY] = 0.8f;
        modules.push_back(mod);
        activeModuleIndices.insert(1);
    }

    // Module 2: BENEFICIATION - separates extracted materials
    {
        UnitModule mod;
        mod.name = "Beneficiation";
        mod.moduleType = "BENEFICIATION";
        mod.tier = 0;
        mod.level = 1;
        mod.isBuilt = true;
        mod.isActive = true;
        mod.efficiency = 0.5f;  // Tier 0: direct output, low separation efficiency
        mod.energyRequired = 20.0f;
        mod.description = "Direct regolith output.\nNo separation processing at Tier 0.";
        mod.tierDependencies = {"MagneticSeparation"};
        mod.consumptionRates[ResourceType::ENERGY] = 0.5f;
        modules.push_back(mod);
        activeModuleIndices.insert(2);
    }

    // Initialize Tier 0 separation chain (direct passthrough)
    separationChain.push_back(SeparationNodes::CreateDirectOutput());

    // Module 3: OPERATIONS - scheduling and efficiency
    {
        UnitModule mod;
        mod.name = "Operations";
        mod.moduleType = "OPERATIONS";
        mod.tier = 0;
        mod.level = 1;
        mod.isBuilt = false;  // Requires tier 1 unlock
        mod.isActive = false;
        mod.efficiency = 0.85f;  // Tier 0: continuous, -15% efficiency penalty
        mod.energyRequired = 5.0f;
        mod.description = "Continuous operation mode.\n-15% efficiency penalty at Tier 0.";
        mod.tierDependencies = {"ShiftScheduling"};
        modules.push_back(mod);
    }

    // Module 4: DIRECTIVES - autonomous control
    {
        UnitModule mod;
        mod.name = "Directives";
        mod.moduleType = "DIRECTIVES";
        mod.tier = 0;
        mod.level = 1;
        mod.isBuilt = false;  // Requires tier 1 unlock
        mod.isActive = false;
        mod.efficiency = 1.0f;
        mod.energyRequired = 5.0f;
        mod.description = "Manual control only.\nNo automated directives at Tier 0.";
        mod.tierDependencies = {"BasicDirectives"};
        modules.push_back(mod);
    }
}

void Unit::InitializeFarmingModules() {
    productionCosts = FARMING_PRODUCTION_COSTS;

    struct ModuleInfo { std::string name; std::string type; std::string desc; };
    std::vector<ModuleInfo> farmModules = {
        {"Irrigation", "IRRIGATION", "Water distribution and soil management."},
        {"Greenhouse", "GREENHOUSE", "Controlled environment agriculture."},
        {"Hydroponics", "HYDROPONICS", "Water-based soilless cultivation."},
        {"Harvest", "HARVEST", "Automated crop collection and processing."},
        {"Storage", "STORAGE", "Cold storage and preservation systems."}
    };

    for (size_t i = 0; i < farmModules.size(); i++)
    {
        UnitModule mod;
        mod.name = farmModules[i].name;
        mod.moduleType = farmModules[i].type;
        mod.tier = 0;
        mod.level = 1;
        mod.isBuilt = (i < 3);   // First 3 built
        mod.isActive = (i < 3);  // First 3 active
        mod.efficiency = parameters.count("Efficiency") ? parameters["Efficiency"] : 0.8f;
        mod.description = farmModules[i].desc;

        if (i == 0)  // Irrigation produces food
        {
            mod.maxProductionRates[ResourceType::FOOD] = parameters.count("FoodProductionRate") ?
                parameters["FoodProductionRate"] : 10.0f;
            mod.productionRates = mod.maxProductionRates;
            mod.consumptionRates[ResourceType::WATER] = parameters.count("WaterConsumption") ?
                parameters["WaterConsumption"] : 3.0f;
            mod.consumptionRates[ResourceType::ENERGY] = parameters.count("EnergyConsumption") ?
                parameters["EnergyConsumption"] : 2.0f;
        }

        modules.push_back(mod);
        if (mod.isActive) activeModuleIndices.insert(static_cast<int>(i));
    }
}

void Unit::InitializeEnergyModules() {
    struct ModuleInfo { std::string name; std::string type; std::string desc; };
    std::vector<ModuleInfo> energyModules = {
        {"Solar Array", "SOLAR_ARRAY", "Photovoltaic energy generation."},
        {"Battery", "BATTERY", "Energy storage systems."},
        {"Nuclear", "NUCLEAR", "Nuclear fission power generation."},
        {"Grid", "GRID", "Power distribution network."},
        {"Emergency", "EMERGENCY", "Backup power and emergency systems."}
    };

    for (size_t i = 0; i < energyModules.size(); i++)
    {
        UnitModule mod;
        mod.name = energyModules[i].name;
        mod.moduleType = energyModules[i].type;
        mod.tier = 0;
        mod.level = 1;
        mod.isBuilt = (i < 3);
        mod.isActive = (i < 3);
        mod.efficiency = parameters.count("Efficiency") ? parameters["Efficiency"] : 0.9f;
        mod.description = energyModules[i].desc;

        if (i == 0)  // Solar Array produces energy
        {
            mod.maxProductionRates[ResourceType::ENERGY] = parameters.count("EnergyOutput") ?
                parameters["EnergyOutput"] : 15.0f;
            mod.productionRates = mod.maxProductionRates;
        }

        modules.push_back(mod);
        if (mod.isActive) activeModuleIndices.insert(static_cast<int>(i));
    }
}

void Unit::InitializeManufactureModules() {
    struct ModuleInfo { std::string name; std::string type; std::string desc; };
    std::vector<ModuleInfo> mfgModules = {
        {"Fabrication", "FABRICATION", "Raw material processing and shaping."},
        {"Assembly", "ASSEMBLY", "Component assembly line."},
        {"Quality", "QUALITY", "Quality control and testing."},
        {"Logistics", "LOGISTICS", "Material handling and routing."},
        {"Automation", "AUTOMATION", "Automated production control."}
    };

    for (size_t i = 0; i < mfgModules.size(); i++)
    {
        UnitModule mod;
        mod.name = mfgModules[i].name;
        mod.moduleType = mfgModules[i].type;
        mod.tier = 0;
        mod.level = 1;
        mod.isBuilt = (i < 3);
        mod.isActive = (i < 3);
        mod.efficiency = parameters.count("ProductionEfficiency") ?
            parameters["ProductionEfficiency"] : 0.85f;
        mod.description = mfgModules[i].desc;

        if (i == 0)  // Fabrication consumes raw materials
        {
            mod.consumptionRates[ResourceType::Fe] = 2.0f;
            mod.consumptionRates[ResourceType::Si] = 1.0f;
            mod.consumptionRates[ResourceType::ENERGY] = 3.0f;
        }

        modules.push_back(mod);
        if (mod.isActive) activeModuleIndices.insert(static_cast<int>(i));
    }
}

void Unit::InitializeResearchModules() {
    struct ModuleInfo { std::string name; std::string type; std::string desc; };
    std::vector<ModuleInfo> resModules = {
        {"Laboratory", "LABORATORY", "Fundamental research facility."},
        {"Analysis", "ANALYSIS", "Data analysis and computation."},
        {"Simulation", "SIMULATION", "Numerical simulation systems."},
        {"Archive", "ARCHIVE", "Research data storage and retrieval."},
        {"Publication", "PUBLICATION", "Research output and knowledge sharing."}
    };

    for (size_t i = 0; i < resModules.size(); i++)
    {
        UnitModule mod;
        mod.name = resModules[i].name;
        mod.moduleType = resModules[i].type;
        mod.tier = 0;
        mod.level = 1;
        mod.isBuilt = (i < 3);
        mod.isActive = (i < 3);
        mod.efficiency = 1.0f;
        mod.description = resModules[i].desc;

        if (i == 0)  // Laboratory produces science
        {
            mod.maxProductionRates[ResourceType::SCIENCE] = parameters.count("ResearchPointsPerTick") ?
                parameters["ResearchPointsPerTick"] : 5.0f;
            mod.productionRates = mod.maxProductionRates;
            mod.consumptionRates[ResourceType::ENERGY] = parameters.count("EnergyConsumption") ?
                parameters["EnergyConsumption"] : 10.0f;
        }

        modules.push_back(mod);
        if (mod.isActive) activeModuleIndices.insert(static_cast<int>(i));
    }
}

void Unit::InitializeGenericModules() {
    UnitModule basicModule;
    basicModule.name = "Basic " + unit_type;
    basicModule.moduleType = "GENERIC";
    basicModule.tier = 0;
    basicModule.level = 1;
    basicModule.isBuilt = true;
    basicModule.isActive = true;
    basicModule.efficiency = parameters.count("Efficiency") ? parameters["Efficiency"] : 0.8f;
    basicModule.description = "Basic module for " + unit_type;

    modules.push_back(basicModule);
    activeModuleIndices.insert(0);
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

    // Recalculate consumption rates if this is an active module
    if (activeModuleIndices.count(moduleIndex) > 0) {
        CalculateConsumption();
    }

    ShowMessage(TextFormat("Module upgraded to level %d - Production set to maximum", module.level));
    return true;
}

void Unit::ProcessModuleEffects(float deltaTime, ResourceManager& resourceManager) {
    if (!IsActive() || activeModuleIndices.empty()) return;

    CalculateConsumption();  // Update consumption rates when modules are activated

    // Process each active module
    for (int moduleIndex : activeModuleIndices) {
        UnitModule& module = modules[moduleIndex];

        // Calculate efficiency based on resource availability (graceful degradation)
        float efficiencyMultiplier = 1.0f;

        for (const auto& [type, rate] : module.consumptionRates) {
            if (rate <= 0.0f) continue;

            float required = rate * deltaTime;
            float available = resourceStorage[type];

            if (available < required) {
                // Calculate what percentage we can operate at
                float resourceRatio = available / required;
                efficiencyMultiplier = std::min(efficiencyMultiplier, resourceRatio);
            }
        }

        // Apply minimum efficiency floor (50% when degraded, 0% if nothing available)
        if (efficiencyMultiplier < 1.0f && efficiencyMultiplier > 0.0f) {
            // Degraded mode: operate at reduced efficiency (minimum 50% of calculated)
            efficiencyMultiplier = std::max(0.5f * efficiencyMultiplier + 0.5f * 1.0f, efficiencyMultiplier);
            // This gives a smoother curve: 0% resources = 50% efficiency, 50% = 75%, 100% = 100%
        }

        if (efficiencyMultiplier <= 0.0f) {
            // No resources at all - skip this module
            continue;
        }

        // Consume resources proportional to efficiency
        for (const auto& [type, rate] : module.consumptionRates) {
            float consumption = rate * deltaTime * efficiencyMultiplier;
            resourceStorage[type] = std::max(0.0f, resourceStorage[type] - consumption);
        }

        // Handle production based on unit type (scaled by efficiency)
        if (unit_type == "Extraction") {
            ProcessExtraction(deltaTime * efficiencyMultiplier, resourceManager);
        }
        else {
            // Normal production for other unit types (scaled by efficiency)
            for (const auto& [type, rate] : module.productionRates) {
                AddResource(type, rate * deltaTime * efficiencyMultiplier);
            }
        }
    }
}

void Unit::ProcessExtraction(float deltaTime, ResourceManager& resourceManager) {
    // Use the first active module for extraction (in future, could have multiple extraction modules)
    if (activeModuleIndices.empty()) return;

    int firstActiveModule = *activeModuleIndices.begin();
    UnitModule& module = modules[firstActiveModule];

    Vector2 gridPos = WorldToGrid(parentSectPosition);
    int gridX = static_cast<int>(gridPos.x);
    int gridY = static_cast<int>(gridPos.y);

    // Get available resources at this location
    auto availableResources = resourceManager.GetResourcesAtGrid(gridX, gridY);

    int worldX = static_cast<int>(parentSectPosition.x);
    int worldY = static_cast<int>(parentSectPosition.y);

    float efficiency = module.efficiency;
    float levelMultiplier = 1.0f + (module.level - 1) * 0.2f;

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
//    std::cout << "\nExtraction rates for each resource:" << std::endl;
    for (const auto& [type, rate] : extractionRates) {
//        std::cout << "Resource " << static_cast<int>(type) << " rate: " << rate << std::endl;
    }

    // Process each available resource
    for (const auto& [resourceType, abundance] : availableResources) {

//        std::cout << "resourceType:" << resourceType << "\t abundance:" << abundance << std::endl;

//        if (abundance < 0.1f) {
//            std::cout << "Resource " << static_cast<int>(resourceType)
//                     << " abundance too low: " << abundance << std::endl;
//           continue;
//        }

        // Get the specific extraction rate for this resource type
        float baseRate = extractionRates[resourceType];
        float extractionAmount = baseRate * efficiency * levelMultiplier * abundance * deltaTime;

        // Deplete the resource from the planet
        resourceManager.UpdateResourceDepletion(gridX, gridY, resourceType, extractionAmount);

        // Add the extracted resource to storage
        AddResource(resourceType, extractionAmount);


        // Debug print for all resources
//        std::cout << "Resource " << static_cast<int>(resourceType)
//                 << " - Extracted: " << extractionAmount
//                 << " (Rate: " << baseRate
//                 << ", Efficiency: " << efficiency
//                 << ", Level Mult: " << levelMultiplier
//                 << ", Abundance: " << abundance << ")" << std::endl;

        // Debug print for Fe
//        if (resourceType == ResourceType::Fe) {
//            std::cout << "Fe Extraction - Amount: " << extractionAmount
//                     << ", Current Storage: " << resourceStorage[ResourceType::Fe] << std::endl;
//        }
    }
}

// Add getters/setters for resource storage
float Unit::GetStoredResource(ResourceType type) const {
    auto it = resourceStorage.find(type);
    return it != resourceStorage.end() ? it->second : 0.0f;
}

void Unit::AddResource(ResourceType type, float amount) {
    // Check if storage has capacity for this resource
    auto capacityIt = storageCapacity.find(type);
    if (capacityIt == storageCapacity.end()) {
        // No capacity limit defined, add directly (shouldn't happen)
        resourceStorage[type] += amount;
        return;
    }

    float currentStorage = resourceStorage[type];
    float maxCapacity = capacityIt->second;
    float availableSpace = maxCapacity - currentStorage;

    if (availableSpace >= amount) {
        // Enough space, add all
        resourceStorage[type] += amount;
    } else if (availableSpace > 0.0f) {
        // Partial space available
        resourceStorage[type] += availableSpace;
        float overflow = amount - availableSpace;

        // Store overflow in buffer
        overflowBuffer[type] += overflow;

        std::cout << "Storage full! Buffered " << overflow << " of resource "
                 << static_cast<int>(type) << " (Total buffer: "
                 << overflowBuffer[type] << ")" << std::endl;
    } else {
        // No space available, all goes to buffer
        overflowBuffer[type] += amount;

        std::cout << "Storage completely full! Buffered " << amount << " of resource "
                 << static_cast<int>(type) << " (Total buffer: "
                 << overflowBuffer[type] << ")" << std::endl;
    }
}

bool Unit::ConsumeResource(ResourceType type, float amount) {
    if (resourceStorage[type] >= amount) {
        resourceStorage[type] -= amount;
        return true;
    }
    return false;
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

bool Unit::ActivateModule(int moduleIndex) {
    // Validate module index
    if (moduleIndex < 0 || moduleIndex >= modules.size()) {
        std::cout << "ERROR: Invalid module index " << moduleIndex << std::endl;
        return false;
    }

    UnitModule& module = modules[moduleIndex];

    // Check if module is built
    if (!module.isBuilt) {
        std::cout << "Cannot activate unbuilt module: " << module.name << std::endl;
        return false;
    }

    // Check if already active
    if (activeModuleIndices.count(moduleIndex) > 0) {
        std::cout << "Module " << module.name << " is already active" << std::endl;
        return false;
    }

    // Activate the module
    module.isActive = true;
    activeModuleIndices.insert(moduleIndex);

    // Update unit status
    UpdateUnitStatus();

    // Recalculate consumption
    CalculateConsumption();

    std::cout << "Activated module: " << module.name << " (index " << moduleIndex << ")" << std::endl;
    return true;
}

bool Unit::DeactivateModule(int moduleIndex) {
    // Validate module index
    if (moduleIndex < 0 || moduleIndex >= modules.size()) {
        std::cout << "ERROR: Invalid module index " << moduleIndex << std::endl;
        return false;
    }

    // Check if module is active
    if (activeModuleIndices.count(moduleIndex) == 0) {
        std::cout << "Module is not currently active" << std::endl;
        return false;
    }

    UnitModule& module = modules[moduleIndex];

    // Deactivate the module
    module.isActive = false;
    activeModuleIndices.erase(moduleIndex);

    // Update unit status
    UpdateUnitStatus();

    // Recalculate consumption
    if (!activeModuleIndices.empty()) {
        CalculateConsumption();
    }

    std::cout << "Deactivated module: " << module.name << " (index " << moduleIndex << ")" << std::endl;
    return true;
}
