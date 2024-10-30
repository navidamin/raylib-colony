#ifndef PLANET_H
#define PLANET_H

#include "raylib.h"
#include <vector>
#include <map>
#include <utility>
#include <optional>
#include <memory>
#include "colony.h"
#include "game_constants.h"
#include "game_structs.h"


class Planet {
public:
    Planet();
    ~Planet();

    struct ActiveArea {
        Vector2 centroid;
        float radius;  // Distance from centroid to furthest colony
    };

    void GenerateMap();
    void AddColony(Colony* colony);
    std::vector<std::string> GetResourceInfo(std::pair<int, int> location) const;
    void Update();
    void Draw(float scale);
    void DrawPlanetGrid();
    void UpdateActiveArea(const std::vector<Colony*>& colonies);
    Vector2 GetActiveCentroid() const;
    float GetActiveRadius() const;
    Vector2 GetWorldPosition(Vector2 gridPos) const;


private:
    std::vector<std::vector<int>> map; // 2D grid representing the planet's surface
    std::vector<Colony*> colonies;
    std::map<std::pair<int, int>, std::vector<std::string>> resources; // Resources at each location
    std::pair<int, int> size; // Planet dimensions
    int time; // Game time

    std::optional<ActiveArea> activeArea;

    ActiveArea CalculateActiveArea(const std::vector<Colony*>&) const;
    Vector2 GridToWorld(int gridX, int gridY) const;
    Vector2 WorldToGrid(Vector2 worldPos) const;

};

#endif // PLANET_H
