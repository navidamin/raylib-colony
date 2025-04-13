#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <vector>
#include <map>
#include <iostream>
#include <random>
#include <algorithm>
#include <iomanip>  // for std::setw and std::setprecision
#include "raylib.h"
#include "raymath.h"

#include "game_constants.h"

class ResourceManager {
public:
    struct ResourceTile {
        std::map<ResourceType, float> resources;  // Resource type -> abundance (0.0 to 1.0)
        bool isExploited;
    };

    // Constructor
    ResourceManager(int gridSize, float cellSize);

    void GenerateResourceMap();
    std::vector<std::pair<ResourceType, float>> GetResourcesAt(Vector2 worldPos) const;
    std::vector<std::pair<ResourceType, float>> GetResourcesAtGrid(int gridX, int gridY) const;
    void DrawResourceDebug(float scale);  // For debugging resource distribution
    void EnsureBasicResources(int x, int y);  // Ensures starting location has basic resources
    void UpdateResourceDepletion(int gridX , int gridY, ResourceType type, float amount);


    void DisplayResourceGrid(Vector2& wordlPos) {
        Vector2 gridPos = WorldToGrid(wordlPos);
        int centerX = static_cast<int>(gridPos.x);
        int centerY = static_cast<int>(gridPos.y);

        std::cout << "\nResource abundances around position (" << centerX << "," << centerY << "):\n";
        std::cout << "Format for each cell: H2 | Si\n\n";


        // Check each cell in a 3x3 grid
        for(int y = centerY - 1; y <= centerY + 1; y++) {
            // Print top border for this row
            std::cout << "+-------------+-------------+-------------+\n";

            for(int x = centerX - 1; x <= centerX + 1; x++) {
                std::cout << "|";

                auto resources = GetResourcesAtGrid(x, y);
                float h2_abundance = 0.0f;
                float si_abundance = 0.0f;


                for(const auto& resource : resources) {

                    // Check what enum value corresponds to H2 and Si in your ResourceType
                    if(resource.first == ResourceType::H2) {
                        h2_abundance = resource.second;
                    }
                    if(resource.first == ResourceType::Si) {
                        si_abundance = resource.second;
                    }
                }

                // Format numbers to 2 decimal places
                std::cout << std::fixed << std::setprecision(2);

                // Highlight center cell with different format
                if(x == centerX && y == centerY) {
                    std::cout << "*";
                    std::cout << std::setw(5) << h2_abundance << "|";
                    std::cout << std::setw(5) << si_abundance;
                    std::cout << "*";
                } else {
                    std::cout << " ";
                    std::cout << std::setw(5) << h2_abundance << "|";
                    std::cout << std::setw(5) << si_abundance;
                    std::cout << " ";
                }
            }
            std::cout << "|\n";
        }
        std::cout << "+-------------+-------------+-------------+\n";
    }
private:
    int gridSize;                    // Size of the grid (20x20)
    float cellSize;                  // Size of each cell in world units
    std::vector<std::vector<ResourceTile>> resourceGrid;

    void GenerateResourceCluster(ResourceType type, Vector2 center, float radius, float maxAbundance);
    Vector2 GridToWorld(int x, int y) const;
    Vector2 WorldToGrid(Vector2 worldPos) const;



};

#endif // RESOURCE_MANAGER_H
