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
#include <utility>
#include <cmath>


class Unit {
public:
    // Constructor
    Unit(std::string type, Vector2& position, ResourceManager& resource, TimeManager &time,
         std::map<ResourceType, float> &storage, std::map<ResourceType, float> &capacity);

    // Destrructor
    ~Unit();

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
