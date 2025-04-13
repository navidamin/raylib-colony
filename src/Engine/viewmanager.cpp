#include "viewmanager.h"

ViewManager::ViewManager(int screenWidth, int screenHeight)
    : screenWidth(screenWidth),
      screenHeight(screenHeight),
      currentView(View::Menu),
      minZoom(0.5f),
      maxZoom(2.0f)
{
    // Initialize camera
    camera.target = {0, 0};
    camera.offset = {static_cast<float>(screenWidth)/2, static_cast<float>(screenHeight)/2};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
}

ViewManager::~ViewManager() {
}

void ViewManager::UpdateCamera(InputManager& inputManager, std::vector<Colony*>& colonies, Planet* planet) {
    HandleCameraControls(inputManager, colonies, planet);
}

void ViewManager::HandleCameraControls(InputManager& inputManager, std::vector<Colony*>& colonies, Planet* planet) {
    if (currentView == View::Planet) {
        HandlePlanetViewCamera(inputManager, planet);
    } else if (currentView == View::Colony) {
        HandleColonyViewCamera(inputManager);
        ClampCameraColonyView();  // Only clamp in Colony view
    }
}

void ViewManager::HandlePlanetViewCamera(InputManager& inputManager, Planet* planet) {
    if (!planet) return;  // Guard against null planet

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        // Get world point before zoom
        Vector2 mouseWorldPos = GetScreenToWorld2D(inputManager.GetMousePosition(), camera);

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
            Vector2 mouseWorldPosNew = GetScreenToWorld2D(inputManager.GetMousePosition(), camera);
            camera.target.x += (mouseWorldPos.x - mouseWorldPosNew.x);
            camera.target.y += (mouseWorldPos.y - mouseWorldPosNew.y);
        }
    }

    // Handle dragging with middle mouse button
    if (inputManager.IsMouseDragging()) {
        Vector2 delta = inputManager.GetMouseDelta();
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

void ViewManager::HandleColonyViewCamera(InputManager& inputManager) {
    // Colony view: More flexible movement but within planet bounds
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        // Get world point before zoom
        Vector2 mouseWorldPos = GetScreenToWorld2D(inputManager.GetMousePosition(), camera);

        float prevZoom = camera.zoom;
        camera.zoom += wheel * 0.1f;

        // Colony view can zoom in more
        float maxZoomOut = std::min(
            screenWidth / (PLANET_WIDTH * 0.5f),
            screenHeight / (PLANET_HEIGHT * 0.5f)
        );
        camera.zoom = Clamp(camera.zoom, maxZoomOut, 4.0f);  // Allow closer zoom

        if (camera.zoom != prevZoom) {
            Vector2 mouseWorldPosNew = GetScreenToWorld2D(inputManager.GetMousePosition(), camera);
            camera.target.x += (mouseWorldPos.x - mouseWorldPosNew.x);
            camera.target.y += (mouseWorldPos.y - mouseWorldPosNew.y);
        }
    }

    // Pan with middle mouse button
    if (inputManager.IsMouseDragging()) {
        Vector2 delta = inputManager.GetMouseDelta();
        camera.target.x -= delta.x / camera.zoom;
        camera.target.y -= delta.y / camera.zoom;
    }
}

void ViewManager::ClampCameraColonyView() {
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

void ViewManager::ResetCameraForCurrentView(View view, std::vector<Colony*>& colonies, Colony* currentColony, Planet* planet) {
    switch (view) {
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

Vector2 ViewManager::GetWorldMousePosition() {
    return GetScreenToWorld2D(::GetMousePosition(), camera);
}

void ViewManager::SwitchToColonyView(Colony* currentColony) {
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
    }
}

void ViewManager::SwitchToSectView(Colony* currentColony, Sect* currentSect) {
    if (currentColony && currentSect) {
        currentView = View::Sect;
    }
}

void ViewManager::SwitchToUnitView(Colony* currentColony, Sect* currentSect, Unit* currentUnit) {
    if (currentColony && currentSect && currentUnit) {
        currentView = View::Unit;
    }
}

void ViewManager::SwitchToPlanetView(Colony* currentColony) {
    currentView = View::Planet;

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
}
