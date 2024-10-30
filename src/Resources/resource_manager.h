#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <vector>
#include <map>
#include <iostream>
#include <random>
#include <algorithm>
#include "raylib.h"
#include "raymath.h"
#include "resource_types.h"

class ResourceManager {
public:
    struct ResourceTile {
        std::map<ResourceType, float> resources;  // Resource type -> abundance (0.0 to 1.0)
        bool isExploited;
    };

    ResourceManager(int gridSize, float cellSize);
    void GenerateResourceMap();
    std::vector<std::pair<ResourceType, float>> GetResourcesAt(Vector2 worldPos);
    std::vector<std::pair<ResourceType, float>> GetResourcesAtGrid(int gridX, int gridY);
    void DrawResourceDebug(float scale);  // For debugging resource distribution
    void EnsureBasicResources(int x, int y);  // Ensures starting location has basic resources

private:
    int gridSize;                    // Size of the grid (20x20)
    float cellSize;                  // Size of each cell in world units
    std::vector<std::vector<ResourceTile>> resourceGrid;

    void GenerateResourceCluster(ResourceType type, Vector2 center, float radius, float maxAbundance);
    Vector2 GridToWorld(int x, int y) const;
    Vector2 WorldToGrid(Vector2 worldPos) const;

};

#endif // RESOURCE_MANAGER_H
