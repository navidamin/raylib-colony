#include "resource_manager.h"


ResourceManager::ResourceManager(int gridSize, float cellSize)
    : gridSize(gridSize), cellSize(cellSize) {
    resourceGrid.resize(gridSize, std::vector<ResourceTile>(gridSize));
}

void ResourceManager::GenerateResourceMap() {
    std::cout << "Starting resource map generation for grid size: " << gridSize << std::endl;

    // Clear existing resources
    resourceGrid = std::vector<std::vector<ResourceTile>>(
        gridSize, std::vector<ResourceTile>(gridSize)
    );

    // Reset the grid
    for (auto& row : resourceGrid) {
        for (auto& tile : row) {
            tile.resources.clear();
            tile.isExploited = false;
        }
    }

    // Random number generation
    std::random_device rd;
    std::mt19937 gen(rd());
    // Use more conservative bounds for cluster centers
    // Ensure clusters stay within grid even with radius
    int margin = 1;  // Larger margin to prevent overflow
    std::uniform_int_distribution<> positionDist(margin, gridSize - margin - 1);
    std::uniform_real_distribution<> radiusDist(2.0f, 5.0f);  // Reduced radius

    // Generate clusters for each resource type
    for (int i = 0; i < gridSize; i++) {  // Generate multiple clusters per resource
        // Generate center coordinates as integers
        int centerX = positionDist(gen);
        int centerY = positionDist(gen);

        Vector2 center = {
            static_cast<float>(centerX),
            static_cast<float>(centerY)
        };

        float radius = radiusDist(gen);

        std::cout << "\nGenerating cluster set " << i + 1
                  << " at position (" << centerX << ", " << centerY
                  << ") with base radius " << radius << std::endl;


        GenerateResourceCluster(ResourceType::H2, center, radius, 0.8f);
        GenerateResourceCluster(ResourceType::O2, center, radius * 0.8f, 0.7f);
        GenerateResourceCluster(ResourceType::C, center, radius * 1.2f, 0.6f);
        GenerateResourceCluster(ResourceType::Fe, center, radius * 0.6f, 0.9f);
        GenerateResourceCluster(ResourceType::Si, center, radius, 0.75f);
    }

}

void ResourceManager::GenerateResourceCluster(ResourceType type, Vector2 center, float radius, float maxAbundance) {
    // Strict bounds checking
    int startX = std::max(0, static_cast<int>(center.x - radius));
    int endX = std::min(gridSize - 1, static_cast<int>(center.x + radius));
    int startY = std::max(0, static_cast<int>(center.y - radius));
    int endY = std::min(gridSize - 1, static_cast<int>(center.y + radius));

    std::cout << "Cluster bounds: (" << startX << "," << startY
              << ") to (" << endX << "," << endY << ")" << std::endl;

    // Add extra validation in the drawing loop
    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            // Double-check bounds
            if (x < 0 || x >= gridSize || y < 0 || y >= gridSize) {
                std::cout << "ERROR: Attempted to access out-of-bounds position: ("
                          << x << "," << y << ")" << std::endl;
                continue;
            }

            Vector2 pos = {static_cast<float>(x), static_cast<float>(y)};
            float dist = Vector2Distance(center, pos);

            if (dist <= radius) {
                float abundance = maxAbundance * (1.0f - (dist / radius));
                resourceGrid[y][x].resources[type] = std::max(
                    abundance,
                    resourceGrid[y][x].resources[type]
                );
            }
        }
    }
}

void ResourceManager::EnsureBasicResources(int x, int y) {
    ResourceTile& tile = resourceGrid[y][x];
    std::map<ResourceType, float> minValues = {
        {ResourceType::H2, 0.3f},
        {ResourceType::O2, 0.3f},
        {ResourceType::C, 0.3f},
        {ResourceType::Fe, 0.3f},
        {ResourceType::Si, 0.2f}
    };

    for (const auto& [type, minValue] : minValues) {
        tile.resources[type] = std::max(tile.resources[type], minValue);
    }
}

std::vector<std::pair<ResourceType, float>> ResourceManager::GetResourcesAt(Vector2 worldPos) const{
    Vector2 gridPos = WorldToGrid(worldPos);
    return GetResourcesAtGrid(static_cast<int>(gridPos.x), static_cast<int>(gridPos.y));
}

std::vector<std::pair<ResourceType, float>> ResourceManager::GetResourcesAtGrid(int gridX, int gridY) const {
    if (gridX < 0 || gridX >= gridSize || gridY < 0 || gridY >= gridSize) {
        return {};
    }

    std::vector<std::pair<ResourceType, float>> result;
    for (const auto& [type, abundance] : resourceGrid[gridY][gridX].resources) {
        if (abundance > 0.0f) {
            result.push_back({type, abundance});
        }
    }
    return result;
}

Vector2 ResourceManager::GridToWorld(int x, int y) const {
    // Clamp grid coordinates
    x = Clamp(x, 0, gridSize - 1);
    y = Clamp(y, 0, gridSize - 1);

    return {x * cellSize, y * cellSize};
}

Vector2 ResourceManager::WorldToGrid(Vector2 worldPos) const {
    // Ensure world coordinates are within bounds
    worldPos.x = Clamp(worldPos.x, 0.0f, cellSize * (gridSize - 1));
    worldPos.y = Clamp(worldPos.y, 0.0f, cellSize * (gridSize - 1));

    // Convert to grid coordinates
    return {
        std::floor(worldPos.x / cellSize),
        std::floor(worldPos.y / cellSize)
    };
}

void ResourceManager::UpdateResourceDepletion(int x , int y, ResourceType type, float amount) {
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize) {
        resourceGrid[y][x].resources[type] =
            std::max(0.0f, resourceGrid[y][x].resources[type] - amount);
    }
    //std::cout << "Resource " << type << " was depleted " << amount << "units" << std::endl;
}

void ResourceManager::DrawResourceDebug(float scale) {
    for (int y = 0; y < gridSize; y++) {
        for (int x = 0; x < gridSize; x++) {
            // Add bounds checking and debug output
            if (x >= gridSize || y >= gridSize) {
                std::cout << "ERROR: Attempting to draw outside grid bounds at ("
                          << x << "," << y << ")" << std::endl;
                continue;
            }

            Vector2 pos = GridToWorld(x, y);
            const auto& tile = resourceGrid[y][x];

            // Debug print for non-empty tiles
            if (!tile.resources.empty()) {
                std::cout << "Resources at (" << x << "," << y << "): ";
                for (const auto& [type, abundance] : tile.resources) {
                    std::cout << static_cast<int>(type) << ":" << abundance << " ";
                }
                std::cout << std::endl;
            }

            // Draw the most abundant resource in this tile
            if (!tile.resources.empty()) {
                auto maxResource = std::max_element(
                    tile.resources.begin(),
                    tile.resources.end(),
                    [](const auto& a, const auto& b) { return a.second < b.second; }
                );

                Color color = ResourceUtils::GetResourceColor(maxResource->first);
                color.a = static_cast<unsigned char>(255 * maxResource->second);

                DrawRectangle(
                    pos.x, pos.y,
                    cellSize, cellSize,
                    color
                );
            }
        }
    }
}
