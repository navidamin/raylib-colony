#include "Engine.h"

#include <iostream>

Engine::Engine(int screenWidth, int screenHeight, const char* title)
    : screenWidth(screenWidth),
      screenHeight(screenHeight),
      currentView(View::Menu),
      planet(new Planet()),
      currentColony(nullptr),
      currentSect(nullptr),
      currentUnit(nullptr),
      lastClickTime(0),
      lastClickPosition({0, 0}),
      minZoom(0.5f),
      maxZoom(2.0f),
      isDragging(false),
      lastUpdateTime(0.0f)
{
    InitWindow(screenWidth, screenHeight, title);
    SetTargetFPS(60);

    // Initialize camera
    camera.target = {0, 0};
    camera.offset = {static_cast<float>(screenWidth)/2, static_cast<float>(screenHeight)/2};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
}

Engine::~Engine() {
    delete planet;  // Clean up in destructor

    // Unregister all colonies from time manager before deletion
    for (Colony* colony : colonies) {
        timeManager.UnregisterColony(colony);
        delete colony;
    }
    colonies.clear();

    CloseWindow();
}

void Engine::InitGame() {
    // Initialize time manager
    lastUpdateTime = GetTime();  // Set initial time
    timeManager.Reset();         // Reset time manager to initial state

    // Generate map/grid/resource map of the planet
    planet->GenerateMap();

    // Create initial colony
    Colony* firstColony = new Colony();
    colonies.push_back(firstColony);
    currentColony = firstColony;

    // Register first colony with time manager
    timeManager.RegisterColony(firstColony);

    // Create initial sect with a position near the center of the map
    Sect* firstSect = new Sect();
    Vector2 initialPosition = planet->GetRandomValidPosition();
    firstSect->SetPosition(initialPosition);

    // Notify planet about sect position to ensure resources
    planet->NotifyFirstSectPosition(initialPosition);

    // Add sect to colony
    currentColony->AddSect(firstSect);
    currentSect = firstSect;

    UpdatePlanetActiveArea();

    // Initialize camera to focus on the first sect
    camera.target = initialPosition;
    camera.offset = (Vector2){ screenWidth/2.0f, screenHeight/2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // Set initial view to Colony view to see the first sect
    currentView = View::Menu;
}

void Engine::Run() {
    while (!WindowShouldClose()) {
        HandleInput();
        Update();
        Draw();
    }
}

bool Engine::IsDoubleClick() {
    double currentTime = GetTime();
    Vector2 currentPosition = GetMousePosition();

    bool isDoubleClick = (currentTime - lastClickTime <= 0.5) &&
                         (Vector2Distance(lastClickPosition, currentPosition) <= 10);

    lastClickTime = currentTime;
    lastClickPosition = currentPosition;

    return isDoubleClick;
}


void Engine::SwitchToColonyView() {
    if (currentColony) {
        currentView = View::Colony;
        Vector2 colonyPos = currentColony->GetCentroid();
        std::cout << "Switching to Colony View. Colony centroid at: ("
                  << colonyPos.x << ", " << colonyPos.y << ")" << std::endl;

        if (!currentColony->GetSects().empty()) {
            Vector2 sectPos = currentColony->GetSects()[0]->GetPosition();
            std::cout << "First sect position: ("
                      << sectPos.x << ", " << sectPos.y << ")" << std::endl;
        }
        ResetCameraForCurrentView();
    }
}

void Engine::SwitchToSectView() {
    if (currentColony && currentSect) {
        currentView = View::Sect;
        ResetCameraForCurrentView();
    }
}

void Engine::SwitchToUnitView() {
    if (currentColony && currentSect && currentUnit) {
        currentView = View::Unit;
    }
}

void Engine::SwitchToPlanetView() {
    currentView = View::Planet;
    UpdatePlanetActiveArea();  // Make sure active area is updated when switching views
    ResetCameraForCurrentView();
    if (currentColony) {
        Vector2 colonyPos = currentColony->GetCentroid();
        std::cout << "Switching to Planet View. Colony centroid at: ("
                  << colonyPos.x << ", " << colonyPos.y << ")" << std::endl;

        // Also print first sect position
        if (!currentColony->GetSects().empty()) {
            Vector2 sectPos = currentColony->GetSects()[0]->GetPosition();
            std::cout << "First sect position: ("
                      << sectPos.x << ", " << sectPos.y << ")" << std::endl;
        }
    }
    ResetCameraForCurrentView();}

void Engine::SelectColony(Vector2 mousePosition) {
    // Logic to determine which colony was clicked
    Vector2 worldMousePos = GetScreenToWorld2D(mousePosition, camera);

    for (auto& colony : colonies) {
                Vector2 colonyWorldPos = colony->GetCentroid();  // Should return world coordinates
        if (Vector2Distance(worldMousePos, colonyWorldPos) <= colony->GetRadius()) {            currentColony = colony;
            SwitchToColonyView();
            break;
        }
    }
}

void Engine::SelectSect(Vector2 mousePosition) {
    // Logic to determine which sect was clicked
    if (currentColony) {
        // Convert screen coordinates to world coordinates using the camera
        Vector2 worldMousePos = GetScreenToWorld2D(mousePosition, camera);

        for (auto& sect : currentColony->GetSects()) {
            Vector2 sectPos = sect->GetPosition();
            float sectRadius = sect->GetRadius();

            if (Vector2Distance(worldMousePos, sectPos) <= sectRadius) {
                currentSect = sect;
                SwitchToSectView();
                break;
            }
        }
    }
}


void Engine::SelectUnit(Vector2 mousePosition) {
    // Logic to determine which unit was clicked
    if (currentSect) {
        for (auto& unit : currentSect->GetUnits()) {
            if (Vector2Distance(mousePosition, unit->GetUnitPosInSectView()) <= unit->GetUnitRadiusInSectView()) {
                currentUnit = unit;
                SwitchToUnitView();
                break;
            }
        }
    }
}

void Engine::HandleInput() {

    HandleCameraControls();  // Always handle camera controls first

    switch (currentView) {
        case View::Menu:
            if (IsKeyPressed(KEY_ENTER)) {
                SwitchToColonyView();
            }
            break;
        case View::Planet:
            if (IsKeyPressed(KEY_C)) {
                SwitchToColonyView();
            }
            break;
        case View::Colony:
            if (IsKeyPressed(KEY_S)) {
                SwitchToSectView();
            }
            if (IsKeyPressed(KEY_P)) {
                SwitchToPlanetView();
            }
            break;
        case View::Sect:
            if (IsKeyPressed(KEY_U)) {
                SwitchToUnitView();
            }
            if (IsKeyPressed(KEY_C)) {
                SwitchToColonyView();
            }
            break;
        case View::Unit:
            if (IsKeyPressed(KEY_S)) {
                SwitchToSectView();
            }
            break;
    }

    // Handle double-click selection of specific colonies, sects, and units
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && IsDoubleClick()) {
        Vector2 mousePosition = GetMousePosition();
        switch (currentView) {
            case View::Planet:
                SelectColony(mousePosition);
                break;
            case View::Colony:
                SelectSect(mousePosition);
                break;
            case View::Sect:
                SelectUnit(mousePosition);
                break;
            case View::Unit:
                // Handle double-click in Unit view if needed
                break;
            case View::Menu:
                // Handle double-click in Menu if needed
                break;
        }
    }
}
void Engine::HandleCameraControls() {
    if (currentView == View::Planet) {
        HandlePlanetViewCamera();
    } else if (currentView == View::Colony) {
        HandleColonyViewCamera();
        ClampCameraColonyView();  // Only clamp in Colony view
    }
}

void Engine::HandlePlanetViewCamera() {
    if (!planet) return;  // Guard against null planet

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        // Get world point before zoom
        Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

        float prevZoom = camera.zoom;
        camera.zoom += wheel * 0.1f;

        // Calculate maximum zoom to always see the whole planet
        float minZoomX = screenWidth / PLANET_WIDTH;
        float minZoomY = screenHeight / PLANET_HEIGHT;
        float minZoom = std::min(minZoomX, minZoomY) * 0.9f;  // 90% to add padding

        // Set maximum zoom to something reasonable
        float maxZoom = minZoom * 5.0f;  // Adjust this multiplier as needed

        camera.zoom = Clamp(camera.zoom, minZoom, maxZoom);

        // If zoom changed, adjust position to zoom towards mouse
        if (camera.zoom != prevZoom) {
            Vector2 mouseWorldPosNew = GetScreenToWorld2D(GetMousePosition(), camera);
            camera.target.x += (mouseWorldPos.x - mouseWorldPosNew.x);
            camera.target.y += (mouseWorldPos.y - mouseWorldPosNew.y);
        }
    }

    // Allow panning in Planet view, but keep centroid in view
    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
        dragStart = GetMousePosition();
        isDragging = true;
    }

    if (IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) {
        isDragging = false;
    }

    if (isDragging) {
        Vector2 delta = GetMouseDelta();
        camera.target.x -= delta.x / camera.zoom;
        camera.target.y -= delta.y / camera.zoom;

        // Keep the active area centroid within the visible area
        Vector2 centroid = planet->GetActiveCentroid();
        float visibleWidth = screenWidth / camera.zoom;
        float visibleHeight = screenHeight / camera.zoom;

        // Calculate bounds to keep centroid visible
        float maxDistanceX = visibleWidth * 0.4f;  // Allow some movement but keep centroid visible
        float maxDistanceY = visibleHeight * 0.4f;

        float dx = camera.target.x - centroid.x;
        float dy = camera.target.y - centroid.y;

        if (abs(dx) > maxDistanceX) {
            camera.target.x = centroid.x + (dx > 0 ? maxDistanceX : -maxDistanceX);
        }
        if (abs(dy) > maxDistanceY) {
            camera.target.y = centroid.y + (dy > 0 ? maxDistanceY : -maxDistanceY);
        }
    }
}


void Engine::HandleColonyViewCamera() {
    // Colony view: More flexible movement but within planet bounds
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        // Get world point before zoom
        Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

        float prevZoom = camera.zoom;
        camera.zoom += wheel * 0.1f;

        // Colony view can zoom in more
        float maxZoomOut = std::min(
            screenWidth / (PLANET_WIDTH * 0.5f),
            screenHeight / (PLANET_HEIGHT * 0.5f)
        );
        camera.zoom = Clamp(camera.zoom, maxZoomOut, 4.0f);  // Allow closer zoom

        if (camera.zoom != prevZoom) {
            Vector2 mouseWorldPosNew = GetScreenToWorld2D(GetMousePosition(), camera);
            camera.target.x += (mouseWorldPos.x - mouseWorldPosNew.x);
            camera.target.y += (mouseWorldPos.y - mouseWorldPosNew.y);
        }
    }

    // Pan with middle mouse button
    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
        dragStart = GetMousePosition();
        isDragging = true;
    }

    if (IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) {
        isDragging = false;
    }

    if (isDragging) {
        Vector2 delta = GetMouseDelta();
        camera.target.x -= delta.x / camera.zoom;
        camera.target.y -= delta.y / camera.zoom;
    }

    // Clamp camera for Colony view
    ClampCameraColonyView();
}

void Engine::ClampCameraColonyView() {
    // Calculate visible area
    float visibleWidth = screenWidth / camera.zoom;
    float visibleHeight = screenHeight / camera.zoom;

    // Calculate bounds with some margin
    float margin = SECT_CORE_RADIUS * 2.0f;  // One cell margin

    // Clamp X
    float minX = std::max(0.0f, visibleWidth / 2.0f - margin);
    float maxX = std::min(PLANET_WIDTH, PLANET_WIDTH - visibleWidth / 2.0f + margin);
    camera.target.x = Clamp(camera.target.x, minX, maxX);

    // Clamp Y
    float minY = std::max(0.0f, visibleHeight / 2.0f - margin);
    float maxY = std::min(PLANET_HEIGHT, PLANET_HEIGHT - visibleHeight / 2.0f + margin);
    camera.target.y = Clamp(camera.target.y, minY, maxY);
}

void Engine::ResetCameraForCurrentView() {
    switch (currentView) {
        case View::Planet: {
            camera.target = {PLANET_WIDTH / 2, PLANET_HEIGHT / 2};  // Center of planet, not colony

            if (!colonies.empty()) {
                camera.target = currentColony->GetCentroid();  // Use colony position
                planet->UpdateActiveArea(colonies);  // Make sure active area is updated
                float activeRadius = planet->GetActiveRadius();

                // Calculate zoom to see either the whole planet or the active colony area,
                // whichever is larger
                float minVisibleWidth = std::max(PLANET_WIDTH, activeRadius * 2.5f);  // Use 2.5 for padding
                float minVisibleHeight = std::max(PLANET_HEIGHT, activeRadius * 2.5f);

                // Calculate zoom to fit everything
                float zoomX = screenWidth / minVisibleWidth;
                float zoomY = screenHeight / minVisibleHeight;

                // Use the more restrictive zoom (smaller value)
                camera.zoom = std::min(zoomX, zoomY);

                // Add a bit of padding by reducing zoom slightly
                camera.zoom *= 0.9f;
            } else {
                // If no colonies, show the whole planet centered
                camera.target = {PLANET_WIDTH / 2, PLANET_HEIGHT / 2};
                float zoomX = screenWidth / PLANET_WIDTH;
                float zoomY = screenHeight / PLANET_HEIGHT;
                camera.zoom = std::min(zoomX, zoomY) * 0.9f;
            }
            break;
        }
        case View::Colony: {
            if (currentColony) {
                camera.target = currentColony->GetCentroid();
                float desiredView = 8 * SECT_CORE_RADIUS;
                camera.zoom = std::min(
                    screenWidth / desiredView,
                    screenHeight / desiredView
                );
                ClampCameraColonyView();
            }
            break;
        }
        default:
            break;
    }
}

void Engine::Update() {

    float currentTime = GetTime();
    float deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;

    timeManager.Update(deltaTime);

    // Register colonies with time manager if needed
    for (auto& colony : colonies) {
        // Register the colony if note registered to timeManager
        if (!timeManager.IsColonyRegistered(colony)) {
         timeManager.RegisterColony(colony);
        }

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

void Engine::UpdatePlanetActiveArea() {
    if (planet) {
        planet->UpdateActiveArea(colonies);
    }
}

void Engine::Draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    switch (currentView) {
        case View::Menu:
            DrawText("COLONY", GetScreenWidth()/2 - MeasureText("COLONY", 60)/2, GetScreenHeight()/3, 60, BLACK);
            DrawText("Press ENTER to start", GetScreenWidth()/2 - MeasureText("Press ENTER to start", 20)/2, GetScreenHeight()/2, 20, GRAY);
            break;
        case View::Planet:{
            BeginMode2D(camera);

            if (planet) {  // Guard against null planet
                // Draw grid
                for (int i = 0; i <= PLANET_SIZE; i++) {
                    float linePos = i * SECT_CORE_RADIUS * 2;
                    DrawLineV({linePos, 0}, {linePos, PLANET_HEIGHT}, LIGHTGRAY);
                    DrawLineV({0, linePos}, {PLANET_WIDTH, linePos}, LIGHTGRAY);
                }

                // Draw colonies if any
                for (const auto& colony : colonies) {
                    colony->Draw(camera.zoom);
                }

            }

            // Show the resource map if TAB is held
            if (IsInfoKeyPressed()) {
                planet->DrawResourceDebug(camera.zoom);
            }

            EndMode2D();

            // Show the Cell info if Ctrl+I is held
            if (IsInfoKeyPressed()) {
                DrawCellInfo(GetMousePosition());
            }

            // Draw UI elements including time
            timeManager.Draw(screenWidth, screenHeight);
            DrawText("Planet View", 10, 10, 20, BLACK);
            DrawText("Press C for Colony View", 10, 40, 20, GRAY);
            break;
        }


        case View::Colony:{
            // Start drawing with camera transformation
            BeginMode2D(camera);

            if (currentColony) {
                // Calculate visible area in world coordinates
                Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera);
                Vector2 bottomRight = GetScreenToWorld2D(
                    {static_cast<float>(screenWidth),
                     static_cast<float>(screenHeight)},
                    camera
                );

                // Calculate grid line positions
                float cellSize = SECT_CORE_RADIUS * 2.0f;
                float planetWidth = PLANET_SIZE * cellSize;  // Total width of planet
                float planetHeight = PLANET_SIZE * cellSize; // Total height of planet

                // Draw vertical grid lines
                int startX = std::max(0, static_cast<int>(topLeft.x / cellSize));
                int endX = std::min(PLANET_SIZE, static_cast<int>(bottomRight.x / cellSize) + 1);

                for (int i = startX; i <= endX; i++) {
                    float x = i * cellSize;
                    if (x <= planetWidth) {  // Only draw if within planet width
                        Vector2 start = {x, 0};
                        Vector2 end = {x, planetHeight};
                        DrawLineV(start, end, Fade(LIGHTGRAY, 0.5f));
                    }
                }

                // Draw horizontal grid lines
                int startY = std::max(0, static_cast<int>(topLeft.y / cellSize));
                int endY = std::min(PLANET_SIZE, static_cast<int>(bottomRight.y / cellSize) + 1);

                for (int i = startY; i <= endY; i++) {
                    float y = i * cellSize;
                    // Draw horizontal line from 0 to planetWidth (not screen width)
                    DrawLineV(
                        {0, y},
                        {planetWidth, y},
                        Fade(LIGHTGRAY, 0.5f)
                    );
                }

                // Draw all sects in the current colony
                for (const auto& sect : currentColony->GetSects()) {
                    sect->DrawInColonyView(sect->GetPosition(), camera.zoom);
                }
            }

            // Show the resource map if TAB is held
            if (IsInfoKeyPressed()) {
                planet->DrawResourceDebug(camera.zoom);
            }

            // End camera transformation
            EndMode2D();

            // Show the Cell info if Ctrl+I is held
            if (IsInfoKeyPressed()) {
                DrawCellInfo(GetMousePosition());
            }

            // Draw UI elements including time
            timeManager.Draw(screenWidth, screenHeight);
            DrawText("Colony View", 10, 10, 20, BLACK);
            DrawText("Press S for Sect View", 10, 40, 20, GRAY);
            DrawText("Press P for Planet View", 10, 70, 20, GRAY);
            break;
        }



        case View::Sect:
            if (currentSect) {
                currentSect->DrawInSectView(Vector2{GetScreenWidth()/2.0f, GetScreenHeight()/2.0f});
            }

            // Draw UI elements including time
            timeManager.Draw(screenWidth, screenHeight);
            DrawText("Sect View", 10, 10, 20, BLACK);
            DrawText("Press U for Unit View", 10, 40, 20, GRAY);
            DrawText("Press C for Colony View", 10, 70, 20, GRAY);
            break;
        case View::Unit:
            if (currentUnit) {
                currentUnit->DrawInUnitView();
            }
            DrawText("Unit View", 10, 10, 20, BLACK);
            DrawText("Press S for Sect View", 10, 40, 20, GRAY);
            break;
    }


    // Draw UI elements (not affected by camera)
    if (currentView != View::Menu) {
        DrawText(TextFormat("Zoom: %.2f", camera.zoom), 10, screenHeight - 20, 20, GRAY);
        if (currentView == View::Planet) {
            DrawText("Press Ctrl+I to see map info", 10, GetScreenHeight() - 40, 20, DARKGRAY);
        } else{
            DrawText("Double-click to select", 10, GetScreenHeight() - 40, 20, DARKGRAY);
        }
    }

    EndDrawing();
}

// Draw information of each grid cell: resource available, the owner colony
void Engine::DrawCellInfo(Vector2 mousePosition) {
    // Convert screen coordinates to world coordinates
    Vector2 worldPos = GetScreenToWorld2D(mousePosition, camera);

    // Get grid coordinates
    int gridX = static_cast<int>(worldPos.x / (SECT_CORE_RADIUS * 2));
    int gridY = static_cast<int>(worldPos.y / (SECT_CORE_RADIUS * 2));

    // Check if position is within planet bounds
    if (gridX < 0 || gridX >= PLANET_SIZE || gridY < 0 || gridY >= PLANET_SIZE) {
        return;
    }

    // Get resource information
    Vector2 gridWorldPos = {
        static_cast<float>(gridX) * SECT_CORE_RADIUS * 2,
        static_cast<float>(gridY) * SECT_CORE_RADIUS * 2
    };
    auto resources = planet->GetResourceInfo(gridWorldPos);

    // Build info text
    std::vector<std::string> infoLines;

    // Add coordinates
    infoLines.push_back(TextFormat("Grid: %d, %d", gridX, gridY));

    // Add resources
    if (!resources.empty()) {
        infoLines.push_back("Resources:");
        for (const auto& [type, abundance] : resources) {
            std::string resourceName = ResourceUtils::GetResourceName(type);
            int percentage = static_cast<int>(abundance * 100);
            infoLines.push_back(TextFormat("  %s: %d%%", resourceName.c_str(), percentage));
        }
    } else {
        infoLines.push_back("No resources");
    }

    // Check jurisdiction
    std::string jurisdiction = "Unclaimed";
    for (const auto& colony : colonies) {
        Vector2 colonyCenter = colony->GetCentroid();
        float colonyRadius = colony->GetRadius();
        if (CheckCollisionPointCircle(gridWorldPos, colonyCenter, colonyRadius)) {
            jurisdiction = "Colony Territory";
            break;
        }
    }
    infoLines.push_back(TextFormat("Status: %s", jurisdiction.c_str()));

    // Calculate popup dimensions
    int padding = 10;
    int lineHeight = 20;
    int maxWidth = 0;
    for (const auto& line : infoLines) {
        int width = MeasureText(line.c_str(), lineHeight);
        maxWidth = std::max(maxWidth, width);
    }
    int boxWidth = maxWidth + (padding * 2);
    int boxHeight = (lineHeight * infoLines.size()) + (padding * 2);

    // Adjust popup position to stay within screen bounds
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    Vector2 popupPos = mousePosition;
    popupPos.x += 20; // Offset from cursor
    popupPos.y += 20;

    // Ensure popup stays within screen bounds
    if (popupPos.x + boxWidth > screenWidth) {
        popupPos.x = screenWidth - boxWidth;
    }
    if (popupPos.y + boxHeight > screenHeight) {
        popupPos.y = screenHeight - boxHeight;
    }

    // Draw popup background
    DrawRectangle(popupPos.x, popupPos.y, boxWidth, boxHeight, ColorAlpha(BLACK, 0.8f));
    DrawRectangleLines(popupPos.x, popupPos.y, boxWidth, boxHeight, WHITE);

    // Draw text
    for (size_t i = 0; i < infoLines.size(); i++) {
        DrawText(
            infoLines[i].c_str(),
            popupPos.x + padding,
            popupPos.y + padding + (i * lineHeight),
            lineHeight,
            WHITE
        );
    }
}
