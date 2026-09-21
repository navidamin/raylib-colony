#include "planet.h"
#include "terrain_synthesis.h"
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

// ---------------------------------------------------------------------------
// The anchored playfield square. The frame is centred on the terrain
// anchor; the square's world origin sits half a playfield up and left of
// it, which is what TerrainGridCellToLatLon's (gx - 9.5) offsets say.
// ---------------------------------------------------------------------------

static LocalFrame PlayfieldFrame()
{
    LocalFrame f;
    GetTerrainAnchor(&f.centre.latDeg, &f.centre.lonDeg);
    return f;
}

Vector2 Planet::WorldOf(const LunarPoint& point)
{
    Vector2 local = PlayfieldFrame().ToLocal(point);
    return Vector2{ local.x + PLANET_WIDTH * 0.5f, local.y + PLANET_HEIGHT * 0.5f };
}

LunarPoint Planet::PointOf(Vector2 world)
{
    Vector2 local = { world.x - PLANET_WIDTH * 0.5f, world.y - PLANET_HEIGHT * 0.5f };
    return PlayfieldFrame().FromLocal(local);
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
        Vector2 colonyCentroid = WorldOf(colony->GetCentre());
        sumX += colonyCentroid.x;
        sumY += colonyCentroid.y;
        count++;
    }

    area.centroid.x = sumX / count;
    area.centroid.y = sumY / count;

    // Calculate radius (distance to furthest colony)
    area.radius = 0;
    for (const auto& colony : colonies) {
        Vector2 colonyCentroid = WorldOf(colony->GetCentre());
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
