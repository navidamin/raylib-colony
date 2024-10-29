#ifndef UNIT_H
#define UNIT_H

#include "raylib.h"
#include <string>
#include <map>
#include <vector>

class Unit {
public:
    Unit(std::string type);
    ~Unit();

    void Start();
    void Stop();
    void Upgrade(int level);
    std::map<std::string, float> CalculateConsumption() const;
    std::map<std::string, float> CalculateProduction() const;
    void DisplayStats() const;
    void Update();
    void DrawInSectView(Vector2 corePosition, float coreRadius, int index);
    void DrawInUnitView();
    void SetInitialParameters();

    // Getters
    std::string GetStatus() const { return status; }
    Vector2 GetUnitPosInSectView() const { return positionInSectView;}
    float GetUnitRadiusInSectView() const { return radiusInSectView;}
    std::string GetUnitType() const { return unit_type;}

    // Setters
    void SetUnitPosInSectView(Vector2 position) {positionInSectView = position;}
    void SetUnitRadiusInSectView(float radius) {radiusInSectView = radius;}
    void SetStatus(const std::string& newStatus) { status = newStatus; }


private:
    Vector2 positionInSectView;
    float radiusInSectView;
    std::string unit_type;
    std::map<std::string, float> parameters;
    std::map<std::string, float> consumption;
    std::map<std::string, float> production;
    std::string status;
    std::vector<std::string> upgrades;
    float energy_cost;
};

#endif // UNIT_H
