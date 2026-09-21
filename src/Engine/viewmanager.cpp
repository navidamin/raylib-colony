#include "viewmanager.h"
#include "terrain_synthesis.h"
#include "lunar_frame.h"

// The colony view's drawing frame: origin at the colony's centre, the
// 25 km window around it.
static float ColonyWindowUnits()
{
    return (float)(COLONY_WINDOW_KM * LOCAL_UNITS_PER_KM);
}

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

        // Zoom that fits the 100 km playfield on screen.
        float fitX = screenWidth / PLANET_WIDTH;
        float fitY = screenHeight / PLANET_HEIGHT;
        float fitPlayfield = std::min(fitX, fitY) * 0.9f;

        // The planet view can now pull all the way out to the whole moon,
        // which the map layer draws around the playfield. The globe spans
        // 360 degrees of longitude at the anchor's scale.
        double latSpanDeg = (PLANET_SIZE * TERRAIN_CELL_KM) / MOON_KM_PER_DEG;
        double anchorLat, anchorLon;
        GetTerrainAnchor(&anchorLat, &anchorLon);
        float updLat = (float)(PLANET_HEIGHT / latSpanDeg);
        float updLon = updLat * (float)std::max(0.2, std::cos(anchorLat * DEG2RAD));
        float globeW = 360.0f * updLon;
        float globeH = 180.0f * updLat;
        float minZoom = std::min(screenWidth / globeW, screenHeight / globeH) * 0.92f;

        float maxZoom = fitPlayfield * 5.0f;

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

        // Out to the whole 25 km window, in to a sect and a half.
        float window = ColonyWindowUnits();
        float maxZoomOut = std::min(screenWidth / window, screenHeight / window);
        camera.zoom = Clamp(camera.zoom, maxZoomOut, 4.0f);

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
    // The colony's frame has its origin at the window's centre: keep the
    // view inside the window, one sect footprint of margin past its edge.
    float visibleWidth = screenWidth / camera.zoom;
    float visibleHeight = screenHeight / camera.zoom;
    float margin = SECT_CORE_RADIUS * 2.0f;
    float half = ColonyWindowUnits() * 0.5f + margin;

    float limitX = std::max(0.0f, half - visibleWidth / 2.0f);
    float limitY = std::max(0.0f, half - visibleHeight / 2.0f);
    camera.target.x = Clamp(camera.target.x, -limitX, limitX);
    camera.target.y = Clamp(camera.target.y, -limitY, limitY);
}

void ViewManager::ResetCameraForCurrentView(View view, std::vector<Colony*>& colonies, Colony* currentColony, Planet* planet) {
    switch (view) {
        case View::Planet: {
            camera.target = {PLANET_WIDTH / 2, PLANET_HEIGHT / 2};  // Center of planet, not colony

            if (!colonies.empty()) {
                planet->UpdateActiveArea(colonies);  // Make sure active area is updated
                camera.target = (currentColony && currentColony->HasCentre())
                    ? Planet::WorldOf(currentColony->GetCentre())
                    : planet->GetActiveCentroid();
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
        const LunarPoint& c = currentColony->GetCentre();
        std::cout << "Switching to Colony View. Colony centred at "
                  << c.latDeg << ", " << c.lonDeg << " with "
                  << currentColony->GetSects().size() << " sect(s)" << std::endl;
    }
}

void ViewManager::SwitchToSectView(Colony* currentColony, Sect* currentSect) {
    if (currentColony && currentSect) {
        currentView = View::Sect;
        std::cout << "Switched to Sect view. Sect has " << currentSect->GetUnits().size() << " units" << std::endl;
    }
}

void ViewManager::SwitchToUnitView(Colony* currentColony, Sect* currentSect, Unit* currentUnit) {
    std::cout << "SwitchToUnitView called: colony=" << (currentColony ? "OK" : "NULL")
              << " sect=" << (currentSect ? "OK" : "NULL")
              << " unit=" << (currentUnit ? "OK" : "NULL") << std::endl;
    if (currentColony && currentSect && currentUnit) {
        currentView = View::Unit;
        std::cout << "  -> Switched to Unit view successfully" << std::endl;
    } else {
        std::cout << "  -> FAILED to switch (one or more is NULL)" << std::endl;
    }
}

void ViewManager::SwitchToOrbitalView() {
    currentView = View::Orbital;
    // Orbital view doesn't use the world camera — DrawOrbitalView
    // positions the disc texture in screen space directly.
}

void ViewManager::SwitchToPlanetView(Colony* currentColony) {
    currentView = View::Planet;

    if (currentColony) {
        const LunarPoint& c = currentColony->GetCentre();
        std::cout << "Switching to Planet View. Current colony at "
                  << c.latDeg << ", " << c.lonDeg << std::endl;
    }
}
