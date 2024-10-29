#ifndef ENGINE_H
#define ENGINE_H

#include "raylib.h"
#include "raymath.h"
#include "planet.h"
#include "colony.h"
#include "sect.h"
#include "unit.h"
#include <vector>

enum class View {
    Menu,
    Planet,
    Colony,
    Sect,
    Unit
};

class Engine {
public:
    Engine(int screenWidth, int screenHeight, const char* title);
    ~Engine();

    void InitGame();
    void Run();

private:
    void HandleInput();
    void Update();
    void Draw();

    bool IsDoubleClick();
    void SwitchToColonyView();
    void SwitchToSectView();
    void SwitchToUnitView();
    void SwitchToPlanetView();
    void SelectColony(Vector2 mousePosition);
    void SelectSect(Vector2 mousePosition);
    void SelectUnit(Vector2 mousePosition);

    int screenWidth;
    int screenHeight;
    View currentView;  // Changed from currentState to currentView

    Planet* planet;
    std::vector<Colony*> colonies;
    Colony* currentColony;
    Sect* currentSect;
    Unit* currentUnit;

    // Camera state
    Camera2D camera;
    float minZoom;
    float maxZoom;
    Vector2 dragStart;
    bool isDragging;

    // Camera methods
    void UpdateCamera();
    void ClampCamera();
    void HandleCameraControls();
    void ResetCameraForCurrentView();
    Vector2 GetWorldMousePosition();

    // Constants for the world
    const float SECT_CORE_RADIUS = 50.0f;
    const int PLANET_SIZE = 20; // 20x20 grid of possible sect locations
    const float PLANET_WIDTH = PLANET_SIZE * SECT_CORE_RADIUS * 2.0f;  // Total width of planet
    const float PLANET_HEIGHT = PLANET_SIZE * SECT_CORE_RADIUS * 2.0f; // Total height of planet

    // Double-click detection
    double lastClickTime;
    Vector2 lastClickPosition;


};

#endif // ENGINE_H
