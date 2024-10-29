#ifndef PLANET_H
#define PLANET_H

#include "raylib.h"
#include <vector>
#include <map>
#include <utility>
#include "colony.h"

class Planet {
public:
    Planet();
    ~Planet();

    void GenerateMap();
    void AddColony(Colony* colony);
    std::vector<std::string> GetResourceInfo(std::pair<int, int> location) const;
    void Update();
    void Draw(float scale);
    void DrawPlanetGrid();

private:
    std::vector<std::vector<int>> map; // 2D grid representing the planet's surface
    std::vector<Colony*> colonies;
    std::map<std::pair<int, int>, std::vector<std::string>> resources; // Resources at each location
    std::pair<int, int> size; // Planet dimensions
    int time; // Game time
};

#endif // PLANET_H
