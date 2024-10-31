// TimeManager/time_manager.h
#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <unordered_map>

#include "colony.h"
#include "unit.h"
//#include "resource_types.h"
#include "game_structs.h"
#include "game_constants.h"



// Production cycle status
struct ProductionCycle {
    float elapsedTime;          // Time elapsed in current cycle
    float cycleTime;            // Total time for one cycle
    float progress;             // Progress percentage (0-1)
    bool isActive;              // Whether production is active
};

// Construction timer for buildings/units
struct ConstructionTimer {
    float elapsedTime;          // Time elapsed in construction
    float totalTime;            // Total required construction time
    float progress;             // Construction progress (0-1)
    bool isComplete;            // Whether construction is complete
};

// Resource production rate tracking
struct ProductionRate {
    ResourceType type;          // Type of resource
    float ratePerTick;         // Amount produced per tick
    float accumulated;         // Accumulated amount since last update
};

class TimeManager {
public:
    TimeManager();
    ~TimeManager();

    // Core time functions
    void Update(float deltaTime);
    void Reset();
    void Pause();
    void Resume();
    void SetTimeScale(float scale);

    // Production management
    void StartProduction(Unit* unit);
    void StopProduction(Unit* unit);
    void UpdateProductionCycle(Unit* unit);
    float GetProductionProgress(Unit* unit) const;

    // Construction management
    void StartConstruction(Unit* unit, float constructionTime);
    void UpdateConstruction(Unit* unit);
    float GetConstructionProgress(Unit* unit) const;
    bool IsConstructionComplete(Unit* unit) const;

    // Resource rate management
    void SetProductionRate(Unit* unit, ResourceType type, float rate);
    void UpdateResourceProduction(Unit* unit);
    float GetAccumulatedResources(Unit* unit, ResourceType type) const;
    void ClearAccumulatedResources(Unit* unit);

    // Colony management (previously defined)
    void RegisterColony(Colony* colony);
    void UnregisterColony(Colony* colony);
    size_t GetRegisteredColonyCount() const;
    bool IsColonyRegistered(Colony* colony) const;

    // Getters/Utility functions (previously defined)
    int GetTicks() const { return currentTicks; }
    float GetGameTime() const { return gameTime; }
    bool IsPaused() const { return isPaused; }
    float GetTimeScale() const { return timeScale; }
    float TicksToSeconds(int ticks) const;
    int SecondsToTicks(float seconds) const;

    // Draw Time functions
    void Draw(int screenWidth, int screenHeight);
    int GetCurrentDay() const;

private:
    void UpdateColonies();
    void UpdateColonyResources(Colony* colony);
    void UpdateColonyConstruction(Colony* colony);
    void UpdateProductionCycles();
    void UpdateConstructionTimers();
    void AccumulateResources();
    void ProcessTick();

    // Time tracking (previously defined)
    float gameTime;
    float timeScale;
    float accumulatedTime;
    int currentTicks;
    bool isPaused;

    // Colony tracking
    std::vector<Colony*> colonies;

    // Production tracking
    std::unordered_map<Unit*, ProductionCycle> productionCycles;
    std::unordered_map<Unit*, ConstructionTimer> constructionTimers;
    std::unordered_map<Unit*, std::vector<ProductionRate>> productionRates;

    // Resource accumulation tracking
    std::unordered_map<Unit*, std::unordered_map<ResourceType, float>> accumulatedResources;

    // TICKS_PER_DAY
    static const int TICKS_PER_DAY = 60;  // 60 seconds = 1 day
};

#endif // TIME_MANAGER_H
