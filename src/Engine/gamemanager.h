#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "raylib.h"
#include "raymath.h"
#include "game_constants.h"
#include "planet.h"
#include "colony.h"
#include "sect.h"
#include "unit.h"
#include "time_manager.h"
#include "inputmanager.h"
#include <vector>

class GameManager {
public:
    GameManager();
    ~GameManager();

    void InitGame();
    void Update(float deltaTime);

    Planet* GetPlanet() const { return planet; }
    std::vector<Colony*>& GetColonies() { return colonies; }
    Colony* GetCurrentColony() const { return currentColony; }
    Sect* GetCurrentSect() const { return currentSect; }
    Unit* GetCurrentUnit() const { return currentUnit; }

    void SelectColony(Vector2 mousePosition);
    void SelectSect(Vector2 mousePosition, Camera2D camera);
    void SelectUnit(Vector2 mousePosition);
    void SelectDefaultUnit();  // Auto-select Extraction unit or first available

    void BuildNewColony(Vector2 worldPos);
    void BuildNewSect(Vector2 worldPos);

    // Test functions for transport
    void BuildAllRoads();
    void CycleTransportModes();

    // Test infrastructure (Phase 2.5)
    void PrintTransportState();           // KEY_0: Print current transport state
    void TestRoadConstruction();          // KEY_1: Build road between first two sects
    void SelectNearestRoad(Vector2 worldPos); // KEY_2: Select road near position
    void RunTransportIntegrationTest();   // KEY_3: Full integration test
    Road* GetSelectedRoad() const { return selectedRoad; }

    void UpdatePlanetActiveArea();
    TimeManager& GetTimeManager() { return timeManager; }

private:
    Planet* planet;
    std::vector<Colony*> colonies;
    Colony* currentColony;
    Sect* currentSect;
    Unit* currentUnit;
    Road* selectedRoad;

    TimeManager timeManager;
    float lastUpdateTime;
};

#endif // GAME_MANAGER_H
