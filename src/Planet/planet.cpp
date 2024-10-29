#include "planet.h"
#include <iostream>

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
