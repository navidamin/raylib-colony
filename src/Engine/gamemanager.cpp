#include "gamemanager.h"
#include <iostream>

GameManager::GameManager()
    : planet(new Planet()),
      currentColony(nullptr),
      currentSect(nullptr),
      currentUnit(nullptr),
      selectedRoad(nullptr),
      buildRoadMode(false),
      roadBuildStartSect(nullptr),
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

        // Manage colony resources (push surplus from sects to colony reserves)
        colony->ManageResources();

        // Process transport jobs
        colony->ProcessTransportJobs(deltaTime);
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
        std::cout << "SelectUnit called. Mouse pos: (" << mousePosition.x << ", " << mousePosition.y << ")" << std::endl;
        for (auto& unit : currentSect->GetUnits()) {
            Vector2 unitPos = unit->GetUnitPosInSectView();
            float unitRadius = unit->GetUnitRadiusInSectView();
            float distance = Vector2Distance(mousePosition, unitPos);
            std::cout << "  Unit " << unit->GetUnitType() << " at (" << unitPos.x << ", " << unitPos.y
                      << ") radius: " << unitRadius << " distance: " << distance << std::endl;
            if (distance <= unitRadius) {
                currentUnit = unit;
                std::cout << "  -> Unit selected: " << unit->GetUnitType() << std::endl;
                break;
            }
        }
    }
}

void GameManager::SelectDefaultUnit() {
    if (!currentSect || currentSect->GetUnits().empty()) {
        currentUnit = nullptr;
        std::cout << "No units available to select" << std::endl;
        return;
    }

    // Try to find Extraction unit first
    for (auto& unit : currentSect->GetUnits()) {
        if (unit->GetUnitType() == "Extraction") {
            currentUnit = unit;
            std::cout << "Auto-selected Extraction unit as default" << std::endl;
            return;
        }
    }

    // If no Extraction unit, select the first available unit
    currentUnit = currentSect->GetUnits()[0];
    std::cout << "Auto-selected first unit: " << currentUnit->GetUnitType() << std::endl;
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

void GameManager::BuildAllRoads() {
    if (!currentColony) {
        std::cout << "No current colony to build roads in" << std::endl;
        return;
    }

    const auto& sects = currentColony->GetSects();
    if (sects.size() < 2) {
        std::cout << "Need at least 2 sects to build roads" << std::endl;
        return;
    }

    // Build roads between all pairs of sects (fully connected graph)
    int roadsBuilt = 0;
    for (size_t i = 0; i < sects.size(); i++) {
        for (size_t j = i + 1; j < sects.size(); j++) {
            // Check if road already exists
            if (currentColony->GetRoad(sects[i], sects[j]) == nullptr) {
                currentColony->BuildRoad(sects[i], sects[j]);
                roadsBuilt++;
            }
        }
    }

    std::cout << "Built " << roadsBuilt << " roads between "
              << sects.size() << " sects" << std::endl;
}

void GameManager::CycleTransportModes() {
    if (!currentColony) {
        std::cout << "[TRANSPORT] No current colony" << std::endl;
        return;
    }

    const auto& roads = currentColony->GetRoads();
    if (roads.empty()) {
        std::cout << "[TRANSPORT] No roads to cycle modes on. Press R to build roads first." << std::endl;
        return;
    }

    // If a road is selected, cycle on that road; otherwise cycle first road
    Road* roadToModify = selectedRoad;
    if (!roadToModify) {
        roadToModify = currentColony->GetRoad(roads[0].sectA, roads[0].sectB);
        std::cout << "[TRANSPORT] No road selected, cycling first road" << std::endl;
    }

    if (roadToModify) {
        TransportMode currentMode = roadToModify->mode;
        TransportMode newMode;
        switch (currentMode) {
            case TransportMode::AUTO_BALANCE:
                newMode = TransportMode::MANUAL;
                std::cout << "[TRANSPORT] Switched to MANUAL mode" << std::endl;
                break;
            case TransportMode::MANUAL:
                newMode = TransportMode::DEFICIT_TRIGGERED;
                std::cout << "[TRANSPORT] Switched to DEFICIT_TRIGGERED mode" << std::endl;
                break;
            case TransportMode::DEFICIT_TRIGGERED:
                newMode = TransportMode::AUTO_BALANCE;
                std::cout << "[TRANSPORT] Switched to AUTO_BALANCE mode" << std::endl;
                break;
        }
        currentColony->SetRoadTransportMode(roadToModify, newMode);
    }
}

// ==========================================================================
// TEST INFRASTRUCTURE (Phase 2.5)
// ==========================================================================

void GameManager::PrintTransportState() {
    std::cout << "\n========== TRANSPORT STATE ==========" << std::endl;

    if (!currentColony) {
        std::cout << "[ERROR] No current colony" << std::endl;
        return;
    }

    const auto& sects = currentColony->GetSects();
    const auto& roads = currentColony->GetRoads();
    const auto& jobs = currentColony->GetTransportJobs();

    std::cout << "[COLONY] Sects: " << sects.size()
              << " | Roads: " << roads.size()
              << " | Active Jobs: " << jobs.size() << std::endl;

    // Print roads info
    std::cout << "\n[ROADS]" << std::endl;
    int roadIndex = 0;
    for (const auto& road : roads) {
        std::string modeStr;
        switch (road.mode) {
            case TransportMode::AUTO_BALANCE: modeStr = "AUTO_BALANCE"; break;
            case TransportMode::MANUAL: modeStr = "MANUAL"; break;
            case TransportMode::DEFICIT_TRIGGERED: modeStr = "DEFICIT_TRIGGERED"; break;
        }
        std::cout << "  Road " << roadIndex++ << ": "
                  << "Length=" << road.length
                  << " | Travel=" << road.travelTime << "s"
                  << " | Mode=" << modeStr;
        if (&road == selectedRoad) {
            std::cout << " [SELECTED]";
        }
        std::cout << std::endl;
    }

    // Print transport jobs
    std::cout << "\n[TRANSPORT JOBS]" << std::endl;
    if (jobs.empty()) {
        std::cout << "  (no active jobs)" << std::endl;
    } else {
        for (const auto& job : jobs) {
            std::string statusStr;
            switch (job.status) {
                case TransportStatus::PENDING: statusStr = "PENDING"; break;
                case TransportStatus::IN_TRANSIT: statusStr = "IN_TRANSIT"; break;
                case TransportStatus::COMPLETED: statusStr = "COMPLETED"; break;
                case TransportStatus::CANCELLED: statusStr = "CANCELLED"; break;
            }
            std::cout << "  Job: Resource=" << static_cast<int>(job.resourceType)
                      << " | Amount=" << job.amount
                      << " | Progress=" << (job.progress * 100) << "%"
                      << " | Status=" << statusStr << std::endl;
        }
    }

    // Print sect storage summary
    std::cout << "\n[SECT STORAGE SUMMARY]" << std::endl;
    int sectIndex = 0;
    for (const auto& sect : sects) {
        float totalStorage = 0;
        float totalCapacity = 0;
        for (int i = 0; i < 11; i++) {
            ResourceType type = static_cast<ResourceType>(i);
            totalStorage += sect->GetResourceStorage(type);
            totalCapacity += sect->GetStorageCapacity(type);
        }
        std::cout << "  Sect " << sectIndex++ << ": "
                  << "Storage=" << totalStorage << "/" << totalCapacity << std::endl;
    }

    std::cout << "======================================\n" << std::endl;
}

void GameManager::TestRoadConstruction() {
    std::cout << "\n[TEST] Road Construction Test" << std::endl;

    if (!currentColony) {
        std::cout << "[ERROR] No current colony" << std::endl;
        return;
    }

    const auto& sects = currentColony->GetSects();
    if (sects.size() < 2) {
        std::cout << "[ERROR] Need at least 2 sects. Use Ctrl+Click in Colony view to add sects." << std::endl;
        return;
    }

    Sect* sectA = sects[0];
    Sect* sectB = sects[1];

    // Check if road already exists
    Road* existingRoad = currentColony->GetRoad(sectA, sectB);
    if (existingRoad) {
        std::cout << "[INFO] Road already exists between first two sects" << std::endl;
        selectedRoad = existingRoad;
        std::cout << "[INFO] Selected existing road" << std::endl;
    } else {
        currentColony->BuildRoad(sectA, sectB);
        Road* newRoad = currentColony->GetRoad(sectA, sectB);
        if (newRoad) {
            std::cout << "[SUCCESS] Built road between sect 0 and sect 1" << std::endl;
            std::cout << "[INFO] Road length: " << newRoad->length
                      << " | Travel time: " << newRoad->travelTime << "s" << std::endl;
            selectedRoad = newRoad;
            std::cout << "[INFO] Selected new road" << std::endl;
        } else {
            std::cout << "[ERROR] Failed to build road" << std::endl;
        }
    }
}

void GameManager::SelectNearestRoad(Vector2 worldPos) {
    std::cout << "\n[TEST] Select Nearest Road at (" << worldPos.x << ", " << worldPos.y << ")" << std::endl;

    if (!currentColony) {
        std::cout << "[ERROR] No current colony" << std::endl;
        return;
    }

    const auto& roads = currentColony->GetRoads();
    if (roads.empty()) {
        std::cout << "[ERROR] No roads. Press R to build roads first." << std::endl;
        return;
    }

    // Find nearest road by calculating distance to line segment
    Road* nearestRoad = nullptr;
    float nearestDistance = 999999.0f;
    const float maxSelectDistance = 50.0f;  // Max distance to select a road

    for (const auto& road : roads) {
        if (!road.sectA || !road.sectB) continue;

        Vector2 posA = road.sectA->GetPosition();
        Vector2 posB = road.sectB->GetPosition();

        // Calculate distance from point to line segment
        Vector2 ab = Vector2Subtract(posB, posA);
        Vector2 ap = Vector2Subtract(worldPos, posA);
        float t = Vector2DotProduct(ap, ab) / Vector2DotProduct(ab, ab);
        t = fmaxf(0.0f, fminf(1.0f, t));  // Clamp to segment

        Vector2 closestPoint = Vector2Add(posA, Vector2Scale(ab, t));
        float distance = Vector2Distance(worldPos, closestPoint);

        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestRoad = currentColony->GetRoad(road.sectA, road.sectB);
        }
    }

    if (nearestRoad && nearestDistance <= maxSelectDistance) {
        selectedRoad = nearestRoad;
        std::string modeStr;
        switch (nearestRoad->mode) {
            case TransportMode::AUTO_BALANCE: modeStr = "AUTO_BALANCE"; break;
            case TransportMode::MANUAL: modeStr = "MANUAL"; break;
            case TransportMode::DEFICIT_TRIGGERED: modeStr = "DEFICIT_TRIGGERED"; break;
        }
        std::cout << "[SUCCESS] Selected road (distance: " << nearestDistance << ")" << std::endl;
        std::cout << "[INFO] Mode: " << modeStr
                  << " | Length: " << nearestRoad->length
                  << " | Travel: " << nearestRoad->travelTime << "s" << std::endl;
    } else {
        selectedRoad = nullptr;
        std::cout << "[INFO] No road nearby (nearest: " << nearestDistance << " units)" << std::endl;
    }
}

void GameManager::RunTransportIntegrationTest() {
    std::cout << "\n========== TRANSPORT INTEGRATION TEST ==========" << std::endl;
    std::cout << "[TEST] Starting integration test..." << std::endl;

    if (!currentColony) {
        std::cout << "[ERROR] No current colony" << std::endl;
        return;
    }

    // Step 1: Print initial state
    std::cout << "\n[STEP 1] Initial State:" << std::endl;
    PrintTransportState();

    // Step 2: Ensure roads exist
    const auto& roads = currentColony->GetRoads();
    if (roads.empty()) {
        std::cout << "\n[STEP 2] Building roads..." << std::endl;
        BuildAllRoads();
    } else {
        std::cout << "\n[STEP 2] Roads already exist: " << roads.size() << std::endl;
    }

    // Step 3: Set different modes on roads
    std::cout << "\n[STEP 3] Setting transport modes..." << std::endl;
    const auto& updatedRoads = currentColony->GetRoads();
    int modeIndex = 0;
    for (const auto& road : updatedRoads) {
        Road* roadPtr = currentColony->GetRoad(road.sectA, road.sectB);
        if (roadPtr) {
            TransportMode mode;
            switch (modeIndex % 3) {
                case 0: mode = TransportMode::AUTO_BALANCE; break;
                case 1: mode = TransportMode::MANUAL; break;
                case 2: mode = TransportMode::DEFICIT_TRIGGERED; break;
            }
            currentColony->SetRoadTransportMode(roadPtr, mode);
            modeIndex++;
        }
    }
    std::cout << "[INFO] Set varied modes on " << modeIndex << " roads" << std::endl;

    // Step 4: Print final state
    std::cout << "\n[STEP 4] Final State:" << std::endl;
    PrintTransportState();

    std::cout << "\n[TEST] Integration test complete!" << std::endl;
    std::cout << "[INFO] Watch console for transport activity over time." << std::endl;
    std::cout << "[INFO] Press 0 at any time to print current transport state." << std::endl;
    std::cout << "=================================================\n" << std::endl;
}

// ==========================================================================
// ROAD CONSTRUCTION MODE (Phase 2.5)
// ==========================================================================

void GameManager::ToggleBuildRoadMode() {
    buildRoadMode = !buildRoadMode;
    roadBuildStartSect = nullptr;  // Reset start sect when toggling

    if (buildRoadMode) {
        std::cout << "\n[ROAD BUILD] Entered road construction mode" << std::endl;
        std::cout << "[ROAD BUILD] Click on a sect to set as start point" << std::endl;
        std::cout << "[ROAD BUILD] Press B again to exit mode" << std::endl;
    } else {
        std::cout << "\n[ROAD BUILD] Exited road construction mode" << std::endl;
    }
}

void GameManager::SelectSectForRoadBuild(Vector2 worldPos) {
    if (!buildRoadMode) return;
    if (!currentColony) {
        std::cout << "[ROAD BUILD] No current colony" << std::endl;
        return;
    }

    // Find the sect at this position
    Sect* selectedSect = nullptr;
    for (auto& sect : currentColony->GetSects()) {
        if (Vector2Distance(worldPos, sect->GetPosition()) <= sect->GetRadius()) {
            selectedSect = sect;
            break;
        }
    }

    if (!selectedSect) {
        std::cout << "[ROAD BUILD] No sect at this position" << std::endl;
        return;
    }

    if (roadBuildStartSect == nullptr) {
        // First click - set start sect
        roadBuildStartSect = selectedSect;
        std::cout << "[ROAD BUILD] Start sect selected. Click another sect to complete road." << std::endl;
    } else {
        // Second click - complete the road
        if (selectedSect == roadBuildStartSect) {
            std::cout << "[ROAD BUILD] Cannot build road to same sect" << std::endl;
            return;
        }

        // Check if road already exists
        Road* existingRoad = currentColony->GetRoad(roadBuildStartSect, selectedSect);
        if (existingRoad) {
            std::cout << "[ROAD BUILD] Road already exists between these sects" << std::endl;
            selectedRoad = existingRoad;  // Select the existing road
        } else {
            // Build the road
            currentColony->BuildRoad(roadBuildStartSect, selectedSect);
            Road* newRoad = currentColony->GetRoad(roadBuildStartSect, selectedSect);
            if (newRoad) {
                std::cout << "[ROAD BUILD] Road built successfully!" << std::endl;
                std::cout << "[ROAD BUILD] Length: " << newRoad->length
                          << " | Travel time: " << newRoad->travelTime << "s" << std::endl;
                selectedRoad = newRoad;  // Select the new road
            } else {
                std::cout << "[ROAD BUILD] Failed to build road" << std::endl;
            }
        }

        // Reset for next road
        roadBuildStartSect = nullptr;
        std::cout << "[ROAD BUILD] Ready for next road. Click a sect or press B to exit." << std::endl;
    }
}
