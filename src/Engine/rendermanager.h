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
#include "site_selection_controller.h"
#include "survey_hints.h"
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

    // The site-selection descent, at whatever rung the controller is on:
    // the globe with the region under the pointer lit and named, the
    // district and site windows with the ladder cursor, the region and
    // level cards, the verdict, the prompt strip. Flights draw the rung
    // being left under the moving camera. Colonies are marked wherever
    // they fall in the picture. The globe's own drag/zoom input is the
    // caller's to run first (SurveyFlow does), so hover lands on this
    // frame's orientation. rendermanager_survey.cpp.
    void DrawSurveyView(const SiteSelectionController& ctl, const SurveyLayout& layout,
                        Planet* planet, std::vector<Colony*>& colonies,
                        const Colony* current);
    // Have a window's ground ready for the frames ahead (a flight's
    // destination). spanKm as for EnsureTerrainAt.
    void RequestGround(const LunarPoint& point, float spanKm);
    // The texture span the survey and Colony views build for a window of
    // spanKm on this screen: widened so a landscape screen's width is
    // covered too.
    float WindowTextureSpanKm(double spanKm) const;
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

public:
    // What a cache entry is: a place, and either the game's own 100/25/5
    // chain with the occupied-site disturbance (spanTenths 0, what the
    // Sect view draws) or a single window of spanKm on natural ground
    // (what the survey rungs and the Colony view draw). Public because
    // the worker pool's jobs carry one.
    struct TerrainKey
    {
        LunarKey place;
        int spanTenths = 0;
        bool operator==(const TerrainKey& o) const
        {
            return place == o.place && spanTenths == o.spanTenths;
        }
    };
    static TerrainKey MakeTerrainKey(const LunarPoint& point, float spanKm);

private:

    struct TerrainCacheEntry
    {
        Texture2D levels[3] = {};
        RenderTexture2D targets[3] = {};    // GPU-built: owns the textures
        int levelCount = 0;                 // how many of levels are real
        TerrainKey key;
        unsigned int lastUsed = 0;          // LRU stamp
        bool valid = false;
    };
    TerrainCacheEntry terrainCache[TERRAIN_CACHE_SLOTS];
    unsigned int terrainClock;          // increments per lookup

    // The chain the view layers currently draw from. For a window entry
    // the picture is the last real level.
    Texture2D terrainLevels[3];
    int terrainLevelCount;
    bool terrainLoaded;
    TerrainKey terrainKey;

    // Bind the chain registered on a place, building it now on a miss.
    // spanKm 0 is the game chain; otherwise one window of that span.
    void EnsureTerrainAt(const LunarPoint& point, float spanKm = 0.0f);
    // Queue a place's chain for the frames ahead.
    void RequestTerrainAt(const LunarPoint& point, float spanKm = 0.0f);
    // The bound window picture (the last real level), or nullptr.
    const Texture2D* BoundTerrainWindow() const;
    void UnloadTerrainLevels();

    // Cache plumbing.
    int FindTerrainSlot(const TerrainKey& key) const;
    int ClaimTerrainSlot();                       // LRU victim, unloaded
    void BindTerrainSlot(int slot);               // slot -> terrainLevels
    void ReleaseTerrainEntry(TerrainCacheEntry& e);
    void UploadReadyTerrain();                    // finished work -> cache
    void ShutdownTerrainWorkers();

    void DrawSectTerrainBackground(Sect* sect);
    // Ground for the panned Colony view. spanCells is how many 5 km sect
    // footprints the level covers; centre is the point in the view's
    // frame the level is registered on.
    void DrawWorldTerrainLayer(int level, Vector2 centre, float spanCells);

    // Survey drawing (rendermanager_survey.cpp). The hint tooltip is
    // queued by the card that owns its row and flushed last, on top.
    SurveyHintResult pendingHint = { nullptr, nullptr };
    int pendingHintRowY = -1;
    int pendingHintRight = 0;
    void SurveyDrawGlobeRung(const SiteSelectionController& ctl, const SurveyLayout& layout,
                             std::vector<Colony*>& colonies, const Colony* current,
                             int w, int h);
    void SurveyDrawWindowRung(const SiteSelectionController& ctl, const SurveyLayout& layout,
                              std::vector<Colony*>& colonies, const Colony* current,
                              int w, int h);
    void SurveyDrawFlight(const SiteSelectionController& ctl, std::vector<Colony*>& colonies,
                          const Colony* current, int w, int h);
    void SurveyDrawGround(const SurveyCursor& cursor, const SurveyViewport& viewport,
                          float zoomK, int w, int h);
    void SurveyDrawRegionCard(const RegionIdentity& id, int level, int px, int py, int pw,
                              const char* hoverKey, float psrKm);
    void SurveyDrawLevelCard(int level, const GroundStats& g,
                             const TerrainBuildability* siteB,
                             const PlacementVerdict* verdict, int px, int py, int pw);
    void SurveyDrawHintTooltip();
    void SurveyDrawStrip(const SiteSelectionController& ctl, const SurveyLayout& layout,
                         int w, int h);
    void SurveyDrawGlobeMarkers(std::vector<Colony*>& colonies, const Colony* current,
                                Vector2 pointer, int w, int h, int* hoverIndex);
    void SurveyDrawWindowMarkers(const SurveyCursor& cursor, const SurveyViewport& viewport,
                                 std::vector<Colony*>& colonies, const Colony* current);

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
