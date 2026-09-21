#ifndef PLANET_H
#define PLANET_H

#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <map>
#include <utility>
#include <optional>
#include <memory>
#include <random>

#include "colony.h"
#include "resource_manager.h"
#include "game_constants.h"
#include "game_structs.h"


// The Moon, as far as the game is concerned: the owner of the ground
// truth (ResourceManager) and the list of colonies on it.
class Planet {
public:
    Planet();
    ~Planet();

    struct ActiveArea {
        Vector2 centroid;
        float radius;  // Distance from centroid to furthest colony
    };

    // Seed the ground truth. 0 = a different Moon each run.
    void GenerateMap(unsigned int worldSeed = 0);
    void AddColony(Colony* colony);
    std::vector<std::pair<ResourceType, float>> GetResourceInfo(const LunarPoint& point) const;
    void Update();
    ResourceManager& GetResourceManager()  { return resourceManager; }
    std::vector<Colony*> GetColonies() const { return colonies;}

    // --- The anchored 100 km playfield, being retired ------------------
    //
    // The Planet view still draws the 20x20 square pinned to the terrain
    // anchor. Until it goes, these map a place on the Moon into that
    // square's world units (origin at its top-left corner) and back, and
    // the camera's "active area" is computed in it.
    static Vector2 WorldOf(const LunarPoint& point);
    static LunarPoint PointOf(Vector2 world);
    void UpdateActiveArea(const std::vector<Colony*>& colonies);
    Vector2 GetActiveCentroid() const;
    float GetActiveRadius() const;

private:
    std::vector<Colony*> colonies;
    ResourceManager resourceManager;
    int time; // Game time

    std::optional<ActiveArea> activeArea;

    ActiveArea CalculateActiveArea(const std::vector<Colony*>&) const;
};

#endif // PLANET_H
