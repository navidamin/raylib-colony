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
#include "transport_types.h"
#include "game_enums.h"
#include <vector>
#include <string>

class RenderManager {
public:
    RenderManager(int screenWidth, int screenHeight);
    ~RenderManager();

    void LoadFonts();

    void BeginDraw();
    void EndDraw();

    void DrawMenuView();
    void DrawPlanetView(Camera2D camera, Planet* planet, std::vector<Colony*>& colonies,
                       InputManager& inputManager, TimeManager& timeManager);
    void DrawColonyView(Camera2D camera, Colony* colony, Planet* planet, std::vector<Colony *> &colonies,
                        InputManager& inputManager, TimeManager& timeManager, Road* selectedRoad = nullptr,
                        bool buildRoadMode = false, Sect* roadBuildStartSect = nullptr);
    void DrawSectView(Sect* sect, TimeManager& timeManager);
    void DrawUnitView(Unit* unit, TimeManager& timeManager);

    void DrawCellInfo(Vector2 mousePosition, Camera2D camera, Planet* planet, std::vector<Colony*>& colonies);
    void DrawPlusIndicator(Vector2 mousePos, View currentView);

    // Site selection view
    void DrawSiteSelectionView(Camera2D camera, Planet* planet, Vector2 hoveredGridPos,
                               TimeManager& timeManager);

    // Transport visualization
    void DrawRoads(Colony* colony, Road* selectedRoad = nullptr);
    void DrawTransportPackets(Colony* colony);
    void DrawRoadInfoPanel(Road* selectedRoad, Colony* colony);

private:
    int screenWidth;
    int screenHeight;

    // UI fonts
    Font uiFont;        // Exo 2 Regular - body text, data readouts
    Font uiHeaderFont;  // Exo 2 Bold - section headers
    bool fontsLoaded;

    // Font size multiplier (XL preset: 1.30x)
    float FS(float baseSize);

    // Moon surface tile textures
    Texture2D moonTiles[3];
    bool tilesLoaded;
    std::vector<int> tilePattern;  // Store which tile to use for each grid cell

    void DrawDebugActiveArea();

    // Extraction unit UI methods
    void DrawExtractionUnitView(Unit* unit, TimeManager& timeManager);
    void DrawExtractionTopBar(Unit* unit, TimeManager& timeManager);
    void DrawExtractionBottomBar(Unit* unit);
    void DrawExtractionModuleList(Unit* unit);
    void DrawExtractionModuleCenter(Unit* unit);
    void DrawExtractionControlPanel(Unit* unit);

    // Module-specific center panels
    void DrawProspectingPanel(Unit* unit, int x, int y, int w, int h);
    void DrawExcavationPanel(Unit* unit, int x, int y, int w, int h);
    void DrawBeneficiationPanel(Unit* unit, int x, int y, int w, int h);
    void DrawOperationsPanel(Unit* unit, int x, int y, int w, int h);
    void DrawDirectivesPanel(Unit* unit, int x, int y, int w, int h);
    void DrawExtractionResourceOverview(Unit* unit, int x, int y, int w, int h);

    // Styled drawing helpers
    void DrawStyledBar(float x, float y, float w, float h, float value, Color fillColor);
    void DrawWearBar(float x, float y, float w, float h, float wear);
    void DrawTierIndicator(float x, float y, int tier, int maxTier = 3);

    // Helper for drawing styled roads
    void DrawDashedLine(Vector2 start, Vector2 end, float dashLength, float gapLength,
                        float thickness, Color color);

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
