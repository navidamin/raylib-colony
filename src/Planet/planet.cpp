#include "planet.h"
#include <iostream>

Planet::Planet() :
    resourceManager(0),
    time(0)
{
}

Planet::~Planet() {
    for (auto colony : colonies) {
        delete colony;
    }
}

void Planet::GenerateMap(unsigned int worldSeed) {
    resourceManager.SetWorldSeed(worldSeed);
}

void Planet::AddColony(Colony* colony) {
    colonies.push_back(colony);
    std::cout << "New colony added to the planet." << std::endl;
}

std::vector<std::pair<ResourceType, float>> Planet::GetResourceInfo(const LunarPoint& point) const {
    return resourceManager.GetResourcesAt(point);
}

void Planet::Update() {
    time++;
    // TODO: Implement update logic (e.g., trigger events, update colonies)
}
