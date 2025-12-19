#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"
#include <vector>
#include <string>

class Entity {
public:
    // Virtual destructor for proper cleanup in derived classes
    virtual ~Entity() = default;

    // Common position-related functions
    virtual Vector2 GetPosition() const = 0;
    virtual void SetPosition(Vector2 position) = 0;

    // Common geometric properties
    virtual float GetRadius() const = 0;

    // Common drawing functions
    virtual void Draw(Vector2 position) = 0;

    // Common update function
    virtual void Update(float deltaTime) = 0;

    // Resource management functions
    virtual void CalculateProduction() = 0;
    virtual void ConsumeResources() = 0;

    // Unit management (for collections of units or modules)
    virtual void AddUnit(void* unit) = 0;  // void* to avoid circular dependencies
    virtual const std::vector<void*>& GetUnits() const = 0;

    // Inquirt Managment
    void sendInquiry(UnitInquiry* inquiry);

};

#endif // ENTITY_H
