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
#include "game_structs.h"
#include "lunar_frame.h"
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
    // The globe, with every colony marked at its real place; `current`
    // is drawn brighter. The argument-less form is for harnesses with
    // no colonies to show.
    void DrawOrbitalView(std::vector<Colony*>& colonies, const Colony* current);
    void DrawOrbitalView();
    void DrawColonyView(Camera2D camera, Colony* colony, Planet* planet, std::vector<Colony *> &colonies,
                        InputManager& inputManager, TimeManager& timeManager, Road* selectedRoad = nullptr,
                        bool buildRoadMode = false, Sect* roadBuildStartSect = nullptr);
    void DrawSectView(Sect* sect, TimeManager& timeManager);
    void DrawUnitView(Unit* unit, TimeManager& timeManager);

    // The ground under the cursor: where it is, whose it is, what is in it.
    void DrawPointInfo(Vector2 mousePosition, const LunarPoint& point, Planet* planet,
                       std::vector<Colony*>& colonies);
    void DrawPlusIndicator(Vector2 mousePos, View currentView);

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

    // Crystal sample sprites, lazy-loaded from src/assets/sprites/samples/
    std::map<std::string, Texture2D> crystalTextures;
    const Texture2D* GetCrystalTexture(const CrystalVisual& visual);
    void DrawCrystalSprite(const CrystalVisual& visual, Rectangle dest);

    // Orbital view textures (baked by prototypes/planet_visuals/asset_bake.py)
    Texture2D orbitalNearTexture;
    bool orbitalAssetsLoaded;
    void LoadOrbitalAssets();
    void UnloadOrbitalAssets();

    // Generated terrain (real-imagery amplification).
    //
    // One chain per place gives all three geographic views their ground:
    // level 0 = PLANET (100 km), 1 = COLONY (25 km), 2 = SECT (5 km),
    // each the centre crop of the one above and all registered on the
    // same LunarPoint, so zooming is continuous. The colony you are
    // looking at is kept alongside the chains of its sects — the only
    // places one click away — and those are built before they are asked
    // for, which is what makes opening a sect free rather than a
    // 33-frame hitch. Two ways to build one, chosen once by
    // GetTerrainPath(): the CPU chain (~0.5 s, so it runs on worker
    // threads) or the GPU chain (milliseconds, main thread, one per
    // frame). Nine chains at 512 is about 28 MB; at the GPU's 1024,
    // 113 MB.
    static const int TERRAIN_CACHE_SLOTS = 9;
    static const int TERRAIN_RES = 512;     // the CPU path's resolution

    struct TerrainCacheEntry
    {
        Texture2D levels[3] = {};
        RenderTexture2D targets[3] = {};    // GPU-built: owns the textures
        LunarKey key;                       // the place, quantised
        unsigned int lastUsed = 0;          // LRU stamp
        bool valid = false;
    };
    TerrainCacheEntry terrainCache[TERRAIN_CACHE_SLOTS];
    unsigned int terrainClock;          // increments per lookup

    // The chain the three view layers currently draw from.
    Texture2D terrainLevels[3];
    bool terrainLoaded;
    LunarKey terrainKey;

    // Bind the chain registered on a place, building it now on a miss.
    void EnsureTerrainAt(const LunarPoint& point);
    // Queue a place's chain for the frames ahead.
    void RequestTerrainAt(const LunarPoint& point);
    void UnloadTerrainLevels();

    // Cache plumbing.
    int FindTerrainSlot(const LunarKey& key) const;
    int ClaimTerrainSlot();                       // LRU victim, unloaded
    void BindTerrainSlot(int slot);               // slot -> terrainLevels
    void ReleaseTerrainEntry(TerrainCacheEntry& e);
    void UploadReadyTerrain();                    // finished work -> cache
    void ShutdownTerrainWorkers();

    void DrawSectTerrainBackground(Sect* sect);
    // Ground for the panned Colony view. spanCells is how many 5 km sect
    // footprints the level covers (5 for the 25 km window); centre is the
    // point in the view's frame the level is registered on.
    void DrawWorldTerrainLayer(int level, Vector2 centre, float spanCells);

    // Shared modular unit UI: chrome used by every unit type
    void DrawModularUnitView(Unit* unit, TimeManager& timeManager);
    void DrawUnitTopBar(Unit* unit, TimeManager& timeManager);
    void DrawUnitBottomBar(Unit* unit);
    void DrawUnitModuleList(Unit* unit);
    void DrawUnitModuleCenter(Unit* unit);
    void DrawUnitControlPanel(Unit* unit);

    // Module-specific center panels (Extraction)
    void DrawProspectingPanel(Unit* unit, int x, int y, int w, int h);
    void DrawExcavationPanel(Unit* unit, int x, int y, int w, int h);
    void DrawBeneficiationPanel(Unit* unit, int x, int y, int w, int h);
    void DrawOperationsPanel(Unit* unit, int x, int y, int w, int h);
    void DrawDirectivesPanel(Unit* unit, int x, int y, int w, int h);

    // Fallback center panel for modules without a bespoke layout yet
    void DrawGenericModulePanel(Unit* unit, int x, int y, int w, int h);

    void DrawUnitResourceOverview(Unit* unit, int x, int y, int w, int h);

    // Styled drawing helpers
    void DrawStyledBar(float x, float y, float w, float h, float value, Color fillColor);
    void DrawWearBar(float x, float y, float w, float h, float wear);
    void DrawTierIndicator(float x, float y, int tier, int maxTier = 3);

    // Helper for drawing styled roads
    void DrawDashedLine(Vector2 start, Vector2 end, float dashLength, float gapLength,
                        float thickness, Color color);

};

#endif // RENDER_MANAGER_H
