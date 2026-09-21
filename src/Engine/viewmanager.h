#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "raylib.h"
#include "raymath.h"
#include "game_constants.h"
#include "game_structs.h"
#include "inputmanager.h"
#include "colony.h"
#include <vector>
#include <algorithm>
#include <iostream>

// Which view is on screen, and the 2D camera the Colony view pans and
// zooms with. The Colony view draws in its colony's local frame (origin
// at the colony's centre, 1 unit = 50 m), so its camera lives in that
// frame; the Orbital view has its own camera (GetOrbitalCamera) and the
// Sect and Unit views are screen-space.
class ViewManager {
public:
    ViewManager(int screenWidth, int screenHeight);
    ~ViewManager();

    void UpdateCamera(InputManager& inputManager);
    void ResetCameraForCurrentView(View view, Colony* currentColony);

    void SwitchToColonyView(Colony* currentColony);
    void SwitchToSectView(Colony* currentColony, Sect* currentSect);
    void SwitchToUnitView(Colony* currentColony, Sect* currentSect, Unit* currentUnit);
    // Back up to the globe. Given a place, the globe is turned to face it
    // so the player sees where they were.
    void SwitchToOrbitalView(const LunarPoint* lookAt = nullptr);

    Vector2 GetWorldMousePosition();
    Camera2D& GetCamera() { return camera; }
    View GetCurrentView() const { return currentView; }
    void SetCurrentView(View view) { currentView = view; }

private:
    int screenWidth;
    int screenHeight;
    View currentView;

    // Camera state
    Camera2D camera;
    float minZoom;
    float maxZoom;

    void HandleColonyViewCamera(InputManager& inputManager);
    void ClampCameraColonyView();
};

#endif // VIEW_MANAGER_H
