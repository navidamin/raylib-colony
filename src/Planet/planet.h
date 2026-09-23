#ifndef PLANET_H
#define PLANET_H

#include <vector>
#include <utility>

#include "colony.h"
#include "resource_manager.h"
#include "game_structs.h"

// The Moon, as far as the game is concerned: the owner of the ground
// truth (ResourceManager) and of the colonies on it. It has no size and
// no grid; a place on it is a LunarPoint, and each view draws in a local
// frame around whatever it is looking at (TerrainGen/lunar_frame.h).
class Planet {
public:
    Planet();
    ~Planet();

    // Seed the ground truth. 0 = a different Moon each run.
    void GenerateMap(unsigned int worldSeed = 0);
    // The planet owns the colony from here on.
    void AddColony(Colony* colony);
    std::vector<std::pair<ResourceType, float>> GetResourceInfo(const LunarPoint& point) const;
    void Update();
    ResourceManager& GetResourceManager() { return resourceManager; }
    const std::vector<Colony*>& GetColonies() const { return colonies; }

private:
    std::vector<Colony*> colonies;
    ResourceManager resourceManager;
    int time; // Game time
};

#endif // PLANET_H
