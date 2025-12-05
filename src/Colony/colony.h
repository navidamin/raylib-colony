#ifndef COLONY_H
#define COLONY_H

#include "raylib.h"
#include <vector>
#include <utility>
#include <map>
#include "sect.h"
#include "resource_types.h"

class Colony {
public:
    Colony();
    ~Colony();

    void AddSect(Sect* sect);
    void BuildRoad(Sect* sect_a, Sect* sect_b);
    void ManageResources();
    void UnlockResearch();
    void Draw(Camera2D &camera);
    void CalculateCentroid();
    void CalculateRadius();
    void DrawJurisdiction();

    // Getters
    Vector2 GetCentroid() const {return centroid;}
    float GetRadius() const {return jurisdiction_radius;}
    const std::vector<Sect*>& GetSects() const {return sects;}
    const std::map<ResourceType, float>& GetStrategicReserves() const {return strategicReserves;}
    const std::map<ResourceType, float>& GetReserveCapacity() const {return reserveCapacity;}
    float GetReserveUsage(ResourceType type) const;

    // Resource management
    bool ReceiveSurplus(ResourceType type, float amount);
    bool CanAcceptResource(ResourceType type, float amount) const;



private:
    std::vector<Sect*> sects;
    Vector2 centroid;
    float jurisdiction_radius;
    std::map<std::string, int> available_resources;
    std::vector<std::pair<Sect*, Sect*>> roads;
    int research_level;

    // Strategic resource reserves
    std::map<ResourceType, float> strategicReserves;
    std::map<ResourceType, float> reserveCapacity;

    // Add transport_network when implemented
};

#endif // COLONY_H
