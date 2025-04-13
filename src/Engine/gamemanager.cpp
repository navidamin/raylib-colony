#include "gamemanager.h"
#include <iostream>

GameManager::GameManager()
    : planet(new Planet()),
      currentColony(nullptr),
      currentSect(nullptr),
      currentUnit(nullptr),
      lastUpdateTime(0.0f)
{
}

GameManager::~GameManager() {
    delete planet;

    /*
    // Unregister all colonies from time manager before deletion
    for (Colony* colony : colonies) {
        timeManager.UnregisterColony(colony);
        delete colony;
    }
    colonies.clear();
    */
}

void GameManager::InitGame() {
    // Initialize time manager
    lastUpdateTime = GetTime();  // Set initial time
    timeManager.Reset();         // Reset time manager to initial state

    // Generate map/grid/resource map of the planet
    planet->GenerateMap();

    // Create initial colony
    Colony* firstColony = new Colony();
    colonies.push_back(firstColony);
    currentColony = firstColony;

    // Create initial sect with a position near the center of the map
    Vector2 initialPosition = planet->GetRandomValidPosition();
    Sect* firstSect = new Sect(initialPosition, planet->GetResourceManager(), timeManager);

    // Notify planet about sect position to ensure resources
    planet->NotifyFirstSectPosition(initialPosition);

    // Add sect to colony
    currentColony->AddSect(firstSect);
    currentSect = firstSect;

    UpdatePlanetActiveArea();
}

void GameManager::Update(float deltaTime) {
    timeManager.Update(deltaTime);

    // Update colonies, sects, and units
    for (auto& colony : colonies) {
        /*
        // Register the colony if note registered to timeManager
        if (!timeManager.IsColonyRegistered(colony)) {
         timeManager.RegisterColony(colony);
        }
        */

        // Loop over sects to update the sects and units
        for (auto& sect: colony->GetSects()) {
            sect->Update(deltaTime);
            for (auto& unit : sect->GetUnits()) {
                if (unit->IsActive()) {
                    unit->Update(deltaTime);
                }
            }
        }
    }
}

void GameManager::SelectColony(Vector2 mousePosition) {
    Vector2 worldMousePos = mousePosition;  // Already in world coords

    for (auto& colony : colonies) {
        Vector2 colonyWorldPos = colony->GetCentroid();  // Should return world coordinates
        if (Vector2Distance(worldMousePos, colonyWorldPos) <= colony->GetRadius()) {
            currentColony = colony;
            break;
        }
    }
}

void GameManager::SelectSect(Vector2 mousePosition, Camera2D camera) {
    if (currentColony) {
        // Convert screen coordinates to world coordinates using the camera
        Vector2 worldMousePos = GetScreenToWorld2D(mousePosition, camera);

        for (auto& sect : currentColony->GetSects()) {
            Vector2 sectPos = sect->GetPosition();
            float sectRadius = sect->GetRadius();

            if (Vector2Distance(worldMousePos, sectPos) <= sectRadius) {
                currentSect = sect;
                break;
            }
        }
    }
}

void GameManager::SelectUnit(Vector2 mousePosition) {
    if (currentSect) {
        for (auto& unit : currentSect->GetUnits()) {
            if (Vector2Distance(mousePosition, unit->GetUnitPosInSectView()) <= unit->GetUnitRadiusInSectView()) {
                currentUnit = unit;
                break;
            }
        }
    }
}

void GameManager::BuildNewColony(Vector2 worldPos) {
    bool intrudingOtherColony = false;
    for (Colony* colony : colonies) {
        if (CheckCollisionPointCircle(worldPos, colony->GetCentroid(), colony->GetRadius())) {
            intrudingOtherColony = true;
            break;
        }
    }

    if (!intrudingOtherColony) {
        Colony* colony = new Colony();
        colonies.push_back(colony);
        currentColony = colony;

        Sect* sect = new Sect(worldPos, planet->GetResourceManager(), timeManager);
        currentColony->AddSect(sect);
        currentSect = sect;
        std::cout << "Colony and sect created successfully\n";
    } else {
        std::cout << "Intruding the jurisdiction of another Colony!\n";
    }
}

void GameManager::BuildNewSect(Vector2 worldPos) {
    bool intrudingOtherColony = false;
    for (Colony* colony : colonies) {
        if (colony == currentColony) continue;
        if (CheckCollisionPointCircle(worldPos, colony->GetCentroid(), colony->GetRadius())) {
            intrudingOtherColony = true;
            break;
        }
    }

    if (!intrudingOtherColony) {
        if (currentColony) {
            Sect* sect = new Sect(worldPos, planet->GetResourceManager(), timeManager);
            currentColony->AddSect(sect);
            currentSect = sect;
            std::cout << "New sect created successfully\n";
        } else {
            std::cout << "Current colony unknown!" << std::endl;
        }
    } else {
        std::cout << "Intruding the jurisdiction of another Colony!\n";
    }
}

void GameManager::UpdatePlanetActiveArea() {
    if (planet) {
        planet->UpdateActiveArea(colonies);
    }
}
