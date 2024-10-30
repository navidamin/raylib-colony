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
    std::vector<std::pair<ResourceType, float>> GetResourceInfo(Vector2 location) const;
    void Update();
    void Draw(float scale);
    void DrawPlanetGrid();
    void UpdateActiveArea(const std::vector<Colony*>& colonies);
    Vector2 GetRandomValidPosition() const;
    void NotifyFirstSectPosition(Vector2 position);
    Vector2 GetActiveCentroid() const;
    float GetActiveRadius() const;
    void DrawResourceDebug(float scale);
    Vector2 GetWorldPosition(Vector2 gridPos) const;


private:
    std::vector<std::vector<int>> map; // 2D grid representing the planet's surface
    std::vector<Colony*> colonies;
    std::unique_ptr<ResourceManager> resourceManager;
    std::map<std::pair<int, int>, std::vector<std::string>> resources; // Resources at each location
    std::pair<int, int> size; // Planet dimensions
    int time; // Game time

    std::optional<ActiveArea> activeArea;

    ActiveArea CalculateActiveArea(const std::vector<Colony*>&) const;
    Vector2 GridToWorld(int gridX, int gridY) const;
    Vector2 WorldToGrid(Vector2 worldPos) const;

};

#endif // PLANET_H
