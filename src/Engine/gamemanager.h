#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "raylib.h"
#include "raymath.h"
#include "game_constants.h"
#include "game_structs.h"
#include "planet.h"
#include "colony.h"
#include "sect.h"
#include "unit.h"
#include "time_manager.h"
#include "inputmanager.h"
#include "region_identity.h"
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

    // Make a colony (one the globe marker was clicked on) the current
    // one; its first sect becomes the current sect.
    void SetCurrentColony(Colony* colony);
    void SelectSect(Vector2 mousePosition, Camera2D camera);
    void SelectUnit(Vector2 mousePosition);
    void SelectDefaultUnit();  // Auto-select Extraction unit or first available

    // Founding. A colony is founded at a place on the Moon: its first sect
    // stands at `point` and the colony's 25 km window is centred on
    // `windowCentre` -- the site rung's window, so the picture does not
    // change at the click. `claimed` is the region card the player read
    // (its archetype becomes the colony's); nullptr takes the ground's
    // own. Refused (nullptr, reason printed) inside another colony's
    // territory. This is the one call the descent makes at FOUND.
    Colony* FoundColony(const LunarPoint& point, const LunarPoint& windowCentre,
                        const RegionIdentity* claimed);
    // The same with the window centred on the sect: tools and tests.
    Colony* FoundColony(const LunarPoint& point);
    // The colony whose centre lies inside a square window of spanKm on
    // `centre`, or nullptr: what the descent finds when it lands on
    // ground that is already someone's.
    Colony* ColonyInWindow(const LunarPoint& centre, double spanKm) const;
    // A new sect of the current colony: not in another colony's
    // territory, a footprint's spacing from every existing sect, and with
    // its whole footprint inside the colony's window. Refused otherwise.
    Sect* FoundSect(const LunarPoint& point);

    // FoundSect from the Colony view's drawing frame (the current
    // colony's local frame).
    Sect* BuildNewSect(Vector2 localPos);

    // Test functions for transport
    void BuildAllRoads();
    void CycleTransportModes();

    // Test infrastructure (Phase 2.5)
    void PrintTransportState();           // KEY_0: Print current transport state
    void TestRoadConstruction();          // KEY_1: Build road between first two sects
    void SelectNearestRoad(Vector2 worldPos); // KEY_2: Select road near position
    void RunTransportIntegrationTest();   // KEY_3: Full integration test
    Road* GetSelectedRoad() const { return selectedRoad; }

    // Road construction mode (Phase 2.5)
    void ToggleBuildRoadMode();           // KEY_B: Toggle road build mode
    void SelectSectForRoadBuild(Vector2 worldPos);  // Select sect in build mode
    bool IsBuildRoadMode() const { return buildRoadMode; }
    Sect* GetRoadBuildStartSect() const { return roadBuildStartSect; }

    TimeManager& GetTimeManager() { return timeManager; }

private:
    Planet* planet;
    std::vector<Colony*> colonies;
    Colony* currentColony;
    Sect* currentSect;
    Unit* currentUnit;
    Road* selectedRoad;

    // Road construction mode
    bool buildRoadMode;
    Sect* roadBuildStartSect;

    TimeManager timeManager;
    float lastUpdateTime;
};

#endif // GAME_MANAGER_H
