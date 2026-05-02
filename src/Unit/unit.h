#ifndef UNIT_H
#define UNIT_H

#include "raylib.h"
#include <string>
#include <map>
#include <vector>
#include <set>

#include "resource_manager.h"
#include "time_manager.h"
#include "game_constants.h"
#include "unit_ui.h"
#include "separation_node.h"
#include "prospecting_system.h"
#include <utility>
#include <cmath>
#include <memory>

class Unit {
public:
    // Constructor
    Unit(std::string type, Vector2& position, ResourceManager& resource, TimeManager &time,
         std::map<ResourceType, float> &storage, std::map<ResourceType, float> &capacity);

    // Destrructor
    ~Unit();

    // Scan profile configuration
    struct ScanProfile {
        std::string name;       // "Visual", "Quick", "Standard", "Deep", "Custom"
        float powerMultiplier;  // 0.5-2.0, affects noise reduction
        int pulseCount;         // 5/15/30, affects noise reduction (sqrt(n))
        float cooldownTime;     // seconds
        float energyCost;       // energy consumed per scan
        float surveyMultiplier = 1.0f;  // multiplier for survey progress gain
    };

    // Campaign entry for automated survey
    struct CampaignEntry {
        int gridX, gridY;
        int profileIndex;  // Which scan profile to use
        bool completed = false;
    };

    // Prospecting objective for discovery gameplay
    struct ProspectingObjective {
        std::string id;
        std::string description;       // Shown to player (may be vague)
        std::string hintText;          // Orbital hint (partial reveal)
        bool revealed = false;         // Has hint been shown?
        bool completed = false;
        int rewardType;                // 0=extraction bonus, 1=confidence bonus, 2=resource grant
        float rewardValue;
        float rewardDuration;          // Game days (0 = permanent)
        std::string conditionType;     // "THRESHOLD", "COVERAGE", "GRADIENT"
        ResourceType conditionResource;
        float conditionValue;
    };

    // AI auto-management settings for prospecting
    struct ProspectingAI {
        bool autoSelectProfile = true;    // AI picks scan profile per cell
        bool autoCalibrate = true;        // AI calibrates when drift > threshold
        bool autoCampaign = false;        // AI designs and runs survey campaigns (T3 only)
        float calibrationThreshold = 0.8f; // AI calibrates when quality drops below this
    };

    struct UnitModule {
        std::string name;
        std::string moduleType;                           // e.g., "PROSPECTING", "EXCAVATION"
        int level = 1;                                    // Legacy level for non-extraction units
        int tier = 0;                                     // Tier 0-3 for extraction modules
        bool isBuilt;
        bool isActive;
        float efficiency = 1.0f;
        float energyRequired = 0.0f;                      // kW required for this tier
        std::string description;
        std::vector<std::string> tierDependencies;         // Tech names required for next tier
        std::map<ResourceType, float> consumptionRates;
        std::map<ResourceType, float> productionRates;
        std::map<ResourceType, float> maxProductionRates;
        std::map<int, std::map<ResourceType, float>> upgradeCosts;
        std::map<int, std::map<std::string, float>> enhancements;
    };


    void Start();
    void Stop();
    void Upgrade(int level);
    void CalculateConsumption();
    std::map<std::string, float> CalculateProduction() const;
    void DisplayStats() const;
    void Update(float deltaTime);
    void DrawInSectView(Vector2 corePosition, float coreRadius, int index);
    void DrawInUnitView();

    void SetInitialParameters();

    // Sect info functions
    Vector2 GetParentSectPosition() {return parentSectPosition;}
    void SetParentSectPosition(Vector2 position) {parentSectPosition = position;}

    // State checking
    bool IsActive() const { return status == "active"; }
    bool IsUnderConstruction() const { return isUnderConstruction; }

    // Getters
    std::string GetStatus() const { return status; }
    Vector2 GetUnitPosInSectView() const { return positionInSectView;}
    float GetUnitRadiusInSectView() const { return radiusInSectView;}
    std::string GetUnitType() const { return unit_type;}

    // Setters
    void SetUnitPosInSectView(Vector2 position) {positionInSectView = position;}
    void SetUnitRadiusInSectView(float radius) {radiusInSectView = radius;}
    void SetStatus(const std::string& newStatus) { status = newStatus; }
    float GetProductionCycleTime() const { return productionCycleTime; }

    // Production processing
    void ProcessFarming(float deltaTime);
    void ProcessEnergy(float deltaTime);

    // Construction processing
    void UpdateConstruction(float deltaTime);
    void OnConstructionComplete();

    // Module Processing functions
    void InitializeModules();
    void InitializeFutureModules();
    bool UpgradeModule(int moduleIndex);
    bool UpgradeModuleTier(int moduleIndex);
    bool DebugUpgradeModuleTier(int moduleIndex);
    void ProcessModuleEffects(float deltaTime, ResourceManager& );
    void ProcessExtraction(float deltaTime, ResourceManager& );
    float GetStoredResource(ResourceType type) const;
    void AddResource(ResourceType type, float amount);
    bool ConsumeResource(ResourceType type, float amount);

    // Module activation/deactivation
    bool ActivateModule(int moduleIndex);
    bool DeactivateModule(int moduleIndex);
    const std::set<int>& GetActiveModuleIndices() const { return activeModuleIndices; }

    // Prospecting system
    struct ScanResult {
        std::map<ResourceType, float> elements;    // Elemental composition (may be noisy)
        std::map<std::string, float> minerals;     // Mineral identification (tier 2+)
        std::map<ResourceType, std::string> categories; // LOW/MED/HIGH per element (tier 0)
        float hydrogenSignal = 0.0f;               // Neutron reading (tier 2+)
        int qualityRating = 0;                     // 0-5 stars
        int scanTier = 0;                          // Tier of prospecting module when scanned
        bool isScanned = false;
        int scanOrder = 0;                         // Sequence number for recency sorting
        int scanCount = 0;                         // Number of times this cell has been scanned
        int scanProfileIndex = 0;                  // Which scan profile was used
        std::map<DepthLayer, std::map<ResourceType, float>> layerElements;  // Per-layer data
        int maxScannedDepthLayer = 0;              // Highest layer revealed (0=surface only)
        float surveyProgress = 0.0f;               // 0.0-1.0, drives extraction efficiency
    };

    // Excavation system
    struct Excavator {
        int id;
        Vector2 gridPos;
        std::string method;    // "scoop", "bucket_wheel", "percussive", "drone"
        float depth = 0.0f;    // Current excavation depth (cm)
        float rate = 30.0f;    // kg/hr
        float wear = 0.0f;    // 0-1, accumulated wear
    };

    // Prospecting getters
    const std::map<std::pair<int,int>, ScanResult>& GetScanHistory() const { return scanHistory; }
    const std::vector<std::pair<int,int>>& GetMarkedSites() const { return markedSites; }
    void PerformLIBSScan(int gridX, int gridY);
    void MarkSiteForExcavation(int gridX, int gridY);
    void UnmarkSite(int gridX, int gridY);
    float GetGeologicalConfidence() const;
    float GetSurveyProgress(int gridX, int gridY) const;

    // Scan profile accessors
    const ScanProfile& GetActiveScanProfile() const { return activeScanProfile; }
    const std::vector<ScanProfile>& GetAvailableProfiles() const { return availableProfiles; }
    int GetActiveScanProfileIndex() const { return activeScanProfileIndex; }
    void SetActiveScanProfile(int index);

    // Calibration accessors
    float GetCalibrationQuality() const { return calibrationQuality; }
    void StartCalibration();
    bool IsCalibrating() const { return isCalibrating; }
    float GetCalibrationTimer() const { return calibrationTimer; }

    // Campaign accessors
    void AddToCampaign(int gx, int gy);
    void RemoveFromCampaign(int index);
    void StartCampaign();
    void PauseCampaign();
    bool IsCampaignActive() const { return campaignActive; }
    const std::vector<CampaignEntry>& GetCampaign() const { return scanCampaign; }
    void ClearCampaign();
    float GetCampaignConfidenceBonus() const { return campaignConfidenceBonus; }

    // Objective accessors
    const std::vector<ProspectingObjective>& GetActiveObjectives() const { return activeObjectives; }
    const std::vector<ProspectingObjective>& GetCompletedObjectives() const { return completedObjectives; }
    float GetObjectiveBonusMultiplier() const { return objectiveBonusMultiplier; }

    // AI management accessors
    ProspectingAI& GetProspectingAI() { return prospectingAI; }
    const ProspectingAI& GetProspectingAI() const { return prospectingAI; }
    void SetProspectingAIPolicy(const ProspectingAI& policy) { prospectingAI = policy; }
    const std::string& GetAILastAction() const { return aiLastAction; }

    // New prospecting system
    ProspectingSystem* GetProspectingSystem() { return prospectingSystem.get(); }
    const ProspectingSystem* GetProspectingSystem() const { return prospectingSystem.get(); }
    bool HasProspectingSystem() const { return prospectingSystem != nullptr; }

    // Excavation getters
    const std::vector<Excavator>& GetExcavators() const { return excavators; }
    void MoveExcavator(int excavatorId, int gridX, int gridY);
    void SetExcavatorDepth(int excavatorId, float depth);
    void SetExcavatorRate(int excavatorId, float rate);
    const std::vector<UnitModule>& GetModules() const { return modules; }

    // Grid position getter (derived from parentSectPosition)
    Vector2 GetGridPosition() const {
        return {
            std::floor(parentSectPosition.x / (SECT_CORE_RADIUS * 2.0f)),
            std::floor(parentSectPosition.y / (SECT_CORE_RADIUS * 2.0f))
        };
    }
    float GetScanCooldown() const { return scanCooldown; }

    // Beneficiation
    const std::vector<SeparationNode>& GetSeparationChain() const { return separationChain; }
    void SwapSeparationNodes(int indexA, int indexB);
    void ToggleSeparationNodeActive(int index);
    void AddSeparationNode(const SeparationNode& node);
    void RemoveSeparationNode(int index);

    // UI state accessors for RenderManager
    int GetSelectedModuleIndex() const { return selectedModuleIndex; }
    void SetSelectedModuleIndex(int index) { selectedModuleIndex = index; }
    bool IsInModuleView() const { return isInModuleView; }
    void SetIsInModuleView(bool val) { isInModuleView = val; }
    bool IsShowingStats() const { return showingStats; }
    void SetShowingStats(bool val) { showingStats = val; }
    const UIMessage& GetCurrentMessage() const { return currentMessage; }
    const std::map<ResourceType, float>& GetResourceStorage() const { return resourceStorage; }
    const std::map<ResourceType, float>& GetStorageCapacity() const { return storageCapacity; }
    const std::map<ResourceType, float>& GetOverflowBuffer() const { return overflowBuffer; }
    float GetTotalRegolithExtracted() const { return totalRegolithExtracted; }

    // Module action wrappers for RenderManager
    bool PublicCanUpgradeModule(int moduleIndex) const;
    bool PublicCanBuildModule(int moduleIndex) const;
    void PublicBuildModule(int moduleIndex);
    void PublicHandleModuleActivation(int moduleIndex);
    void PublicShowMessage(const std::string& text);

    // Operations & Directives
    enum class DirectiveType {
        NONE,
        PRIORITIZE,      // Focus on a specific resource
        MAXIMIZE,        // Maximum output regardless of cost
        CONSERVE,        // Minimize energy consumption
        EXPLORATION_MODE,// Focus prospecting over extraction
        EMERGENCY_HARVEST,// Max extraction at wear cost
        THERMAL_SYNC     // Sync with thermal cycles
    };

    struct ActiveDirective {
        DirectiveType type = DirectiveType::NONE;
        ResourceType targetResource = ResourceType::Fe;  // For PRIORITIZE
        float strength = 1.0f;  // 0-1 modifier
    };

    void SetDirective(const ActiveDirective& directive);
    const ActiveDirective& GetDirective() const { return activeDirective; }
    float GetOperationsEfficiencyModifier() const;
    bool IsOperationsActive() const;

private:
    // Include UI-related members
    UNIT_UI_PRIVATE_MEMBERS

    Vector2 parentSectPosition;
    ResourceManager& resourceManager;
    TimeManager& timeManager;
    std::map<ResourceType, float>& resourceStorage;
    std::map<ResourceType, float>& storageCapacity;
    std::map<ResourceType, float> overflowBuffer;  // Buffer for resources that exceed capacity

    std::vector<UnitModule> modules;
    std::set<int> activeModuleIndices;  // Indices of currently active modules
    std::map<ResourceType, std::map<ResourceType, float>> productionCosts;

    // Prospecting data
    std::map<std::pair<int,int>, ScanResult> scanHistory;
    std::vector<std::pair<int,int>> markedSites;
    float scanCooldown = 0.0f;
    int nextScanOrder = 1;

    // Scan profiles
    ScanProfile activeScanProfile;
    std::vector<ScanProfile> availableProfiles;
    int activeScanProfileIndex = 0;
    void InitializeScanProfiles();

    // Calibration data
    float calibrationQuality = 1.0f;   // 1.0 = perfect, degrades toward 0.5
    bool isCalibrating = false;
    float calibrationTimer = 0.0f;

    // Campaign data
    std::vector<CampaignEntry> scanCampaign;
    bool campaignActive = false;
    int campaignCurrentIndex = 0;
    float campaignConfidenceBonus = 0.0f;  // Flat bonus from completed campaigns

    // Objective data
    std::vector<ProspectingObjective> activeObjectives;
    std::vector<ProspectingObjective> completedObjectives;
    float objectiveBonusMultiplier = 1.0f;
    float objectiveBonusExpiry = 0.0f;     // Ticks until bonus expires
    void GenerateObjectives();
    void EvaluateObjectives(int gridX, int gridY);

    // AI auto-management
    ProspectingAI prospectingAI;
    std::string aiLastAction;
    void UpdateProspectingAI(float deltaTime);

    // New prospecting system (extraction units only)
    std::unique_ptr<ProspectingSystem> prospectingSystem;

    // Excavation data
    std::vector<Excavator> excavators;
    float totalRegolithExtracted = 0.0f;

    // Beneficiation data
    std::vector<SeparationNode> separationChain;

    // Operations & Directives data
    ActiveDirective activeDirective;

    bool isUnderConstruction;
    float productionCycleTime;
    Vector2 positionInSectView;
    float radiusInSectView;
    std::string unit_type;
    std::map<std::string, float> parameters;
    std::map<std::string, float> consumption;
    std::map<std::string, float> production;
    std::string status;
    std::vector<std::string> upgrades;
    float energy_cost;

    // Include UI-related methods
    UNIT_UI_PRIVATE_METHODS

    void InitializeStorage();
    void UpdateStorage();

    // Specialized module initialization
    void InitializeExtractionModules();
    void InitializeFarmingModules();
    void InitializeEnergyModules();
    void InitializeManufactureModules();
    void InitializeResearchModules();
    void InitializeGenericModules();

    void UpdateUnitStatus();

    Vector2 WorldToGrid(Vector2 worldPos) const;


};

#endif // UNIT_H
