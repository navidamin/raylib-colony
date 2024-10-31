#ifndef ENGINE_H
#define ENGINE_H

#include "raylib.h"
#include "raymath.h"
#include <vector>
#include "planet.h"
#include "colony.h"
#include "sect.h"
#include "unit.h"
#include "game_constants.h"
#include "time_manager.h"


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
    void DrawCellInfo(Vector2 mousePosition);
    bool IsInfoKeyPressed() const { return IsKeyDown(KEY_TAB); }


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
    void HandleCameraControls();
    void HandlePlanetViewCamera();
    void HandleColonyViewCamera();
    void ClampCameraColonyView();
    void ResetCameraForCurrentView();
    Vector2 GetWorldMousePosition();
    void UpdatePlanetActiveArea();



    // Double-click detection
    double lastClickTime;
    Vector2 lastClickPosition;

    void DrawDebugActiveArea();

};

#endif // ENGINE_H
