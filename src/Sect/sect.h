#ifndef SECT_H
#define SECT_H

#include "raylib.h"
#include <vector>
#include <utility>
#include <map>
#include "unit.h"
#include <cmath>  // Add this for cosf, sinf, etc.

#include "resource_manager.h"
#include "game_enums.h"

class Sect {
public:
    // constructor
    Sect(ResourceManager* resource);

    // deconstructor
    ~Sect();

    void AddUnit(Unit* unit);
    void CalculateProduction();
    void ConsumeResources();
    void BuildUnit(std::string unit_type);
    void UpgradeUnit(Unit* unit);
    void Update(float deltaTime);
    void Draw(Vector2 position);
    void DrawInColonyView(Vector2 position, float scale);
    void DrawInSectView(Vector2 position);

    // Setters
    void SetPosition(Vector2 position) {SectPosition = position;}

    // Getters
    Vector2 GetPosition() const {return SectPosition;}
    const std::vector<Unit*>& GetUnits() const { return units; }
    float GetRadius() const { return coreRadius; }

    // Transportation processing
    void UpdateRoadConstruction(float deltaTime);

private:
    ResourceManager* resourceManager;

    struct RoadConstruction {
        Vector2 startPos;
        Vector2 endPos;
        float progress = 0.0f;
        float totalTime = 30.0f; // 30 seconds to build a road
    };

    std::vector<RoadConstruction> roadsUnderConstruction;
    // Geometric/Visual properties (basic types first)
    float defaultCoreRadius;        // Constant value
    float coreRadius;               // Derived from default
    Color color;                    // Visual property

    // Position/Location data
    Vector2 SectPosition;           // Position in world space
    std::pair<int, int> location;   // Grid location

    // Core gameplay elements
    std::vector<Unit*> units;       // Collection of units
    Unit* core;                     // Reference to core unit
    float development_percentage;    // Progress tracking

    // Resource management
    std::vector<std::string> production_priority;  // Order of production
    std::map<std::string, int> resources;         // Resource storage

    // Private member functions
    void CreateInitialUnits();
    void DrawTransparentRightPanel();
    void DrawResourceStats(Vector2 position, float coreRadius);
};

#endif // SECT_H
