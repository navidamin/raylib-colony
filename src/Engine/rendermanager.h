#ifndef RENDER_MANAGER_H
#define RENDER_MANAGER_H

#include "raylib.h"
#include "game_constants.h"
#include "planet.h"
#include "colony.h"
#include "sect.h"
#include "unit.h"
#include "time_manager.h"
#include "inputmanager.h"
#include <vector>
#include <string>

class RenderManager {
public:
    RenderManager(int screenWidth, int screenHeight);
    ~RenderManager();

    void BeginDraw();
    void EndDraw();

    void DrawMenuView();
    void DrawPlanetView(Camera2D camera, Planet* planet, std::vector<Colony*>& colonies,
                       InputManager& inputManager, TimeManager& timeManager);
    void DrawColonyView(Camera2D camera, Colony* colony, Planet* planet, std::vector<Colony *> &colonies,
                        InputManager& inputManager, TimeManager& timeManager);
    void DrawSectView(Sect* sect, TimeManager& timeManager);
    void DrawUnitView(Unit* unit);

    void DrawCellInfo(Vector2 mousePosition, Camera2D camera, Planet* planet, std::vector<Colony*>& colonies);
    void DrawPlusIndicator(Vector2 mousePos, View currentView);

private:
    int screenWidth;
    int screenHeight;

    // Moon surface tile textures
    Texture2D moonTiles[3];
    bool tilesLoaded;
    std::vector<int> tilePattern;  // Store which tile to use for each grid cell

    void DrawDebugActiveArea();

    // Function to load the moon surface tiles
    void LoadMoonTiles();
    // Function to render the tiled moon surface
    void RenderMoonSurface();
    // Function to unload moon surface tiles
    void UnloadMoonTiles();
    // Function to generate tile pattern
    void GenerateTilePattern();
};

#endif // RENDER_MANAGER_H
