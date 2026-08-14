#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "raylib.h"
#include "raymath.h"
#include "game_constants.h"
#include "inputmanager.h"
#include "colony.h"
#include "planet.h"
#include <vector>
#include <algorithm>
#include <iostream>

class ViewManager {
public:
    ViewManager(int screenWidth, int screenHeight);
    ~ViewManager();

    void UpdateCamera(InputManager& inputManager, std::vector<Colony*>& colonies, Planet* planet);
    void ResetCameraForCurrentView(View view, std::vector<Colony*>& colonies, Colony* currentColony, Planet* planet);

    void SwitchToColonyView(Colony* currentColony);
    void SwitchToSectView(Colony* currentColony, Sect* currentSect);
    void SwitchToUnitView(Colony* currentColony, Sect* currentSect, Unit* currentUnit);
    void SwitchToPlanetView(Colony* currentColony);
    void SwitchToOrbitalView();

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

    void HandleCameraControls(InputManager& inputManager, std::vector<Colony*>& colonies, Planet* planet);
    void HandlePlanetViewCamera(InputManager& inputManager, Planet* planet);
    void HandleColonyViewCamera(InputManager& inputManager);
    void ClampCameraColonyView();
};

#endif // VIEW_MANAGER_H
