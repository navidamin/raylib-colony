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

    // Global variables for the background
    std::vector<BackgroundTile> backgroundTiles;
    std::vector<Texture2D> tileTextures;
    bool texturesLoaded;
    bool bgGenerated;


    void DrawDebugActiveArea();

    // Function to load the tile textures
    void LoadTileTextures();
    // Function to generate the background with random tiles
    void GenerateBackground(int screenWidth, int screenHeight, int tileSize);
    // Function to render the background
    void RenderBackground();
    // Function to unload textures
    void UnloadTileTextures();

};
#endif // RENDER_MANAGER_H
