#ifndef COLONY_H
#define COLONY_H

#include "raylib.h"
#include <vector>
#include <utility>
#include "sect.h"

class Colony {
public:
    Colony();
    ~Colony();

    void AddSect(Sect* sect);
    void BuildRoad(Sect* sect_a, Sect* sect_b);
    void ManageResources();
    void UnlockResearch();
    void Draw(float scale);
    void CalculateCentroid();

    // Getters
    Vector2 GetCentroid() const {return centroid;}
    float GetRadius() const {return jurisdiction_radius;}
    const std::vector<Sect*>& GetSects() const {return sects;}



private:
    std::vector<Sect*> sects;
    Vector2 centroid;
    float jurisdiction_radius;
    std::map<std::string, int> available_resources;
    std::vector<std::pair<Sect*, Sect*>> roads;
    int research_level;


    // Add transport_network when implemented
};

#endif // COLONY_H
