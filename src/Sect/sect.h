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

// CLITERAL is raylib's portability shim: it expands to `(Color)` in C and
// to nothing in C++. Writing the C compound literal `(Color){...}` directly
// is a GCC/Clang extension that MSVC rejects outright (error C4576).
#define CHINAROSE CLITERAL(Color){ 160, 70, 104, 255 }


// Which way a unit dome faces.
//
// A unit dome carries a single socket, and it has to look back at the
// sect core so the connector arm meets it square. That follows entirely
// from where the unit stands relative to the core: no state to keep, no
// index to pass around, and it stays right if a unit is ever moved off
// the even ring.
//
// Screen space, so y grows downward, and the angle is degrees clockwise
// from the +x axis - the convention raylib's rotations already use.
inline float UnitSocketDirection(Vector2 unitPos, Vector2 corePos)
{
    return atan2f(corePos.y - unitPos.y, corePos.x - unitPos.x) * RAD2DEG;
}

// Where that socket sits on the dome's rim.
inline Vector2 UnitSocketPosition(Vector2 unitPos, Vector2 corePos,
                                  float rimRadius)
{
    float a = UnitSocketDirection(unitPos, corePos) * DEG2RAD;
    return Vector2{ unitPos.x + cosf(a) * rimRadius,
                    unitPos.y + sinf(a) * rimRadius };
}

// The rotation to draw a unit dome SPRITE at, so its socket ends up
// pointing at the core. socketAngleInArt is where the socket sits in the
// source art, measured the same way: the dome sheet draws it at the
// bottom of the sprite, which is +90 degrees with y down.
inline float UnitSpriteRotation(Vector2 unitPos, Vector2 corePos,
                                float socketAngleInArt = 90.0f)
{
    return UnitSocketDirection(unitPos, corePos) - socketAngleInArt;
}


class Sect {
public:
    // constructor
    Sect(Vector2& position, ResourceManager& resource, TimeManager& time);

    // deconstructor
    ~Sect();



    void AddUnit(Unit* unit);
    void CalculateProduction();
    void ConsumeResources();
    void BuildUnit(std::string unit_type);
    void UpgradeUnit(Unit* unit);
    void Update(float deltaTime);
    void Draw(Vector2 position);
    void DrawInColonyView(Vector2 position);
    void DrawInSectView(Vector2 position);

    // Setters
    void SetPosition(Vector2 position) {SectPosition = position;}

    // Getters
    Vector2 GetPosition() const {return SectPosition;}
    const std::vector<Unit*>& GetUnits() const { return units; }
    float GetRadius() const { return coreRadius; }
    const std::map<ResourceType, float>& GetResourceStorage() const { return resourceStorage; }
    const std::map<ResourceType, float>& GetStorageCapacity() const { return storageCapacity; }
    float GetStorageUsage(ResourceType type) const;

    // Typed resource getters
    const std::map<ResourceType, std::vector<TypedResource>>& GetTypedResources() const { return typedResourceStorage; }
    int GetTypedResourceCount(ResourceType type, const std::string& subtype) const;
    int GetTotalTypedResourceCount(ResourceType type) const;

    // Resource management methods
    void PushSurplusToColony(class Colony* colony);
    void PullDeficitFromColony(class Colony* colony);
    bool CanAcceptResource(ResourceType type, float amount) const;
    bool IsDeficit(ResourceType type) const;
    bool IsSurplus(ResourceType type) const;
    float GetResourceStorage(ResourceType type) const;
    float GetStorageCapacity(ResourceType type) const;
    void AddResource(ResourceType type, float amount);
    void ConsumeResource(ResourceType type, float amount);

    // Typed resource management
    bool AddTypedResource(const TypedResource& resource);
    bool RemoveTypedResource(ResourceType type, const std::string& subtype);
    bool HasTypedResource(ResourceType type, const std::string& subtype) const;

    // Ambient energy generation
    void GenerateAmbientEnergy(float deltaTime, float timeOfDay);

    // Storage upgrades
    int GetStorageLevel() const { return storageLevel; }
    bool CanUpgradeStorage() const;
    void UpgradeStorage();

    // Transportation processing
    void UpdateRoadConstruction(float deltaTime);

private:
    ResourceManager& resourceManager;
    TimeManager& timeManager;

    struct RoadConstruction {
        Vector2 startPos;
        Vector2 endPos;
        float progress = 0.0f;
        float totalTime = 30.0f; // 30 seconds to build a road
    };

    std::vector<RoadConstruction> roadsUnderConstruction;
    // Geometric/Visual properties (basic types first)
    float defaultCoreRadius = SECT_CORE_RADIUS;        // Constant value
    float coreRadius;               // Derived from default
    Color color;                    // Visual property

    // Texture assets for visual rendering
    Texture2D domeTexture;                          // Central dome texture
    std::map<std::string, Texture2D> unitTextures;  // Unit type -> texture mapping

    // Position/Location data
    Vector2 SectPosition;           // Position in world space
    std::pair<int, int> location;   // Grid location

    // Core gameplay elements
    std::vector<Unit*> units;       // Collection of units
    Unit* core;                     // Reference to core unit
    float development_percentage;    // Progress tracking

    // Resource management (singular resources)
    std::vector<std::string> production_priority;  // Order of production
    std::map<ResourceType, float> resourceStorage;
    std::map<ResourceType, float> storageCapacity;
    int storageLevel = 0;  // Storage upgrade level (0-3)

    // Typed resource storage (MACHINERY, ELECTRONICS, ALLOYS, CONSTRUCTION_MATERIALS)
    std::map<ResourceType, std::vector<TypedResource>> typedResourceStorage;
    static const int TYPED_RESOURCE_CAPACITY = 50;  // Max items per type

    // Private member functions
    void CreateInitialUnits(Vector2 &position);
    void DrawTransparentRightPanel();
    void DrawResourceStats(Vector2 position, float coreRadius);

    // Texture management
    void LoadTextures();    // Load dome and unit textures
    void UnloadTextures();  // Free texture memory
};

#endif // SECT_H
