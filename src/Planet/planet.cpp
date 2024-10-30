#include "planet.h"
#include <iostream>

Planet::Planet() : size(PLANET_SIZE, PLANET_SIZE), time(0) {
Planet::Planet() : size(20, 20), time(0) {
    // Initialize the map with empty tiles
    map.resize(size.first, std::vector<int>(size.second, 0));
}

Planet::~Planet() {
    for (auto colony : colonies) {
        delete colony;
    }
}

void Planet::GenerateMap() {
    std::cout << "Generating planet map..." << std::endl;
    // TODO: Implement map generation algorithm
}

void Planet::AddColony(Colony* colony) {
    colonies.push_back(colony);
    std::cout << "New colony added to the planet." << std::endl;
}

std::vector<std::string> Planet::GetResourceInfo(std::pair<int, int> location) const {
    auto it = resources.find(location);
    if (it != resources.end()) {
        return it->second;
    }
    return std::vector<std::string>(); // Return empty vector if no resources found
}

void Planet::Update() {
    time++;
    // TODO: Implement update logic (e.g., trigger events, update colonies)
    std::cout << "Planet updated. Current time: " << time << std::endl;
}

Planet::ActiveArea Planet::CalculateActiveArea(const std::vector<Colony*>& colonies) const {
    ActiveArea area = {{0, 0}, 0};

    // If no colonies, return center of planet
    if (colonies.empty()) {
        area.centroid = {
            PLANET_WIDTH / 2,
            PLANET_HEIGHT / 2
        };
        area.radius = 0;
        return area;
    }

    // Calculate centroid
    float sumX = 0, sumY = 0;
    int count = 0;

    for (const auto& colony : colonies) {
        Vector2 colonyCentroid = colony->GetCentroid();
        sumX += colonyCentroid.x;
        sumY += colonyCentroid.y;
        count++;
    }

    area.centroid.x = sumX / count;
    area.centroid.y = sumY / count;

    // Calculate radius (distance to furthest colony)
    area.radius = 0;
    for (const auto& colony : colonies) {
        Vector2 colonyCentroid = colony->GetCentroid();
        float distance = Vector2Distance(area.centroid, colonyCentroid);
        area.radius = std::max(area.radius, distance);
    }

    // Add some padding to the radius
    area.radius *= 1.2f;  // 20% padding

    return area;
}

Vector2 Planet::GetActiveCentroid() const {
    if (!activeArea.has_value()) {
        return {PLANET_WIDTH / 2, PLANET_HEIGHT / 2};
    }
    return activeArea->centroid;
}

float Planet::GetActiveRadius() const {
    if (!activeArea.has_value()) {
        return 0;
    }
    return activeArea->radius;
}

void Planet::UpdateActiveArea(const std::vector<Colony*>& colonies) {
    activeArea = CalculateActiveArea(colonies);
}

Vector2 Planet::GetRandomValidPosition() const {
Vector2 Planet::GetWorldPosition(Vector2 gridPos) const {
    return GridToWorld(gridPos.x, gridPos.y);
}

Vector2 Planet::GridToWorld(int gridX, int gridY) const {
    return Vector2{
        gridX * SECT_CORE_RADIUS * 2.0f,
        gridY * SECT_CORE_RADIUS * 2.0f
    };
}

Vector2 Planet::WorldToGrid(Vector2 worldPos) const {
    return Vector2{
        std::floor(worldPos.x / (SECT_CORE_RADIUS * 2.0f)),
        std::floor(worldPos.y / (SECT_CORE_RADIUS * 2.0f))
    };
}

void Planet::Draw(float scale) {
    // Draw the planet background and grid
        DrawPlanetGrid();
        for (const auto& colony : colonies) {
            colony->Draw(scale);  // Call Colony's Draw() for each colony
        }
}

void Planet::DrawPlanetGrid() {
    // Implement planet grid drawing logic here
    // For example:
    for (int i = 0; i < 100; i++) {
        DrawLine(i * 10, 0, i * 10, GetScreenHeight(), LIGHTGRAY);
        DrawLine(0, i * 10, GetScreenWidth(), i * 10, LIGHTGRAY);
    }
}
