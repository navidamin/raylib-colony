#ifndef UNIT_H
#define UNIT_H

#include "raylib.h"
#include <string>
#include <map>
#include <vector>

#include "resource_manager.h"
#include "time_manager.h"
#include "game_constants.h"
#include "unit_ui.h"



class Unit {
public:
    // Constructor
    Unit(std::string type, Vector2& position, ResourceManager& resource, TimeManager &time);

    // Destrructor
    ~Unit();

    struct UnitModule {
        std::string name;
        int level = 1;
        bool isBuilt;
        bool isActive;
        float efficiency = 1.0f;
        std::string description;
        std::map<ResourceType, float> consumptionRates;
        std::map<ResourceType, float> productionRates;
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
    bool UpgradeModule(int moduleIndex);
    void ProcessModuleEffects(float deltaTime, ResourceManager& );
    void ProcessExtraction(float deltaTime, ResourceManager& );
    float GetStoredResource(ResourceType type) const;
    void AddResource(ResourceType type, float amount);
    bool ConsumeResource(ResourceType type, float amount);

    void DischargeAllResources(std::map<ResourceType, float>& collected);

private:
    // Include UI-related members
    UNIT_UI_PRIVATE_MEMBERS

    Vector2 parentSectPosition;
    ResourceManager& resourceManager;
    TimeManager& timeManager;

    std::vector<UnitModule> modules;
    std::map<ResourceType, float> resourceStorage;
    UnitModule* activeModule = nullptr;
    std::map<ResourceType, std::map<ResourceType, float>> productionCosts;

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

    Vector2 WorldToGrid(Vector2 worldPos) const;


};

#endif // UNIT_H
