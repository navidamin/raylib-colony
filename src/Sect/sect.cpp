#include "sect.h"
#include <iostream>

Sect::Sect(Vector2 &position, ResourceManager& resource, TimeManager& time)
    : resourceManager(resource),
      timeManager(time),
      defaultCoreRadius(50.0f),
      coreRadius(defaultCoreRadius),
      color(GRAY),
      SectPosition(position),
      units(),
      core(nullptr),
      development_percentage(0.0f),
      production_priority(),
      resourceStorage()
{
    // Initialize all resource types to 0
    resourceStorage[ResourceType::H2] = 0.0f;
    resourceStorage[ResourceType::O2] = 0.0f;
    resourceStorage[ResourceType::C] = 0.0f;
    resourceStorage[ResourceType::Fe] = 0.0f;
    resourceStorage[ResourceType::Si] = 0.0f;
    resourceStorage[ResourceType::ENERGY] = 0.0f;
    resourceStorage[ResourceType::WATER] = 0.0f;
    resourceStorage[ResourceType::FOOD] = 0.0f;

    CreateInitialUnits(position);
}

Sect::~Sect() {
    for (auto unit : units) {
        delete unit;
    }
}

void Sect::AddUnit(Unit* unit) {
    units.push_back(unit);
    std::cout << "New unit added to the sect." << std::endl;
}


void Sect::ConsumeResources() {
    // TODO: Implement resource consumption logic
    std::cout << "Sect resources consumed." << std::endl;
}

void Sect::BuildUnit(std::string unit_type) {
    // TODO: Implement unit building logic
    std::cout << "Building new unit of type: " << unit_type << std::endl;
}

void Sect::UpgradeUnit(Unit* unit) {
    // TODO: Implement unit upgrade logic
    std::cout << "Upgrading unit." << std::endl;
}

void Sect::Update(float deltaTime) {
    static int lastCollectionDay = -1;
    int currentDay = timeManager.GetCurrentDay();

    // Update all units
    for (Unit* unit : units) {
        if (unit) {
            unit->Update(deltaTime);
        }
    }

    // Collect resources once per day
    if (currentDay > lastCollectionDay) {
        std::cout << "\nDay " << currentDay << ": Beginning resource collection" << std::endl;

        // Collect from all units
        for (Unit* unit : units) {
            if (unit) {
                CollectResourcesFromUnit(*unit);
            }
        }

        lastCollectionDay = currentDay;
/*
        // Debug print sect storage after collection
        std::cout << "Sect storage after collection:" << std::endl;
        for (const auto& [type, amount] : resourceStorage) {
            if (amount > 0) {
                std::cout << "Resource " << static_cast<int>(type)
                         << ": " << amount << std::endl;
            }
        }
        */
    }

    // Update road construction if any are in progress
    UpdateRoadConstruction(deltaTime);
}


void Sect::UpdateRoadConstruction(float deltaTime) {
    // Update each road under construction
    auto it = roadsUnderConstruction.begin();
    while (it != roadsUnderConstruction.end()) {
        it->progress += deltaTime;

        if (it->progress >= it->totalTime) {
            // Road construction complete
            // Add to completed roads list (implementation depends on your road system)
            it = roadsUnderConstruction.erase(it);
        } else {
            ++it;
        }
    }
}

void Sect::CollectResourcesFromUnit(Unit& unit) {
    std::map<ResourceType, float> collectedResources;

    unit.DischargeAllResources(collectedResources);

    // Add collected resources to sect storage
    for (const auto& [type, amount] : collectedResources) {
        if (amount > 0) {
            resourceStorage[type] += amount;
            std::cout << "Sect collected " << amount << " of resource type "
                     << static_cast<int>(type) << " from unit type "
                     << unit.GetUnitType() << std::endl;
        }
    }
}

void Sect::CreateInitialUnits(Vector2& position) {
    std::vector<std::string> unit_types = {
        "Extraction", "Farming", "Manufacture", "Transport", "Communication", "Research","Energy", "Construction"
    };

    for (const auto& type : unit_types) {
        Unit* unit = new Unit(type, position, resourceManager, timeManager);
        if (type == "Extraction") {
            unit->Start();
            core = unit; // Set the Extraction unit as the core
        } else {
            unit->Stop();
        }
        AddUnit(unit);
    }

    std::cout << "All initial units created for the sect." << std::endl;
}


void Sect::DrawInColonyView(Vector2 pos, float scale) {
    coreRadius = defaultCoreRadius * scale; // Scale the radius based on zoom level

    // Draw main sect circle (smaller in Colony view)
    DrawCircle(pos.x, pos.y, coreRadius, color);
    //DrawCircleLines(pos.x, pos.y, radius, BLACK);



    // Draw active units indicator as small dots around the sect
    float indicatorRadius = coreRadius * 0.3f;

    for (size_t i = 0; i < units.size(); i++) {
        float angle = (90.0f - (i * 45.0f)) * DEG2RAD;  // 8 units, 45 degrees apart
        Vector2 indicatorPos = {
            pos.x + (coreRadius * 1.4f) * cosf(angle),
            pos.y - (coreRadius * 1.4f) * sinf(angle)
        };

        if ( units[i]->GetStatus() == "active" ) {DrawCircle(indicatorPos.x, indicatorPos.y, indicatorRadius, GREEN);}
        else {DrawCircle(indicatorPos.x, indicatorPos.y, indicatorRadius, GRAY);}
    }

    // Draw development percentage as a progress arc
    if (development_percentage > 0) {
        DrawRing(
            pos,
            coreRadius * 1.1f,
            coreRadius * 1.2f,
            0,
            development_percentage * 360,
            32,
            Fade(GREEN, 0.5f)
        );
    }
}


void Sect::DrawInSectView(Vector2 position) {
    float coreRadius = GetScreenHeight() * 0.28f;  // Core takes 60% of screen height

    // Draw the main core circle
    DrawCircle(position.x, position.y, coreRadius, LIGHTGRAY);
    DrawCircleLines(position.x, position.y, coreRadius, BLACK);

    // Draw core information
    DrawText(TextFormat("Development: %.1f%%", development_percentage * 100),
            position.x - MeasureText("Development: 100.0%", 20)/2,
            position.y - 10,
            20,
            BLACK);

    // Draw resource stats in the core
    DrawResourceStats(position, coreRadius);

    // Draw the units around the core
    float unitRadius = coreRadius * 0.2f;  // Units are 20% the size of core
    float orbitRadius = coreRadius * 1.3f; // Distance from core to units

    for (size_t i = 0; i < units.size(); ++i) {
        // Start from 90 degrees (top) and go clockwise
        float angle = (90.0f - (i * 45.0f)) * DEG2RAD;  // 8 units, 45 degrees apart

        Vector2 unitPos = {
            position.x + orbitRadius * cosf(angle),
            position.y - orbitRadius * sinf(angle)  // Subtract because Y grows downward
        };

        // Store the position for click detection
        units[i]->SetUnitPosInSectView(unitPos);
        units[i]->SetUnitRadiusInSectView(unitRadius);

        // Draw the unit circle
        Color fillColor = units[i]->GetStatus() == "active" ? GREEN : GRAY;
        DrawCircle(unitPos.x, unitPos.y, unitRadius, fillColor);
        DrawCircleLines(unitPos.x, unitPos.y, unitRadius, BLACK);

        // Draw first letter of unit type (only in Sect view)
        const char* unitType = units[i]->GetUnitType().c_str();
        char firstLetter[2] = {unitType[0], '\0'};

        // Center the letter in the circle
        int fontSize = (int)(unitRadius);
        Vector2 textSize = MeasureTextEx(GetFontDefault(), firstLetter, fontSize, 1);
        Vector2 textPos = {
            unitPos.x - textSize.x/2,
            unitPos.y - textSize.y/2
        };

        DrawText(firstLetter, textPos.x, textPos.y, fontSize, BLACK);
    }

    // Draw the transparent right panel
    DrawTransparentRightPanel();
}

void Sect::DrawResourceStats(Vector2 position, float coreRadius) {
    // Draw production/consumption stats in the core
    const float statsY = position.y - coreRadius * 0.5f;
    const float statsSpacing = 25;
    int statIndex = 0;

    // Example resource stats (replace with actual resource tracking)
    std::vector<std::pair<const char*, int>> stats = {
        {"Energy: ", 100},
        {"Iron: ", 50},
        {"Food: ", 75},
    };

    for (const auto& stat : stats) {
        const char* text = TextFormat("%s%d", stat.first, stat.second);
        DrawText(text,
                position.x - MeasureText(text, 20)/2,
                statsY + statIndex * statsSpacing,
                20,
                BLACK);
        statIndex++;
    }
}


void Sect::DrawTransparentRightPanel() {
    int panelWidth = 100;
    Rectangle panel = {
        (float)GetScreenWidth() - panelWidth,
        0,
        (float)panelWidth,
        (float)GetScreenHeight()
    };
    DrawRectangleRec(panel, Fade(WHITE, 0.5f));

    // Draw panel content (e.g., notifications, alerts)
    DrawText("Updates",
            GetScreenWidth() - panelWidth + 10,
            10,
            20,
            BLACK);
}
