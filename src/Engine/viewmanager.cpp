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

void ViewManager::UpdateCamera(InputManager& inputManager) {
    if (currentView == View::Colony) {
        HandleColonyViewCamera(inputManager);
        ClampCameraColonyView();
    }
}

void ViewManager::HandleColonyViewCamera(InputManager& inputManager) {
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

void ViewManager::ResetCameraForCurrentView(View view, Colony* currentColony) {
    switch (view) {
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
        ResetCameraForCurrentView(View::Colony, currentColony);
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

void ViewManager::SwitchToOrbitalView(const LunarPoint* lookAt) {
    currentView = View::Orbital;
    // The orbital view has its own camera: the globe's. Turn it to face
    // the place we came from, keeping whatever zoom the player had.
    if (lookAt) {
        OrbitalCamera cam = GetOrbitalCamera();
        cam.subLatDeg = lookAt->latDeg;
        cam.subLonDeg = lookAt->lonDeg;
        SetOrbitalCamera(cam);
    }
}
