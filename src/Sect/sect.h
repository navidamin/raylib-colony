#ifndef SECT_H
#define SECT_H

#include "raylib.h"
#include <vector>
#include <utility>
#include <map>
#include "unit.h"
#include <cmath>  // Add this for cosf, sinf, etc.

class Sect {
public:
    Sect();
    ~Sect();

    void AddUnit(Unit* unit);
    void CalculateProduction();
    void ConsumeResources();
    void BuildUnit(std::string unit_type);
    void UpgradeUnit(Unit* unit);
    void Update();
    void Draw(Vector2 position);
    void DrawInColonyView(Vector2 position, float scale);
    void DrawInSectView(Vector2 position);

    // Setters
    void SetPosition(Vector2 position) {SectPosition = position;}

    // Getters
    Vector2 GetPosition() const {return SectPosition;}
    const std::vector<Unit*>& GetUnits() const { return units; }
    float GetRadius() const { return coreRadius; }

private:
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
