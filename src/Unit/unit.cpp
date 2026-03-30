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

    // Update scan cooldown
    if (scanCooldown > 0.0f)
    {
        scanCooldown -= deltaTime;
        if (scanCooldown < 0.0f) scanCooldown = 0.0f;
    }

    // Update calibration timer
    if (isCalibrating)
    {
        calibrationTimer -= deltaTime;
        if (calibrationTimer <= 0.0f)
        {
            isCalibrating = false;
            calibrationTimer = 0.0f;
            calibrationQuality = 1.0f;
            ShowMessage("Calibration complete.");
            std::cout << "[PROSPECTING] Calibration complete. Quality restored to 100%." << std::endl;
        }
    }

    // --- Campaign auto-advance ---
    if (campaignActive && scanCooldown <= 0.0f && !isCalibrating &&
        campaignCurrentIndex < static_cast<int>(scanCampaign.size()))
    {
        auto& entry = scanCampaign[campaignCurrentIndex];
        if (!entry.completed)
        {
            // Set profile for this entry
            if (entry.profileIndex >= 0 && entry.profileIndex < static_cast<int>(availableProfiles.size()))
            {
                activeScanProfileIndex = entry.profileIndex;
                activeScanProfile = availableProfiles[entry.profileIndex];
            }
            PerformLIBSScan(entry.gridX, entry.gridY);
            entry.completed = true;
        }
        campaignCurrentIndex++;

        // Check if campaign is complete
        if (campaignCurrentIndex >= static_cast<int>(scanCampaign.size()))
        {
            campaignActive = false;
            campaignConfidenceBonus += CAMPAIGN_COMPLETION_CONFIDENCE;
            ShowMessage("Campaign complete! +5% confidence bonus.");
            std::cout << "[PROSPECTING] Campaign completed. Confidence bonus: "
                      << static_cast<int>(campaignConfidenceBonus * 100.0f) << "%" << std::endl;
        }
    }

    // --- Objective bonus expiry ---
    if (objectiveBonusExpiry > 0.0f)
    {
        objectiveBonusExpiry -= deltaTime;
        if (objectiveBonusExpiry <= 0.0f)
        {
            objectiveBonusMultiplier = 1.0f;
            objectiveBonusExpiry = 0.0f;
        }
    }

    // --- AI auto-management ---
    if (unit_type == "Extraction")
    {
        UpdateProspectingAI(deltaTime);
    }

    // Update excavator wear
    for (auto& exc : excavators)
    {
        if (exc.rate > 0.0f)
        {
            exc.wear += deltaTime * 0.001f * exc.rate / 30.0f;
            exc.wear = std::min(exc.wear, 1.0f);
        }
    }

    // Flush overflow buffer into sect storage
    for (auto& [type, buffered] : overflowBuffer)
    {
        if (buffered <= 0.0f) continue;

        auto capacityIt = storageCapacity.find(type);
        if (capacityIt == storageCapacity.end()) continue;

        float available = capacityIt->second - resourceStorage[type];
        if (available > 0.0f)
        {
            float transfer = std::min(buffered, available);
            resourceStorage[type] += transfer;
            buffered -= transfer;
        }

        // Cap overflow buffer to prevent unbounded growth
        const float OVERFLOW_BUFFER_CAP = 200.0f;
        if (buffered > OVERFLOW_BUFFER_CAP)
        {
            buffered = OVERFLOW_BUFFER_CAP;
        }
    }
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

    // Calculate water needed and check availability (graceful degradation)
    float waterNeeded = waterConsumption * deltaTime;
    float waterAvailable = resourceStorage[ResourceType::WATER];
    float efficiencyMultiplier = 1.0f;

    if (waterAvailable < waterNeeded)
    {
        if (waterAvailable <= 0.0f)
        {
            return;  // No water at all — cannot farm
        }
        float ratio = waterAvailable / waterNeeded;
        efficiencyMultiplier = std::max(0.5f*ratio + 0.5f, ratio);
    }

    // Consume water proportional to efficiency
    resourceStorage[ResourceType::WATER] = std::max(0.0f,
        resourceStorage[ResourceType::WATER] - waterNeeded*efficiencyMultiplier);

    // Calculate and deposit food production
    float foodProduced = productionRate * fertility * growthBoost * deltaTime * efficiencyMultiplier;
    AddResource(ResourceType::FOOD, foodProduced);

    // Reduce fertility over time (soil degradation)
    parameters["FertilityLevel"] = std::max(0.2f, fertility - (0.01f * deltaTime));
}

void Unit::ProcessEnergy(float deltaTime) {
    if (!IsActive()) return;

    // Get relevant parameters
    float energyOutput = parameters["EnergyOutput"];
    float efficiency = parameters["Efficiency"];
    float weatherImpact = parameters["WeatherImpact"];
    float fuelConsumption = parameters["FuelConsumption"];

    // Check fuel availability (graceful degradation)
    float efficiencyMultiplier = 1.0f;
    float fuelNeeded = fuelConsumption * deltaTime;

    if (fuelNeeded > 0.0f)
    {
        float fuelAvailable = resourceStorage[ResourceType::H2];
        if (fuelAvailable < fuelNeeded)
        {
            if (fuelAvailable <= 0.0f)
            {
                return;  // No fuel — cannot produce energy
            }
            float ratio = fuelAvailable / fuelNeeded;
            efficiencyMultiplier = std::max(0.5f*ratio + 0.5f, ratio);
        }

        // Consume fuel proportional to efficiency
        resourceStorage[ResourceType::H2] = std::max(0.0f,
            resourceStorage[ResourceType::H2] - fuelNeeded*efficiencyMultiplier);
    }

    // Calculate and deposit energy production
    float energyProduced = energyOutput * efficiency * (1.0f + weatherImpact) * deltaTime * efficiencyMultiplier;
    AddResource(ResourceType::ENERGY, energyProduced);

    // Apply maintenance degradation
    float maintenanceCost = parameters["MaintenanceCost"];
    parameters["Efficiency"] = std::max(0.2f, efficiency - (maintenanceCost * deltaTime));
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

void Unit::InitializeScanProfiles() {
    availableProfiles.clear();

    // Find prospecting tier
    int prospectingTier = 0;
    for (const auto& mod : modules)
    {
        if (mod.moduleType == "PROSPECTING")
        {
            prospectingTier = mod.tier;
            break;
        }
    }

    if (prospectingTier == 0)
    {
        // Tier 0: Visual only
        availableProfiles.push_back({"Visual", 0.3f, 1, 5.0f, 10.0f, 0.8f});
        activeScanProfileIndex = 0;
    }
    else
    {
        // T1+: Quick, Standard, Deep
        availableProfiles.push_back({"Quick", 0.5f, 5, 2.0f, 20.0f, 0.6f});
        availableProfiles.push_back({"Standard", 1.0f, 15, 3.0f, 50.0f, 1.0f});
        availableProfiles.push_back({"Deep", 2.0f, 30, 8.0f, 100.0f, 1.5f});
        activeScanProfileIndex = 1;  // Default to Standard
    }

    activeScanProfile = availableProfiles[activeScanProfileIndex];
}

void Unit::SetActiveScanProfile(int index) {
    if (index >= 0 && index < static_cast<int>(availableProfiles.size()))
    {
        activeScanProfileIndex = index;
        activeScanProfile = availableProfiles[index];
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

    // Initialize scan profiles based on prospecting tier
    InitializeScanProfiles();

    // Generate initial objectives (1 for T1, 2 for T2, 3 for T3)
    GenerateObjectives();
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

bool Unit::UpgradeModuleTier(int moduleIndex) {
    if (moduleIndex < 0 || moduleIndex >= static_cast<int>(modules.size()))
    {
        return false;
    }

    UnitModule& module = modules[moduleIndex];

    if (module.tier >= 3)
    {
        ShowMessage("Module already at maximum tier (3).");
        return false;
    }

    // Check tech dependencies for next tier
    auto& registry = UnlockRegistry::Instance();
    for (const auto& dep : module.tierDependencies)
    {
        if (!registry.IsUnlocked(dep))
        {
            ShowMessage(TextFormat("Requires tech: %s", dep.c_str()));
            std::cout << "[TIER UPGRADE] Module " << module.name
                      << " requires tech: " << dep << std::endl;
            return false;
        }
    }

    // Check upgrade resource costs (use tier+1 as key in upgradeCosts)
    int nextTier = module.tier + 1;
    if (module.upgradeCosts.count(nextTier) > 0)
    {
        const auto& costs = module.upgradeCosts[nextTier];
        for (const auto& [resource, cost] : costs)
        {
            if (resourceStorage[resource] < cost)
            {
                ShowMessage(TextFormat("Not enough %s for tier upgrade.",
                            ResourceTypeToString(resource)));
                return false;
            }
        }

        // Consume upgrade costs
        for (const auto& [resource, cost] : costs)
        {
            ConsumeResource(resource, cost);
        }
    }

    // Perform the upgrade
    module.tier = nextTier;

    // Scale efficiency and rates by tier
    static const float tierMults[] = {1.0f, 1.4f, 1.9f, 2.5f};
    float tierMultiplier = tierMults[std::min(module.tier, 3)];
    module.efficiency = std::min(1.0f, 0.5f + module.tier * 0.18f);

    // Scale production rates
    for (auto& [type, rate] : module.maxProductionRates)
    {
        rate *= tierMultiplier / tierMults[std::min(module.tier - 1, 3)];  // Incremental increase
    }
    module.productionRates = module.maxProductionRates;

    // Update description based on tier
    if (module.moduleType == "PROSPECTING")
    {
        const char* tierDescs[] = {
            "Visual estimation. ~40% accuracy.",
            "LIBS Scanner. Click to scan cells.",
            "Multi-Spectral Suite. LIBS + Raman + Neutron.",
            "Deep Survey Array. GPR depth profiling."
        };
        module.description = tierDescs[module.tier];
    }
    else if (module.moduleType == "EXCAVATION")
    {
        const char* tierDescs[] = {
            "Manual scoop. 1 excavator, 10cm depth.",
            "Mechanized. 2 excavators, bucket wheel.",
            "Heavy equipment. 4 excavators, percussive.",
            "Autonomous fleet. 8 drones, zone painting."
        };
        module.description = tierDescs[module.tier];
    }
    else if (module.moduleType == "BENEFICIATION")
    {
        const char* tierDescs[] = {
            "Direct output. Raw regolith passthrough.",
            "Basic separation. Size sort + magnetic. ~60% Fe.",
            "Processing chain. Drag-reorder nodes.",
            "Refinery complex. MRE + purity control."
        };
        module.description = tierDescs[module.tier];

        // Upgrade separation chain based on tier
        separationChain.clear();
        if (module.tier == 0)
        {
            separationChain.push_back(SeparationNodes::CreateDirectOutput());
        }
        else if (module.tier == 1)
        {
            separationChain.push_back(SeparationNodes::CreateSizeSort());
            separationChain.push_back(SeparationNodes::CreateMagnetic());
        }
        else if (module.tier == 2)
        {
            separationChain.push_back(SeparationNodes::CreateSizeSort());
            separationChain.push_back(SeparationNodes::CreateMagnetic());
            separationChain.push_back(SeparationNodes::CreateElectrostatic());
            separationChain.push_back(SeparationNodes::CreateThermal());
        }
        else if (module.tier == 3)
        {
            separationChain.push_back(SeparationNodes::CreateSizeSort());
            separationChain.push_back(SeparationNodes::CreateMagnetic());
            separationChain.push_back(SeparationNodes::CreateElectrostatic());
            separationChain.push_back(SeparationNodes::CreateThermal());
            separationChain.push_back(SeparationNodes::CreateMRE());
        }
    }
    else if (module.moduleType == "EXCAVATION")
    {
        // Add excavators based on tier
        int targetCount = 1;
        if (module.tier == 1) targetCount = 2;
        else if (module.tier == 2) targetCount = 4;
        else if (module.tier == 3) targetCount = 8;

        while (static_cast<int>(excavators.size()) < targetCount)
        {
            Excavator exc;
            exc.id = static_cast<int>(excavators.size());
            exc.gridPos = WorldToGrid(parentSectPosition);
            exc.method = module.tier >= 3 ? "drone" :
                         module.tier >= 2 ? "percussive" :
                         module.tier >= 1 ? "bucket_wheel" : "scoop";
            exc.depth = 0.0f;
            exc.rate = 30.0f * tierMults[std::min(module.tier, 3)];
            exc.wear = 0.0f;
            excavators.push_back(exc);
        }
    }

    // Update tier dependencies for the NEXT tier
    if (module.moduleType == "PROSPECTING")
    {
        std::vector<std::vector<std::string>> deps = {
            {"Spectroscopy"}, {"Geophysics"}, {"SwarmAI"}, {}
        };
        if (module.tier < 3) module.tierDependencies = deps[module.tier];
        else module.tierDependencies.clear();
    }
    else if (module.moduleType == "EXCAVATION")
    {
        std::vector<std::vector<std::string>> deps = {
            {"MechanizedDrilling"}, {"HeavyEquipment"}, {"AutonomousFleet"}, {}
        };
        if (module.tier < 3) module.tierDependencies = deps[module.tier];
        else module.tierDependencies.clear();
    }
    else if (module.moduleType == "BENEFICIATION")
    {
        std::vector<std::vector<std::string>> deps = {
            {"MagneticSeparation"}, {"ProcessingChain"}, {"RefineryComplex"}, {}
        };
        if (module.tier < 3) module.tierDependencies = deps[module.tier];
        else module.tierDependencies.clear();
    }

    // Recalculate consumption
    if (activeModuleIndices.count(moduleIndex) > 0)
    {
        CalculateConsumption();
    }

    // Reinitialize scan profiles and objectives when prospecting module upgrades
    if (module.moduleType == "PROSPECTING")
    {
        InitializeScanProfiles();
        GenerateObjectives();
    }

    ShowMessage(TextFormat("%s upgraded to Tier %d", module.name.c_str(), module.tier));
    std::cout << "[TIER UPGRADE] " << module.name << " -> Tier " << module.tier << std::endl;
    return true;
}

bool Unit::DebugUpgradeModuleTier(int moduleIndex)
{
    if (moduleIndex < 0 || moduleIndex >= static_cast<int>(modules.size()))
    {
        std::cout << "[DEBUG] Invalid module index: " << moduleIndex << std::endl;
        return false;
    }

    UnitModule& module = modules[moduleIndex];

    if (module.tier >= 3)
    {
        std::cout << "[DEBUG] " << module.name << " already at max tier (3)." << std::endl;
        ShowMessage("Module already at maximum tier (3).");
        return false;
    }

    // Perform the upgrade (same logic as UpgradeModuleTier, skipping tech/cost checks)
    int nextTier = module.tier + 1;
    module.tier = nextTier;

    static const float tierMults[] = {1.0f, 1.4f, 1.9f, 2.5f};
    float tierMultiplier = tierMults[std::min(module.tier, 3)];
    module.efficiency = std::min(1.0f, 0.5f + module.tier * 0.18f);

    for (auto& [type, rate] : module.maxProductionRates)
    {
        rate *= tierMultiplier / tierMults[std::min(module.tier - 1, 3)];
    }
    module.productionRates = module.maxProductionRates;

    if (module.moduleType == "PROSPECTING")
    {
        const char* tierDescs[] = {
            "Visual estimation. ~40% accuracy.",
            "LIBS Scanner. Click to scan cells.",
            "Multi-Spectral Suite. LIBS + Raman + Neutron.",
            "Deep Survey Array. GPR depth profiling."
        };
        module.description = tierDescs[module.tier];
    }
    else if (module.moduleType == "EXCAVATION")
    {
        const char* tierDescs[] = {
            "Manual scoop. 1 excavator, 10cm depth.",
            "Mechanized. 2 excavators, bucket wheel.",
            "Heavy equipment. 4 excavators, percussive.",
            "Autonomous fleet. 8 drones, zone painting."
        };
        module.description = tierDescs[module.tier];
    }
    else if (module.moduleType == "BENEFICIATION")
    {
        const char* tierDescs[] = {
            "Direct output. Raw regolith passthrough.",
            "Basic separation. Size sort + magnetic. ~60% Fe.",
            "Processing chain. Drag-reorder nodes.",
            "Refinery complex. MRE + purity control."
        };
        module.description = tierDescs[module.tier];

        separationChain.clear();
        if (module.tier == 0)
        {
            separationChain.push_back(SeparationNodes::CreateDirectOutput());
        }
        else if (module.tier == 1)
        {
            separationChain.push_back(SeparationNodes::CreateSizeSort());
            separationChain.push_back(SeparationNodes::CreateMagnetic());
        }
        else if (module.tier == 2)
        {
            separationChain.push_back(SeparationNodes::CreateSizeSort());
            separationChain.push_back(SeparationNodes::CreateMagnetic());
            separationChain.push_back(SeparationNodes::CreateElectrostatic());
            separationChain.push_back(SeparationNodes::CreateThermal());
        }
        else if (module.tier == 3)
        {
            separationChain.push_back(SeparationNodes::CreateSizeSort());
            separationChain.push_back(SeparationNodes::CreateMagnetic());
            separationChain.push_back(SeparationNodes::CreateElectrostatic());
            separationChain.push_back(SeparationNodes::CreateThermal());
            separationChain.push_back(SeparationNodes::CreateMRE());
        }
    }
    else if (module.moduleType == "EXCAVATION")
    {
        int targetCount = 1;
        if (module.tier == 1) targetCount = 2;
        else if (module.tier == 2) targetCount = 4;
        else if (module.tier == 3) targetCount = 8;

        while (static_cast<int>(excavators.size()) < targetCount)
        {
            Excavator exc;
            exc.id = static_cast<int>(excavators.size());
            exc.gridPos = WorldToGrid(parentSectPosition);
            exc.method = module.tier >= 3 ? "drone" :
                         module.tier >= 2 ? "percussive" :
                         module.tier >= 1 ? "bucket_wheel" : "scoop";
            exc.depth = 0.0f;
            exc.rate = 30.0f * tierMults[std::min(module.tier, 3)];
            exc.wear = 0.0f;
            excavators.push_back(exc);
        }
    }

    if (module.moduleType == "PROSPECTING")
    {
        std::vector<std::vector<std::string>> deps = {
            {"Spectroscopy"}, {"Geophysics"}, {"SwarmAI"}, {}
        };
        if (module.tier < 3) module.tierDependencies = deps[module.tier];
        else module.tierDependencies.clear();
    }
    else if (module.moduleType == "EXCAVATION")
    {
        std::vector<std::vector<std::string>> deps = {
            {"MechanizedDrilling"}, {"HeavyEquipment"}, {"AutonomousFleet"}, {}
        };
        if (module.tier < 3) module.tierDependencies = deps[module.tier];
        else module.tierDependencies.clear();
    }
    else if (module.moduleType == "BENEFICIATION")
    {
        std::vector<std::vector<std::string>> deps = {
            {"MagneticSeparation"}, {"ProcessingChain"}, {"RefineryComplex"}, {}
        };
        if (module.tier < 3) module.tierDependencies = deps[module.tier];
        else module.tierDependencies.clear();
    }

    if (activeModuleIndices.count(moduleIndex) > 0)
    {
        CalculateConsumption();
    }

    // Reinitialize scan profiles and objectives when prospecting module upgrades
    if (module.moduleType == "PROSPECTING")
    {
        InitializeScanProfiles();
        GenerateObjectives();
    }

    ShowMessage(TextFormat("[DEBUG] %s force-upgraded to Tier %d", module.name.c_str(), module.tier));
    std::cout << "[DEBUG] Force upgraded " << module.name << " to tier " << module.tier << std::endl;
    return true;
}

void Unit::ProcessModuleEffects(float deltaTime, ResourceManager& resourceManager) {
    if (!IsActive() || activeModuleIndices.empty()) return;

    CalculateConsumption();  // Update consumption rates when modules are activated

    // --- Dynamic energy consumption for extraction modules ---
    if (unit_type == "Extraction")
    {
        for (int idx : activeModuleIndices)
        {
            UnitModule& mod = modules[idx];

            if (mod.moduleType == "PROSPECTING")
            {
                mod.consumptionRates[ResourceType::ENERGY] = 0.2f;
            }
            else if (mod.moduleType == "EXCAVATION")
            {
                int activeExcavators = 0;
                for (const auto& exc : excavators)
                {
                    if (exc.wear < 1.0f) activeExcavators++;
                }
                static const float tierEnergyCost[] = {0.8f, 1.0f, 1.2f, 1.5f};
                float excavationEnergy = tierEnergyCost[std::min(mod.tier, 3)] *
                                          std::max(1, activeExcavators);
                mod.consumptionRates[ResourceType::ENERGY] = excavationEnergy;
            }
            else if (mod.moduleType == "BENEFICIATION")
            {
                float benefEnergy = 0.3f;  // Base overhead
                for (const auto& node : separationChain)
                {
                    if (node.isActive)
                    {
                        benefEnergy += node.energyConsumption * 0.01f;
                    }
                }
                mod.consumptionRates[ResourceType::ENERGY] = benefEnergy;
            }
            else if (mod.moduleType == "OPERATIONS")
            {
                static const float opsEnergy[] = {0.1f, 0.2f, 0.35f, 0.5f};
                mod.consumptionRates[ResourceType::ENERGY] = opsEnergy[std::min(mod.tier, 3)];
            }
            else if (mod.moduleType == "DIRECTIVES")
            {
                float directiveEnergy = 0.15f;  // Base
                if (activeDirective.type == DirectiveType::MAXIMIZE)
                    directiveEnergy += 0.5f;
                else if (activeDirective.type == DirectiveType::PRIORITIZE)
                    directiveEnergy += 0.3f;
                else if (activeDirective.type == DirectiveType::CONSERVE)
                    directiveEnergy -= 0.1f;
                mod.consumptionRates[ResourceType::ENERGY] = directiveEnergy;
            }
        }
    }

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
    if (activeModuleIndices.empty()) return;

    Vector2 gridPos = WorldToGrid(parentSectPosition);
    int gridX = static_cast<int>(gridPos.x);
    int gridY = static_cast<int>(gridPos.y);

    // --- Determine depth layer from first excavator ---
    DepthLayer activeLayer = DepthLayer::SURFACE;
    if (!excavators.empty())
    {
        float depth = excavators[0].depth;
        if (depth >= 100.0f) activeLayer = DepthLayer::DEEP;
        else if (depth >= 30.0f) activeLayer = DepthLayer::MID;
        else if (depth >= 10.0f) activeLayer = DepthLayer::SHALLOW;
    }

    // Get available resources at this location and depth layer
    auto availableResources = resourceManager.GetResourcesAtGridLayer(gridX, gridY, activeLayer);

    // --- Survey-gated extraction efficiency ---
    float scanMultiplier = SURVEY_UNSCANNED_EFFICIENCY;  // Unscanned: 35% efficiency
    auto scanIt = scanHistory.find({gridX, gridY});
    if (scanIt != scanHistory.end() && scanIt->second.isScanned)
    {
        float survey = scanIt->second.surveyProgress;
        // Backward compat: migrate old scans that have scanCount but no surveyProgress
        if (survey <= 0.0f && scanIt->second.scanCount > 0)
        {
            survey = std::min(1.0f, static_cast<float>(scanIt->second.scanCount) / 3.0f);
        }
        scanMultiplier = SURVEY_UNSCANNED_EFFICIENCY + SURVEY_SCANNED_BONUS * survey;

        // Check if site is marked for excavation: +15% bonus (additive)
        for (const auto& site : markedSites)
        {
            if (site.first == gridX && site.second == gridY)
            {
                scanMultiplier += SURVEY_MARKED_SITE_BONUS;
                break;
            }
        }
    }

    // Apply objective bonus multiplier
    scanMultiplier *= objectiveBonusMultiplier;

    // --- Apply Operations efficiency modifier ---
    float opsModifier = GetOperationsEfficiencyModifier();

    // --- Apply Directive modifier ---
    float directiveModifier = 1.0f;
    ResourceType prioritizedResource = ResourceType::Fe;
    if (activeDirective.type == DirectiveType::MAXIMIZE)
    {
        directiveModifier = 1.25f;  // +25% output, but more energy
    }
    else if (activeDirective.type == DirectiveType::CONSERVE)
    {
        directiveModifier = 0.7f;  // -30% output, less energy
    }
    else if (activeDirective.type == DirectiveType::EMERGENCY_HARVEST)
    {
        directiveModifier = 1.5f;  // +50% output at wear cost
        for (auto& exc : excavators)
        {
            exc.wear += deltaTime * 0.005f;
        }
    }
    else if (activeDirective.type == DirectiveType::EXPLORATION_MODE)
    {
        directiveModifier = 0.5f;  // -50% extraction
    }
    else if (activeDirective.type == DirectiveType::THERMAL_SYNC)
    {
        float dayFraction = fmodf(timeManager.GetTicks() /
                                  static_cast<float>(TICKS_PER_DAY), 1.0f);
        float thermalBonus = 0.9f + 0.3f * sinf(dayFraction * 2.0f * PI);
        directiveModifier = thermalBonus;
    }
    else if (activeDirective.type == DirectiveType::PRIORITIZE)
    {
        prioritizedResource = activeDirective.targetResource;
    }

    // --- Find Excavation module for extraction rates ---
    UnitModule* excavationMod = nullptr;
    for (auto& mod : modules)
    {
        if (mod.moduleType == "EXCAVATION" && mod.isActive)
        {
            excavationMod = &mod;
            break;
        }
    }

    if (!excavationMod) return;

    float efficiency = excavationMod->efficiency;
    static const float tierMults[] = {1.0f, 1.4f, 1.9f, 2.5f};
    float tierMultiplier = tierMults[std::min(excavationMod->tier, 3)];

    // Map for base extraction rates
    std::map<ResourceType, float> extractionRates = {
        {ResourceType::H2, parameters["H2ExtractionRate"]},
        {ResourceType::O2, parameters["O2ExtractionRate"]},
        {ResourceType::C,  parameters["CExtractionRate"]},
        {ResourceType::Fe, parameters["FeExtractionRate"]},
        {ResourceType::Si, parameters["SiExtractionRate"]}
    };

    // --- Stage 1: Excavation (raw regolith) ---
    std::map<ResourceType, float> rawRegolith;

    for (const auto& [resourceType, abundance] : availableResources)
    {
        float baseRate = extractionRates.count(resourceType) ?
            extractionRates[resourceType] : 0.01f;

        // Apply directive priority boost
        float priorityBoost = 1.0f;
        if (activeDirective.type == DirectiveType::PRIORITIZE &&
            resourceType == prioritizedResource)
        {
            priorityBoost = 1.4f;  // +40% for prioritized resource
        }

        float extractionAmount = baseRate * efficiency * tierMultiplier *
                                  abundance * opsModifier * directiveModifier *
                                  scanMultiplier * priorityBoost * deltaTime;

        // Scale by number of active excavators
        int activeExcavators = 0;
        for (const auto& exc : excavators)
        {
            if (exc.wear < 1.0f) activeExcavators++;
        }
        extractionAmount *= std::max(1, activeExcavators);

        // Deplete from planet
        resourceManager.UpdateResourceDepletion(gridX, gridY, resourceType, extractionAmount);

        rawRegolith[resourceType] = extractionAmount;
    }

    // --- Stage 2: Beneficiation (separation chain) ---
    std::map<ResourceType, float> processedOutput = rawRegolith;

    // Find beneficiation module efficiency
    float beneficiationEfficiency = 0.5f;
    for (const auto& mod : modules)
    {
        if (mod.moduleType == "BENEFICIATION" && mod.isActive)
        {
            beneficiationEfficiency = mod.efficiency;
            break;
        }
    }

    // Process through each node in the separation chain
    for (const auto& node : separationChain)
    {
        if (!node.isActive) continue;

        auto nodeOutput = node.Process(processedOutput, 1.0f);

        // Merge node output back (replace values for processed types)
        for (const auto& [type, amount] : nodeOutput)
        {
            processedOutput[type] = amount;  // No per-node efficiency multiply
        }
    }

    // Apply beneficiation module efficiency once after all nodes
    for (auto& [type, amount] : processedOutput)
    {
        amount *= beneficiationEfficiency;
    }

    // --- Stage 3: Add to storage ---
    for (const auto& [resourceType, amount] : processedOutput)
    {
        if (amount > 0.0f)
        {
            AddResource(resourceType, amount);
        }
    }

    // Track total extracted
    for (const auto& [type, amount] : rawRegolith)
    {
        totalRegolithExtracted += amount;
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

        // std::cout << "Storage full! Buffered " << overflow << " of resource "
        //          << static_cast<int>(type) << " (Total buffer: "
        //          << overflowBuffer[type] << ")" << std::endl;
    } else {
        // No space available, all goes to buffer
        overflowBuffer[type] += amount;

        // std::cout << "Storage completely full! Buffered " << amount << " of resource "
        //          << static_cast<int>(type) << " (Total buffer: "
        //          << overflowBuffer[type] << ")" << std::endl;
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

// --- Prospecting Methods ---

void Unit::PerformLIBSScan(int gridX, int gridY) {
    // Find prospecting module — Tier 0+ can scan
    int prospectingTier = -1;
    for (const auto& mod : modules)
    {
        if (mod.moduleType == "PROSPECTING" && mod.isActive)
        {
            prospectingTier = mod.tier;
            break;
        }
    }

    if (prospectingTier < 0)
    {
        ShowMessage("Prospecting module not active.");
        return;
    }

    if (isCalibrating)
    {
        ShowMessage("Calibrating... scanning blocked.");
        return;
    }

    if (scanCooldown > 0.0f)
    {
        ShowMessage("Scanner on cooldown...");
        return;
    }

    // --- AI auto-select profile ---
    if (prospectingTier >= 1 && prospectingAI.autoSelectProfile)
    {
        auto existingScan = scanHistory.find({gridX, gridY});
        bool alreadyScanned = (existingScan != scanHistory.end() && existingScan->second.isScanned);
        bool isMarked = false;
        for (const auto& site : markedSites)
        {
            if (site.first == gridX && site.second == gridY)
            {
                isMarked = true;
                break;
            }
        }

        int aiProfileIdx = 1;  // Default Standard
        if (!alreadyScanned)
        {
            aiProfileIdx = 0;  // Quick for first scan
            aiLastAction = "AI: Quick scan (first pass)";
        }
        else if (isMarked && existingScan->second.surveyProgress < 0.8f)
        {
            aiProfileIdx = 2;  // Deep for marked sites needing more survey
            aiLastAction = "AI: Deep scan (marked site, low survey)";
        }
        else if (alreadyScanned && existingScan->second.surveyProgress < 0.5f)
        {
            aiProfileIdx = 1;  // Standard for low survey progress
            aiLastAction = "AI: Standard scan (improve survey)";
        }
        else
        {
            aiProfileIdx = 0;  // Quick for already-good data
            aiLastAction = "AI: Quick scan (good coverage)";
        }

        if (aiProfileIdx < static_cast<int>(availableProfiles.size()))
        {
            activeScanProfileIndex = aiProfileIdx;
            activeScanProfile = availableProfiles[aiProfileIdx];
        }
    }

    // --- Determine profile to use ---
    const ScanProfile& profile = (prospectingTier == 0) ?
        availableProfiles[0] : activeScanProfile;

    // Get accurate resource data from ResourceManager
    auto resources = resourceManager.GetResourcesAtGrid(gridX, gridY);

    // Check for existing scan (confidence accumulation)
    auto existingIt = scanHistory.find({gridX, gridY});
    bool isRescan = (existingIt != scanHistory.end() && existingIt->second.isScanned);
    int oldScanCount = isRescan ? existingIt->second.scanCount : 0;
    int newScanCount = oldScanCount + 1;

    // --- Calculate noise ---
    // Base tier noise: T0=40%, T1=15%, T2=5%, T3=0%
    float baseTierNoise[] = {0.40f, 0.15f, 0.05f, 0.0f};
    float tierNoise = baseTierNoise[std::min(prospectingTier, 3)];

    // Profile noise multiplier: 1.0 / (power * sqrt(pulses/15))
    float profileNoiseMult = 1.0f;
    if (prospectingTier >= 1)
    {
        float pulseFactor = std::sqrt(static_cast<float>(profile.pulseCount) / 15.0f);
        profileNoiseMult = 1.0f / (profile.powerMultiplier * pulseFactor);
    }

    // Calibration effect: noise * (2.0 - calibrationQuality)
    float calibrationNoiseMult = (prospectingTier >= 1) ? (2.0f - calibrationQuality) : 1.0f;

    // Confidence accumulation: effective noise / sqrt(scanCount)
    float accumulationMult = 1.0f / std::sqrt(static_cast<float>(newScanCount));

    float effectiveNoise = tierNoise * profileNoiseMult * calibrationNoiseMult * accumulationMult;

    ScanResult result;
    if (isRescan)
    {
        result = existingIt->second;  // Start from existing data
    }
    result.isScanned = true;
    result.scanTier = std::max(result.scanTier, prospectingTier);  // Keep highest tier
    result.scanCount = newScanCount;
    result.scanProfileIndex = activeScanProfileIndex;

    // Quality rating (always computed, all tiers)
    float totalValue = 0.0f;
    for (const auto& [type, val] : resources)
    {
        totalValue += val;
    }
    result.qualityRating = std::min(5, static_cast<int>(totalValue / 2000.0f));

    if (prospectingTier == 0)
    {
        // Tier 0: Visual estimation — categories only, no numeric data
        for (const auto& [type, abundance] : resources)
        {
            const char* cat = abundance > 3000.0f ? "HIGH" :
                              abundance > 500.0f ? "MED" : "LOW";
            result.categories[type] = cat;
        }
    }
    else if (prospectingTier >= 1)
    {
        // Tier 1+: Numeric values with noise
        float feAbundance = 0.0f, tiAbundance = 0.0f, siAbundance = 0.0f;
        for (const auto& [type, abundance] : resources)
        {
            // Generate noisy measurement
            float noiseRange = effectiveNoise * 1000.0f;
            int noiseInt = (noiseRange > 0.0f) ?
                GetRandomValue(static_cast<int>(-noiseRange), static_cast<int>(noiseRange)) : 0;
            float noise = 1.0f + (noiseInt / 1000.0f);
            float freshVal = abundance * noise;

            // Weighted average with existing data (confidence accumulation)
            if (isRescan && result.elements.count(type) > 0)
            {
                float oldVal = result.elements[type];
                result.elements[type] = (oldVal * oldScanCount + freshVal) / static_cast<float>(newScanCount);
            }
            else
            {
                result.elements[type] = freshVal;
            }

            // Categories from real abundance (not noisy)
            const char* cat = abundance > 3000.0f ? "HIGH" :
                              abundance > 500.0f ? "MED" : "LOW";
            result.categories[type] = cat;

            if (type == ResourceType::Fe) feAbundance = abundance;
            else if (type == ResourceType::Ti) tiAbundance = abundance;
            else if (type == ResourceType::Si) siAbundance = abundance;
        }

        // Tier 2+: Minerals and hydrogen
        if (prospectingTier >= 2)
        {
            result.minerals["Ilmenite"] = (feAbundance + tiAbundance) * 0.01f;
            result.minerals["Plagioclase"] = siAbundance * 0.015f;
            result.minerals["Pyroxene"] = feAbundance * 0.01f;

            auto survey = resourceManager.GetOrbitalSurveyAt(gridX, gridY);
            result.hydrogenSignal = survey.hydrogenSignal;
        }

        // --- Depth layer scanning ---
        if (prospectingTier >= 1)
        {
            // T1: Surface layer only
            auto surfaceRes = resourceManager.GetResourcesAtGridLayer(gridX, gridY, DepthLayer::SURFACE);
            for (const auto& [type, abundance] : surfaceRes)
            {
                float noiseRange2 = effectiveNoise * 1000.0f;
                int noiseInt2 = (noiseRange2 > 0.0f) ?
                    GetRandomValue(static_cast<int>(-noiseRange2), static_cast<int>(noiseRange2)) : 0;
                result.layerElements[DepthLayer::SURFACE][type] = abundance * (1.0f + noiseInt2 / 1000.0f);
            }
            result.maxScannedDepthLayer = std::max(result.maxScannedDepthLayer, 0);
        }
        if (prospectingTier >= 2)
        {
            // T2: + Shallow layer
            auto shallowRes = resourceManager.GetResourcesAtGridLayer(gridX, gridY, DepthLayer::SHALLOW);
            for (const auto& [type, abundance] : shallowRes)
            {
                float noiseRange2 = effectiveNoise * 1000.0f;
                int noiseInt2 = (noiseRange2 > 0.0f) ?
                    GetRandomValue(static_cast<int>(-noiseRange2), static_cast<int>(noiseRange2)) : 0;
                result.layerElements[DepthLayer::SHALLOW][type] = abundance * (1.0f + noiseInt2 / 1000.0f);
            }
            result.maxScannedDepthLayer = std::max(result.maxScannedDepthLayer, 1);
        }
        if (prospectingTier >= 3)
        {
            // T3: All 4 layers
            for (int li = 2; li <= 3; li++)
            {
                DepthLayer dl = static_cast<DepthLayer>(li);
                auto layerRes = resourceManager.GetResourcesAtGridLayer(gridX, gridY, dl);
                for (const auto& [type, abundance] : layerRes)
                {
                    result.layerElements[dl][type] = abundance;  // T3 = no noise
                }
            }
            result.maxScannedDepthLayer = 3;
        }
    }

    // --- Compute Survey Progress ---
    static const float baseSurveyProgress[] = {
        SURVEY_BASE_PROGRESS_T0, SURVEY_BASE_PROGRESS_T1,
        SURVEY_BASE_PROGRESS_T2, SURVEY_BASE_PROGRESS_T3
    };
    float baseProgress = baseSurveyProgress[std::min(prospectingTier, 3)];
    float profileSurveyMult = profile.surveyMultiplier;
    float calSurveyMult = (prospectingTier >= 1) ? calibrationQuality : 1.0f;
    float currentSurvey = isRescan ? result.surveyProgress : 0.0f;
    float progressGain = baseProgress * profileSurveyMult * calSurveyMult * std::sqrt(1.0f - currentSurvey);
    result.surveyProgress = std::min(1.0f, currentSurvey + progressGain);

    result.scanOrder = nextScanOrder++;
    scanHistory[{gridX, gridY}] = result;

    // Apply profile cooldown and energy cost
    scanCooldown = profile.cooldownTime;
    ConsumeResource(ResourceType::ENERGY, profile.energyCost);

    // --- Calibration drift ---
    if (prospectingTier >= 1 && prospectingTier < 3)
    {
        calibrationQuality -= CALIBRATION_DRIFT_PER_SCAN;
        calibrationQuality = std::max(calibrationQuality, CALIBRATION_MIN_QUALITY);
    }

    // --- Evaluate objectives ---
    EvaluateObjectives(gridX, gridY);

    const char* tierLabels[] = {"Visual", "LIBS", "Multi-Spectral", "Deep Survey"};
    std::cout << "[PROSPECTING] " << tierLabels[std::min(prospectingTier, 3)]
              << " scan (" << profile.name << " profile) at (" << gridX << "," << gridY
              << ") complete. Scan #" << newScanCount
              << " Cal:" << static_cast<int>(calibrationQuality * 100.0f) << "%"
              << " Survey:" << static_cast<int>(result.surveyProgress * 100.0f) << "%" << std::endl;
}

void Unit::MarkSiteForExcavation(int gridX, int gridY) {
    auto key = std::make_pair(gridX, gridY);

    // Check if already marked
    for (const auto& site : markedSites)
    {
        if (site.first == gridX && site.second == gridY)
        {
            ShowMessage("Site already marked.");
            return;
        }
    }

    markedSites.push_back(key);
    std::cout << "[PROSPECTING] Site (" << gridX << "," << gridY << ") marked for excavation." << std::endl;
    ShowMessage(TextFormat("Site (%d,%d) marked for excavation.", gridX, gridY));
}

void Unit::UnmarkSite(int gridX, int gridY) {
    for (auto it = markedSites.begin(); it != markedSites.end(); ++it)
    {
        if (it->first == gridX && it->second == gridY)
        {
            markedSites.erase(it);
            ShowMessage(TextFormat("Site (%d,%d) unmarked.", gridX, gridY));
            return;
        }
    }
}

// --- Excavation Methods ---

void Unit::MoveExcavator(int excavatorId, int gridX, int gridY) {
    for (auto& exc : excavators)
    {
        if (exc.id == excavatorId)
        {
            exc.gridPos = {static_cast<float>(gridX), static_cast<float>(gridY)};
            std::cout << "[EXCAVATION] Excavator " << excavatorId
                      << " moved to (" << gridX << "," << gridY << ")" << std::endl;
            return;
        }
    }
}

void Unit::SetExcavatorDepth(int excavatorId, float depth) {
    for (auto& exc : excavators)
    {
        if (exc.id == excavatorId)
        {
            // Max depth depends on excavation tier
            float maxDepth = 10.0f;  // Tier 0 default
            for (const auto& mod : modules)
            {
                if (mod.moduleType == "EXCAVATION")
                {
                    float tierMaxDepths[] = {10.0f, 30.0f, 100.0f, 300.0f};
                    maxDepth = tierMaxDepths[std::min(mod.tier, 3)];
                    break;
                }
            }
            exc.depth = std::clamp(depth, 0.0f, maxDepth);
            return;
        }
    }
}

void Unit::SetExcavatorRate(int excavatorId, float rate) {
    for (auto& exc : excavators)
    {
        if (exc.id == excavatorId)
        {
            exc.rate = std::clamp(rate, 0.0f, 500.0f);
            return;
        }
    }
}

// --- Beneficiation Methods ---

void Unit::SwapSeparationNodes(int indexA, int indexB) {
    if (indexA >= 0 && indexA < static_cast<int>(separationChain.size()) &&
        indexB >= 0 && indexB < static_cast<int>(separationChain.size()) &&
        indexA != indexB)
    {
        std::swap(separationChain[indexA], separationChain[indexB]);
        std::cout << "[BENEFICIATION] Swapped nodes " << indexA << " and " << indexB << std::endl;
    }
}

void Unit::ToggleSeparationNodeActive(int index) {
    if (index >= 0 && index < static_cast<int>(separationChain.size()))
    {
        separationChain[index].isActive = !separationChain[index].isActive;
    }
}

void Unit::AddSeparationNode(const SeparationNode& node) {
    separationChain.push_back(node);
    std::cout << "[BENEFICIATION] Added node: " << node.name << std::endl;
}

void Unit::RemoveSeparationNode(int index) {
    if (index >= 0 && index < static_cast<int>(separationChain.size()))
    {
        std::cout << "[BENEFICIATION] Removed node: " << separationChain[index].name << std::endl;
        separationChain.erase(separationChain.begin() + index);
    }
}

// --- Geological Confidence ---

float Unit::GetGeologicalConfidence() const {
    Vector2 gridPos = WorldToGrid(parentSectPosition);
    int centerGX = static_cast<int>(gridPos.x);
    int centerGY = static_cast<int>(gridPos.y);

    int scannedCount = 0;
    for (int dy = -2; dy <= 2; dy++)
    {
        for (int dx = -2; dx <= 2; dx++)
        {
            auto it = scanHistory.find({centerGX + dx, centerGY + dy});
            if (it != scanHistory.end() && it->second.isScanned)
            {
                scannedCount++;
            }
        }
    }

    // 25 cells in 5x5 grid, each contributes 4%, plus campaign bonus
    return std::min(1.0f, scannedCount / 25.0f + campaignConfidenceBonus);
}

float Unit::GetSurveyProgress(int gridX, int gridY) const {
    auto it = scanHistory.find({gridX, gridY});
    if (it != scanHistory.end() && it->second.isScanned)
    {
        float survey = it->second.surveyProgress;
        // Backward compat: migrate old scans
        if (survey <= 0.0f && it->second.scanCount > 0)
        {
            survey = std::min(1.0f, static_cast<float>(it->second.scanCount) / 3.0f);
        }
        return survey;
    }
    return 0.0f;
}

// --- Calibration Methods ---

void Unit::StartCalibration() {
    int prospectingTier = 0;
    for (const auto& mod : modules)
    {
        if (mod.moduleType == "PROSPECTING")
        {
            prospectingTier = mod.tier;
            break;
        }
    }

    if (prospectingTier == 0)
    {
        ShowMessage("Calibration not available at Tier 0.");
        return;
    }

    if (prospectingTier >= 3)
    {
        ShowMessage("Tier 3: Auto-calibration active.");
        calibrationQuality = 1.0f;
        return;
    }

    if (isCalibrating)
    {
        ShowMessage("Already calibrating...");
        return;
    }

    isCalibrating = true;
    calibrationTimer = CALIBRATION_DURATION;
    ShowMessage("Calibration started...");
    std::cout << "[PROSPECTING] Calibration started. Duration: "
              << CALIBRATION_DURATION << "s" << std::endl;
}

// --- Campaign Methods ---

void Unit::AddToCampaign(int gx, int gy) {
    int prospectingTier = 0;
    for (const auto& mod : modules)
    {
        if (mod.moduleType == "PROSPECTING")
        {
            prospectingTier = mod.tier;
            break;
        }
    }

    if (prospectingTier < 2)
    {
        ShowMessage("Campaign requires Tier 2+.");
        return;
    }

    int cap = (prospectingTier >= 3) ? 999 : CAMPAIGN_QUEUE_CAP_T2;
    if (static_cast<int>(scanCampaign.size()) >= cap)
    {
        ShowMessage(TextFormat("Campaign queue full (%d max).", cap));
        return;
    }

    CampaignEntry entry;
    entry.gridX = gx;
    entry.gridY = gy;
    entry.profileIndex = activeScanProfileIndex;
    entry.completed = false;
    scanCampaign.push_back(entry);
}

void Unit::RemoveFromCampaign(int index) {
    if (index >= 0 && index < static_cast<int>(scanCampaign.size()))
    {
        scanCampaign.erase(scanCampaign.begin() + index);
        if (campaignCurrentIndex > index)
        {
            campaignCurrentIndex--;
        }
    }
}

void Unit::StartCampaign() {
    if (scanCampaign.empty())
    {
        ShowMessage("No cells queued for campaign.");
        return;
    }
    campaignActive = true;
    campaignCurrentIndex = 0;
    ShowMessage(TextFormat("Campaign started: %d cells queued.", static_cast<int>(scanCampaign.size())));
}

void Unit::PauseCampaign() {
    campaignActive = false;
    ShowMessage("Campaign paused.");
}

void Unit::ClearCampaign() {
    scanCampaign.clear();
    campaignActive = false;
    campaignCurrentIndex = 0;
    ShowMessage("Campaign cleared.");
}

// --- Objective Methods ---

void Unit::GenerateObjectives() {
    int prospectingTier = 0;
    for (const auto& mod : modules)
    {
        if (mod.moduleType == "PROSPECTING")
        {
            prospectingTier = mod.tier;
            break;
        }
    }

    if (prospectingTier < 1) return;

    int count = std::min(prospectingTier, 3);
    activeObjectives.clear();

    // Resource types for threshold objectives
    ResourceType targetTypes[] = {ResourceType::Fe, ResourceType::Ti, ResourceType::Si,
                                   ResourceType::H2, ResourceType::Al, ResourceType::Ca};
    int numTargets = 6;

    for (int i = 0; i < count; i++)
    {
        ProspectingObjective obj;
        int objType = GetRandomValue(0, 2);

        if (objType == 0)
        {
            // THRESHOLD objective
            ResourceType target = targetTypes[GetRandomValue(0, numTargets - 1)];
            float threshold = 2000.0f + GetRandomValue(0, 3000);
            obj.id = TextFormat("threshold_%d", i);
            obj.description = TextFormat("Find cell with >%.0f %s",
                threshold, ResourceTypeToString(target));
            obj.hintText = "Orbital data suggests rich deposits nearby";
            obj.conditionType = "THRESHOLD";
            obj.conditionResource = target;
            obj.conditionValue = threshold;
            obj.rewardType = 0;  // Extraction bonus
            obj.rewardValue = OBJECTIVE_THRESHOLD_BONUS;
            obj.rewardDuration = OBJECTIVE_THRESHOLD_DURATION;
        }
        else if (objType == 1)
        {
            // COVERAGE objective
            int coverageTarget = 5 + GetRandomValue(0, 10);
            obj.id = TextFormat("coverage_%d", i);
            obj.description = TextFormat("Scan %d cells in grid", coverageTarget);
            obj.hintText = "Survey the area systematically";
            obj.conditionType = "COVERAGE";
            obj.conditionResource = ResourceType::ENERGY;  // Unused
            obj.conditionValue = static_cast<float>(coverageTarget);
            obj.rewardType = 1;  // Confidence bonus
            obj.rewardValue = OBJECTIVE_COVERAGE_BONUS;
            obj.rewardDuration = 0.0f;  // Permanent
        }
        else
        {
            // GRADIENT objective
            obj.id = TextFormat("gradient_%d", i);
            obj.description = "Find a resource gradient (>50% diff)";
            obj.hintText = "Look for deposit boundaries";
            obj.conditionType = "GRADIENT";
            obj.conditionResource = ResourceType::Fe;
            obj.conditionValue = OBJECTIVE_GRADIENT_THRESHOLD;
            obj.rewardType = 0;  // Extraction bonus
            obj.rewardValue = OBJECTIVE_GRADIENT_BONUS;
            obj.rewardDuration = OBJECTIVE_GRADIENT_DURATION;
        }

        obj.revealed = (i == 0);  // First objective always revealed
        activeObjectives.push_back(obj);
    }
}

void Unit::EvaluateObjectives(int gridX, int gridY) {
    std::vector<int> completedIndices;

    for (size_t i = 0; i < activeObjectives.size(); i++)
    {
        auto& obj = activeObjectives[i];
        if (obj.completed) continue;

        // Reveal unrevealed objectives when scanning nearby
        if (!obj.revealed)
        {
            obj.revealed = true;
        }

        bool satisfied = false;

        if (obj.conditionType == "THRESHOLD")
        {
            auto scanIt = scanHistory.find({gridX, gridY});
            if (scanIt != scanHistory.end())
            {
                auto elemIt = scanIt->second.elements.find(obj.conditionResource);
                if (elemIt != scanIt->second.elements.end() && elemIt->second > obj.conditionValue)
                {
                    satisfied = true;
                }
            }
        }
        else if (obj.conditionType == "COVERAGE")
        {
            int scannedCount = 0;
            for (const auto& [coords, scan] : scanHistory)
            {
                if (scan.isScanned) scannedCount++;
            }
            if (static_cast<float>(scannedCount) >= obj.conditionValue)
            {
                satisfied = true;
            }
        }
        else if (obj.conditionType == "GRADIENT")
        {
            // Check if any adjacent scanned cell has >50% difference for any resource
            auto scanIt = scanHistory.find({gridX, gridY});
            if (scanIt != scanHistory.end())
            {
                int dirs[][2] = {{-1,0},{1,0},{0,-1},{0,1}};
                for (auto& d : dirs)
                {
                    auto neighborIt = scanHistory.find({gridX + d[0], gridY + d[1]});
                    if (neighborIt != scanHistory.end() && neighborIt->second.isScanned)
                    {
                        for (const auto& [type, val] : scanIt->second.elements)
                        {
                            auto nIt = neighborIt->second.elements.find(type);
                            if (nIt != neighborIt->second.elements.end() && nIt->second > 0.0f)
                            {
                                float diff = std::abs(val - nIt->second) / std::max(val, nIt->second);
                                if (diff > obj.conditionValue)
                                {
                                    satisfied = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (satisfied) break;
                }
            }
        }

        if (satisfied)
        {
            obj.completed = true;
            completedIndices.push_back(static_cast<int>(i));
        }
    }

    // Apply rewards and move completed objectives
    for (int idx : completedIndices)
    {
        auto& obj = activeObjectives[idx];

        if (obj.rewardType == 0)
        {
            // Extraction bonus
            objectiveBonusMultiplier = 1.0f + obj.rewardValue;
            objectiveBonusExpiry = obj.rewardDuration * TICKS_PER_DAY;
            ShowMessage(TextFormat("Objective complete! +%.0f%% extraction for %.0f days.",
                obj.rewardValue * 100.0f, obj.rewardDuration));
        }
        else if (obj.rewardType == 1)
        {
            // Confidence bonus (permanent)
            campaignConfidenceBonus += obj.rewardValue;
            ShowMessage(TextFormat("Objective complete! +%.0f%% confidence.",
                obj.rewardValue * 100.0f));
        }

        completedObjectives.push_back(obj);
        std::cout << "[OBJECTIVES] Completed: " << obj.description << std::endl;
    }

    // Remove completed from active list (reverse order)
    for (int i = static_cast<int>(completedIndices.size()) - 1; i >= 0; i--)
    {
        activeObjectives.erase(activeObjectives.begin() + completedIndices[i]);
    }

    // Generate replacement objectives
    if (!completedIndices.empty())
    {
        int prospectingTier = 0;
        for (const auto& mod : modules)
        {
            if (mod.moduleType == "PROSPECTING")
            {
                prospectingTier = mod.tier;
                break;
            }
        }
        int maxObj = std::min(prospectingTier, 3);
        while (static_cast<int>(activeObjectives.size()) < maxObj)
        {
            // Generate one new objective
            ProspectingObjective obj;
            int objType = GetRandomValue(0, 2);
            ResourceType targetTypes2[] = {ResourceType::Fe, ResourceType::Ti, ResourceType::Si,
                                            ResourceType::H2, ResourceType::Al, ResourceType::Ca};

            if (objType == 0)
            {
                ResourceType target = targetTypes2[GetRandomValue(0, 5)];
                float threshold = 2000.0f + GetRandomValue(0, 3000);
                obj.id = TextFormat("threshold_%d", static_cast<int>(completedObjectives.size()));
                obj.description = TextFormat("Find cell with >%.0f %s",
                    threshold, ResourceTypeToString(target));
                obj.hintText = "Rich deposits detected";
                obj.conditionType = "THRESHOLD";
                obj.conditionResource = target;
                obj.conditionValue = threshold;
                obj.rewardType = 0;
                obj.rewardValue = OBJECTIVE_THRESHOLD_BONUS;
                obj.rewardDuration = OBJECTIVE_THRESHOLD_DURATION;
            }
            else if (objType == 1)
            {
                int coverageTarget = static_cast<int>(scanHistory.size()) + 3 + GetRandomValue(0, 5);
                obj.id = TextFormat("coverage_%d", static_cast<int>(completedObjectives.size()));
                obj.description = TextFormat("Scan %d cells total", coverageTarget);
                obj.hintText = "Expand survey coverage";
                obj.conditionType = "COVERAGE";
                obj.conditionResource = ResourceType::ENERGY;
                obj.conditionValue = static_cast<float>(coverageTarget);
                obj.rewardType = 1;
                obj.rewardValue = OBJECTIVE_COVERAGE_BONUS;
                obj.rewardDuration = 0.0f;
            }
            else
            {
                obj.id = TextFormat("gradient_%d", static_cast<int>(completedObjectives.size()));
                obj.description = "Find a resource gradient (>50% diff)";
                obj.hintText = "Deposit boundary nearby";
                obj.conditionType = "GRADIENT";
                obj.conditionResource = ResourceType::Fe;
                obj.conditionValue = OBJECTIVE_GRADIENT_THRESHOLD;
                obj.rewardType = 0;
                obj.rewardValue = OBJECTIVE_GRADIENT_BONUS;
                obj.rewardDuration = OBJECTIVE_GRADIENT_DURATION;
            }
            obj.revealed = true;
            activeObjectives.push_back(obj);
        }
    }
}

// --- AI Auto-Management ---

void Unit::UpdateProspectingAI(float deltaTime) {
    int prospectingTier = 0;
    for (const auto& mod : modules)
    {
        if (mod.moduleType == "PROSPECTING")
        {
            prospectingTier = mod.tier;
            break;
        }
    }

    if (prospectingTier < 1) return;

    // Auto-calibration
    if (prospectingAI.autoCalibrate && !isCalibrating &&
        calibrationQuality < prospectingAI.calibrationThreshold &&
        prospectingTier >= 1 && prospectingTier < 3)
    {
        StartCalibration();
        aiLastAction = "AI: Auto-calibrating (quality low)";
    }

    // T3: Auto-calibration (drift eliminated)
    if (prospectingTier >= 3)
    {
        calibrationQuality = 1.0f;
    }

    // Auto-campaign (T3 only)
    if (prospectingAI.autoCampaign && prospectingTier >= 3 &&
        !campaignActive && scanCampaign.empty())
    {
        // Generate a spiral campaign covering unscanned cells
        Vector2 gridPos = GetGridPosition();
        int cx = static_cast<int>(gridPos.x);
        int cy = static_cast<int>(gridPos.y);

        // Spiral outward from center
        int dirs[][2] = {{1,0}, {0,1}, {-1,0}, {0,-1}};
        int x = cx, y = cy;
        int stepSize = 1, dirIdx = 0, stepsTaken = 0, stepsInDir = 0;

        for (int i = 0; i < 25; i++)
        {
            if (x >= 0 && x < PLANET_SIZE && y >= 0 && y < PLANET_SIZE)
            {
                auto it = scanHistory.find({x, y});
                if (it == scanHistory.end() || !it->second.isScanned)
                {
                    CampaignEntry entry;
                    entry.gridX = x;
                    entry.gridY = y;
                    entry.profileIndex = 1;  // Standard
                    entry.completed = false;
                    scanCampaign.push_back(entry);
                }
            }

            x += dirs[dirIdx][0];
            y += dirs[dirIdx][1];
            stepsInDir++;
            if (stepsInDir >= stepSize)
            {
                stepsInDir = 0;
                dirIdx = (dirIdx + 1) % 4;
                stepsTaken++;
                if (stepsTaken >= 2)
                {
                    stepsTaken = 0;
                    stepSize++;
                }
            }
        }

        if (!scanCampaign.empty())
        {
            StartCampaign();
            aiLastAction = "AI: Auto-campaign started";
        }
    }
}

// --- Operations & Directives Methods ---

void Unit::SetDirective(const ActiveDirective& directive) {
    // Check if Directives module is active and at sufficient tier
    bool canSetDirective = false;
    int directivesTier = 0;

    for (const auto& mod : modules)
    {
        if (mod.moduleType == "DIRECTIVES" && mod.isActive)
        {
            canSetDirective = true;
            directivesTier = mod.tier;
            break;
        }
    }

    if (!canSetDirective && directive.type != DirectiveType::NONE)
    {
        ShowMessage("Directives module not active.");
        return;
    }

    // Tier 0: manual only (NONE)
    if (directivesTier == 0 && directive.type != DirectiveType::NONE)
    {
        ShowMessage("Directives Tier 1+ required for automated directives.");
        return;
    }

    // Tier 1: basic cards only
    if (directivesTier == 1 && directive.type != DirectiveType::NONE &&
        directive.type != DirectiveType::PRIORITIZE &&
        directive.type != DirectiveType::MAXIMIZE &&
        directive.type != DirectiveType::CONSERVE)
    {
        ShowMessage("Advanced directive requires Tier 2+.");
        return;
    }

    activeDirective = directive;

    const char* directiveNames[] = {
        "NONE", "PRIORITIZE", "MAXIMIZE", "CONSERVE",
        "EXPLORATION_MODE", "EMERGENCY_HARVEST", "THERMAL_SYNC"
    };
    std::cout << "[DIRECTIVES] Set directive: "
              << directiveNames[static_cast<int>(directive.type)] << std::endl;
}

float Unit::GetOperationsEfficiencyModifier() const {
    // Find operations module
    for (const auto& mod : modules)
    {
        if (mod.moduleType == "OPERATIONS")
        {
            if (!mod.isBuilt || !mod.isActive)
            {
                // Operations not active: no penalty but no bonus
                return 1.0f;
            }

            float baseMod = 1.0f;
            // Tier 0: continuous, -15% efficiency penalty
            if (mod.tier == 0) baseMod = 0.85f;
            // Tier 1: manual scheduling, neutral
            else if (mod.tier == 1) baseMod = 1.0f;
            // Tier 2: optimized scheduling, +10%
            else if (mod.tier == 2) baseMod = 1.1f;
            // Tier 3: AI scheduling, +20%
            else if (mod.tier == 3) baseMod = 1.20f;

            // Geological confidence bonus: up to +10% at full coverage
            float confidence = GetGeologicalConfidence();
            float confidenceBonus = confidence * 0.10f;

            return baseMod + confidenceBonus;
        }
    }
    return 1.0f;  // No operations module
}

bool Unit::IsOperationsActive() const {
    for (const auto& mod : modules)
    {
        if (mod.moduleType == "OPERATIONS" && mod.isBuilt && mod.isActive)
        {
            return true;
        }
    }
    return false;
}
