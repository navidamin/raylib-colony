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
#include <map>

class RenderManager {
public:
    RenderManager(int screenWidth, int screenHeight);
    ~RenderManager();

    void LoadFonts();

    void BeginDraw();
    void EndDraw();

    void DrawMenuView();
    void DrawOrbitalView();
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

    // Crystal sample sprites, lazy-loaded from src/assets/sprites/samples/
    std::map<std::string, Texture2D> crystalTextures;
    const Texture2D* GetCrystalTexture(const CrystalVisual& visual);
    void DrawCrystalSprite(const CrystalVisual& visual, Rectangle dest);

    // Orbital view textures (baked by prototypes/planet_visuals/asset_bake.py)
    Texture2D orbitalNearTexture;
    Texture2D orbitalFarTexture;
    bool orbitalAssetsLoaded;
    void LoadOrbitalAssets();
    void UnloadOrbitalAssets();

    // Generated terrain (real-imagery amplification).
    //
    // One chain per location gives all three geographic views their
    // ground: level 0 = PLANET (100 km), 1 = COLONY (25 km), 2 = SECT
    // (5 km). Because each level is the centre of the one above, the
    // views are registered to each other and zooming is continuous.
    Texture2D terrainLevels[3];
    bool terrainLoaded;
    int terrainCellX;
    int terrainCellY;
    unsigned int terrainAnchorVersion;
    void EnsureTerrainForCell(int gx, int gy);
    void UnloadTerrainLevels();

    // Full-planet 2D map (the whole moon, equirectangular) that the
    // planet view zooms out to. Aligned with the playfield grid where
    // the two meet, so zooming out is continuous.
    Texture2D planetMapTexture;
    bool planetMapLoaded;
    void LoadPlanetMap();
    void DrawPlanetMapLayer(Camera2D camera);

    void DrawSectTerrainBackground(Sect* sect);
    // World-space ground for the panned views. spanCells is how many
    // 5 km grid cells the level covers (20 for PLANET, 5 for COLONY);
    // centre is the world point the level is registered on.
    void DrawWorldTerrainLayer(int level, Vector2 centre, float spanCells);

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
