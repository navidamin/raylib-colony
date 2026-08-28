#include "rendermanager.h"
#include "resource_manager.h"
#include "terrain_synthesis.h"
#include "resource_types.h"
#include "survey_progress_engine.h"
#include <algorithm>
#include <iostream>
#include <cmath>

RenderManager::RenderManager(int screenWidth, int screenHeight)
    : screenWidth(screenWidth),
      screenHeight(screenHeight),
      fontsLoaded(false),
      tilesLoaded(false),
      orbitalAssetsLoaded(false),
      terrainLoaded(false),
      terrainCellX(-1),
      terrainCellY(-1),
      terrainAnchorVersion(0),
      planetMapLoaded(false)
{
    planetMapTexture = {0};
    orbitalNearTexture = {0};
    orbitalFarTexture = {0};
    for (int i = 0; i < 3; i++) terrainLevels[i] = {0};
}

void RenderManager::LoadFonts()
{
    uiFont = LoadFontEx("src/assets/fonts/Exo2-Regular.ttf", 48, nullptr, 0);
    uiHeaderFont = LoadFontEx("src/assets/fonts/Exo2-Bold.ttf", 48, nullptr, 0);

    if (uiFont.glyphCount > 0 && uiHeaderFont.glyphCount > 0)
    {
        SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(uiHeaderFont.texture, TEXTURE_FILTER_BILINEAR);
        fontsLoaded = true;
    }
    else
    {
        std::cout << "WARNING: Failed to load UI fonts, falling back to default" << std::endl;
    }
}

RenderManager::~RenderManager() {
    // Unload fonts
    if (fontsLoaded)
    {
        UnloadFont(uiFont);
        UnloadFont(uiHeaderFont);
    }

    // Unload moon surface tiles when done
    UnloadMoonTiles();

    // Unload cached crystal sample sprites
    for (auto& [path, texture] : crystalTextures)
    {
        UnloadTexture(texture);
    }
    crystalTextures.clear();

    UnloadOrbitalAssets();

    UnloadTerrainLevels();

    if (planetMapLoaded)
    {
        UnloadTexture(planetMapTexture);
    }
}

// --- Crystal sample sprites ------------------------------------------------
// Pre-rendered 3D crystal sprites live in src/assets/sprites/samples/, one
// directory per shape family/template, one file per size x glow variant.
// CrystalVisual encodes exactly those coordinates, plus the element tint.

const Texture2D* RenderManager::GetCrystalTexture(const CrystalVisual& visual)
{
    static const char* familyDirs[4] = {"family_a", "family_b", "family_c", "family_d"};
    static const char* shapeDirs[4][5] = {
        {"a1_cleaved", "a2_shatter", "a3_wedge", "a4_stacked", "a5_corner"},
        {"b1_crystal", "b2_twin", "b3_needle", "b4_tabular", "b5_druzy"},
        {"c1_cobble", "c2_botryoidal", "c3_concretion", "c4_pebble", "c5_split"},
        {"d1_flagstone", "d2_shale", "d3_crossbed", "d4_laminate", "d5_breccia"},
    };

    int family = std::min(std::max(static_cast<int>(visual.shapeFamily), 0), 3);
    int shape = std::min(std::max(visual.templateIndex, 0), 4);
    int size = std::min(std::max(visual.sizeLevel, 1), 4);
    int glow = std::min(std::max(visual.glowLevel, 0), 4);

    std::string path = TextFormat("src/assets/sprites/samples/%s/%s/size_%d_glow_%d.png",
                                  familyDirs[family], shapeDirs[family][shape], size, glow);

    auto it = crystalTextures.find(path);
    if (it != crystalTextures.end()) return &it->second;

    if (!FileExists(path.c_str())) return nullptr;

    Texture2D texture = LoadTexture(path.c_str());
    if (texture.id == 0) return nullptr;

    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    auto [inserted, ok] = crystalTextures.emplace(path, texture);
    return &inserted->second;
}

void RenderManager::DrawCrystalSprite(const CrystalVisual& visual, Rectangle dest)
{
    const Texture2D* texture = GetCrystalTexture(visual);
    if (!texture)
    {
        // Fallback: element-colored chip if the sprite set is missing
        Color c = visual.elementColor;
        c.a = 200;
        DrawRectangleRounded(dest, 0.3f, 4, c);
        return;
    }

    // Fit inside dest preserving aspect ratio. The sprites carry generous
    // transparent margins, so overscan slightly to make the crystal fill
    // the slot.
    float scale = std::min(dest.width / texture->width, dest.height / texture->height) * 1.4f;
    float drawW = texture->width * scale;
    float drawH = texture->height * scale;
    Rectangle target = {dest.x + (dest.width - drawW) / 2.0f,
                        dest.y + (dest.height - drawH) / 2.0f, drawW, drawH};

    // Tint toward the dominant element, lifted so dark tints don't kill the
    // sprite's baked-in lighting
    Color tint = visual.elementColor;
    tint.r = static_cast<unsigned char>(tint.r + (255 - tint.r) * 0.5f);
    tint.g = static_cast<unsigned char>(tint.g + (255 - tint.g) * 0.5f);
    tint.b = static_cast<unsigned char>(tint.b + (255 - tint.b) * 0.5f);

    DrawTexturePro(*texture, {0, 0, static_cast<float>(texture->width), static_cast<float>(texture->height)},
                   target, {0, 0}, 0.0f, tint);
}

void RenderManager::BeginDraw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
}

void RenderManager::EndDraw() {
    EndDrawing();
}

void RenderManager::DrawMenuView() {
    Color IVORY = {249,246,231,255};
    int fontSize = 60;
    // Loaded once, not per frame. This used to call LoadTexture every
    // draw with no matching unload -- ~1.6 MB leaked per frame, which
    // walked the menu down from 60 fps to 41 in twelve seconds.
    static Texture2D image = LoadTexture("src/assets/Logo.png");

    int textX = GetScreenWidth()/2 - MeasureText("COLONY", 60)/2;
    int textY = GetScreenHeight()/3;
    int imageX = textX - image.width + 30;  // Position image 10 pixels to the left of the text
    int imageY = GetScreenHeight()/2;  // Center the image vertically with the text

    ClearBackground(IVORY);
    DrawTexture(image, imageX, imageY, WHITE);  // Draw the image on the left
    DrawText("COLONY", GetScreenWidth()/2 - MeasureText("COLONY", 60)/2, GetScreenHeight()/3, 60, BLACK);
    DrawText("Press ENTER to start", GetScreenWidth()/2 - MeasureText("Press ENTER to start", 20)/2, GetScreenHeight()/2, 20, GRAY);
}

void RenderManager::DrawPlanetView(Camera2D camera, Planet* planet, std::vector<Colony*>& colonies,
                                  InputManager& inputManager, TimeManager& timeManager) {
    BeginMode2D(camera);

    if (planet) {  // Guard against null planet
        ClearBackground(BLACK);

        // The whole moon underneath, so zooming out leaves the
        // playfield and reveals the globe around it.
        DrawPlanetMapLayer(camera);

        // Ground: level 0 of the terrain chain (100 km) spans the whole
        // 20x20 grid, registered on the playfield anchor. This is the
        // same generated ground the sect stands on, seen from 100 km —
        // so zooming in approaches it instead of cutting to tiles.
        EnsureTerrainForCell(PLANET_SIZE / 2, PLANET_SIZE / 2);
        if (terrainLoaded && terrainLevels[0].id != 0) {
            DrawWorldTerrainLayer(0,
                Vector2{PLANET_WIDTH / 2.0f, PLANET_HEIGHT / 2.0f},
                (float)PLANET_SIZE);
        } else {
            // Fallback: the legacy 3-tile shuffle.
            if (!tilesLoaded) {
                LoadMoonTiles();
                GenerateTilePattern();
                tilesLoaded = true;
            }
            RenderMoonSurface();
        }
/*
        // Draw grid
        for (int i = 0; i <= PLANET_SIZE; i++) {
            float linePos = i * SECT_CORE_RADIUS * 2;
            DrawLineV({linePos, 0}, {linePos, PLANET_HEIGHT}, LIGHTGRAY);
            DrawLineV({0, linePos}, {PLANET_WIDTH, linePos}, LIGHTGRAY);
        }*/

        // Draw colonies if any
        for (const auto& colony : colonies) {
            colony->Draw(camera);
        }
    }

    // Show the resource map if TAB is held
    if (inputManager.IsInfoKeyPressed()) {
        planet->DrawResourceDebug(camera.zoom);
    }

    EndMode2D();

    // Show the Cell info if Ctrl+I is held
    if (inputManager.IsInfoKeyPressed()) {
        DrawCellInfo(inputManager.GetMousePosition(), camera, planet, colonies);
    }

    // Draw Add Sect when Left_ctrl pressed
    if (inputManager.IsCommandPressed()) {
        Vector2 mousePos = inputManager.GetMousePosition();
        DrawCellInfo(mousePos, camera, planet, colonies);
        DrawPlusIndicator(mousePos, View::Planet);
    }

    // Draw UI elements including time
    timeManager.Draw(screenWidth, screenHeight);
    DrawText("Planet View", 10, 10, 20, BLACK);
    DrawText("Press C for Colony View", 10, 40, 20, GRAY);

    DrawText(TextFormat("Zoom: %.2f", camera.zoom), 10, screenHeight - 20, 20, GRAY);
    DrawText("Press Ctrl+I to see map info", 10, GetScreenHeight() - 40, 20, DARKGRAY);
}

void RenderManager::DrawColonyView(Camera2D camera, Colony* colony, Planet* planet,
                                   std::vector<Colony*>& colonies, InputManager& inputManager,
                                   TimeManager& timeManager, Road* selectedRoad,
                                   bool buildRoadMode, Sect* roadBuildStartSect) {
    // Start drawing with camera transformation
    BeginMode2D(camera);

    if (colony) {
        // Ground: level 1 of the chain (25 km) centred on the colony —
        // the same ground as the planet view above and the sect below,
        // one zoom step closer.
        Vector2 colonyCentre = colony->GetSects().empty()
            ? Vector2{PLANET_WIDTH / 2.0f, PLANET_HEIGHT / 2.0f}
            : colony->GetSects()[0]->GetPosition();
        int cgx = std::clamp((int)(colonyCentre.x / (SECT_CORE_RADIUS * 2.0f)),
                             0, PLANET_SIZE - 1);
        int cgy = std::clamp((int)(colonyCentre.y / (SECT_CORE_RADIUS * 2.0f)),
                             0, PLANET_SIZE - 1);
        EnsureTerrainForCell(cgx, cgy);
        if (terrainLoaded && terrainLevels[1].id != 0) {
            // The level is registered on its cell centre, not the sect's
            // arbitrary position, so it lines up with the grid.
            Vector2 cellCentre = {
                (cgx + 0.5f) * SECT_CORE_RADIUS * 2.0f,
                (cgy + 0.5f) * SECT_CORE_RADIUS * 2.0f};
            DrawWorldTerrainLayer(1, cellCentre, 5.0f);
        } else {
            if (!tilesLoaded) {
                LoadMoonTiles();
                GenerateTilePattern();
                tilesLoaded = true;
            }
            RenderMoonSurface();
        }

        // Calculate visible area in world coordinates
        Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera);
        Vector2 bottomRight = GetScreenToWorld2D(
            {static_cast<float>(screenWidth),
             static_cast<float>(screenHeight)},
            camera
        );

        // Calculate grid line positions
        float cellSize = SECT_CORE_RADIUS * 2.0f;
        float planetWidth = PLANET_SIZE * cellSize;  // Total width of planet
        float planetHeight = PLANET_SIZE * cellSize; // Total height of planet
/*
        // Draw vertical grid lines
        int startX = std::max(0, static_cast<int>(topLeft.x / cellSize));
        int endX = std::min(PLANET_SIZE, static_cast<int>(bottomRight.x / cellSize) + 1);

        for (int i = startX; i <= endX; i++) {
            float x = i * cellSize;
            if (x <= planetWidth) {  // Only draw if within planet width
                Vector2 start = {x, 0};
                Vector2 end = {x, planetHeight};
                DrawLineV(start, end, Fade(LIGHTGRAY, 0.5f));
            }
        }

        // Draw horizontal grid lines
        int startY = std::max(0, static_cast<int>(topLeft.y / cellSize));
        int endY = std::min(PLANET_SIZE, static_cast<int>(bottomRight.y / cellSize) + 1);

        for (int i = startY; i <= endY; i++) {
            float y = i * cellSize;
            // Draw horizontal line from 0 to planetWidth (not screen width)
            DrawLineV(
                {0, y},
                {planetWidth, y},
                Fade(LIGHTGRAY, 0.5f)
            );
        }
*/
        // Draw roads between sects (behind sects)
        DrawRoads(colony, selectedRoad);

        // Draw all sects in the current colony
        for (const auto& sect : colony->GetSects()) {
            sect->DrawInColonyView(sect->GetPosition());

            // In build road mode, highlight sects
            if (buildRoadMode) {
                Vector2 sectPos = sect->GetPosition();
                float sectRadius = sect->GetRadius();

                if (sect == roadBuildStartSect) {
                    // Highlight start sect in green
                    DrawCircleLinesV(sectPos, sectRadius + 10.0f, GREEN);
                    DrawCircleLinesV(sectPos, sectRadius + 12.0f, GREEN);
                    DrawText("START", sectPos.x - 20, sectPos.y - sectRadius - 25, 12, GREEN);
                } else {
                    // Highlight other sects as potential targets
                    DrawCircleLinesV(sectPos, sectRadius + 8.0f, ColorAlpha(YELLOW, 0.5f));
                }
            }
        }

        // In build mode with start sect selected, draw line to cursor
        if (buildRoadMode && roadBuildStartSect) {
            Vector2 startPos = roadBuildStartSect->GetPosition();
            Vector2 cursorWorldPos = GetScreenToWorld2D(inputManager.GetMousePosition(), camera);
            DrawLineEx(startPos, cursorWorldPos, 3.0f, ColorAlpha(GREEN, 0.6f));
            DrawCircleV(cursorWorldPos, 8.0f, ColorAlpha(GREEN, 0.6f));
        }

        // Draw transport packets on roads (in front of sects)
        DrawTransportPackets(colony);
    }

    // Show the resource map if TAB is held
    if (inputManager.IsInfoKeyPressed()) {
        planet->DrawResourceDebug(camera.zoom);
    }

    EndMode2D();

    // Draw road info panel (screen-space, after EndMode2D)
    if (selectedRoad) {
        DrawRoadInfoPanel(selectedRoad, colony);
    }

    // Build road mode indicator (screen-space)
    if (buildRoadMode) {
        // Mode indicator at top center
        const char* modeText = "ROAD BUILD MODE";
        int textWidth = MeasureText(modeText, 24);
        DrawRectangle(screenWidth/2 - textWidth/2 - 10, 5, textWidth + 20, 35, ColorAlpha(GREEN, 0.8f));
        DrawText(modeText, screenWidth/2 - textWidth/2, 10, 24, WHITE);

        // Instructions
        const char* instructions = roadBuildStartSect ?
            "Click another sect to complete road | B to exit" :
            "Click a sect to start | B to exit";
        int instrWidth = MeasureText(instructions, 16);
        DrawText(instructions, screenWidth/2 - instrWidth/2, 45, 16, GREEN);
    }

    // Show the Cell info if Ctrl+I is held
    if (inputManager.IsInfoKeyPressed()) {
        DrawCellInfo(inputManager.GetMousePosition(), camera, planet, colonies);
    }

    if (inputManager.IsCommandPressed()) {
        Vector2 mousePos = inputManager.GetMousePosition();
        DrawCellInfo(mousePos, camera, planet, colonies);
        DrawPlusIndicator(mousePos, View::Colony);

        // Resource preview overlay for sect placement
        if (planet)
        {
            Vector2 worldPos = GetScreenToWorld2D(mousePos, camera);
            float cellSize = SECT_CORE_RADIUS * 2.0f;
            int gx = static_cast<int>(std::floor(worldPos.x / cellSize));
            int gy = static_cast<int>(std::floor(worldPos.y / cellSize));

            if (gx >= 0 && gx < PLANET_SIZE && gy >= 0 && gy < PLANET_SIZE)
            {
                auto survey = planet->GetResourceManager().GetOrbitalSurveyAt(gx, gy);
                auto resources = planet->GetResourceManager().GetResourcesAtGrid(gx, gy);

                // Draw tooltip near cursor
                int tooltipX = static_cast<int>(mousePos.x) + 20;
                int tooltipY = static_cast<int>(mousePos.y) - 120;
                int tooltipW = 200;
                int tooltipH = 140;

                // Clamp to screen
                if (tooltipX + tooltipW > screenWidth) tooltipX = static_cast<int>(mousePos.x) - tooltipW - 20;
                if (tooltipY < 0) tooltipY = static_cast<int>(mousePos.y) + 20;

                DrawRectangle(tooltipX, tooltipY, tooltipW, tooltipH, {20, 20, 40, 220});
                DrawRectangleLines(tooltipX, tooltipY, tooltipW, tooltipH, {100, 100, 200, 200});

                int ty = tooltipY + 5;
                DrawText("ORBITAL SURVEY", tooltipX + 5, ty, 12, {200, 255, 200, 255});
                ty += 16;

                // Show key resources — categories only, no exact values
                const ResourceType rawTypes[] = {
                    ResourceType::Fe, ResourceType::Ti, ResourceType::Si,
                    ResourceType::Al, ResourceType::Ca, ResourceType::H2
                };
                const char* rawNames[] = {"Fe", "Ti", "Si", "Al", "Ca", "H2"};

                for (int i = 0; i < 6; i++)
                {
                    float abundance = 0.0f;
                    for (const auto& [type, val] : resources)
                    {
                        if (type == rawTypes[i])
                        {
                            abundance = val;
                            break;
                        }
                    }

                    const char* level = abundance > 3000.0f ? "HIGH" :
                                        abundance > 500.0f ? "MED" : "LOW";
                    Color levelColor = abundance > 3000.0f ? GREEN :
                                       abundance > 500.0f ? YELLOW : RED;

                    DrawText(TextFormat("%-4s %s", rawNames[i], level),
                             tooltipX + 5, ty, 11, levelColor);
                    ty += 14;
                }

                // Overall site rating
                float totalScore = 0.0f;
                for (const auto& [type, val] : resources)
                {
                    totalScore += val;
                }
                const char* scoreLabel = totalScore > 15000.0f ? "HIGH" :
                                         totalScore > 5000.0f ? "MED" : "LOW";
                Color scoreColor = totalScore > 15000.0f ? GREEN :
                                   totalScore > 5000.0f ? YELLOW : RED;

                DrawText(TextFormat("Overall: %s", scoreLabel),
                         tooltipX + 5, ty, 12, scoreColor);
                ty += 16;
                DrawText("(prospect for detail)", tooltipX + 5, ty, 10, {150, 150, 180, 200});
            }
        }
    }

    // Draw UI elements including time
    timeManager.Draw(screenWidth, screenHeight);
    DrawText("Colony View", 10, 10, 20, BLACK);
    DrawText("Press S for Sect View", 10, 40, 20, GRAY);
    DrawText("Press P for Planet View", 10, 70, 20, GRAY);

    DrawText(TextFormat("Zoom: %.2f", camera.zoom), 10, screenHeight - 20, 20, GRAY);
    DrawText("Press Ctrl+I to see map info", 10, GetScreenHeight() - 40, 20, DARKGRAY);

    // Draw colony reserve dashboard (left side)
    if (colony)
    {
        const auto& reserves = colony->GetStrategicReserves();
        const auto& resCap = colony->GetReserveCapacity();
        const auto& resources = GetResourceDescriptors();

        int dashX = 10;
        int dashY = 110;
        int dashW = 240;

        DrawRectangle(dashX, dashY, dashW, 20, Color{30, 30, 40, 220});
        DrawText("COLONY RESERVES", dashX + 5, dashY + 3, 14, WHITE);
        dashY += 22;

        for (const auto& res : resources)
        {
            if (res.category != ResourceCategory::SINGULAR) continue;

            auto sIt = reserves.find(res.type);
            auto cIt = resCap.find(res.type);
            float stored = (sIt != reserves.end()) ? sIt->second : 0.0f;
            float cap = (cIt != resCap.end()) ? cIt->second : 0.0f;

            if (cap <= 0.0f && stored <= 0.0f) continue;

            float fill = cap > 0.0f ? stored / cap : 0.0f;
            Color barColor;
            if (fill > 0.9f) barColor = Color{255, 100, 100, 255};
            else if (fill > 0.7f) barColor = YELLOW;
            else barColor = Color{80, 180, 220, 255};

            DrawText(res.name, dashX + 5, dashY + 1, 11, LIGHTGRAY);
            float barX = (float)(dashX + 80);
            float barW = (float)(dashW - 80 - 5);
            DrawRectangle((int)barX, dashY, (int)barW, 14, Color{40, 40, 60, 200});
            DrawRectangle((int)barX, dashY, (int)(barW * fill), 14, barColor);
            DrawText(TextFormat("%.0f/%.0f", stored, cap), (int)barX + 3, dashY + 1, 11, WHITE);

            dashY += 16;
        }
    }

    // Draw colony reserve upgrade panel (bottom-right)
    if (colony)
    {
        int panelW = 280;
        int panelH = 100;
        int panelX = screenWidth - panelW - 10;
        int panelY = screenHeight - panelH - 10;

        DrawRectangle(panelX, panelY, panelW, panelH, Color{30, 30, 40, 220});
        DrawRectangleLines(panelX, panelY, panelW, panelH, Color{80, 80, 100, 255});

        int level = colony->GetReserveLevel();
        float currentCap = COLONY_BASE_RESERVES * STORAGE_LEVEL_MULTIPLIERS[level];
        DrawText(TextFormat("Reserves Lv.%d (%.0f)", level, currentCap),
                 panelX + 10, panelY + 10, 16, WHITE);

        if (level < MAX_STORAGE_LEVEL)
        {
            int next = level + 1;
            float nextCap = COLONY_BASE_RESERVES * STORAGE_LEVEL_MULTIPLIERS[next];
            DrawText(TextFormat("Next: %.0f  Cost: Fe %.0f Si %.0f E %.0f",
                     nextCap, COLONY_UPGRADE_COST_FE[next],
                     COLONY_UPGRADE_COST_SI[next], COLONY_UPGRADE_COST_ENERGY[next]),
                     panelX + 10, panelY + 32, 12, LIGHTGRAY);

            Rectangle btnRect = {(float)(panelX + 10), (float)(panelY + 55), 120.0f, 30.0f};
            bool canUpgrade = colony->CanUpgradeReserves();
            Color btnColor = canUpgrade ? Color{60, 140, 60, 255} : Color{80, 80, 80, 255};
            Color btnTextColor = canUpgrade ? WHITE : GRAY;

            DrawRectangleRec(btnRect, btnColor);
            DrawRectangleLinesEx(btnRect, 1.0f, canUpgrade ? GREEN : DARKGRAY);
            DrawText("UPGRADE", (int)btnRect.x + 20, (int)btnRect.y + 8, 14, btnTextColor);

            if (canUpgrade && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                Vector2 mouse = GetMousePosition();
                if (CheckCollisionPointRec(mouse, btnRect))
                {
                    colony->UpgradeReserves();
                }
            }
        }
        else
        {
            DrawText("MAX LEVEL", panelX + 10, panelY + 55, 16, Color{100, 200, 100, 255});
        }
    }
}

// ---------------------------------------------------------------------------
// Full-planet map
//
// The 20x20 grid covers 100 km of real moon centred on the playfield
// anchor. Extending that same projection across the whole globe gives
// the planet view something to zoom out INTO: one world space holding
// both the playfield and the entire moon, aligned exactly where they
// meet, so zooming out never cuts to a different scene.
//
// Longitude uses the anchor's cos(lat) scale, which is what makes the
// playfield land on the grid exactly. The cost is that the far side of
// the globe is squashed horizontally by that same factor — acceptable
// while the map is context rather than a place you operate.
// ---------------------------------------------------------------------------

static float PlanetUnitsPerDegLat()
{
    double latSpanDeg = (PLANET_SIZE * TERRAIN_CELL_KM) / MOON_KM_PER_DEG;
    return (float)(PLANET_HEIGHT / latSpanDeg);
}

static float PlanetUnitsPerDegLon()
{
    double alat, alon;
    GetTerrainAnchor(&alat, &alon);
    (void)alon;
    return PlanetUnitsPerDegLat()
           * (float)std::max(0.2, std::cos(alat * DEG2RAD));
}

// World rect the whole moon occupies (lon -180..180, lat +90..-90).
static Rectangle PlanetMapWorldRect()
{
    double alat, alon;
    GetTerrainAnchor(&alat, &alon);
    float updLat = PlanetUnitsPerDegLat();
    float updLon = PlanetUnitsPerDegLon();
    float originX = PLANET_WIDTH * 0.5f - (float)(alon + 180.0) * updLon;
    float originY = PLANET_HEIGHT * 0.5f - (float)(90.0 - alat) * updLat;
    return Rectangle{originX, originY, 360.0f * updLon, 180.0f * updLat};
}

void RenderManager::LoadPlanetMap()
{
    if (planetMapLoaded) return;
    Image img = LoadImage("src/assets/planet/wac_global.jpg");
    if (img.data == nullptr)
    {
        planetMapLoaded = true;       // don't retry every frame
        return;
    }
    // 8K is far more than this view needs and costs VRAM; half of one
    // screen width per 180 degrees is plenty at full zoom-out.
    ImageResize(&img, 2048, 1024);
    planetMapTexture = LoadTextureFromImage(img);
    SetTextureFilter(planetMapTexture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
    planetMapLoaded = true;
}

void RenderManager::DrawPlanetMapLayer(Camera2D camera)
{
    LoadPlanetMap();
    if (planetMapTexture.id == 0) return;

    Rectangle dst = PlanetMapWorldRect();
    Rectangle src = {0, 0, (float)planetMapTexture.width,
                     (float)planetMapTexture.height};
    DrawTexturePro(planetMapTexture, src, dst, Vector2{0, 0}, 0.0f, WHITE);

    // Once the playfield is small on screen, mark it so it stays findable.
    float playfieldPx = PLANET_WIDTH * camera.zoom;
    if (playfieldPx < 220.0f)
    {
        Color gold = Color{255, 200, 100, 255};
        float pad = 6.0f / std::max(camera.zoom, 0.0001f);
        DrawRectangleLinesEx(
            Rectangle{-pad, -pad, PLANET_WIDTH + pad * 2,
                      PLANET_HEIGHT + pad * 2},
            2.0f / std::max(camera.zoom, 0.0001f), gold);
    }
}

void RenderManager::UnloadTerrainLevels()
{
    if (!terrainLoaded) return;
    for (int i = 0; i < 3; i++)
    {
        if (terrainLevels[i].id != 0) UnloadTexture(terrainLevels[i]);
        terrainLevels[i] = {0};
    }
    terrainLoaded = false;
}

// Generate (and cache) the whole 100 / 25 / 5 km chain registered on a
// grid cell. Regenerates when the cell changes or the playfield anchor
// moves (the player picking a new region from orbit).
void RenderManager::EnsureTerrainForCell(int gx, int gy)
{
    unsigned int anchorVersion = GetTerrainAnchorVersion();
    if (terrainLoaded && gx == terrainCellX && gy == terrainCellY
        && anchorVersion == terrainAnchorVersion)
    {
        return;
    }

    UnloadTerrainLevels();

    double lat, lon;
    TerrainGridCellToLatLon(gx, gy, &lat, &lon);
    // The cell we generate for is occupied, so work its ground over.
    TerrainSiteDisturbance site;
    site.enabled = true;
    Image levels[3] = {};
    GenerateTerrainChain(lat, lon, 512, levels, &site);
    for (int i = 0; i < 3; i++)
    {
        terrainLevels[i] = LoadTextureFromImage(levels[i]);
        SetTextureFilter(terrainLevels[i], TEXTURE_FILTER_BILINEAR);
        UnloadImage(levels[i]);
    }
    terrainLoaded = true;
    terrainCellX = gx;
    terrainCellY = gy;
    terrainAnchorVersion = anchorVersion;
}

// Draw a chain level as world-space ground. Called inside BeginMode2D,
// so it pans and zooms with the camera exactly like the entities on it.
void RenderManager::DrawWorldTerrainLayer(int level, Vector2 centre,
                                          float spanCells)
{
    if (!terrainLoaded || level < 0 || level > 2) return;
    if (terrainLevels[level].id == 0) return;

    float cellUnits = SECT_CORE_RADIUS * 2.0f;      // 100 units = 5 km
    float span = spanCells * cellUnits;
    Rectangle src = {0, 0, (float)terrainLevels[level].width,
                     (float)terrainLevels[level].height};
    Rectangle dst = {centre.x - span / 2.0f, centre.y - span / 2.0f,
                     span, span};
    DrawTexturePro(terrainLevels[level], src, dst, Vector2{0, 0}, 0.0f,
                   WHITE);
}

void RenderManager::DrawSectTerrainBackground(Sect* sect)
{
    if (!sect) return;

    Vector2 pos = sect->GetPosition();
    int gx = std::clamp((int)(pos.x / (SECT_CORE_RADIUS * 2.0f)), 0,
                        PLANET_SIZE - 1);
    int gy = std::clamp((int)(pos.y / (SECT_CORE_RADIUS * 2.0f)), 0,
                        PLANET_SIZE - 1);
    EnsureTerrainForCell(gx, gy);
    if (!terrainLoaded || terrainLevels[2].id == 0) return;

    // Sect view is screen-space: the 5 km cell fills the screen.
    float scale = std::max(screenWidth / (float)terrainLevels[2].width,
                           screenHeight / (float)terrainLevels[2].height);
    float drawW = terrainLevels[2].width * scale;
    float drawH = terrainLevels[2].height * scale;
    Rectangle src = {0, 0, (float)terrainLevels[2].width,
                     (float)terrainLevels[2].height};
    Rectangle dst = {(screenWidth - drawW) / 2.0f,
                     (screenHeight - drawH) / 2.0f, drawW, drawH};
    DrawTexturePro(terrainLevels[2], src, dst, Vector2{0, 0}, 0.0f, WHITE);
}

void RenderManager::DrawSectView(Sect* sect, TimeManager& timeManager) {
    DrawSectTerrainBackground(sect);
    if (sect) {
        sect->DrawInSectView(Vector2{screenWidth/2.0f, screenHeight/2.0f});
    }

    // Draw UI elements including time
    timeManager.Draw(screenWidth, screenHeight);
    DrawText("Sect View", 10, 10, 20, RAYWHITE);
    DrawText("Press U for Unit View", 10, 40, 20, LIGHTGRAY);
    DrawText("Press C for Colony View", 10, 70, 20, LIGHTGRAY);

    // Draw sect resource dashboard (left side)
    if (sect)
    {
        const auto& storage = sect->GetResourceStorage();
        const auto& capacity = sect->GetStorageCapacity();
        const auto& resources = GetResourceDescriptors();

        int dashX = 10;
        int dashY = 110;
        int dashW = 220;

        DrawRectangle(dashX, dashY, dashW, 20, Color{30, 30, 40, 220});
        DrawText("RESOURCES", dashX + 5, dashY + 3, 14, WHITE);
        dashY += 22;

        for (const auto& res : resources)
        {
            if (res.category != ResourceCategory::SINGULAR) continue;

            auto sIt = storage.find(res.type);
            auto cIt = capacity.find(res.type);
            float stored = (sIt != storage.end()) ? sIt->second : 0.0f;
            float cap = (cIt != capacity.end()) ? cIt->second : 0.0f;

            if (cap <= 0.0f && stored <= 0.0f) continue;

            float fill = cap > 0.0f ? stored / cap : 0.0f;
            Color barColor;
            if (fill > 0.9f) barColor = Color{255, 100, 100, 255};
            else if (fill > 0.7f) barColor = YELLOW;
            else barColor = Color{80, 180, 220, 255};

            DrawText(res.name, dashX + 5, dashY + 1, 11, LIGHTGRAY);
            float barX = (float)(dashX + 70);
            float barW = (float)(dashW - 70 - 5);
            DrawRectangle((int)barX, dashY, (int)barW, 14, Color{40, 40, 60, 200});
            DrawRectangle((int)barX, dashY, (int)(barW * fill), 14, barColor);
            DrawText(TextFormat("%.0f", stored), (int)barX + 3, dashY + 1, 11, WHITE);

            dashY += 16;
        }
    }

    // Draw storage upgrade panel (bottom-right)
    if (sect)
    {
        int panelW = 250;
        int panelH = 100;
        int panelX = screenWidth - panelW - 10;
        int panelY = screenHeight - panelH - 10;

        DrawRectangle(panelX, panelY, panelW, panelH, Color{30, 30, 40, 220});
        DrawRectangleLines(panelX, panelY, panelW, panelH, Color{80, 80, 100, 255});

        int level = sect->GetStorageLevel();
        float currentCap = SECT_BASE_STORAGE * STORAGE_LEVEL_MULTIPLIERS[level];
        DrawText(TextFormat("Storage Lv.%d (%.0f)", level, currentCap),
                 panelX + 10, panelY + 10, 16, WHITE);

        if (level < MAX_STORAGE_LEVEL)
        {
            int next = level + 1;
            float nextCap = SECT_BASE_STORAGE * STORAGE_LEVEL_MULTIPLIERS[next];
            DrawText(TextFormat("Next: %.0f  Cost: Fe %.0f Si %.0f E %.0f",
                     nextCap, SECT_UPGRADE_COST_FE[next],
                     SECT_UPGRADE_COST_SI[next], SECT_UPGRADE_COST_ENERGY[next]),
                     panelX + 10, panelY + 32, 12, LIGHTGRAY);

            // Upgrade button
            Rectangle btnRect = {(float)(panelX + 10), (float)(panelY + 55), 120.0f, 30.0f};
            bool canUpgrade = sect->CanUpgradeStorage();
            Color btnColor = canUpgrade ? Color{60, 140, 60, 255} : Color{80, 80, 80, 255};
            Color btnTextColor = canUpgrade ? WHITE : GRAY;

            DrawRectangleRec(btnRect, btnColor);
            DrawRectangleLinesEx(btnRect, 1.0f, canUpgrade ? GREEN : DARKGRAY);
            DrawText("UPGRADE", (int)btnRect.x + 20, (int)btnRect.y + 8, 14, btnTextColor);

            // Check click
            if (canUpgrade && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                Vector2 mouse = GetMousePosition();
                if (CheckCollisionPointRec(mouse, btnRect))
                {
                    sect->UpgradeStorage();
                }
            }
        }
        else
        {
            DrawText("MAX LEVEL", panelX + 10, panelY + 55, 16, Color{100, 200, 100, 255});
        }
    }
}

void RenderManager::DrawUnitView(Unit* unit, TimeManager& timeManager) {
    if (!unit) return;

    // Every unit type shares the dark-themed modular chrome. Modules without a
    // bespoke centre panel fall back to DrawGenericModulePanel.
    DrawModularUnitView(unit, timeManager);
}

void RenderManager::DrawCellInfo(Vector2 mousePosition, Camera2D camera, Planet* planet, std::vector<Colony*>& colonies) {
    // Convert screen coordinates to world coordinates
    Vector2 worldPos = GetScreenToWorld2D(mousePosition, camera);

    // Get grid coordinates
    int gridX = static_cast<int>(worldPos.x / (SECT_CORE_RADIUS * 2));
    int gridY = static_cast<int>(worldPos.y / (SECT_CORE_RADIUS * 2));

    // Check if position is within planet bounds
    if (gridX < 0 || gridX >= PLANET_SIZE || gridY < 0 || gridY >= PLANET_SIZE) {
        return;
    }

    // Get resource information
    Vector2 gridWorldPos = {
        static_cast<float>(gridX) * SECT_CORE_RADIUS * 2,
        static_cast<float>(gridY) * SECT_CORE_RADIUS * 2
    };
    auto resources = planet->GetResourceInfo(gridWorldPos);

    // Build info text
    std::vector<std::string> infoLines;

    // Add coordinates
    infoLines.push_back(TextFormat("Grid: %d, %d", gridX, gridY));

    // Add resources
    if (!resources.empty()) {
        infoLines.push_back("Resources:");
        for (const auto& [type, abundance] : resources) {
            std::string resourceName = ResourceUtils::GetResourceName(type);
            int kiloTonnes = static_cast<int>(abundance * 100);
            infoLines.push_back(TextFormat("  %s: %d kilo Tonnes", resourceName.c_str(), kiloTonnes));
        }
    } else {
        infoLines.push_back("No resources");
    }

    // Check jurisdiction
    std::string jurisdiction = "Unclaimed";
    for (const auto& colony : colonies) {
        Vector2 colonyCenter = colony->GetCentroid();
        float colonyRadius = colony->GetRadius();
        if (CheckCollisionPointCircle(gridWorldPos, colonyCenter, colonyRadius)) {
            jurisdiction = "Colony Territory";
            break;
        }
    }
    infoLines.push_back(TextFormat("Status: %s", jurisdiction.c_str()));

    // Calculate popup dimensions
    int padding = 10;
    int lineHeight = 20;
    int maxWidth = 0;
    for (const auto& line : infoLines) {
        int width = MeasureText(line.c_str(), lineHeight);
        maxWidth = std::max(maxWidth, width);
    }
    int boxWidth = maxWidth + (padding * 2);
    int boxHeight = (lineHeight * infoLines.size()) + (padding * 2);

    // Adjust popup position to stay within screen bounds
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    Vector2 popupPos = mousePosition;
    popupPos.x += 20; // Offset from cursor
    popupPos.y += 20;

    // Ensure popup stays within screen bounds
    if (popupPos.x + boxWidth > screenWidth) {
        popupPos.x = screenWidth - boxWidth;
    }
    if (popupPos.y + boxHeight > screenHeight) {
        popupPos.y = screenHeight - boxHeight;
    }

    // Draw popup background
    DrawRectangle(popupPos.x, popupPos.y, boxWidth, boxHeight, ColorAlpha(BLACK, 0.8f));
    DrawRectangleLines(popupPos.x, popupPos.y, boxWidth, boxHeight, WHITE);

    // Draw text
    for (size_t i = 0; i < infoLines.size(); i++) {
        DrawText(
            infoLines[i].c_str(),
            popupPos.x + padding,
            popupPos.y + padding + (i * lineHeight),
            lineHeight,
            WHITE
        );
    }
}

void RenderManager::DrawPlusIndicator(Vector2 mousePos, View currentView) {
    // Draw the + symbol
    const int crossSize = 20;  // Size of the plus symbol
    const int lineThickness = 2;  // Thickness of the lines
    Color plusColor = BLACK;

    // Draw vertical line of the +
    DrawRectangle(
        mousePos.x - lineThickness/2,
        mousePos.y - crossSize/2,
        lineThickness,
        crossSize,
        plusColor
    );

    // Draw horizontal line of the +
    DrawRectangle(
        mousePos.x - crossSize/2,
        mousePos.y - lineThickness/2,
        crossSize,
        lineThickness,
        plusColor
    );

    // Draw the text "add a new sect/colony" with transparency
    const int fontSize = 20;
    const int textPadding = 5;  // Padding between plus and text
    Color textColor = BLACK;
    textColor.a = 128;  // 50% transparency

    const char* text;

    if (currentView == View::Planet) {
        text = "DOUBLE-CLICK to add a new colony";
    } else if (currentView == View::Colony) {
        text = "DOUBLE-CLICK to add a new sect";
    } else {
        text = "";
    }

    // Calculate text dimensions
    int textWidth = MeasureText(text, fontSize);
    int textHeight = fontSize;

    // Position text above the plus symbol
    int textX = mousePos.x - textWidth/2;
    int textY = mousePos.y - crossSize/2 - textHeight - textPadding;

    DrawText(text, textX, textY, fontSize, textColor);
}

// Function to load the moon surface tiles
void RenderManager::LoadMoonTiles() {
    const char* tileFiles[3] = {
        "src/assets/moonsurface_tile1.png",
        "src/assets/moonsurface_tile2.png",
        "src/assets/moonsurface_tile3.png"
    };

    for (int i = 0; i < 3; i++) {
        moonTiles[i] = LoadTexture(tileFiles[i]);

        if (moonTiles[i].id == 0) {
            std::cout << "ERROR: Failed to load tile texture: " << tileFiles[i] << std::endl;
        } else {
            std::cout << "Loaded tile texture: " << tileFiles[i] << std::endl;
        }
    }
}

// Function to generate random tile pattern for the planet surface
void RenderManager::GenerateTilePattern() {
    // Calculate total number of tiles needed
    int tilesX = (PLANET_WIDTH / 100) + 2;  // Add extra for coverage
    int tilesY = (PLANET_HEIGHT / 100) + 2;
    int totalTiles = tilesX * tilesY;

    tilePattern.clear();
    tilePattern.reserve(totalTiles);

    // Use a fixed seed for consistent pattern
    SetRandomSeed(12345);

    // Generate random tile indices (0-2)
    for (int i = 0; i < totalTiles; i++) {
        tilePattern.push_back(GetRandomValue(0, 2));
    }
}

// Function to render the tiled moon surface
void RenderManager::RenderMoonSurface() {
    // Safety check before rendering
    if (!tilesLoaded || moonTiles[0].id == 0) {
        return;
    }

    // Get tile size (assuming all tiles are same size)
    int tileWidth = moonTiles[0].width;
    int tileHeight = moonTiles[0].height;

    // Calculate how many tiles we need
    int tilesX = (PLANET_WIDTH / tileWidth) + 2;
    int tilesY = (PLANET_HEIGHT / tileHeight) + 2;

    // Draw tiles across the planet surface
    int patternIndex = 0;
    for (int y = -1; y < tilesY; y++) {
        for (int x = -1; x < tilesX; x++) {
            // Get which tile to use from pattern
            int tileIndex = tilePattern[patternIndex % tilePattern.size()];
            patternIndex++;

            // Calculate position
            Vector2 position = {
                static_cast<float>(x * tileWidth),
                static_cast<float>(y * tileHeight)
            };

            // Draw the tile
            DrawTextureV(moonTiles[tileIndex], position, WHITE);
        }
    }
}

// Function to unload moon surface tiles
void RenderManager::UnloadMoonTiles() {
    for (int i = 0; i < 3; i++) {
        if (moonTiles[i].id != 0) {
            UnloadTexture(moonTiles[i]);
            moonTiles[i].id = 0;
        }
    }
    tilesLoaded = false;
}


// Transport visualization functions

void RenderManager::DrawDashedLine(Vector2 start, Vector2 end, float dashLength, float gapLength,
                                   float thickness, Color color) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float distance = sqrtf(dx*dx + dy*dy);

    if (distance < 0.001f) return;

    // Normalize direction
    float nx = dx / distance;
    float ny = dy / distance;

    float currentPos = 0.0f;
    bool drawing = true;

    while (currentPos < distance) {
        float segmentLength = drawing ? dashLength : gapLength;
        float endPos = std::min(currentPos + segmentLength, distance);

        if (drawing) {
            Vector2 segStart = {
                start.x + nx * currentPos,
                start.y + ny * currentPos
            };
            Vector2 segEnd = {
                start.x + nx * endPos,
                start.y + ny * endPos
            };
            DrawLineEx(segStart, segEnd, thickness, color);
        }

        currentPos = endPos;
        drawing = !drawing;
    }
}

void RenderManager::DrawRoads(Colony* colony, Road* selectedRoad) {
    if (!colony) return;

    const auto& roads = colony->GetRoads();

    for (const auto& road : roads) {
        if (!road.sectA || !road.sectB || !road.isConstructed) continue;

        Vector2 posA = road.sectA->GetPosition();
        Vector2 posB = road.sectB->GetPosition();

        // Check if this road is selected
        bool isSelected = (selectedRoad != nullptr &&
                          selectedRoad->sectA == road.sectA &&
                          selectedRoad->sectB == road.sectB);

        // Choose color based on transport mode
        Color roadColor;
        switch (road.mode) {
            case TransportMode::AUTO_BALANCE:
                roadColor = ColorAlpha(BLUE, 0.7f);
                break;
            case TransportMode::MANUAL:
                roadColor = ColorAlpha(YELLOW, 0.7f);
                break;
            case TransportMode::DEFICIT_TRIGGERED:
                roadColor = ColorAlpha(ORANGE, 0.7f);
                break;
            default:
                roadColor = ColorAlpha(GRAY, 0.7f);
        }

        // If selected, draw highlight first (thicker white line behind)
        if (isSelected) {
            DrawDashedLine(posA, posB, 15.0f, 8.0f, 8.0f, WHITE);  // Thicker white background
            DrawCircleV(posA, 10.0f, WHITE);
            DrawCircleV(posB, 10.0f, WHITE);
        }

        // Draw dashed road
        float thickness = isSelected ? 5.0f : 3.0f;  // Thicker if selected
        DrawDashedLine(posA, posB, 15.0f, 8.0f, thickness, roadColor);

        // Draw small indicators at road endpoints
        float endpointRadius = isSelected ? 8.0f : 5.0f;
        DrawCircleV(posA, endpointRadius, roadColor);
        DrawCircleV(posB, endpointRadius, roadColor);

        // Draw selection indicator text at midpoint
        if (isSelected) {
            Vector2 midpoint = {(posA.x + posB.x) / 2.0f, (posA.y + posB.y) / 2.0f - 20.0f};
            DrawText("SELECTED", midpoint.x - 30, midpoint.y, 12, WHITE);
        }
    }
}

void RenderManager::DrawTransportPackets(Colony* colony) {
    if (!colony) return;

    const auto& jobs = colony->GetTransportJobs();

    for (const auto& job : jobs) {
        if (job.status != TransportStatus::IN_TRANSIT) continue;

        Vector2 packetPos = job.GetCurrentPosition();

        // Get resource color for packet
        Color packetColor = ResourceUtils::GetResourceColor(job.resourceType);

        // Draw the packet as a colored circle with a ring
        float packetRadius = 8.0f;
        DrawCircleV(packetPos, packetRadius, packetColor);
        DrawCircleLinesV(packetPos, packetRadius + 2.0f, WHITE);

        // Draw small progress indicator
        float progressWidth = 20.0f;
        float progressHeight = 4.0f;
        Vector2 progressPos = {packetPos.x - progressWidth/2.0f, packetPos.y - packetRadius - 8.0f};
        DrawRectangle(progressPos.x, progressPos.y, progressWidth, progressHeight, DARKGRAY);
        DrawRectangle(progressPos.x, progressPos.y, progressWidth * job.progress, progressHeight, GREEN);
    }
}

void RenderManager::DrawRoadInfoPanel(Road* selectedRoad, Colony* colony) {
    if (!selectedRoad || !colony) return;

    // Panel dimensions and position (top-right corner)
    int panelWidth = 250;
    int panelHeight = 150;
    int panelX = screenWidth - panelWidth - 10;
    int panelY = 100;
    int padding = 10;
    int lineHeight = 20;

    // Draw panel background
    DrawRectangle(panelX, panelY, panelWidth, panelHeight, ColorAlpha(BLACK, 0.8f));
    DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, WHITE);

    // Panel title
    DrawText("SELECTED ROAD", panelX + padding, panelY + padding, 16, WHITE);

    int y = panelY + padding + lineHeight + 5;

    // Transport mode
    const char* modeStr;
    Color modeColor;
    switch (selectedRoad->mode) {
        case TransportMode::AUTO_BALANCE:
            modeStr = "AUTO_BALANCE";
            modeColor = BLUE;
            break;
        case TransportMode::MANUAL:
            modeStr = "MANUAL";
            modeColor = YELLOW;
            break;
        case TransportMode::DEFICIT_TRIGGERED:
            modeStr = "DEFICIT_TRIGGERED";
            modeColor = ORANGE;
            break;
        default:
            modeStr = "UNKNOWN";
            modeColor = GRAY;
    }
    DrawText(TextFormat("Mode: %s", modeStr), panelX + padding, y, 14, modeColor);
    y += lineHeight;

    // Road length and travel time
    DrawText(TextFormat("Length: %.1f units", selectedRoad->length), panelX + padding, y, 14, LIGHTGRAY);
    y += lineHeight;

    DrawText(TextFormat("Travel Time: %.1f sec", selectedRoad->travelTime), panelX + padding, y, 14, LIGHTGRAY);
    y += lineHeight;

    // Count active jobs on this road
    int activeJobs = 0;
    const auto& jobs = colony->GetTransportJobs();
    for (const auto& job : jobs) {
        if (job.road == selectedRoad && job.status == TransportStatus::IN_TRANSIT) {
            activeJobs++;
        }
    }
    DrawText(TextFormat("Active Jobs: %d", activeJobs), panelX + padding, y, 14, GREEN);
    y += lineHeight;

    // Controls hint
    DrawText("Press T to cycle mode", panelX + padding, y, 12, DARKGRAY);
}

// ============================================================================
// SITE SELECTION VIEW
// ============================================================================

void RenderManager::DrawSiteSelectionView(Camera2D camera, Planet* planet, Vector2 hoveredGridPos,
                                          TimeManager& timeManager) {
    if (!planet) return;

    ResourceManager& rm = planet->GetResourceManager();
    float cellSize = SECT_CORE_RADIUS * 2.0f;

    // --- Draw world-space elements (orbital map with colored grid) ---
    BeginMode2D(camera);

    // Draw tiled moon surface background
    if (tilesLoaded)
    {
        RenderMoonSurface();
    }

    // Draw colored overlay for each grid cell
    for (int y = 0; y < PLANET_SIZE; y++)
    {
        for (int x = 0; x < PLANET_SIZE; x++)
        {
            auto survey = rm.GetOrbitalSurveyAt(x, y);

            // Color based on composition: dark = mare (Fe/Ti), light = highland (Si/Al)
            float mareIntensity = (survey.fePercent + survey.tiPercent) * 0.5f;
            float highlandIntensity = (survey.siPercent + survey.alPercent) * 0.5f;
            float hydrogenTint = survey.hydrogenSignal;

            // Blend: dark grey for mare, light grey for highland, blue tint for hydrogen
            unsigned char r = static_cast<unsigned char>(60.0f + highlandIntensity * 140.0f);
            unsigned char g = static_cast<unsigned char>(50.0f + highlandIntensity * 130.0f);
            unsigned char b = static_cast<unsigned char>(60.0f + highlandIntensity * 120.0f + hydrogenTint * 80.0f);

            // Darken for mare regions
            r = static_cast<unsigned char>(r * (1.0f - mareIntensity * 0.5f));
            g = static_cast<unsigned char>(g * (1.0f - mareIntensity * 0.5f));

            Color cellColor = {r, g, b, 140};

            float worldX = x * cellSize;
            float worldY = y * cellSize;
            DrawRectangle(static_cast<int>(worldX), static_cast<int>(worldY),
                         static_cast<int>(cellSize), static_cast<int>(cellSize), cellColor);

            // Draw thin grid lines
            DrawRectangleLines(static_cast<int>(worldX), static_cast<int>(worldY),
                              static_cast<int>(cellSize), static_cast<int>(cellSize),
                              {100, 100, 100, 80});
        }
    }

    // Highlight hovered cell
    int hx = static_cast<int>(hoveredGridPos.x);
    int hy = static_cast<int>(hoveredGridPos.y);
    if (hx >= 0 && hx < PLANET_SIZE && hy >= 0 && hy < PLANET_SIZE)
    {
        float worldX = hx * cellSize;
        float worldY = hy * cellSize;
        DrawRectangleLines(static_cast<int>(worldX), static_cast<int>(worldY),
                          static_cast<int>(cellSize), static_cast<int>(cellSize),
                          {255, 255, 0, 255});
        DrawRectangleLinesEx(
            {worldX + 1, worldY + 1, cellSize - 2, cellSize - 2},
            2.0f, {255, 255, 0, 200});
    }

    EndMode2D();

    // --- Draw screen-space UI panels ---

    // Choose fonts (fall back to default if loading failed)
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    float sp = 1.0f;  // Letter spacing

    // Title bar
    DrawRectangle(0, 0, screenWidth, 40, {20, 20, 40, 230});
    DrawTextEx(headerFont, "COLONY SITE SELECTION", {20.0f, 8.0f}, 22.0f, sp, WHITE);
    DrawTextEx(bodyFont, "[ENTER] Confirm  [ESC] Cancel",
               {(float)(screenWidth - 320), 12.0f}, 16.0f, sp, LIGHTGRAY);

    // Get survey data for hovered cell
    if (hx >= 0 && hx < PLANET_SIZE && hy >= 0 && hy < PLANET_SIZE)
    {
        auto survey = rm.GetOrbitalSurveyAt(hx, hy);
        SiteArchetype archetype = rm.GetSiteArchetype(hx, hy);

        const char* archetypeNames[] = {
            "MARE INDUSTRIAL", "HIGHLAND CONSTRUCTION", "POLAR VOLATILE",
            "KREEP SCIENTIFIC", "LAVA TUBE", "MIXED"
        };

        int panelX = screenWidth - 340;
        int panelY = 50;
        int panelW = 330;
        int panelH = 530;

        // Panel background
        DrawRectangle(panelX, panelY, panelW, panelH, {20, 20, 40, 220});
        DrawRectangleLines(panelX, panelY, panelW, panelH, {100, 100, 200, 200});

        int padding = 10;
        float yPos = static_cast<float>(panelY + padding);
        float lineH = 20.0f;
        float px = static_cast<float>(panelX + padding);

        // Cell coordinates
        DrawTextEx(headerFont, TextFormat("Grid Position: (%d, %d)", hx, hy),
                   {px, yPos}, 16.0f, sp, WHITE);
        yPos += lineH + 5.0f;

        // --- Gamma-Ray Spectrometer ---
        DrawTextEx(headerFont, "GAMMA-RAY SPECTROMETER", {px, yPos}, 14.0f, sp, {150, 150, 255, 255});
        yPos += lineH;

        // Bar charts for elemental composition
        auto DrawBar = [&](const char* label, float value, Color color) {
            DrawTextEx(bodyFont, TextFormat("%-4s", label), {px, yPos}, 13.0f, sp, LIGHTGRAY);
            float barX = px + 40.0f;
            int barW = 180;
            int barH = 13;
            DrawRectangle(static_cast<int>(barX), static_cast<int>(yPos + 1.0f), barW, barH, {40, 40, 40, 200});
            DrawRectangle(static_cast<int>(barX), static_cast<int>(yPos + 1.0f), static_cast<int>(barW * value), barH, color);
            DrawTextEx(bodyFont, TextFormat("%.0f%%", value * 100.0f),
                       {barX + barW + 5.0f, yPos}, 13.0f, sp, LIGHTGRAY);
            yPos += lineH;
        };

        DrawBar("Fe", survey.fePercent, {139, 69, 19, 255});
        DrawBar("Ti", survey.tiPercent, {180, 160, 200, 255});
        DrawBar("Si", survey.siPercent, {144, 180, 148, 255});
        DrawBar("Al", survey.alPercent, {200, 200, 220, 255});
        DrawBar("Ca", survey.caPercent, {220, 210, 190, 255});

        DrawTextEx(bodyFont, TextFormat("Th: %.1f ppm", survey.thPpm), {px, yPos}, 13.0f, sp, LIGHTGRAY);
        yPos += lineH;
        DrawTextEx(bodyFont, TextFormat("K:  %.0f ppm", survey.kPpm), {px, yPos}, 13.0f, sp, LIGHTGRAY);
        yPos += lineH + 8.0f;

        // --- Neutron Spectrometer ---
        DrawTextEx(headerFont, "NEUTRON SPECTROMETER", {px, yPos}, 14.0f, sp, {100, 200, 255, 255});
        yPos += lineH;

        DrawBar("H", survey.hydrogenSignal, {100, 200, 255, 255});

        const char* iceLikelihood = survey.hydrogenSignal > 0.6f ? "HIGH" :
                                    survey.hydrogenSignal > 0.3f ? "MODERATE" : "LOW";
        Color iceColor = survey.hydrogenSignal > 0.6f ? GREEN :
                         survey.hydrogenSignal > 0.3f ? YELLOW : RED;
        DrawTextEx(bodyFont, TextFormat("Ice likelihood: %s", iceLikelihood), {px, yPos}, 13.0f, sp, iceColor);
        yPos += lineH + 8.0f;

        // --- Thermal Mapper ---
        DrawTextEx(headerFont, "THERMAL MAPPER", {px, yPos}, 14.0f, sp, {255, 200, 100, 255});
        yPos += lineH;

        DrawBar("Solar", survey.solarIllumination, {255, 255, 100, 255});

        float dayTemp = -173.0f + survey.solarIllumination * 300.0f;
        float nightTemp = -173.0f + survey.solarIllumination * 20.0f;
        DrawTextEx(bodyFont, TextFormat("Day: %+.0f C  Night: %+.0f C", dayTemp, nightTemp),
                   {px, yPos}, 13.0f, sp, LIGHTGRAY);
        yPos += lineH + 8.0f;

        // --- Site Assessment ---
        DrawTextEx(headerFont, "SITE ASSESSMENT", {px, yPos}, 14.0f, sp, {200, 255, 200, 255});
        yPos += lineH;

        // Terrain slope
        const char* terrainDesc = survey.terrainSlope < 5.0f ? "Flat" :
                                  survey.terrainSlope < 15.0f ? "Moderate" : "Steep";
        Color terrainColor = survey.terrainSlope < 5.0f ? GREEN :
                             survey.terrainSlope < 15.0f ? YELLOW : RED;
        DrawTextEx(bodyFont, TextFormat("Terrain: %.0f deg (%s)", survey.terrainSlope, terrainDesc),
                   {px, yPos}, 13.0f, sp, terrainColor);
        yPos += lineH;

        // Solar access
        const char* solarDesc = survey.solarIllumination > 0.7f ? "Excellent" :
                                survey.solarIllumination > 0.4f ? "Good" : "Poor";
        Color solarColor = survey.solarIllumination > 0.7f ? GREEN :
                           survey.solarIllumination > 0.4f ? YELLOW : RED;
        DrawTextEx(bodyFont, TextFormat("Solar Access: %.0f%% (%s)", survey.solarIllumination * 100.0f, solarDesc),
                   {px, yPos}, 13.0f, sp, solarColor);
        yPos += lineH;

        // Earth visibility
        const char* earthDesc = survey.earthVisibility > 0.7f ? "Reliable" :
                                survey.earthVisibility > 0.4f ? "Intermittent" : "Poor";
        Color earthColor = survey.earthVisibility > 0.7f ? GREEN :
                           survey.earthVisibility > 0.4f ? YELLOW : RED;
        DrawTextEx(bodyFont, TextFormat("Earth Comms: %.0f%% (%s)", survey.earthVisibility * 100.0f, earthDesc),
                   {px, yPos}, 13.0f, sp, earthColor);
        yPos += lineH + 8.0f;

        // Archetype recommendation
        DrawRectangle(static_cast<int>(px) - 2, static_cast<int>(yPos) - 2,
                      panelW - padding * 2 + 4, static_cast<int>(lineH) + 6, {40, 40, 80, 200});
        DrawTextEx(headerFont, TextFormat("ARCHETYPE: %s", archetypeNames[static_cast<int>(archetype)]),
                   {px, yPos}, 16.0f, sp, {255, 220, 100, 255});
        yPos += lineH + 4.0f;

        // Archetype bonus description
        const char* bonusDesc[] = {
            "+20% Fe/Ti extraction",
            "+20% Si/Al extraction",
            "+50% water extraction",
            "+30% Science generation",
            "+15% all production",
            "No special bonus"
        };
        DrawTextEx(bodyFont, TextFormat("Bonus: %s", bonusDesc[static_cast<int>(archetype)]),
                   {px, yPos}, 13.0f, sp, {180, 180, 255, 255});
    }

    // Bottom bar
    int bottomY = screenHeight - 40;
    DrawRectangle(0, bottomY, screenWidth, 40, {20, 20, 40, 230});
    DrawTextEx(bodyFont, "Ctrl+Click to enter  |  Mouse: hover cells  |  Enter: confirm  |  Esc: cancel",
               {20.0f, static_cast<float>(bottomY + 10)}, 14.0f, sp, LIGHTGRAY);

    // Time display
    DrawTextEx(bodyFont, TextFormat("Day %d", timeManager.GetCurrentDay()),
               {static_cast<float>(screenWidth - 100), static_cast<float>(bottomY + 10)}, 14.0f, sp, WHITE);
}

// ============================================================================
// EXTRACTION UNIT UI
// ============================================================================

// Design tokens for the extraction UI (dark sci-fi kit)
static const Color EXT_SCREEN_BG     = {5, 7, 14, 255};       // near-black backdrop
static const Color EXT_PANEL_BG      = {10, 15, 28, 255};     // card fill
static const Color EXT_PANEL_BG2     = {14, 21, 38, 255};     // raised fill (buttons, chips)
static const Color EXT_PANEL_BORDER  = {36, 62, 92, 255};     // dim slate-cyan hairline
static const Color EXT_FRAME_ACCENT  = {70, 190, 230, 170};   // corner bracket accents
static const Color EXT_HEADER_COLOR  = {168, 130, 255, 255};  // purple section headers
static const Color EXT_ACCENT_CYAN   = {80, 225, 255, 255};
static const Color EXT_ACCENT_GREEN  = {80, 230, 150, 255};
static const Color EXT_ACCENT_GOLD   = {255, 200, 80, 255};
static const Color EXT_ACCENT_RED    = {235, 70, 70, 255};
static const Color EXT_ACCENT_VIOLET = {170, 110, 255, 255};
static const Color EXT_DIM_TEXT      = {120, 138, 165, 255};
static const Color EXT_TEXT          = {225, 235, 245, 255};

// Layout constants
static const int EXT_TOP_BAR_H    = 56;
static const int EXT_BOTTOM_BAR_H = 118;   // status segments bar + message bar
static const int EXT_LEFT_PANEL_W  = 280;
static const int EXT_RIGHT_PANEL_W = 300;
static const int EXT_GAP           = 8;    // margin around floating cards

float RenderManager::FS(float baseSize)
{
    return baseSize * 1.30f;
}

// --- Procedural UI-kit widgets -------------------------------------------
// The visual design uses simple line icons and framed cards; everything is
// drawn from primitives so it stays resolution-independent and theme-driven.

// Sci-fi card: rounded fill, hairline border, bracket accents on the corners.
static void ExtDrawPanelFrame(Rectangle r)
{
    DrawRectangleRounded(r, 0.04f, 4, EXT_PANEL_BG);
    DrawRectangleRoundedLinesEx(r, 0.04f, 4, 1.0f, EXT_PANEL_BORDER);

    const float len = 14.0f;
    const float t = 2.0f;
    Color c = EXT_FRAME_ACCENT;
    DrawRectangleRec({r.x, r.y, len, t}, c);
    DrawRectangleRec({r.x, r.y, t, len}, c);
    DrawRectangleRec({r.x + r.width - len, r.y, len, t}, c);
    DrawRectangleRec({r.x + r.width - t, r.y, t, len}, c);
    DrawRectangleRec({r.x, r.y + r.height - t, len, t}, c);
    DrawRectangleRec({r.x, r.y + r.height - len, t, len}, c);
    DrawRectangleRec({r.x + r.width - len, r.y + r.height - t, len, t}, c);
    DrawRectangleRec({r.x + r.width - t, r.y + r.height - len, t, len}, c);
}

enum class ExtIcon
{
    RADAR, EXCAVATOR, NODES, GEAR, CROSSHAIR,
    FLASK, SLIDERS, BOLT, WARNING, OVERVIEW, HAMBURGER,
    // Farming
    DROPLET, GREENHOUSE, TRAYS, SHEAF, CRATE,
    // Energy
    SOLAR_PANEL, BATTERY, ATOM, PYLON, BEACON,
    // Manufacture
    FURNACE, ROBOT_ARM, MAGNIFIER, PALLET, CHIP,
    // Research
    CHART, ORB, SERVER_RACK, BROADCAST,
    // Construction
    STAKES, REBAR, GIRDER, FITOUT, WRENCH,
    // Transport
    HAULER, ROUTE_NODES, DEPOT, LIFT, CLIPBOARD,
    // Core (centre dome)
    LIFE_LOOP, CREW, COMMAND, WAVEFORM, HAZARD,
    // Communication
    DISH, RELAY_TOWER, PADLOCK, MESH
};

// Line icon set matching the UI kit. (cx, cy) is the center, s the half-size.
static void ExtDrawIcon(ExtIcon icon, float cx, float cy, float s, Color c)
{
    switch (icon)
    {
        case ExtIcon::RADAR:
        {
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), s, c);
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), s * 0.55f, Fade(c, 0.7f));
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), s * 0.18f, c);
            break;
        }
        case ExtIcon::EXCAVATOR:
        {
            // tracks + cab + articulated boom + bucket claw
            DrawRectangleLinesEx({cx - s, cy + s * 0.05f, s * 0.85f, s * 0.6f}, 1.5f, c);
            DrawLineEx({cx - s * 1.05f, cy + s * 0.85f}, {cx + s * 0.15f, cy + s * 0.85f}, 2.5f, c);
            DrawLineEx({cx - s * 0.15f, cy + s * 0.05f}, {cx + s * 0.35f, cy - s * 0.75f}, 1.6f, c);
            DrawLineEx({cx + s * 0.35f, cy - s * 0.75f}, {cx + s * 0.95f, cy - s * 0.05f}, 1.6f, c);
            DrawLineEx({cx + s * 0.95f, cy - s * 0.05f}, {cx + s * 0.65f, cy + s * 0.35f}, 1.6f, c);
            DrawLineEx({cx + s * 0.65f, cy + s * 0.35f}, {cx + s * 0.95f, cy + s * 0.45f}, 1.6f, c);
            break;
        }
        case ExtIcon::NODES:
        {
            Vector2 a = {cx, cy - s * 0.65f};
            Vector2 b = {cx - s * 0.7f, cy + s * 0.55f};
            Vector2 d = {cx + s * 0.7f, cy + s * 0.55f};
            DrawLineEx(a, b, 1.2f, Fade(c, 0.7f));
            DrawLineEx(b, d, 1.2f, Fade(c, 0.7f));
            DrawLineEx(d, a, 1.2f, Fade(c, 0.7f));
            DrawCircleLines(static_cast<int>(a.x), static_cast<int>(a.y), s * 0.3f, c);
            DrawCircleLines(static_cast<int>(b.x), static_cast<int>(b.y), s * 0.3f, c);
            DrawCircleLines(static_cast<int>(d.x), static_cast<int>(d.y), s * 0.3f, c);
            break;
        }
        case ExtIcon::GEAR:
        {
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), s * 0.6f, c);
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), s * 0.22f, c);
            for (int i = 0; i < 8; i++)
            {
                float ang = i * PI / 4.0f;
                Vector2 in = {cx + cosf(ang) * s * 0.6f, cy + sinf(ang) * s * 0.6f};
                Vector2 out = {cx + cosf(ang) * s, cy + sinf(ang) * s};
                DrawLineEx(in, out, 2.2f, c);
            }
            break;
        }
        case ExtIcon::CROSSHAIR:
        {
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), s * 0.62f, c);
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), s * 0.14f, c);
            DrawLineEx({cx, cy - s}, {cx, cy - s * 0.45f}, 1.5f, c);
            DrawLineEx({cx, cy + s * 0.45f}, {cx, cy + s}, 1.5f, c);
            DrawLineEx({cx - s, cy}, {cx - s * 0.45f, cy}, 1.5f, c);
            DrawLineEx({cx + s * 0.45f, cy}, {cx + s, cy}, 1.5f, c);
            break;
        }
        case ExtIcon::FLASK:
        {
            DrawLineEx({cx - s * 0.25f, cy - s}, {cx - s * 0.25f, cy - s * 0.15f}, 1.5f, c);
            DrawLineEx({cx + s * 0.25f, cy - s}, {cx + s * 0.25f, cy - s * 0.15f}, 1.5f, c);
            DrawLineEx({cx - s * 0.25f, cy - s * 0.15f}, {cx - s * 0.7f, cy + s * 0.8f}, 1.5f, c);
            DrawLineEx({cx + s * 0.25f, cy - s * 0.15f}, {cx + s * 0.7f, cy + s * 0.8f}, 1.5f, c);
            DrawLineEx({cx - s * 0.7f, cy + s * 0.8f}, {cx + s * 0.7f, cy + s * 0.8f}, 1.5f, c);
            DrawLineEx({cx - s * 0.45f, cy - s}, {cx + s * 0.45f, cy - s}, 1.5f, c);
            DrawLineEx({cx - s * 0.42f, cy + s * 0.35f}, {cx + s * 0.42f, cy + s * 0.35f}, 1.5f, Fade(c, 0.6f));
            break;
        }
        case ExtIcon::SLIDERS:
        {
            float rows[3] = {cy - s * 0.6f, cy, cy + s * 0.6f};
            float knobs[3] = {cx - s * 0.35f, cx + s * 0.4f, cx - s * 0.05f};
            for (int i = 0; i < 3; i++)
            {
                DrawLineEx({cx - s, rows[i]}, {cx + s, rows[i]}, 1.5f, Fade(c, 0.7f));
                DrawCircle(static_cast<int>(knobs[i]), static_cast<int>(rows[i]), s * 0.18f, c);
            }
            break;
        }
        case ExtIcon::BOLT:
        {
            DrawLineEx({cx + s * 0.45f, cy - s}, {cx - s * 0.25f, cy - s * 0.05f}, 2.4f, c);
            DrawLineEx({cx - s * 0.25f, cy - s * 0.05f}, {cx + s * 0.25f, cy + s * 0.05f}, 2.4f, c);
            DrawLineEx({cx + s * 0.25f, cy + s * 0.05f}, {cx - s * 0.45f, cy + s}, 2.4f, c);
            break;
        }
        case ExtIcon::WARNING:
        {
            Vector2 top = {cx, cy - s};
            Vector2 bl = {cx - s * 0.95f, cy + s * 0.75f};
            Vector2 br = {cx + s * 0.95f, cy + s * 0.75f};
            DrawLineEx(top, bl, 2.0f, c);
            DrawLineEx(bl, br, 2.0f, c);
            DrawLineEx(br, top, 2.0f, c);
            DrawLineEx({cx, cy - s * 0.4f}, {cx, cy + s * 0.2f}, 2.0f, c);
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy + s * 0.48f), 1.5f, c);
            break;
        }
        case ExtIcon::OVERVIEW:
        {
            DrawRectangleLinesEx({cx - s, cy - s * 0.75f, s * 2.0f, s * 1.5f}, 1.5f, c);
            DrawLineEx({cx - s * 0.6f, cy - s * 0.25f}, {cx + s * 0.6f, cy - s * 0.25f}, 1.5f, Fade(c, 0.7f));
            DrawLineEx({cx - s * 0.6f, cy + s * 0.2f}, {cx + s * 0.15f, cy + s * 0.2f}, 1.5f, Fade(c, 0.5f));
            break;
        }
        case ExtIcon::HAMBURGER:
        {
            for (int i = -1; i <= 1; i++)
            {
                float ly = cy + i * s * 0.6f;
                DrawLineEx({cx - s, ly}, {cx + s, ly}, 2.0f, c);
            }
            break;
        }

        // --- Farming ---
        case ExtIcon::DROPLET:
        {
            // Teardrop: two shoulders curving up to a point
            Vector2 tip = {cx, cy - s};
            for (int side = -1; side <= 1; side += 2)
            {
                Vector2 prev = tip;
                for (int i = 1; i <= 8; i++)
                {
                    float t = i / 8.0f;
                    Vector2 next = {cx + side * sinf(t * PI * 0.86f) * s * 0.78f,
                                    cy - s + t * s * 1.75f};
                    DrawLineEx(prev, next, 1.6f, c);
                    prev = next;
                }
            }
            DrawLineEx({cx - s * 0.34f, cy + s * 0.72f}, {cx + s * 0.34f, cy + s * 0.72f}, 1.6f, c);
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy + s * 0.24f), s * 0.24f, Fade(c, 0.55f));
            break;
        }
        case ExtIcon::GREENHOUSE:
        {
            // Arched hoop house with a sprout inside
            Vector2 bl = {cx - s, cy + s * 0.8f};
            Vector2 br = {cx + s, cy + s * 0.8f};
            DrawLineEx(bl, br, 1.8f, c);
            DrawLineEx(bl, {bl.x, cy - s * 0.1f}, 1.6f, c);
            DrawLineEx(br, {br.x, cy - s * 0.1f}, 1.6f, c);
            Vector2 prev = {cx - s, cy - s * 0.1f};
            for (int i = 1; i <= 10; i++)
            {
                float t = i / 10.0f;
                Vector2 next = {cx - s + t * s * 2.0f, cy - s * 0.1f - sinf(t * PI) * s * 0.82f};
                DrawLineEx(prev, next, 1.6f, c);
                prev = next;
            }
            DrawLineEx({cx, cy + s * 0.75f}, {cx, cy + s * 0.05f}, 1.5f, Fade(c, 0.8f));
            DrawLineEx({cx, cy + s * 0.35f}, {cx - s * 0.36f, cy + s * 0.02f}, 1.4f, Fade(c, 0.7f));
            DrawLineEx({cx, cy + s * 0.35f}, {cx + s * 0.36f, cy + s * 0.02f}, 1.4f, Fade(c, 0.7f));
            break;
        }
        case ExtIcon::TRAYS:
        {
            // Stacked grow trays fed by a riser pipe
            for (int i = 0; i < 3; i++)
            {
                float ty = cy - s * 0.62f + i * s * 0.72f;
                DrawRectangleLinesEx({cx - s * 0.85f, ty, s * 1.7f, s * 0.34f}, 1.4f,
                                     Fade(c, 1.0f - i * 0.16f));
                for (int k = 0; k < 3; k++)
                {
                    float sx = cx - s * 0.5f + k * s * 0.5f;
                    DrawLineEx({sx, ty}, {sx, ty - s * 0.22f}, 1.3f, Fade(c, 0.7f));
                }
            }
            DrawLineEx({cx + s * 0.95f, cy - s * 0.7f}, {cx + s * 0.95f, cy + s * 0.85f}, 1.6f, Fade(c, 0.8f));
            break;
        }
        case ExtIcon::SHEAF:
        {
            // Bundled grain stalks tied at the waist
            for (int i = -2; i <= 2; i++)
            {
                float lean = i * 0.17f;
                Vector2 top = {cx + lean * s * 1.5f, cy - s};
                Vector2 mid = {cx + lean * s * 0.4f, cy + s * 0.15f};
                DrawLineEx(top, mid, 1.5f, Fade(c, 0.85f));
                DrawLineEx(mid, {cx + lean * s * 0.9f, cy + s * 0.9f}, 1.5f, Fade(c, 0.85f));
                DrawCircle(static_cast<int>(top.x), static_cast<int>(top.y), s * 0.13f, c);
            }
            DrawLineEx({cx - s * 0.5f, cy + s * 0.2f}, {cx + s * 0.5f, cy + s * 0.2f}, 1.8f, c);
            break;
        }
        case ExtIcon::CRATE:
        {
            // Sealed shipping crate with corner banding
            Rectangle box = {cx - s * 0.88f, cy - s * 0.7f, s * 1.76f, s * 1.5f};
            DrawRectangleLinesEx(box, 1.7f, c);
            DrawLineEx({box.x, box.y + box.height * 0.32f},
                       {box.x + box.width, box.y + box.height * 0.32f}, 1.4f, Fade(c, 0.6f));
            DrawLineEx({cx, box.y}, {cx, box.y + box.height}, 1.4f, Fade(c, 0.45f));
            DrawLineEx({box.x, box.y}, {box.x + s * 0.4f, box.y}, 2.4f, c);
            DrawLineEx({box.x + box.width - s * 0.4f, box.y + box.height},
                       {box.x + box.width, box.y + box.height}, 2.4f, c);
            break;
        }

        // --- Energy ---
        case ExtIcon::SOLAR_PANEL:
        {
            // Tilted PV array on a mast, sun overhead
            Vector2 tl = {cx - s * 0.95f, cy - s * 0.15f};
            Vector2 tr = {cx + s * 0.7f, cy - s * 0.6f};
            Vector2 br2 = {cx + s * 0.95f, cy + s * 0.2f};
            Vector2 bl2 = {cx - s * 0.7f, cy + s * 0.62f};
            DrawLineEx(tl, tr, 1.6f, c);
            DrawLineEx(tr, br2, 1.6f, c);
            DrawLineEx(br2, bl2, 1.6f, c);
            DrawLineEx(bl2, tl, 1.6f, c);
            for (int i = 1; i <= 2; i++)
            {
                float t = i / 3.0f;
                DrawLineEx({tl.x + (tr.x - tl.x) * t, tl.y + (tr.y - tl.y) * t},
                           {bl2.x + (br2.x - bl2.x) * t, bl2.y + (br2.y - bl2.y) * t},
                           1.2f, Fade(c, 0.5f));
            }
            DrawLineEx({cx + s * 0.1f, cy + s * 0.4f}, {cx + s * 0.1f, cy + s}, 1.5f, Fade(c, 0.8f));
            DrawCircleLines(static_cast<int>(cx - s * 0.55f), static_cast<int>(cy - s * 0.78f),
                            s * 0.22f, Fade(c, 0.85f));
            break;
        }
        case ExtIcon::BATTERY:
        {
            // Cell body, terminal nub, three charge bars
            Rectangle body = {cx - s * 0.9f, cy - s * 0.55f, s * 1.65f, s * 1.1f};
            DrawRectangleLinesEx(body, 1.7f, c);
            DrawRectangleRec({body.x + body.width, cy - s * 0.22f, s * 0.16f, s * 0.44f}, c);
            for (int i = 0; i < 3; i++)
            {
                DrawRectangleRec({body.x + s * 0.16f + i * s * 0.48f, body.y + s * 0.2f,
                                  s * 0.3f, body.height - s * 0.4f},
                                 Fade(c, 0.9f - i * 0.25f));
            }
            break;
        }
        case ExtIcon::ATOM:
        {
            // Nucleus with two crossed electron shells
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), s * 0.2f, c);
            for (int orbit = 0; orbit < 2; orbit++)
            {
                float rot = orbit * PI / 3.0f + PI / 6.0f;
                Vector2 prev = {0};
                for (int i = 0; i <= 20; i++)
                {
                    float a = i / 20.0f * 2.0f * PI;
                    float ex = cosf(a) * s * 0.98f;
                    float ey = sinf(a) * s * 0.42f;
                    Vector2 p = {cx + ex * cosf(rot) - ey * sinf(rot),
                                 cy + ex * sinf(rot) + ey * cosf(rot)};
                    if (i > 0) DrawLineEx(prev, p, 1.3f, Fade(c, 0.8f));
                    prev = p;
                }
            }
            break;
        }
        case ExtIcon::PYLON:
        {
            // Transmission tower with a sagging cable
            DrawLineEx({cx - s * 0.6f, cy + s * 0.9f}, {cx - s * 0.12f, cy - s * 0.9f}, 1.6f, c);
            DrawLineEx({cx + s * 0.6f, cy + s * 0.9f}, {cx + s * 0.12f, cy - s * 0.9f}, 1.6f, c);
            DrawLineEx({cx - s * 0.78f, cy - s * 0.3f}, {cx + s * 0.78f, cy - s * 0.3f}, 1.5f, c);
            DrawLineEx({cx - s * 0.42f, cy + s * 0.28f}, {cx + s * 0.42f, cy + s * 0.28f}, 1.4f, Fade(c, 0.7f));
            DrawLineEx({cx - s * 0.36f, cy - s * 0.3f}, {cx + s * 0.36f, cy + s * 0.28f}, 1.1f, Fade(c, 0.4f));
            DrawLineEx({cx + s * 0.36f, cy - s * 0.3f}, {cx - s * 0.36f, cy + s * 0.28f}, 1.1f, Fade(c, 0.4f));
            Vector2 prev = {cx - s, cy - s * 0.44f};
            for (int i = 1; i <= 6; i++)
            {
                float t = i / 6.0f;
                Vector2 next = {cx - s + t * s * 2.0f, cy - s * 0.44f + sinf(t * PI) * s * 0.3f};
                DrawLineEx(prev, next, 1.2f, Fade(c, 0.6f));
                prev = next;
            }
            break;
        }
        case ExtIcon::BEACON:
        {
            // Alarm dome on a base, radiating arcs
            DrawLineEx({cx - s * 0.7f, cy + s * 0.85f}, {cx + s * 0.7f, cy + s * 0.85f}, 1.8f, c);
            DrawLineEx({cx - s * 0.45f, cy + s * 0.85f}, {cx - s * 0.45f, cy + s * 0.35f}, 1.5f, c);
            DrawLineEx({cx + s * 0.45f, cy + s * 0.85f}, {cx + s * 0.45f, cy + s * 0.35f}, 1.5f, c);
            Vector2 prev = {cx - s * 0.45f, cy + s * 0.35f};
            for (int i = 1; i <= 10; i++)
            {
                float t = i / 10.0f;
                Vector2 next = {cx - s * 0.45f + t * s * 0.9f, cy + s * 0.35f - sinf(t * PI) * s * 0.5f};
                DrawLineEx(prev, next, 1.6f, c);
                prev = next;
            }
            for (int side = -1; side <= 1; side += 2)
            {
                DrawLineEx({cx + side * s * 0.68f, cy - s * 0.15f},
                           {cx + side * s * 0.98f, cy - s * 0.42f}, 1.4f, Fade(c, 0.75f));
                DrawLineEx({cx + side * s * 0.6f, cy + s * 0.15f},
                           {cx + side * s * 1.0f, cy + s * 0.12f}, 1.4f, Fade(c, 0.5f));
            }
            break;
        }

        // --- Manufacture ---
        case ExtIcon::FURNACE:
        {
            // Melting furnace: shell, glowing port, exhaust stack
            Rectangle shell = {cx - s * 0.85f, cy - s * 0.5f, s * 1.7f, s * 1.35f};
            DrawRectangleLinesEx(shell, 1.7f, c);
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy + s * 0.22f), s * 0.36f, c);
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy + s * 0.22f), s * 0.16f, Fade(c, 0.8f));
            DrawLineEx({cx - s * 0.4f, cy - s * 0.5f}, {cx - s * 0.4f, cy - s}, 1.5f, c);
            DrawLineEx({cx + s * 0.4f, cy - s * 0.5f}, {cx + s * 0.4f, cy - s}, 1.5f, c);
            DrawLineEx({cx - s * 0.55f, cy - s}, {cx - s * 0.25f, cy - s}, 1.5f, Fade(c, 0.7f));
            DrawLineEx({cx + s * 0.25f, cy - s}, {cx + s * 0.55f, cy - s}, 1.5f, Fade(c, 0.7f));
            break;
        }
        case ExtIcon::ROBOT_ARM:
        {
            // Articulated arm: base, two links, gripper
            DrawLineEx({cx - s * 0.85f, cy + s * 0.9f}, {cx - s * 0.15f, cy + s * 0.9f}, 2.0f, c);
            Vector2 shoulder = {cx - s * 0.5f, cy + s * 0.9f};
            Vector2 elbow = {cx - s * 0.05f, cy - s * 0.1f};
            Vector2 wrist = {cx + s * 0.7f, cy - s * 0.55f};
            DrawLineEx(shoulder, elbow, 1.9f, c);
            DrawLineEx(elbow, wrist, 1.7f, c);
            DrawCircleLines(static_cast<int>(shoulder.x), static_cast<int>(shoulder.y), s * 0.2f, c);
            DrawCircleLines(static_cast<int>(elbow.x), static_cast<int>(elbow.y), s * 0.17f, c);
            DrawLineEx(wrist, {wrist.x + s * 0.3f, wrist.y - s * 0.3f}, 1.5f, c);
            DrawLineEx(wrist, {wrist.x + s * 0.34f, wrist.y + s * 0.16f}, 1.5f, c);
            break;
        }
        case ExtIcon::MAGNIFIER:
        {
            // Inspection lens with a pass tick inside
            float lr = s * 0.6f;
            float lx = cx - s * 0.18f, ly = cy - s * 0.2f;
            DrawCircleLines(static_cast<int>(lx), static_cast<int>(ly), lr, c);
            DrawCircleLines(static_cast<int>(lx), static_cast<int>(ly), lr - 1.5f, Fade(c, 0.4f));
            DrawLineEx({lx + lr * 0.72f, ly + lr * 0.72f}, {cx + s * 0.85f, cy + s * 0.88f}, 2.2f, c);
            DrawLineEx({lx - lr * 0.4f, ly}, {lx - lr * 0.08f, ly + lr * 0.34f}, 1.6f, c);
            DrawLineEx({lx - lr * 0.08f, ly + lr * 0.34f}, {lx + lr * 0.48f, ly - lr * 0.36f}, 1.6f, c);
            break;
        }
        case ExtIcon::PALLET:
        {
            // Loaded pallet with a routing arrow
            DrawRectangleLinesEx({cx - s * 0.9f, cy + s * 0.42f, s * 1.8f, s * 0.34f}, 1.6f, c);
            DrawLineEx({cx - s * 0.5f, cy + s * 0.76f}, {cx - s * 0.5f, cy + s * 0.95f}, 1.5f, Fade(c, 0.7f));
            DrawLineEx({cx + s * 0.5f, cy + s * 0.76f}, {cx + s * 0.5f, cy + s * 0.95f}, 1.5f, Fade(c, 0.7f));
            DrawRectangleLinesEx({cx - s * 0.62f, cy - s * 0.28f, s * 0.7f, s * 0.7f}, 1.5f, c);
            DrawRectangleLinesEx({cx + s * 0.1f, cy - s * 0.05f, s * 0.5f, s * 0.47f}, 1.4f, Fade(c, 0.75f));
            DrawLineEx({cx - s * 0.3f, cy - s * 0.72f}, {cx + s * 0.62f, cy - s * 0.72f}, 1.5f, Fade(c, 0.8f));
            DrawLineEx({cx + s * 0.34f, cy - s * 0.95f}, {cx + s * 0.62f, cy - s * 0.72f}, 1.5f, Fade(c, 0.8f));
            DrawLineEx({cx + s * 0.34f, cy - s * 0.49f}, {cx + s * 0.62f, cy - s * 0.72f}, 1.5f, Fade(c, 0.8f));
            break;
        }
        case ExtIcon::CHIP:
        {
            // Controller die with pins on all four edges
            Rectangle die = {cx - s * 0.58f, cy - s * 0.58f, s * 1.16f, s * 1.16f};
            DrawRectangleLinesEx(die, 1.7f, c);
            DrawRectangleLinesEx({cx - s * 0.24f, cy - s * 0.24f, s * 0.48f, s * 0.48f}, 1.3f, Fade(c, 0.6f));
            for (int i = -1; i <= 1; i++)
            {
                float o = i * s * 0.34f;
                DrawLineEx({cx + o, die.y}, {cx + o, die.y - s * 0.36f}, 1.4f, Fade(c, 0.85f));
                DrawLineEx({cx + o, die.y + die.height}, {cx + o, die.y + die.height + s * 0.36f}, 1.4f, Fade(c, 0.85f));
                DrawLineEx({die.x, cy + o}, {die.x - s * 0.36f, cy + o}, 1.4f, Fade(c, 0.85f));
                DrawLineEx({die.x + die.width, cy + o}, {die.x + die.width + s * 0.36f, cy + o}, 1.4f, Fade(c, 0.85f));
            }
            break;
        }

        // --- Research ---
        case ExtIcon::CHART:
        {
            // Axes with a rising trend line and plotted points
            DrawLineEx({cx - s * 0.8f, cy - s * 0.85f}, {cx - s * 0.8f, cy + s * 0.8f}, 1.6f, c);
            DrawLineEx({cx - s * 0.8f, cy + s * 0.8f}, {cx + s * 0.9f, cy + s * 0.8f}, 1.6f, c);
            Vector2 pts[4] = {{cx - s * 0.45f, cy + s * 0.35f}, {cx - s * 0.05f, cy - s * 0.15f},
                              {cx + s * 0.32f, cy + s * 0.1f}, {cx + s * 0.72f, cy - s * 0.62f}};
            for (int i = 0; i < 3; i++) DrawLineEx(pts[i], pts[i + 1], 1.6f, Fade(c, 0.9f));
            for (const Vector2& p : pts) DrawCircle(static_cast<int>(p.x), static_cast<int>(p.y), s * 0.13f, c);
            break;
        }
        case ExtIcon::ORB:
        {
            // Holographic globe: sphere with latitude/longitude rings
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), s * 0.85f, c);
            for (int i = -1; i <= 1; i++)
            {
                float ry = s * 0.85f * sqrtf(std::max(0.0f, 1.0f - (i * 0.5f) * (i * 0.5f)));
                DrawEllipseLines(static_cast<int>(cx), static_cast<int>(cy + i * s * 0.42f),
                                 ry, ry * 0.22f, Fade(c, 0.55f));
            }
            DrawEllipseLines(static_cast<int>(cx), static_cast<int>(cy), s * 0.3f, s * 0.85f, Fade(c, 0.55f));
            break;
        }
        case ExtIcon::SERVER_RACK:
        {
            // Archive rack: cabinet with stacked drives and activity dots
            Rectangle cab = {cx - s * 0.68f, cy - s * 0.9f, s * 1.36f, s * 1.8f};
            DrawRectangleLinesEx(cab, 1.7f, c);
            for (int i = 0; i < 4; i++)
            {
                float ry = cab.y + s * 0.16f + i * s * 0.42f;
                DrawRectangleLinesEx({cab.x + s * 0.14f, ry, cab.width - s * 0.28f, s * 0.26f},
                                     1.3f, Fade(c, 0.85f));
                DrawCircle(static_cast<int>(cab.x + cab.width - s * 0.28f),
                           static_cast<int>(ry + s * 0.13f), s * 0.07f, Fade(c, 0.9f));
            }
            break;
        }
        // --- Construction ---
        case ExtIcon::STAKES:
        {
            // Survey stakes on graded ground, with a level line between them
            DrawLineEx({cx - s, cy + s * 0.75f}, {cx + s, cy + s * 0.75f}, 1.7f, c);
            for (int i = -1; i <= 1; i += 2)
            {
                float sx = cx + i * s * 0.62f;
                DrawLineEx({sx, cy + s * 0.75f}, {sx, cy - s * 0.5f}, 1.6f, c);
                DrawLineEx({sx - s * 0.16f, cy - s * 0.5f}, {sx + s * 0.16f, cy - s * 0.62f},
                           1.4f, Fade(c, 0.85f));
            }
            DrawLineEx({cx - s * 0.62f, cy - s * 0.28f}, {cx + s * 0.62f, cy - s * 0.28f},
                       1.3f, Fade(c, 0.6f));
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy - s * 0.28f), s * 0.11f, c);
            break;
        }
        case ExtIcon::REBAR:
        {
            // Poured footing with a rebar lattice showing through
            Rectangle slab = {cx - s * 0.92f, cy + s * 0.1f, s * 1.84f, s * 0.72f};
            DrawRectangleLinesEx(slab, 1.7f, c);
            for (int i = 0; i < 3; i++)
            {
                float gx = slab.x + s * 0.42f + i * s * 0.5f;
                DrawLineEx({gx, slab.y + slab.height}, {gx, cy - s * 0.72f}, 1.4f, Fade(c, 0.8f));
            }
            DrawLineEx({cx - s * 0.7f, cy - s * 0.34f}, {cx + s * 0.7f, cy - s * 0.34f},
                       1.3f, Fade(c, 0.6f));
            DrawLineEx({cx - s * 0.7f, cy - s * 0.7f}, {cx + s * 0.7f, cy - s * 0.7f},
                       1.3f, Fade(c, 0.6f));
            break;
        }
        case ExtIcon::GIRDER:
        {
            // Steel frame: two columns, a top beam, and cross bracing
            DrawLineEx({cx - s * 0.75f, cy + s * 0.85f}, {cx - s * 0.75f, cy - s * 0.7f}, 1.8f, c);
            DrawLineEx({cx + s * 0.75f, cy + s * 0.85f}, {cx + s * 0.75f, cy - s * 0.7f}, 1.8f, c);
            DrawLineEx({cx - s * 0.95f, cy - s * 0.7f}, {cx + s * 0.95f, cy - s * 0.7f}, 1.8f, c);
            DrawLineEx({cx - s * 0.75f, cy - s * 0.7f}, {cx + s * 0.75f, cy + s * 0.3f},
                       1.3f, Fade(c, 0.6f));
            DrawLineEx({cx + s * 0.75f, cy - s * 0.7f}, {cx - s * 0.75f, cy + s * 0.3f},
                       1.3f, Fade(c, 0.6f));
            DrawLineEx({cx - s * 0.75f, cy + s * 0.3f}, {cx + s * 0.75f, cy + s * 0.3f},
                       1.3f, Fade(c, 0.75f));
            break;
        }
        case ExtIcon::FITOUT:
        {
            // Wall panel being fitted: outlet, conduit run, and a partial panel edge
            Rectangle wall = {cx - s * 0.9f, cy - s * 0.85f, s * 1.8f, s * 1.7f};
            DrawRectangleLinesEx(wall, 1.7f, c);
            DrawLineEx({cx + s * 0.1f, wall.y}, {cx + s * 0.1f, wall.y + wall.height},
                       1.4f, Fade(c, 0.7f));
            DrawRectangleLinesEx({cx - s * 0.62f, cy - s * 0.4f, s * 0.42f, s * 0.42f},
                                 1.4f, Fade(c, 0.9f));
            DrawLineEx({cx - s * 0.41f, cy + s * 0.02f}, {cx - s * 0.41f, cy + s * 0.6f},
                       1.3f, Fade(c, 0.7f));
            DrawLineEx({cx - s * 0.41f, cy + s * 0.6f}, {cx + s * 0.1f, cy + s * 0.6f},
                       1.3f, Fade(c, 0.7f));
            DrawLineEx({cx + s * 0.34f, cy - s * 0.5f}, {cx + s * 0.72f, cy - s * 0.5f},
                       1.3f, Fade(c, 0.55f));
            DrawLineEx({cx + s * 0.34f, cy - s * 0.14f}, {cx + s * 0.72f, cy - s * 0.14f},
                       1.3f, Fade(c, 0.55f));
            break;
        }
        case ExtIcon::WRENCH:
        {
            // Open-ended spanner laid diagonally
            Vector2 dir = {0.66f, -0.75f};
            Vector2 tail = {cx - dir.x * s * 0.85f, cy - dir.y * s * 0.85f};
            Vector2 head = {cx + dir.x * s * 0.5f, cy + dir.y * s * 0.5f};
            DrawLineEx(tail, head, 2.4f, c);
            // Jaw: a broken ring opening away from the shaft
            Vector2 prev = {0};
            for (int i = 0; i <= 14; i++)
            {
                float a = -0.85f + i / 14.0f * 4.6f;
                float rot = atan2f(dir.y, dir.x);
                Vector2 p = {head.x + cosf(a + rot) * s * 0.42f,
                             head.y + sinf(a + rot) * s * 0.42f};
                if (i > 0) DrawLineEx(prev, p, 1.8f, c);
                prev = p;
            }
            DrawCircleLines(static_cast<int>(tail.x), static_cast<int>(tail.y), s * 0.2f,
                            Fade(c, 0.7f));
            break;
        }

        // --- Transport ---
        case ExtIcon::HAULER:
        {
            // Cab plus cargo box on two wheels
            DrawRectangleLinesEx({cx - s * 0.95f, cy - s * 0.55f, s * 1.1f, s * 0.95f}, 1.6f, c);
            DrawLineEx({cx + s * 0.15f, cy + s * 0.4f}, {cx + s * 0.15f, cy - s * 0.15f}, 1.6f, c);
            DrawLineEx({cx + s * 0.15f, cy - s * 0.15f}, {cx + s * 0.55f, cy - s * 0.15f}, 1.6f, c);
            DrawLineEx({cx + s * 0.55f, cy - s * 0.15f}, {cx + s * 0.95f, cy + s * 0.15f}, 1.6f, c);
            DrawLineEx({cx + s * 0.95f, cy + s * 0.15f}, {cx + s * 0.95f, cy + s * 0.4f}, 1.6f, c);
            DrawLineEx({cx - s * 0.95f, cy + s * 0.4f}, {cx + s * 0.95f, cy + s * 0.4f}, 1.6f, c);
            DrawCircleLines(static_cast<int>(cx - s * 0.5f), static_cast<int>(cy + s * 0.62f),
                            s * 0.26f, c);
            DrawCircleLines(static_cast<int>(cx + s * 0.58f), static_cast<int>(cy + s * 0.62f),
                            s * 0.26f, c);
            break;
        }
        case ExtIcon::ROUTE_NODES:
        {
            // Waypoints joined by a chosen path, with one alternate branch dimmed
            Vector2 a = {cx - s * 0.8f, cy + s * 0.6f};
            Vector2 b = {cx - s * 0.05f, cy - s * 0.1f};
            Vector2 d = {cx + s * 0.8f, cy - s * 0.7f};
            Vector2 alt = {cx + s * 0.5f, cy + s * 0.62f};
            DrawLineEx(a, b, 1.7f, c);
            DrawLineEx(b, d, 1.7f, c);
            DrawLineEx(b, alt, 1.3f, Fade(c, 0.4f));
            for (const Vector2& p : {a, b, d})
            {
                DrawCircle(static_cast<int>(p.x), static_cast<int>(p.y), s * 0.16f, c);
            }
            DrawCircleLines(static_cast<int>(alt.x), static_cast<int>(alt.y), s * 0.15f,
                            Fade(c, 0.5f));
            break;
        }
        case ExtIcon::DEPOT:
        {
            // Shed with a roller door and a loading apron
            DrawLineEx({cx - s, cy - s * 0.15f}, {cx, cy - s * 0.85f}, 1.7f, c);
            DrawLineEx({cx, cy - s * 0.85f}, {cx + s, cy - s * 0.15f}, 1.7f, c);
            DrawLineEx({cx - s * 0.85f, cy - s * 0.15f}, {cx - s * 0.85f, cy + s * 0.8f}, 1.6f, c);
            DrawLineEx({cx + s * 0.85f, cy - s * 0.15f}, {cx + s * 0.85f, cy + s * 0.8f}, 1.6f, c);
            DrawLineEx({cx - s * 0.85f, cy + s * 0.8f}, {cx + s * 0.85f, cy + s * 0.8f}, 1.7f, c);
            DrawRectangleLinesEx({cx - s * 0.42f, cy + s * 0.1f, s * 0.84f, s * 0.7f}, 1.4f,
                                 Fade(c, 0.85f));
            for (int i = 0; i < 2; i++)
            {
                float ly = cy + s * 0.32f + i * s * 0.22f;
                DrawLineEx({cx - s * 0.42f, ly}, {cx + s * 0.42f, ly}, 1.2f, Fade(c, 0.5f));
            }
            break;
        }
        case ExtIcon::LIFT:
        {
            // Vehicle on a service lift: platform raised on a column
            DrawLineEx({cx - s * 0.85f, cy + s * 0.9f}, {cx + s * 0.85f, cy + s * 0.9f}, 1.7f, c);
            DrawLineEx({cx, cy + s * 0.9f}, {cx, cy + s * 0.1f}, 2.0f, c);
            DrawLineEx({cx - s * 0.8f, cy + s * 0.1f}, {cx + s * 0.8f, cy + s * 0.1f}, 1.8f, c);
            DrawRectangleLinesEx({cx - s * 0.6f, cy - s * 0.55f, s * 1.2f, s * 0.65f}, 1.5f,
                                 Fade(c, 0.9f));
            DrawCircleLines(static_cast<int>(cx - s * 0.34f), static_cast<int>(cy - s * 0.55f),
                            s * 0.17f, Fade(c, 0.7f));
            DrawCircleLines(static_cast<int>(cx + s * 0.34f), static_cast<int>(cy - s * 0.55f),
                            s * 0.17f, Fade(c, 0.7f));
            for (int i = -1; i <= 1; i += 2)
            {
                DrawLineEx({cx, cy + s * 0.5f}, {cx + i * s * 0.3f, cy + s * 0.9f},
                           1.3f, Fade(c, 0.55f));
            }
            break;
        }
        case ExtIcon::CLIPBOARD:
        {
            // Dispatch board: schedule rows with a clock overlay
            Rectangle board = {cx - s * 0.72f, cy - s * 0.78f, s * 1.44f, s * 1.7f};
            DrawRectangleLinesEx(board, 1.7f, c);
            DrawRectangleLinesEx({cx - s * 0.26f, board.y - s * 0.16f, s * 0.52f, s * 0.3f},
                                 1.4f, c);
            for (int i = 0; i < 3; i++)
            {
                float ly = board.y + s * 0.42f + i * s * 0.34f;
                DrawLineEx({board.x + s * 0.16f, ly}, {board.x + s * 0.8f, ly}, 1.3f,
                           Fade(c, 0.8f - i * 0.15f));
            }
            DrawCircleLines(static_cast<int>(cx + s * 0.52f), static_cast<int>(cy + s * 0.6f),
                            s * 0.35f, c);
            DrawLineEx({cx + s * 0.52f, cy + s * 0.6f}, {cx + s * 0.52f, cy + s * 0.38f}, 1.3f, c);
            DrawLineEx({cx + s * 0.52f, cy + s * 0.6f}, {cx + s * 0.72f, cy + s * 0.6f}, 1.3f, c);
            break;
        }

        // --- Communication ---
        case ExtIcon::DISH:
        {
            // Satellite dish: an elliptical face tilted up-left on a short mast.
            // Drawn as a filled-outline ellipse rather than a parabola -- at list
            // size a bare parabola plus mast reads as the letter "A".
            Vector2 face = {cx - s * 0.12f, cy - s * 0.2f};
            DrawEllipseLines(static_cast<int>(face.x), static_cast<int>(face.y),
                             s * 0.72f, s * 0.5f, c);
            DrawEllipseLines(static_cast<int>(face.x), static_cast<int>(face.y),
                             s * 0.4f, s * 0.27f, Fade(c, 0.45f));
            // Feed arm reaching out of the dish to a horn at the focus
            DrawLineEx(face, {face.x + s * 0.62f, face.y - s * 0.52f}, 1.4f, c);
            DrawCircle(static_cast<int>(face.x + s * 0.62f),
                       static_cast<int>(face.y - s * 0.52f), s * 0.13f, c);
            // Mast and tripod foot
            DrawLineEx({face.x, face.y + s * 0.42f}, {cx, cy + s * 0.72f}, 1.6f, c);
            DrawLineEx({cx - s * 0.42f, cy + s * 0.88f}, {cx + s * 0.42f, cy + s * 0.88f}, 1.6f, c);
            DrawLineEx({cx, cy + s * 0.72f}, {cx - s * 0.3f, cy + s * 0.88f}, 1.3f, Fade(c, 0.8f));
            DrawLineEx({cx, cy + s * 0.72f}, {cx + s * 0.3f, cy + s * 0.88f}, 1.3f, Fade(c, 0.8f));
            break;
        }
        case ExtIcon::RELAY_TOWER:
        {
            // Mast with paired transmission arcs on both sides
            DrawLineEx({cx - s * 0.42f, cy + s * 0.9f}, {cx - s * 0.12f, cy - s * 0.55f}, 1.6f, c);
            DrawLineEx({cx + s * 0.42f, cy + s * 0.9f}, {cx + s * 0.12f, cy - s * 0.55f}, 1.6f, c);
            for (int i = 0; i < 2; i++)
            {
                float ly = cy + s * 0.42f - i * s * 0.5f;
                float half = s * (0.32f - i * 0.08f);
                DrawLineEx({cx - half, ly}, {cx + half, ly}, 1.3f, Fade(c, 0.7f));
            }
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy - s * 0.68f), s * 0.13f, c);
            for (int side = -1; side <= 1; side += 2)
            {
                for (int ring = 1; ring <= 2; ring++)
                {
                    float r = s * 0.3f * ring;
                    Vector2 prev = {0};
                    for (int k = 0; k <= 8; k++)
                    {
                        float a = 0.5f + k / 8.0f * 1.4f;
                        Vector2 p = {cx + side * sinf(a) * r,
                                     cy - s * 0.68f - cosf(a) * r};
                        if (k > 0) DrawLineEx(prev, p, 1.3f, Fade(c, 0.8f - ring * 0.22f));
                        prev = p;
                    }
                }
            }
            break;
        }
        case ExtIcon::PADLOCK:
        {
            // Closed padlock with a keyhole
            Rectangle body = {cx - s * 0.62f, cy - s * 0.1f, s * 1.24f, s * 0.95f};
            DrawRectangleLinesEx(body, 1.7f, c);
            Vector2 prev = {cx - s * 0.38f, cy - s * 0.1f};
            for (int i = 1; i <= 12; i++)
            {
                float a = PI - i / 12.0f * PI;
                Vector2 p = {cx + cosf(a) * s * 0.38f, cy - s * 0.1f - sinf(a) * s * 0.52f};
                DrawLineEx(prev, p, 1.6f, c);
                prev = p;
            }
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy + s * 0.3f), s * 0.15f, c);
            DrawLineEx({cx, cy + s * 0.42f}, {cx, cy + s * 0.62f}, 1.4f, c);
            break;
        }
        case ExtIcon::MESH:
        {
            // Hub-and-spoke network: four outer nodes around a larger hub. A
            // fully-connected five-node graph was tried first and filled in to a
            // solid blob at list size -- spokes plus a partial ring stay legible.
            Vector2 outer[4] = {
                {cx, cy - s * 0.82f}, {cx + s * 0.82f, cy},
                {cx, cy + s * 0.82f}, {cx - s * 0.82f, cy}
            };
            Vector2 hub = {cx, cy};

            for (int i = 0; i < 4; i++)
            {
                DrawLineEx(hub, outer[i], 1.4f, Fade(c, 0.85f));
                DrawLineEx(outer[i], outer[(i + 1) % 4], 1.0f, Fade(c, 0.3f));
            }
            for (const Vector2& n : outer)
            {
                DrawCircle(static_cast<int>(n.x), static_cast<int>(n.y), s * 0.16f, c);
            }
            DrawCircleLines(static_cast<int>(hub.x), static_cast<int>(hub.y), s * 0.28f, c);
            DrawCircle(static_cast<int>(hub.x), static_cast<int>(hub.y), s * 0.13f, c);
            break;
        }

        // --- Core (centre dome) ---
        case ExtIcon::LIFE_LOOP:
        {
            // Closed-loop life support: recirculation arrows around a bubble
            for (int half = 0; half < 2; half++)
            {
                Vector2 prev = {0};
                float base = half * PI;
                for (int i = 0; i <= 12; i++)
                {
                    float a = base + 0.35f + i / 12.0f * 2.2f;
                    Vector2 p = {cx + cosf(a) * s * 0.78f, cy + sinf(a) * s * 0.78f};
                    if (i > 0) DrawLineEx(prev, p, 1.6f, c);
                    prev = p;
                }
                // Arrowhead closing each half-loop
                float tipA = base + 0.35f + 2.2f;
                Vector2 tip = {cx + cosf(tipA) * s * 0.78f, cy + sinf(tipA) * s * 0.78f};
                Vector2 back = {cx + cosf(tipA - 0.3f) * s * 0.78f,
                                cy + sinf(tipA - 0.3f) * s * 0.78f};
                DrawLineEx(tip, {back.x + (tip.x - cx) * 0.28f, back.y + (tip.y - cy) * 0.28f}, 1.5f, c);
                DrawLineEx(tip, {back.x - (tip.x - cx) * 0.18f, back.y - (tip.y - cy) * 0.18f}, 1.5f, c);
            }
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), s * 0.3f, Fade(c, 0.85f));
            DrawCircle(static_cast<int>(cx - s * 0.1f), static_cast<int>(cy - s * 0.08f), s * 0.08f, Fade(c, 0.6f));
            break;
        }
        case ExtIcon::CREW:
        {
            // Three crew figures, the centre one forward
            struct Figure { float dx; float dy; float scale; float alpha; };
            Figure figures[3] = {{-0.62f, 0.1f, 0.82f, 0.65f},
                                 {0.62f, 0.1f, 0.82f, 0.65f},
                                 {0.0f, -0.05f, 1.0f, 1.0f}};
            for (const Figure& f : figures)
            {
                float fx = cx + f.dx * s;
                float fy = cy + f.dy * s;
                float fs = s * f.scale;
                Color fc = Fade(c, f.alpha);
                DrawCircleLines(static_cast<int>(fx), static_cast<int>(fy - fs * 0.42f), fs * 0.26f, fc);
                // Shoulders: a shallow arc under the head
                Vector2 prev = {0};
                for (int i = 0; i <= 10; i++)
                {
                    float a = PI + i / 10.0f * PI;
                    Vector2 p = {fx + cosf(a) * fs * 0.44f, fy + fs * 0.68f + sinf(a) * fs * 0.52f};
                    if (i > 0) DrawLineEx(prev, p, 1.5f, fc);
                    prev = p;
                }
            }
            break;
        }
        case ExtIcon::COMMAND:
        {
            // Rank chevrons over a command console bar
            for (int i = 0; i < 3; i++)
            {
                float cyy = cy - s * 0.55f + i * s * 0.38f;
                DrawLineEx({cx - s * 0.62f, cyy}, {cx, cyy + s * 0.3f}, 1.8f,
                           Fade(c, 1.0f - i * 0.22f));
                DrawLineEx({cx, cyy + s * 0.3f}, {cx + s * 0.62f, cyy}, 1.8f,
                           Fade(c, 1.0f - i * 0.22f));
            }
            DrawLineEx({cx - s * 0.8f, cy + s * 0.82f}, {cx + s * 0.8f, cy + s * 0.82f}, 1.7f, c);
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy + s * 0.82f), s * 0.11f, c);
            break;
        }
        case ExtIcon::WAVEFORM:
        {
            // Monitoring: a live trace across a framed screen
            DrawRectangleLinesEx({cx - s * 0.95f, cy - s * 0.7f, s * 1.9f, s * 1.4f}, 1.6f, c);
            Vector2 prev = {cx - s * 0.8f, cy};
            for (int i = 1; i <= 24; i++)
            {
                float t = i / 24.0f;
                float amp = (i % 8 < 4) ? 0.42f : 0.2f;
                Vector2 p = {cx - s * 0.8f + t * s * 1.6f,
                             cy - sinf(t * PI * 4.0f) * s * amp};
                DrawLineEx(prev, p, 1.5f, c);
                prev = p;
            }
            DrawLineEx({cx - s * 0.95f, cy}, {cx - s * 0.8f, cy}, 1.2f, Fade(c, 0.5f));
            break;
        }
        case ExtIcon::HAZARD:
        {
            // Radiation trefoil: three blades around a central source
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), s * 0.19f, c);
            for (int blade = 0; blade < 3; blade++)
            {
                float mid = -PI * 0.5f + blade * (2.0f * PI / 3.0f);
                float halfWidth = 0.52f;
                Vector2 inner1 = {cx + cosf(mid - halfWidth) * s * 0.34f,
                                  cy + sinf(mid - halfWidth) * s * 0.34f};
                Vector2 inner2 = {cx + cosf(mid + halfWidth) * s * 0.34f,
                                  cy + sinf(mid + halfWidth) * s * 0.34f};
                Vector2 outer1 = {cx + cosf(mid - halfWidth) * s * 0.92f,
                                  cy + sinf(mid - halfWidth) * s * 0.92f};
                Vector2 outer2 = {cx + cosf(mid + halfWidth) * s * 0.92f,
                                  cy + sinf(mid + halfWidth) * s * 0.92f};
                DrawLineEx(inner1, outer1, 1.5f, c);
                DrawLineEx(inner2, outer2, 1.5f, c);
                Vector2 prev = outer1;
                for (int i = 1; i <= 6; i++)
                {
                    float a = mid - halfWidth + i / 6.0f * halfWidth * 2.0f;
                    Vector2 p = {cx + cosf(a) * s * 0.92f, cy + sinf(a) * s * 0.92f};
                    DrawLineEx(prev, p, 1.5f, c);
                    prev = p;
                }
            }
            break;
        }


        case ExtIcon::BROADCAST:
        {
            // Publication: transmitter point with expanding wavefronts
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy + s * 0.5f), s * 0.16f, c);
            DrawLineEx({cx, cy + s * 0.5f}, {cx, cy + s * 0.95f}, 1.5f, Fade(c, 0.7f));
            for (int i = 1; i <= 3; i++)
            {
                float r = s * 0.3f * i;
                for (int side = -1; side <= 1; side += 2)
                {
                    Vector2 prev = {0};
                    for (int k = 0; k <= 8; k++)
                    {
                        float a = -PI * 0.5f + side * (0.12f + k / 8.0f * 0.62f) * PI;
                        Vector2 p = {cx + cosf(a) * r, cy + s * 0.5f + sinf(a) * r};
                        if (k > 0) DrawLineEx(prev, p, 1.4f, Fade(c, 0.85f - i * 0.16f));
                        prev = p;
                    }
                }
            }
            break;
        }
    }
}

// Per-module icon lookup for the module list and status segments.
static ExtIcon ExtModuleIcon(const std::string& moduleType)
{
    // Extraction
    if (moduleType == "PROSPECTING") return ExtIcon::RADAR;
    if (moduleType == "EXCAVATION") return ExtIcon::EXCAVATOR;
    if (moduleType == "BENEFICIATION") return ExtIcon::NODES;
    if (moduleType == "OPERATIONS") return ExtIcon::GEAR;
    if (moduleType == "DIRECTIVES") return ExtIcon::CROSSHAIR;

    // Farming
    if (moduleType == "IRRIGATION") return ExtIcon::DROPLET;
    if (moduleType == "GREENHOUSE") return ExtIcon::GREENHOUSE;
    if (moduleType == "HYDROPONICS") return ExtIcon::TRAYS;
    if (moduleType == "HARVEST") return ExtIcon::SHEAF;
    if (moduleType == "STORAGE") return ExtIcon::CRATE;

    // Energy
    if (moduleType == "SOLAR_ARRAY") return ExtIcon::SOLAR_PANEL;
    if (moduleType == "BATTERY") return ExtIcon::BATTERY;
    if (moduleType == "NUCLEAR") return ExtIcon::ATOM;
    if (moduleType == "GRID") return ExtIcon::PYLON;
    if (moduleType == "EMERGENCY") return ExtIcon::BEACON;

    // Manufacture
    if (moduleType == "FABRICATION") return ExtIcon::FURNACE;
    if (moduleType == "ASSEMBLY") return ExtIcon::ROBOT_ARM;
    if (moduleType == "QUALITY") return ExtIcon::MAGNIFIER;
    if (moduleType == "LOGISTICS") return ExtIcon::PALLET;
    if (moduleType == "AUTOMATION") return ExtIcon::CHIP;

    // Research
    if (moduleType == "LABORATORY") return ExtIcon::FLASK;
    if (moduleType == "ANALYSIS") return ExtIcon::CHART;
    if (moduleType == "SIMULATION") return ExtIcon::ORB;
    if (moduleType == "ARCHIVE") return ExtIcon::SERVER_RACK;
    if (moduleType == "PUBLICATION") return ExtIcon::BROADCAST;

    // Construction
    if (moduleType == "SITE_PREP") return ExtIcon::STAKES;
    if (moduleType == "FOUNDATION") return ExtIcon::REBAR;
    if (moduleType == "STRUCTURES") return ExtIcon::GIRDER;
    if (moduleType == "FITOUT") return ExtIcon::FITOUT;
    if (moduleType == "MAINTENANCE") return ExtIcon::WRENCH;

    // Transport
    if (moduleType == "FLEET") return ExtIcon::HAULER;
    if (moduleType == "ROUTING") return ExtIcon::ROUTE_NODES;
    if (moduleType == "DEPOT") return ExtIcon::DEPOT;
    if (moduleType == "SERVICING") return ExtIcon::LIFT;
    if (moduleType == "DISPATCH") return ExtIcon::CLIPBOARD;

    // Communication
    if (moduleType == "ANTENNA") return ExtIcon::DISH;
    if (moduleType == "RELAY") return ExtIcon::RELAY_TOWER;
    if (moduleType == "TELEMETRY") return ExtIcon::WAVEFORM;
    if (moduleType == "ENCRYPTION") return ExtIcon::PADLOCK;
    if (moduleType == "NETWORK") return ExtIcon::MESH;

    // Core
    if (moduleType == "LIFE_SUPPORT") return ExtIcon::LIFE_LOOP;
    if (moduleType == "ROSTER") return ExtIcon::CREW;
    if (moduleType == "COMMAND") return ExtIcon::COMMAND;
    if (moduleType == "MONITORING") return ExtIcon::WAVEFORM;
    if (moduleType == "SAFETY") return ExtIcon::HAZARD;

    return ExtIcon::OVERVIEW;
}

// Unit-type identity for the top bar: icon chip, display title, accent colour.
struct UnitIdentity
{
    ExtIcon icon;
    const char* title;
    Color accent;
};

static UnitIdentity ExtUnitIdentity(const std::string& unitType)
{
    if (unitType == "Extraction")  return {ExtIcon::EXCAVATOR, "EXTRACTION UNIT",  EXT_ACCENT_CYAN};
    if (unitType == "Farming")     return {ExtIcon::GREENHOUSE, "FARMING UNIT",    EXT_ACCENT_GREEN};
    if (unitType == "Energy")      return {ExtIcon::BOLT,       "ENERGY UNIT",     EXT_ACCENT_GOLD};
    if (unitType == "Manufacture") return {ExtIcon::ROBOT_ARM,  "MANUFACTURE UNIT", EXT_ACCENT_CYAN};
    if (unitType == "Research")    return {ExtIcon::ORB,        "RESEARCH UNIT",   EXT_ACCENT_VIOLET};
    if (unitType == "Construction") return {ExtIcon::GIRDER, "CONSTRUCTION UNIT", EXT_ACCENT_GOLD};
    if (unitType == "Transport")   return {ExtIcon::HAULER,     "TRANSPORT UNIT",  EXT_ACCENT_CYAN};
    if (unitType == "Core")        return {ExtIcon::CREW,       "CORE / HABITAT",  EXT_ACCENT_GREEN};
    if (unitType == "Communication") return {ExtIcon::DISH, "COMMUNICATION UNIT", EXT_ACCENT_VIOLET};
    return {ExtIcon::OVERVIEW, "UNIT", EXT_ACCENT_CYAN};
}

// Blueprint plate stamped under the control-panel art, per the UI kit.
static const char* ExtUnitBlueprintTag(const std::string& unitType)
{
    if (unitType == "Extraction")  return "UE-3";
    if (unitType == "Farming")     return "UF-1";
    if (unitType == "Energy")      return "UN-2";
    if (unitType == "Manufacture") return "UM-4";
    if (unitType == "Research")    return "UR-5";
    if (unitType == "Construction") return "UC-6";
    if (unitType == "Transport")   return "UT-7";
    if (unitType == "Core")        return "CR-0";
    if (unitType == "Communication") return "UK-8";
    return "U-0";
}

// Segmented meter (calibration gauge in the kit).
static void ExtDrawSegBar(float x, float y, float w, float h, float value, Color color)
{
    const int segs = 10;
    DrawRectangleRounded({x, y, w, h}, 0.5f, 3, {18, 26, 44, 255});
    DrawRectangleRoundedLinesEx({x, y, w, h}, 0.5f, 3, 1.0f, EXT_PANEL_BORDER);

    float segW = (w - 6.0f) / segs;
    int lit = static_cast<int>(value * segs + 0.5f);
    for (int i = 0; i < lit; i++)
    {
        DrawRectangleRec({x + 3.0f + i * segW + 1.0f, y + 3.0f, segW - 2.0f, h - 6.0f},
                         Fade(color, 0.45f + 0.55f * (i + 1) / segs));
    }
}

// Diagonal hazard stripes clipped to a rectangle (danger button edges).
static void ExtDrawHazardStripes(Rectangle r, Color c)
{
    const float stride = 14.0f;
    BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y),
                     static_cast<int>(r.width), static_cast<int>(r.height));
    for (float sx = r.x - r.height; sx < r.x + r.width; sx += stride)
    {
        DrawLineEx({sx, r.y + r.height}, {sx + r.height, r.y}, 4.0f, c);
    }
    EndScissorMode();
}

// Double chevron ">>" (action buttons in the kit).
static void ExtDrawChevrons(float x, float y, float s, Color c)
{
    for (int i = 0; i < 2; i++)
    {
        float ox = x + i * s * 0.9f;
        DrawLineEx({ox, y - s}, {ox + s * 0.75f, y}, 2.0f, c);
        DrawLineEx({ox + s * 0.75f, y}, {ox, y + s}, 2.0f, c);
    }
}

// One cuboid of the wireframe illustration, in a simple isometric projection.
static void ExtDrawWireBox(Vector2 origin, float scale,
                           float x, float y, float z, float w, float d, float h, Color c)
{
    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };

    Vector2 p[8] = {
        project(x, y, z),         project(x + w, y, z),
        project(x + w, y + d, z), project(x, y + d, z),
        project(x, y, z + h),         project(x + w, y, z + h),
        project(x + w, y + d, z + h), project(x, y + d, z + h)
    };

    const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
    };
    for (const auto& e : edges) DrawLineEx(p[e[0]], p[e[1]], 1.0f, c);
}

// Blueprint drawing of the extraction unit (control panel art).
static void ExtDrawWireframeExtraction(Rectangle area, Color c)
{
    Vector2 origin = {area.x + area.width * 0.5f, area.y + area.height * 0.62f};
    float scale = std::min(area.width, area.height) * 0.058f;

    Color dim = Fade(c, 0.35f);
    Color mid = Fade(c, 0.55f);

    // Base platform, processing block, small annex
    ExtDrawWireBox(origin, scale, -4.0f, -4.0f, 0.0f, 8.0f, 8.0f, 0.8f, dim);
    ExtDrawWireBox(origin, scale, -2.8f, -2.2f, 0.8f, 4.2f, 4.2f, 2.0f, mid);
    ExtDrawWireBox(origin, scale, 1.6f, -3.2f, 0.8f, 1.9f, 1.9f, 1.2f, dim);

    // Derrick tower: four legs converging, with cross braces
    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };
    float baseZ = 2.8f, topZ = 7.2f;
    Vector2 legs[4] = {project(-1.4f, -1.4f, baseZ), project(1.4f, -1.4f, baseZ),
                       project(1.4f, 1.4f, baseZ), project(-1.4f, 1.4f, baseZ)};
    Vector2 apex = project(0.0f, 0.0f, topZ);
    for (const Vector2& leg : legs) DrawLineEx(leg, apex, 1.0f, mid);
    for (int level = 0; level < 3; level++)
    {
        float t = 0.22f + level * 0.26f;
        float half = 1.4f * (1.0f - t);
        float z = baseZ + (topZ - baseZ) * t;
        Vector2 ring[4] = {project(-half, -half, z), project(half, -half, z),
                           project(half, half, z), project(-half, half, z)};
        for (int k = 0; k < 4; k++) DrawLineEx(ring[k], ring[(k + 1) % 4], 1.0f, dim);
    }
    DrawLineEx(apex, {apex.x, apex.y - scale * 1.2f}, 1.0f, mid);
    DrawCircle(static_cast<int>(apex.x), static_cast<int>(apex.y - scale * 1.4f), 1.5f, c);
}

// Blueprint of the farming unit: platform with a row of arched grow houses.
static void ExtDrawWireframeFarming(Rectangle area, Color c)
{
    Vector2 origin = {area.x + area.width * 0.5f, area.y + area.height * 0.62f};
    float scale = std::min(area.width, area.height) * 0.058f;
    Color dim = Fade(c, 0.35f);
    Color mid = Fade(c, 0.55f);

    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };

    ExtDrawWireBox(origin, scale, -4.0f, -4.0f, 0.0f, 8.0f, 8.0f, 0.8f, dim);

    // Three barrel-vault greenhouses running along the platform. The arch rises
    // ~3 units so it reads at the control panel's small art size.
    for (int bay = 0; bay < 3; bay++)
    {
        float bx = -3.4f + bay * 2.4f;
        const float span = 2.0f;
        Vector2 prevNear = project(bx, -3.0f, 0.8f);
        Vector2 prevFar = project(bx, 3.0f, 0.8f);
        for (int i = 1; i <= 10; i++)
        {
            float t = i / 10.0f;
            float ax = bx + t * span;
            float az = 0.8f + sinf(t * PI) * 3.0f;
            Vector2 near2 = project(ax, -3.0f, az);
            Vector2 far2 = project(ax, 3.0f, az);
            DrawLineEx(prevNear, near2, 1.0f, mid);
            DrawLineEx(prevFar, far2, 1.0f, dim);
            prevNear = near2;
            prevFar = far2;
        }
        // Ridge and eaves running the length of the bay
        DrawLineEx(project(bx + span * 0.5f, -3.0f, 3.8f),
                   project(bx + span * 0.5f, 3.0f, 3.8f), 1.0f, mid);
        DrawLineEx(project(bx, -3.0f, 0.8f), project(bx, 3.0f, 0.8f), 1.0f, dim);
        DrawLineEx(project(bx + span, -3.0f, 0.8f), project(bx + span, 3.0f, 0.8f), 1.0f, dim);
    }

    // Irrigation mast with a sprinkler head
    Vector2 mastBase = project(3.4f, 3.2f, 0.8f);
    Vector2 mastTop = project(3.4f, 3.2f, 4.4f);
    DrawLineEx(mastBase, mastTop, 1.0f, mid);
    DrawCircle(static_cast<int>(mastTop.x), static_cast<int>(mastTop.y), 1.5f, c);
    for (int i = -1; i <= 1; i++)
    {
        DrawLineEx(mastTop, project(3.4f + i * 1.4f, 3.2f - 1.2f, 3.5f), 1.0f, dim);
    }
}

// Blueprint of the energy unit: solar field, reactor drum, transmission mast.
static void ExtDrawWireframeEnergy(Rectangle area, Color c)
{
    Vector2 origin = {area.x + area.width * 0.5f, area.y + area.height * 0.62f};
    float scale = std::min(area.width, area.height) * 0.058f;
    Color dim = Fade(c, 0.35f);
    Color mid = Fade(c, 0.55f);

    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };

    ExtDrawWireBox(origin, scale, -4.0f, -4.0f, 0.0f, 8.0f, 8.0f, 0.8f, dim);

    // Tilted solar panels in two rows
    for (int row = 0; row < 2; row++)
    {
        for (int col = 0; col < 3; col++)
        {
            float sx = -3.4f + col * 1.7f;
            float sy = -3.0f + row * 1.9f;
            Vector2 lo1 = project(sx, sy, 0.8f);
            Vector2 lo2 = project(sx + 1.3f, sy, 0.8f);
            Vector2 hi1 = project(sx, sy + 0.1f, 2.0f);
            Vector2 hi2 = project(sx + 1.3f, sy + 0.1f, 2.0f);
            DrawLineEx(lo1, lo2, 1.0f, dim);
            DrawLineEx(hi1, hi2, 1.0f, mid);
            DrawLineEx(lo1, hi1, 1.0f, mid);
            DrawLineEx(lo2, hi2, 1.0f, mid);
        }
    }

    // Reactor containment drum
    for (int ring = 0; ring < 4; ring++)
    {
        float rz = 0.8f + ring * 1.1f;
        Vector2 prev = {0};
        for (int i = 0; i <= 16; i++)
        {
            float a = i / 16.0f * 2.0f * PI;
            Vector2 p = project(2.4f + cosf(a) * 1.3f, 2.4f + sinf(a) * 1.3f, rz);
            if (i > 0) DrawLineEx(prev, p, 1.0f, ring == 3 ? mid : dim);
            prev = p;
        }
    }
    DrawLineEx(project(1.1f, 2.4f, 0.8f), project(1.1f, 2.4f, 4.1f), 1.0f, mid);
    DrawLineEx(project(3.7f, 2.4f, 0.8f), project(3.7f, 2.4f, 4.1f), 1.0f, mid);

    // Transmission mast with cross-arms
    Vector2 mastBase = project(-3.2f, 3.2f, 0.8f);
    Vector2 mastTop = project(-3.2f, 3.2f, 5.6f);
    DrawLineEx(mastBase, mastTop, 1.0f, mid);
    for (int i = 0; i < 2; i++)
    {
        float az = 3.6f + i * 1.2f;
        DrawLineEx(project(-4.4f, 3.2f, az), project(-2.0f, 3.2f, az), 1.0f, dim);
    }
    DrawCircle(static_cast<int>(mastTop.x), static_cast<int>(mastTop.y), 1.5f, c);
}

// Blueprint of the manufacture unit: shop floor, gantry, stack of finished goods.
static void ExtDrawWireframeManufacture(Rectangle area, Color c)
{
    Vector2 origin = {area.x + area.width * 0.5f, area.y + area.height * 0.62f};
    float scale = std::min(area.width, area.height) * 0.058f;
    Color dim = Fade(c, 0.35f);
    Color mid = Fade(c, 0.55f);

    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };

    ExtDrawWireBox(origin, scale, -4.0f, -4.0f, 0.0f, 8.0f, 8.0f, 0.8f, dim);

    // Main shop hall with a saw-tooth roof
    ExtDrawWireBox(origin, scale, -3.4f, -3.0f, 0.8f, 5.2f, 4.2f, 2.6f, mid);
    for (int tooth = 0; tooth < 3; tooth++)
    {
        float tx = -3.2f + tooth * 1.7f;
        DrawLineEx(project(tx, -3.0f, 3.4f), project(tx + 0.9f, -3.0f, 4.3f), 1.0f, mid);
        DrawLineEx(project(tx + 0.9f, -3.0f, 4.3f), project(tx + 0.9f, -3.0f, 3.4f), 1.0f, dim);
        DrawLineEx(project(tx, 1.2f, 3.4f), project(tx + 0.9f, 1.2f, 4.3f), 1.0f, dim);
        DrawLineEx(project(tx + 0.9f, -3.0f, 4.3f), project(tx + 0.9f, 1.2f, 4.3f), 1.0f, dim);
    }

    // Overhead gantry crane spanning the yard
    DrawLineEx(project(-3.6f, 2.4f, 0.8f), project(-3.6f, 2.4f, 3.6f), 1.0f, mid);
    DrawLineEx(project(3.6f, 2.4f, 0.8f), project(3.6f, 2.4f, 3.6f), 1.0f, mid);
    DrawLineEx(project(-3.6f, 2.4f, 3.6f), project(3.6f, 2.4f, 3.6f), 1.0f, mid);
    DrawLineEx(project(0.6f, 2.4f, 3.6f), project(0.6f, 2.4f, 2.2f), 1.0f, dim);

    // Palletised output stacked in the yard
    ExtDrawWireBox(origin, scale, 2.0f, -2.6f, 0.8f, 1.5f, 1.5f, 1.1f, mid);
    ExtDrawWireBox(origin, scale, 2.0f, -2.6f, 1.9f, 1.5f, 1.5f, 0.9f, dim);
    ExtDrawWireBox(origin, scale, 2.2f, -0.7f, 0.8f, 1.1f, 1.1f, 0.8f, dim);
}

// Blueprint of the research unit: lab block under an observation dome.
static void ExtDrawWireframeResearch(Rectangle area, Color c)
{
    Vector2 origin = {area.x + area.width * 0.5f, area.y + area.height * 0.62f};
    float scale = std::min(area.width, area.height) * 0.058f;
    Color dim = Fade(c, 0.35f);
    Color mid = Fade(c, 0.55f);

    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };

    ExtDrawWireBox(origin, scale, -4.0f, -4.0f, 0.0f, 8.0f, 8.0f, 0.8f, dim);
    ExtDrawWireBox(origin, scale, -3.0f, -3.0f, 0.8f, 6.0f, 6.0f, 1.6f, mid);

    // Geodesic observation dome: latitude rings plus meridians
    const float domeR = 2.6f;
    const float domeZ = 2.4f;
    for (int ring = 1; ring <= 3; ring++)
    {
        float t = ring / 4.0f;
        float rr = domeR * cosf(t * PI * 0.5f);
        float rz = domeZ + domeR * sinf(t * PI * 0.5f);
        Vector2 prev = {0};
        for (int i = 0; i <= 20; i++)
        {
            float a = i / 20.0f * 2.0f * PI;
            Vector2 p = project(cosf(a) * rr, sinf(a) * rr, rz);
            if (i > 0) DrawLineEx(prev, p, 1.0f, ring == 1 ? mid : dim);
            prev = p;
        }
    }
    Vector2 apex = project(0.0f, 0.0f, domeZ + domeR);
    for (int m = 0; m < 6; m++)
    {
        float a = m / 6.0f * 2.0f * PI;
        Vector2 prev = project(cosf(a) * domeR, sinf(a) * domeR, domeZ);
        for (int i = 1; i <= 6; i++)
        {
            float t = i / 6.0f;
            float rr = domeR * cosf(t * PI * 0.5f);
            float rz = domeZ + domeR * sinf(t * PI * 0.5f);
            Vector2 p = project(cosf(a) * rr, sinf(a) * rr, rz);
            DrawLineEx(prev, p, 1.0f, dim);
            prev = p;
        }
    }
    DrawCircle(static_cast<int>(apex.x), static_cast<int>(apex.y), 1.5f, c);

    // Dish antenna on the corner of the lab block
    Vector2 dishBase = project(3.2f, 3.2f, 0.8f);
    Vector2 dishTop = project(3.2f, 3.2f, 3.0f);
    DrawLineEx(dishBase, dishTop, 1.0f, mid);
    Vector2 prevRim = {0};
    for (int i = 0; i <= 12; i++)
    {
        float a = i / 12.0f * PI;
        Vector2 p = project(3.2f + cosf(a) * 1.0f, 3.2f, 3.0f + sinf(a) * 1.0f);
        if (i > 0) DrawLineEx(prevRim, p, 1.0f, dim);
        prevRim = p;
    }
    DrawLineEx(project(2.2f, 3.2f, 3.0f), project(4.2f, 3.2f, 3.0f), 1.0f, mid);
}

// Blueprint of the construction unit: tower crane over a half-built frame.
static void ExtDrawWireframeConstruction(Rectangle area, Color c)
{
    Vector2 origin = {area.x + area.width * 0.5f, area.y + area.height * 0.62f};
    float scale = std::min(area.width, area.height) * 0.058f;
    Color dim = Fade(c, 0.35f);
    Color mid = Fade(c, 0.55f);

    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };

    ExtDrawWireBox(origin, scale, -4.0f, -4.0f, 0.0f, 8.0f, 8.0f, 0.8f, dim);

    // Structure under construction: three storeys, the top one only a frame
    ExtDrawWireBox(origin, scale, -3.0f, -2.4f, 0.8f, 3.6f, 3.6f, 1.3f, mid);
    ExtDrawWireBox(origin, scale, -3.0f, -2.4f, 2.1f, 3.6f, 3.6f, 1.3f, mid);
    for (int corner = 0; corner < 4; corner++)
    {
        float px2 = (corner == 0 || corner == 3) ? -3.0f : 0.6f;
        float py2 = (corner < 2) ? -2.4f : 1.2f;
        DrawLineEx(project(px2, py2, 3.4f), project(px2, py2, 4.7f), 1.0f, dim);
    }
    DrawLineEx(project(-3.0f, -2.4f, 4.7f), project(0.6f, -2.4f, 4.7f), 1.0f, dim);
    DrawLineEx(project(-3.0f, 1.2f, 4.7f), project(0.6f, 1.2f, 4.7f), 1.0f, dim);

    // Tower crane: mast, horizontal jib, counter-jib, and hook line
    Vector2 mastBase = project(2.8f, 2.8f, 0.8f);
    Vector2 mastTop = project(2.8f, 2.8f, 7.0f);
    DrawLineEx(mastBase, mastTop, 1.4f, mid);
    for (int i = 1; i < 5; i++)
    {
        float z = 0.8f + i * 1.24f;
        DrawLineEx(project(2.4f, 2.8f, z), project(3.2f, 2.8f, z), 1.0f, dim);
    }
    Vector2 jibEnd = project(-2.6f, 2.8f, 7.0f);
    Vector2 counterEnd = project(4.6f, 2.8f, 7.0f);
    DrawLineEx(mastTop, jibEnd, 1.2f, mid);
    DrawLineEx(mastTop, counterEnd, 1.2f, dim);
    DrawLineEx({mastTop.x, mastTop.y - scale * 1.1f}, jibEnd, 1.0f, dim);
    DrawLineEx({mastTop.x, mastTop.y - scale * 1.1f}, counterEnd, 1.0f, dim);
    DrawLineEx(mastTop, {mastTop.x, mastTop.y - scale * 1.1f}, 1.0f, mid);

    // Hook and suspended load
    Vector2 hookTop = project(-1.2f, 2.8f, 7.0f);
    Vector2 hookBottom = project(-1.2f, 2.8f, 4.0f);
    DrawLineEx(hookTop, hookBottom, 1.0f, mid);
    ExtDrawWireBox(origin, scale, -1.7f, 2.3f, 3.2f, 1.0f, 1.0f, 0.8f, c);
}

// Blueprint of the transport unit: depot shed, apron loop, and parked haulers.
static void ExtDrawWireframeTransport(Rectangle area, Color c)
{
    Vector2 origin = {area.x + area.width * 0.5f, area.y + area.height * 0.62f};
    float scale = std::min(area.width, area.height) * 0.058f;
    Color dim = Fade(c, 0.35f);
    Color mid = Fade(c, 0.55f);

    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };

    ExtDrawWireBox(origin, scale, -4.0f, -4.0f, 0.0f, 8.0f, 8.0f, 0.8f, dim);

    // Depot shed with three open bays along the front
    ExtDrawWireBox(origin, scale, -3.4f, -3.2f, 0.8f, 5.0f, 2.6f, 2.2f, mid);
    for (int bay = 0; bay < 3; bay++)
    {
        float bx = -3.0f + bay * 1.6f;
        DrawLineEx(project(bx, -0.6f, 0.8f), project(bx, -0.6f, 2.4f), 1.0f, dim);
        DrawLineEx(project(bx + 1.1f, -0.6f, 0.8f), project(bx + 1.1f, -0.6f, 2.4f), 1.0f, dim);
        DrawLineEx(project(bx, -0.6f, 2.4f), project(bx + 1.1f, -0.6f, 2.4f), 1.0f, dim);
    }

    // Apron loop road in front of the bays
    Vector2 prev = {0};
    for (int i = 0; i <= 28; i++)
    {
        float a = i / 28.0f * 2.0f * PI;
        Vector2 p = project(0.2f + cosf(a) * 2.9f, 1.8f + sinf(a) * 1.7f, 0.8f);
        if (i > 0) DrawLineEx(prev, p, 1.0f, mid);
        prev = p;
    }

    // Two haulers on the apron, one boxier than the other
    ExtDrawWireBox(origin, scale, -1.6f, 2.6f, 0.8f, 1.6f, 0.9f, 0.9f, c);
    ExtDrawWireBox(origin, scale, 1.6f, 0.4f, 0.8f, 1.3f, 0.8f, 0.7f, mid);
}

// Blueprint of the Core: pressurised dome with habitat modules and an airlock.
static void ExtDrawWireframeCore(Rectangle area, Color c)
{
    Vector2 origin = {area.x + area.width * 0.5f, area.y + area.height * 0.62f};
    float scale = std::min(area.width, area.height) * 0.058f;
    Color dim = Fade(c, 0.35f);
    Color mid = Fade(c, 0.55f);

    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };

    ExtDrawWireBox(origin, scale, -4.0f, -4.0f, 0.0f, 8.0f, 8.0f, 0.8f, dim);

    // Regolith berm ringing the dome -- the radiation shielding
    Vector2 prevBerm = {0};
    for (int i = 0; i <= 28; i++)
    {
        float a = i / 28.0f * 2.0f * PI;
        Vector2 p = project(cosf(a) * 3.5f, sinf(a) * 3.5f, 1.1f);
        if (i > 0) DrawLineEx(prevBerm, p, 1.0f, dim);
        prevBerm = p;
    }

    // Main pressurised dome: latitude rings plus meridians
    const float domeR = 2.7f;
    const float domeZ = 0.8f;
    for (int ring = 0; ring < 3; ring++)
    {
        float t = ring / 3.0f;
        float rr = domeR * cosf(t * PI * 0.5f);
        float rz = domeZ + domeR * sinf(t * PI * 0.5f);
        Vector2 prev = {0};
        for (int i = 0; i <= 24; i++)
        {
            float a = i / 24.0f * 2.0f * PI;
            Vector2 p = project(cosf(a) * rr, sinf(a) * rr, rz);
            if (i > 0) DrawLineEx(prev, p, 1.0f, ring == 0 ? mid : dim);
            prev = p;
        }
    }
    Vector2 apex = project(0.0f, 0.0f, domeZ + domeR);
    for (int m = 0; m < 6; m++)
    {
        float a = m / 6.0f * 2.0f * PI;
        Vector2 prev = project(cosf(a) * domeR, sinf(a) * domeR, domeZ);
        for (int i = 1; i <= 6; i++)
        {
            float t = i / 6.0f;
            float rr = domeR * cosf(t * PI * 0.5f);
            float rz = domeZ + domeR * sinf(t * PI * 0.5f);
            Vector2 p = project(cosf(a) * rr, sinf(a) * rr, rz);
            DrawLineEx(prev, p, 1.0f, mid);
            prev = p;
        }
    }
    DrawCircle(static_cast<int>(apex.x), static_cast<int>(apex.y), 1.5f, c);

    // Airlock vestibule and connecting tunnel to the ring
    ExtDrawWireBox(origin, scale, 2.4f, -0.7f, 0.8f, 1.5f, 1.4f, 1.2f, mid);
    DrawLineEx(project(2.4f, 0.0f, 1.4f), project(1.6f, 0.0f, 1.4f), 1.0f, mid);
    DrawLineEx(project(2.4f, -0.7f, 1.4f), project(1.6f, -0.7f, 1.4f), 1.0f, dim);

    // Life-support plant: two tanks and a radiator panel
    ExtDrawWireBox(origin, scale, -3.6f, 1.6f, 0.8f, 0.9f, 0.9f, 1.6f, dim);
    ExtDrawWireBox(origin, scale, -2.4f, 1.9f, 0.8f, 0.8f, 0.8f, 1.3f, dim);
    for (int fin = 0; fin < 4; fin++)
    {
        float fx = -3.4f + fin * 0.42f;
        DrawLineEx(project(fx, -2.6f, 0.8f), project(fx, -2.6f, 2.1f), 1.0f, dim);
    }
    DrawLineEx(project(-3.4f, -2.6f, 2.1f), project(-2.14f, -2.6f, 2.1f), 1.0f, mid);
}

static void ExtDrawWireframeCommunication(Rectangle area, Color c)
{
    Vector2 origin = {area.x + area.width * 0.5f, area.y + area.height * 0.62f};
    float scale = std::min(area.width, area.height) * 0.058f;
    Color dim = Fade(c, 0.35f);
    Color mid = Fade(c, 0.55f);

    auto project = [&](float wx, float wy, float wz) -> Vector2
    {
        return {origin.x + (wx - wy) * 0.866f * scale,
                origin.y + (wx + wy) * 0.5f * scale - wz * scale};
    };

    ExtDrawWireBox(origin, scale, -4.0f, -4.0f, 0.0f, 8.0f, 8.0f, 0.8f, dim);
    ExtDrawWireBox(origin, scale, -2.6f, -1.4f, 0.8f, 3.0f, 3.0f, 1.2f, mid);

    // Steerable dish: concentric rings on a tilted axis, on a pedestal
    Vector2 dishCentre = {-1.1f, 0.1f};
    float dishZ = 4.4f;
    DrawLineEx(project(dishCentre.x, dishCentre.y, 2.0f),
               project(dishCentre.x, dishCentre.y, dishZ), 1.3f, mid);
    for (int ring = 1; ring <= 3; ring++)
    {
        float rr = 0.62f * ring;
        Vector2 prev = {0};
        for (int i = 0; i <= 20; i++)
        {
            float a = i / 20.0f * 2.0f * PI;
            // Tilt the dish face back by squashing one axis and lifting with it
            Vector2 p = project(dishCentre.x + cosf(a) * rr,
                                dishCentre.y + sinf(a) * rr * 0.45f,
                                dishZ + sinf(a) * rr * 0.72f);
            if (i > 0) DrawLineEx(prev, p, 1.0f, ring == 3 ? mid : dim);
            prev = p;
        }
    }
    DrawCircle(static_cast<int>(project(dishCentre.x, dishCentre.y, dishZ).x),
               static_cast<int>(project(dishCentre.x, dishCentre.y, dishZ).y), 1.5f, c);

    // Relay mast array of three guyed poles at descending heights
    for (int m = 0; m < 3; m++)
    {
        float mx = 1.4f + m * 1.1f;
        float my = 2.6f - m * 0.7f;
        float mz = 5.4f - m * 1.1f;
        Vector2 base = project(mx, my, 0.8f);
        Vector2 top = project(mx, my, mz);
        DrawLineEx(base, top, 1.1f, mid);
        DrawCircle(static_cast<int>(top.x), static_cast<int>(top.y), 1.3f, c);
        for (int guy = -1; guy <= 1; guy += 2)
        {
            DrawLineEx(top, project(mx + guy * 0.8f, my + guy * 0.5f, 0.8f), 1.0f, dim);
        }
    }
}

// Control-panel blueprint art, chosen by unit type.
static void ExtDrawWireframeUnit(Rectangle area, Color c, const std::string& unitType)
{
    if (unitType == "Farming")           ExtDrawWireframeFarming(area, c);
    else if (unitType == "Energy")       ExtDrawWireframeEnergy(area, c);
    else if (unitType == "Manufacture")  ExtDrawWireframeManufacture(area, c);
    else if (unitType == "Research")     ExtDrawWireframeResearch(area, c);
    else if (unitType == "Construction") ExtDrawWireframeConstruction(area, c);
    else if (unitType == "Transport")    ExtDrawWireframeTransport(area, c);
    else if (unitType == "Core")         ExtDrawWireframeCore(area, c);
    else if (unitType == "Communication") ExtDrawWireframeCommunication(area, c);
    else                                 ExtDrawWireframeExtraction(area, c);
}

// ============================================================================

void RenderManager::DrawModularUnitView(Unit* unit, TimeManager& timeManager)
{
    // Full dark background
    DrawRectangle(0, 0, screenWidth, screenHeight, EXT_SCREEN_BG);

    DrawUnitTopBar(unit, timeManager);
    DrawUnitBottomBar(unit);
    DrawUnitModuleList(unit);
    DrawUnitModuleCenter(unit);
    DrawUnitControlPanel(unit);

    // Update message fade (done in unit since it owns the state)
    // The unit still handles UpdateMessage in its own Update/Draw cycle
}

void RenderManager::DrawUnitTopBar(Unit* unit, TimeManager& timeManager)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;

    DrawRectangle(0, 0, screenWidth, EXT_TOP_BAR_H, EXT_PANEL_BG);
    DrawLine(0, EXT_TOP_BAR_H, screenWidth, EXT_TOP_BAR_H, EXT_PANEL_BORDER);

    float midY = EXT_TOP_BAR_H / 2.0f;

    UnitIdentity identity = ExtUnitIdentity(unit->GetUnitType());

    // Unit icon chip
    Rectangle chip = {14.0f, midY - 16.0f, 32.0f, 32.0f};
    DrawRectangleRounded(chip, 0.3f, 4, EXT_PANEL_BG2);
    DrawRectangleRoundedLinesEx(chip, 0.3f, 4, 1.0f, EXT_PANEL_BORDER);
    ExtDrawIcon(identity.icon, chip.x + 16.0f, chip.y + 16.0f, 9.0f, identity.accent);

    // Unit title + status, measured so they never collide
    float titleSize = FS(18.0f);
    const char* title = identity.title;
    float titleX = chip.x + chip.width + 14.0f;
    Vector2 titleDim = MeasureTextEx(headerFont, title, titleSize, sp);
    DrawTextEx(headerFont, title, {titleX, midY - titleDim.y / 2.0f}, titleSize, sp, EXT_TEXT);

    bool isActive = unit->IsActive();
    Color statusColor = isActive ? identity.accent : EXT_ACCENT_RED;
    const char* statusText = isActive ? "ONLINE" : "OFFLINE";
    float statusSize = FS(12.0f);
    Vector2 statusDim = MeasureTextEx(bodyFont, statusText, statusSize, sp);
    DrawTextEx(bodyFont, statusText,
               {titleX + titleDim.x + 18.0f, midY - statusDim.y / 2.0f}, statusSize, sp, statusColor);

    // Right side: nav hint, day counter, menu icon
    ExtDrawIcon(ExtIcon::HAMBURGER, screenWidth - 26.0f, midY, 8.0f, EXT_DIM_TEXT);

    const char* dayText = TextFormat("DAY %d", timeManager.GetCurrentDay());
    float daySize = FS(14.0f);
    Vector2 dayDim = MeasureTextEx(headerFont, dayText, daySize, sp);
    float dayX = screenWidth - 52.0f - dayDim.x;
    DrawTextEx(headerFont, dayText, {dayX, midY - dayDim.y / 2.0f}, daySize, sp, EXT_TEXT);

    const char* hint = "Press S for Sect View";
    float hintSize = FS(11.0f);
    Vector2 hintDim = MeasureTextEx(bodyFont, hint, hintSize, sp);
    DrawTextEx(bodyFont, hint, {dayX - hintDim.x - 24.0f, midY - hintDim.y / 2.0f},
               hintSize, sp, Fade(EXT_DIM_TEXT, 0.7f));
}

void RenderManager::DrawUnitBottomBar(Unit* unit)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;

    // Region: [gap][status segments 60][gap][message bar 40][gap]
    float statusY = static_cast<float>(screenHeight - EXT_BOTTOM_BAR_H + 6);
    float statusH = 60.0f;
    float msgY = statusY + statusH + 6.0f;
    float msgH = 40.0f;
    float barX = static_cast<float>(EXT_GAP);
    float barW = static_cast<float>(screenWidth - EXT_GAP * 2);

    // --- Status segments ---
    ExtDrawPanelFrame({barX, statusY, barW, statusH});

    struct Segment
    {
        ExtIcon icon;
        const char* name;
        std::string value;
        std::string sub;
        Color subColor;
    };
    std::vector<Segment> segments;

    if (unit->HasProspectingSystem())
    {
        auto* ps = unit->GetProspectingSystem();
        CellSurveyResult sr = SurveyProgressEngine::Calculate(ps->GetGrid(), ps->GetTray());
        float calQ = ps->GetSweep().GetCalibrationQuality();
        bool marked = ps->IsMarkedSite();

        segments.push_back({ExtIcon::RADAR, "SWEEP",
                            TextFormat("%.0f%%", sr.sweepConfidence * 100.0f),
                            marked ? "MARKED SITE" : "UNMARKED",
                            marked ? EXT_ACCENT_GREEN : EXT_DIM_TEXT});
        segments.push_back({ExtIcon::FLASK, "SAMPLES",
                            TextFormat("%.0f%%", sr.sampleConfidence * 100.0f),
                            TextFormat("CAL: %.0f%%", calQ * 100.0f),
                            calQ >= 0.8f ? EXT_ACCENT_GREEN : EXT_ACCENT_GOLD});
        segments.push_back({ExtIcon::SLIDERS, "TESTING",
                            TextFormat("%.0f%%", sr.testingConfidence * 100.0f),
                            TextFormat("TIER %d", ps->GetTier()),
                            EXT_DIM_TEXT});
    }
    else
    {
        // Units without a bespoke subsystem show generic module telemetry, so the
        // status bar keeps the same three-segments-plus-energy shape.
        const auto& mods = unit->GetModules();
        int built = 0;
        int maxTier = 0;
        float totalOutput = 0.0f;
        for (const auto& m : mods)
        {
            if (!m.isBuilt) continue;
            built++;
            maxTier = std::max(maxTier, m.tier);
            if (!m.isActive) continue;
            for (const auto& [type, rate] : m.productionRates) totalOutput += rate;
        }
        int activeCount = static_cast<int>(unit->GetActiveModuleIndices().size());

        segments.push_back({ExtIcon::OVERVIEW, "MODULES",
                            TextFormat("%d / %d", built, static_cast<int>(mods.size())),
                            TextFormat("%d ACTIVE", activeCount),
                            activeCount > 0 ? EXT_ACCENT_GREEN : EXT_DIM_TEXT});
        segments.push_back({ExtIcon::NODES, "OUTPUT",
                            TextFormat("%.1f /s", totalOutput),
                            totalOutput > 0.0f ? "PRODUCING" : "IDLE",
                            totalOutput > 0.0f ? EXT_ACCENT_GREEN : EXT_ACCENT_GOLD});
        segments.push_back({ExtIcon::SLIDERS, "TIER",
                            TextFormat("%d / 3", maxTier),
                            "HIGHEST BUILT",
                            EXT_DIM_TEXT});
    }

    // Energy segment: selected module in module view, else total active draw
    float energy = 0.0f;
    const auto& modules = unit->GetModules();
    int selIdx = unit->GetSelectedModuleIndex();
    if (unit->IsInModuleView() && selIdx >= 0 && selIdx < static_cast<int>(modules.size()))
    {
        energy = modules[selIdx].energyRequired;
    }
    else
    {
        for (int i : unit->GetActiveModuleIndices()) energy += modules[i].energyRequired;
    }
    // Stored energy is the spendable resource for prospecting actions; the
    // module draw is shown underneath as context.
    float storedEnergy = unit->GetStoredResource(ResourceType::ENERGY);
    Color energyColor = storedEnergy < 100.0f ? EXT_ACCENT_RED
                                              : (storedEnergy < 300.0f ? EXT_ACCENT_GOLD
                                                                       : EXT_DIM_TEXT);
    segments.push_back({ExtIcon::BOLT, "ENERGY",
                        TextFormat("%.0f E", storedEnergy),
                        TextFormat("%.1f kW draw", energy), energyColor});

    float segW = barW / segments.size();
    for (size_t i = 0; i < segments.size(); i++)
    {
        const Segment& seg = segments[i];
        float segX = barX + i * segW;

        if (i > 0)
        {
            DrawLineEx({segX, statusY + 10.0f}, {segX, statusY + statusH - 10.0f},
                       1.0f, Fade(EXT_PANEL_BORDER, 0.8f));
        }

        ExtDrawIcon(seg.icon, segX + 28.0f, statusY + statusH / 2.0f, 10.0f, EXT_ACCENT_CYAN);

        float textX = segX + 50.0f;
        DrawTextEx(bodyFont, seg.name, {textX, statusY + 12.0f}, FS(11.0f), sp, EXT_DIM_TEXT);

        float valSize = FS(12.0f);
        Vector2 valDim = MeasureTextEx(headerFont, seg.value.c_str(), valSize, sp);
        DrawTextEx(headerFont, seg.value.c_str(),
                   {segX + segW - valDim.x - 22.0f, statusY + 11.0f}, valSize, sp, EXT_TEXT);

        if (!seg.sub.empty())
        {
            DrawTextEx(bodyFont, seg.sub.c_str(), {textX, statusY + 33.0f},
                       FS(10.0f), sp, seg.subColor);
        }
    }

    // --- Message bar ---
    ExtDrawPanelFrame({barX, msgY, barW, msgH});

    const UIMessage& msg = unit->GetCurrentMessage();
    if (msg.opacity > 0 && !msg.text.empty())
    {
        float alpha = msg.opacity;
        float textX = barX + 16.0f;
        float textY = msgY + msgH / 2.0f - FS(12.0f) / 2.0f - 2.0f;

        // "[TAG]" prefix rendered as a violet accent, body in plain text
        std::string text = msg.text;
        if (!text.empty() && text.front() == '[')
        {
            size_t close = text.find(']');
            if (close != std::string::npos)
            {
                std::string tag = text.substr(0, close + 1);
                DrawTextEx(headerFont, tag.c_str(), {textX, textY}, FS(12.0f), sp,
                           Fade(EXT_ACCENT_VIOLET, alpha));
                textX += MeasureTextEx(headerFont, tag.c_str(), FS(12.0f), sp).x + 10.0f;
                text = text.substr(close + 1);
                while (!text.empty() && text.front() == ' ') text.erase(text.begin());
            }
        }
        DrawTextEx(bodyFont, text.c_str(), {textX, textY}, FS(12.0f), sp, Fade(EXT_TEXT, alpha));
    }
}

// --- Shared Helpers ---

void RenderManager::DrawStyledBar(float x, float y, float w, float h, float value, Color fillColor)
{
    value = Clamp(value, 0.0f, 1.0f);
    Rectangle track = {x, y, w, h};
    DrawRectangleRounded(track, 0.5f, 3, {18, 26, 44, 255});
    if (w * value > 3.0f)
    {
        DrawRectangleRounded({x + 1.5f, y + 1.5f, w * value - 3.0f, h - 3.0f}, 0.5f, 3, fillColor);
    }
    DrawRectangleRoundedLinesEx(track, 0.5f, 3, 1.0f, EXT_PANEL_BORDER);
}

void RenderManager::DrawWearBar(float x, float y, float w, float h, float wear)
{
    Color wearColor;
    if (wear < 0.3f) wearColor = GREEN;
    else if (wear < 0.7f) wearColor = YELLOW;
    else wearColor = RED;
    DrawStyledBar(x, y, w, h, wear, wearColor);
}

void RenderManager::DrawTierIndicator(float x, float y, int tier, int maxTier)
{
    // Tier pips: cyan -> blue -> purple -> violet, like the kit's color dots
    Color tierColors[] = {
        {80, 225, 255, 255}, {80, 140, 255, 255}, {150, 110, 255, 255}, {200, 120, 255, 255}
    };
    for (int i = 0; i <= maxTier; i++)
    {
        float cx = x + i * 17.0f;
        float cy = y;
        if (i <= tier)
        {
            Color c = tierColors[std::min(i, 3)];
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 5, c);
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 6, Fade(c, 0.35f));
        }
        else
        {
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 5, Fade(EXT_DIM_TEXT, 0.7f));
        }
    }
}

// --- Left Panel: Module List ---

void RenderManager::DrawUnitModuleList(Unit* unit)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;

    int panelY = EXT_TOP_BAR_H;
    int panelH = screenHeight - EXT_TOP_BAR_H - EXT_BOTTOM_BAR_H;

    // Floating card
    Rectangle panel = {static_cast<float>(EXT_GAP), static_cast<float>(panelY + EXT_GAP),
                       static_cast<float>(EXT_LEFT_PANEL_W - EXT_GAP * 2),
                       static_cast<float>(panelH - EXT_GAP * 2)};
    ExtDrawPanelFrame(panel);

    float padding = 12.0f;
    float innerX = panel.x + padding;
    float innerW = panel.width - padding * 2;
    float yPos = panel.y + padding;

    // "UNIT OVERVIEW" button: icon + label + chevron
    Rectangle overviewBtn = {innerX, yPos, innerW, 44.0f};
    bool overviewHovered = CheckCollisionPointRec(GetMousePosition(), overviewBtn);
    bool overviewSelected = !unit->IsInModuleView();

    DrawRectangleRounded(overviewBtn, 0.2f, 4,
                         overviewSelected ? Color{16, 38, 54, 255}
                                          : (overviewHovered ? Color{16, 28, 46, 255} : EXT_PANEL_BG2));
    DrawRectangleRoundedLinesEx(overviewBtn, 0.2f, 4, overviewSelected ? 1.5f : 1.0f,
                                overviewSelected ? EXT_ACCENT_CYAN : EXT_PANEL_BORDER);

    ExtDrawIcon(ExtIcon::OVERVIEW, overviewBtn.x + 20.0f, overviewBtn.y + 22.0f, 9.0f,
                overviewSelected ? EXT_ACCENT_CYAN : EXT_DIM_TEXT);
    DrawTextEx(headerFont, "UNIT OVERVIEW", {overviewBtn.x + 40.0f, overviewBtn.y + 13.0f},
               FS(13.0f), sp, overviewSelected ? EXT_ACCENT_CYAN : Fade(EXT_TEXT, 0.85f));
    ExtDrawChevrons(overviewBtn.x + overviewBtn.width - 22.0f, overviewBtn.y + 22.0f, 5.0f,
                    Fade(EXT_DIM_TEXT, 0.8f));

    if (overviewHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        unit->SetIsInModuleView(false);
        unit->SetShowingStats(false);
        unit->PublicShowMessage("Switched to unit overview");
    }

    yPos += 60.0f;

    // Section label
    DrawTextEx(bodyFont, "MODULES", {innerX, yPos}, FS(11.0f), sp, EXT_DIM_TEXT);
    yPos += 24.0f;

    // Module cards
    const auto& modules = unit->GetModules();
    int selectedIdx = unit->GetSelectedModuleIndex();

    for (size_t i = 0; i < modules.size(); i++)
    {
        const auto& mod = modules[i];

        Rectangle btn = {innerX, yPos, innerW, 62.0f};
        bool isHovered = CheckCollisionPointRec(GetMousePosition(), btn);
        bool isSelected = unit->IsInModuleView() && selectedIdx == static_cast<int>(i);

        Color btnBg;
        if (isSelected) btnBg = {16, 38, 54, 255};
        else if (isHovered) btnBg = {16, 28, 46, 255};
        else if (!mod.isBuilt) btnBg = {11, 16, 30, 255};
        else btnBg = EXT_PANEL_BG2;

        DrawRectangleRounded(btn, 0.15f, 4, btnBg);
        DrawRectangleRoundedLinesEx(btn, 0.15f, 4, isSelected ? 1.5f : 1.0f,
                                    isSelected ? EXT_ACCENT_CYAN
                                               : Fade(EXT_PANEL_BORDER, mod.isBuilt ? 1.0f : 0.6f));

        // Icon
        Color iconColor = !mod.isBuilt ? Fade(EXT_DIM_TEXT, 0.7f)
                                       : (isSelected ? EXT_ACCENT_CYAN : Fade(EXT_TEXT, 0.75f));
        ExtDrawIcon(ExtModuleIcon(mod.moduleType), btn.x + 22.0f, btn.y + 31.0f, 10.0f, iconColor);

        // Name (uppercase per the kit)
        std::string nameUpper = mod.name;
        for (auto& ch : nameUpper) ch = static_cast<char>(toupper(ch));
        Color nameColor = mod.isBuilt ? EXT_TEXT : EXT_DIM_TEXT;
        DrawTextEx(headerFont, nameUpper.c_str(), {btn.x + 44.0f, btn.y + 12.0f},
                   FS(12.0f), sp, nameColor);

        // Tier pips
        DrawTierIndicator(btn.x + 50.0f, btn.y + 43.0f, mod.isBuilt ? mod.tier : -1);

        // Status text
        const char* statusText = !mod.isBuilt ? "NOT BUILT" : (mod.isActive ? "ACTIVE" : "INACTIVE");
        Color statusColor = !mod.isBuilt ? Fade(EXT_DIM_TEXT, 0.8f)
                                         : (mod.isActive ? EXT_ACCENT_GREEN : EXT_ACCENT_GOLD);
        float statusWidth = MeasureTextEx(bodyFont, statusText, FS(10.0f), sp).x;
        DrawTextEx(bodyFont, statusText,
                   {btn.x + btn.width - statusWidth - 12.0f, btn.y + 38.0f}, FS(10.0f), sp, statusColor);

        if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            unit->SetSelectedModuleIndex(static_cast<int>(i));
            unit->SetIsInModuleView(true);
            unit->SetShowingStats(false);
            unit->PublicShowMessage("Viewing " + mod.name);
        }

        yPos += 68.0f;
    }

    // Decorative hazard strip at the card's bottom-left, per the kit
    ExtDrawHazardStripes({panel.x + 8.0f, panel.y + panel.height - 14.0f, 70.0f, 6.0f},
                         Fade(EXT_DIM_TEXT, 0.25f));
}

// --- Center Panel Router ---

void RenderManager::DrawUnitModuleCenter(Unit* unit)
{
    int panelX = EXT_LEFT_PANEL_W;
    int panelY = EXT_TOP_BAR_H;
    int panelW = screenWidth - EXT_LEFT_PANEL_W - EXT_RIGHT_PANEL_W;
    int panelH = screenHeight - EXT_TOP_BAR_H - EXT_BOTTOM_BAR_H;

    // Shared floating card behind every center panel
    ExtDrawPanelFrame({static_cast<float>(panelX + EXT_GAP), static_cast<float>(panelY + EXT_GAP),
                       static_cast<float>(panelW - EXT_GAP * 2),
                       static_cast<float>(panelH - EXT_GAP * 2)});

    if (!unit->IsInModuleView())
    {
        DrawUnitResourceOverview(unit, panelX, panelY, panelW, panelH);
        return;
    }

    int idx = unit->GetSelectedModuleIndex();
    const auto& modules = unit->GetModules();
    if (idx < 0 || idx >= static_cast<int>(modules.size())) return;

    const std::string& moduleType = modules[idx].moduleType;

    if (moduleType == "PROSPECTING")
        DrawProspectingPanel(unit, panelX, panelY, panelW, panelH);
    else if (moduleType == "EXCAVATION")
        DrawExcavationPanel(unit, panelX, panelY, panelW, panelH);
    else if (moduleType == "BENEFICIATION")
        DrawBeneficiationPanel(unit, panelX, panelY, panelW, panelH);
    else if (moduleType == "OPERATIONS")
        DrawOperationsPanel(unit, panelX, panelY, panelW, panelH);
    else if (moduleType == "DIRECTIVES")
        DrawDirectivesPanel(unit, panelX, panelY, panelW, panelH);
    else
        DrawGenericModulePanel(unit, panelX, panelY, panelW, panelH);
}

// --- Generic Center Panel ---
//
// Stands in for modules that have no bespoke layout yet (every Farming, Energy,
// Manufacture, and Research module). It renders the module's real data -- tier
// arc, throughput, energy, dependencies -- so the module is inspectable rather
// than merely present, and states plainly that the mechanic is not built.

void RenderManager::DrawGenericModulePanel(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;

    int idx = unit->GetSelectedModuleIndex();
    const auto& modules = unit->GetModules();
    if (idx < 0 || idx >= static_cast<int>(modules.size())) return;
    const auto& mod = modules[idx];

    UnitIdentity identity = ExtUnitIdentity(unit->GetUnitType());

    float padding = 22.0f;
    float px = static_cast<float>(x) + EXT_GAP + padding;
    float innerW = static_cast<float>(w) - (EXT_GAP + padding) * 2.0f;
    float yPos = static_cast<float>(y) + EXT_GAP + padding;

    // --- Header: icon, name, status pill ---
    Rectangle chip = {px, yPos, 44.0f, 44.0f};
    DrawRectangleRounded(chip, 0.25f, 4, EXT_PANEL_BG2);
    DrawRectangleRoundedLinesEx(chip, 0.25f, 4, 1.0f, Fade(identity.accent, 0.5f));
    ExtDrawIcon(ExtModuleIcon(mod.moduleType), chip.x + 22.0f, chip.y + 22.0f, 13.0f,
                mod.isBuilt ? identity.accent : EXT_DIM_TEXT);

    std::string nameUpper = mod.name;
    for (auto& ch : nameUpper) ch = static_cast<char>(toupper(ch));
    DrawTextEx(headerFont, nameUpper.c_str(), {chip.x + 58.0f, yPos + 4.0f},
               FS(20.0f), sp, EXT_TEXT);
    DrawTextEx(bodyFont, TextFormat("%s  /  %s MODULE",
                                    mod.moduleType.c_str(), unit->GetUnitType().c_str()),
               {chip.x + 58.0f, yPos + 28.0f}, FS(11.0f), sp, EXT_DIM_TEXT);

    const char* stateText = !mod.isBuilt ? "NOT BUILT" : (mod.isActive ? "ACTIVE" : "STANDBY");
    Color stateColor = !mod.isBuilt ? EXT_DIM_TEXT
                                    : (mod.isActive ? EXT_ACCENT_GREEN : EXT_ACCENT_GOLD);
    float stateW = MeasureTextEx(bodyFont, stateText, FS(11.0f), sp).x;
    Rectangle pill = {px + innerW - stateW - 26.0f, yPos + 10.0f, stateW + 22.0f, 24.0f};
    DrawRectangleRounded(pill, 0.5f, 4, Fade(stateColor, 0.12f));
    DrawRectangleRoundedLinesEx(pill, 0.5f, 4, 1.0f, Fade(stateColor, 0.7f));
    DrawTextEx(bodyFont, stateText, {pill.x + 11.0f, pill.y + 6.0f}, FS(11.0f), sp, stateColor);

    yPos += 60.0f;
    DrawLine(static_cast<int>(px), static_cast<int>(yPos),
             static_cast<int>(px + innerW), static_cast<int>(yPos), EXT_PANEL_BORDER);
    yPos += 18.0f;

    // --- Description ---
    if (!mod.description.empty())
    {
        std::string line;
        for (size_t i = 0; i <= mod.description.size(); i++)
        {
            if (i == mod.description.size() || mod.description[i] == '\n')
            {
                DrawTextEx(bodyFont, line.c_str(), {px, yPos}, FS(13.0f), sp, Fade(EXT_TEXT, 0.8f));
                yPos += 20.0f;
                line.clear();
            }
            else
            {
                line += mod.description[i];
            }
        }
        yPos += 10.0f;
    }

    // --- Tier arc ---
    DrawTextEx(headerFont, "TIER PROGRESSION", {px, yPos}, FS(13.0f), sp, identity.accent);
    yPos += 24.0f;

    float stepW = innerW / 4.0f;
    for (int t = 0; t <= 3; t++)
    {
        float sx = px + t * stepW;
        bool reached = mod.isBuilt && t <= mod.tier;
        bool current = mod.isBuilt && t == mod.tier;

        Rectangle step = {sx, yPos, stepW - 10.0f, 46.0f};
        DrawRectangleRounded(step, 0.18f, 4, current ? Color{16, 38, 54, 255} : EXT_PANEL_BG2);
        DrawRectangleRoundedLinesEx(step, 0.18f, 4, current ? 1.5f : 1.0f,
                                    current ? identity.accent
                                            : Fade(EXT_PANEL_BORDER, reached ? 1.0f : 0.5f));

        Color labelColor = reached ? EXT_TEXT : Fade(EXT_DIM_TEXT, 0.8f);
        DrawTextEx(headerFont, TextFormat("TIER %d", t), {step.x + 12.0f, step.y + 9.0f},
                   FS(12.0f), sp, labelColor);
        DrawTextEx(bodyFont, current ? "CURRENT" : (reached ? "UNLOCKED" : "LOCKED"),
                   {step.x + 12.0f, step.y + 27.0f}, FS(10.0f), sp,
                   current ? identity.accent : Fade(EXT_DIM_TEXT, reached ? 0.9f : 0.6f));

        if (t < 3)
        {
            // Sits in the 10px gutter between steps; nudged left so both
            // chevrons stay clear of the next step's border.
            ExtDrawChevrons(sx + stepW - 8.0f, yPos + 23.0f, 3.5f,
                            Fade(reached ? identity.accent : EXT_DIM_TEXT, reached ? 0.8f : 0.45f));
        }
    }
    yPos += 62.0f;

    // --- Throughput: production and consumption side by side ---
    DrawTextEx(headerFont, "THROUGHPUT", {px, yPos}, FS(13.0f), sp, identity.accent);
    yPos += 24.0f;

    float colW = innerW / 2.0f - 8.0f;
    float colTop = yPos;

    struct RateColumn
    {
        const char* title;
        const std::map<ResourceType, float>* rates;
        Color color;
        char sign;
    };
    RateColumn columns[2] = {
        {"PRODUCES", &mod.productionRates, EXT_ACCENT_GREEN, '+'},
        {"CONSUMES", &mod.consumptionRates, EXT_ACCENT_RED, '-'}
    };

    float maxColBottom = colTop;
    for (int col = 0; col < 2; col++)
    {
        float cx2 = px + col * (colW + 16.0f);
        float cy2 = colTop;

        DrawTextEx(bodyFont, columns[col].title, {cx2, cy2}, FS(11.0f), sp, EXT_DIM_TEXT);
        cy2 += 20.0f;

        bool any = false;
        for (const auto& [type, rate] : *columns[col].rates)
        {
            if (rate <= 0.0f) continue;
            any = true;
            std::string resName = ResourceUtils::GetResourceName(type);
            DrawCircle(static_cast<int>(cx2 + 4.0f), static_cast<int>(cy2 + 7.0f), 3.0f,
                       columns[col].color);
            DrawTextEx(bodyFont, resName.c_str(), {cx2 + 14.0f, cy2}, FS(12.0f), sp, EXT_TEXT);
            const char* rateText = TextFormat("%c%.2f/s", columns[col].sign, rate);
            float rw = MeasureTextEx(bodyFont, rateText, FS(12.0f), sp).x;
            DrawTextEx(bodyFont, rateText, {cx2 + colW - rw, cy2}, FS(12.0f), sp,
                       columns[col].color);
            cy2 += 19.0f;
        }
        if (!any)
        {
            DrawTextEx(bodyFont, "None", {cx2 + 14.0f, cy2}, FS(12.0f), sp,
                       Fade(EXT_DIM_TEXT, 0.8f));
            cy2 += 19.0f;
        }
        maxColBottom = std::max(maxColBottom, cy2);
    }
    yPos = maxColBottom + 14.0f;

    // --- Efficiency and energy meters ---
    DrawTextEx(bodyFont, "EFFICIENCY", {px, yPos}, FS(11.0f), sp, EXT_DIM_TEXT);
    DrawTextEx(bodyFont, TextFormat("%.0f%%", mod.efficiency * 100.0f),
               {px + colW - 44.0f, yPos}, FS(11.0f), sp, EXT_TEXT);
    ExtDrawSegBar(px, yPos + 18.0f, colW, 12.0f, Clamp(mod.efficiency, 0.0f, 1.0f),
                  identity.accent);

    float ex = px + colW + 16.0f;
    float storedEnergy = unit->GetStoredResource(ResourceType::ENERGY);
    DrawTextEx(bodyFont, "ENERGY DRAW", {ex, yPos}, FS(11.0f), sp, EXT_DIM_TEXT);
    DrawTextEx(bodyFont, TextFormat("%.1f kW", mod.energyRequired),
               {ex + colW - 54.0f, yPos}, FS(11.0f), sp, EXT_TEXT);
    // Drawn against a nominal 20 kW ceiling purely as a visual reference; the
    // real per-tier energy budget arrives with each module's own design.
    ExtDrawSegBar(ex, yPos + 18.0f, colW, 12.0f,
                  Clamp(mod.energyRequired / 20.0f, 0.0f, 1.0f), EXT_ACCENT_GOLD);
    DrawTextEx(bodyFont, TextFormat("Unit store: %.0f E", storedEnergy),
               {ex, yPos + 36.0f}, FS(10.0f), sp, Fade(EXT_DIM_TEXT, 0.9f));

    yPos += 62.0f;

    // --- Required tech ---
    if (!mod.tierDependencies.empty())
    {
        DrawTextEx(bodyFont, "REQUIRED TECH", {px, yPos}, FS(11.0f), sp, EXT_DIM_TEXT);
        yPos += 20.0f;
        float chipX = px;
        for (const auto& dep : mod.tierDependencies)
        {
            float dw = MeasureTextEx(bodyFont, dep.c_str(), FS(11.0f), sp).x;
            if (chipX + dw + 22.0f > px + innerW)
            {
                chipX = px;
                yPos += 28.0f;
            }
            Rectangle tag = {chipX, yPos, dw + 18.0f, 22.0f};
            DrawRectangleRounded(tag, 0.5f, 4, EXT_PANEL_BG2);
            DrawRectangleRoundedLinesEx(tag, 0.5f, 4, 1.0f, Fade(EXT_ACCENT_VIOLET, 0.6f));
            DrawTextEx(bodyFont, dep.c_str(), {tag.x + 9.0f, tag.y + 5.0f}, FS(11.0f), sp,
                       Fade(EXT_TEXT, 0.85f));
            chipX += tag.width + 8.0f;
        }
        yPos += 34.0f;
    }

    // --- Stub notice, pinned to the bottom of the card ---
    float cardBottom = static_cast<float>(y + h) - EXT_GAP - padding;
    Rectangle notice = {px, cardBottom - 46.0f, innerW, 46.0f};
    if (notice.y > yPos)
    {
        DrawRectangleRounded(notice, 0.15f, 4, Color{26, 20, 8, 255});
        DrawRectangleRoundedLinesEx(notice, 0.15f, 4, 1.0f, Fade(EXT_ACCENT_GOLD, 0.55f));
        ExtDrawIcon(ExtIcon::WARNING, notice.x + 24.0f, notice.y + 23.0f, 9.0f, EXT_ACCENT_GOLD);
        DrawTextEx(headerFont, "PRELIMINARY MODULE",
                   {notice.x + 44.0f, notice.y + 8.0f}, FS(12.0f), sp, EXT_ACCENT_GOLD);
        DrawTextEx(bodyFont, "Build, tier, and activation work. Bespoke mechanics not designed yet.",
                   {notice.x + 44.0f, notice.y + 26.0f}, FS(11.0f), sp, Fade(EXT_DIM_TEXT, 0.95f));
    }
}

// --- Right Panel: Controls ---

void RenderManager::DrawUnitControlPanel(Unit* unit)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;

    int panelX = screenWidth - EXT_RIGHT_PANEL_W;
    int panelY = EXT_TOP_BAR_H;
    int panelH = screenHeight - EXT_TOP_BAR_H - EXT_BOTTOM_BAR_H;
    int padding = EXT_GAP + 12;   // card inset + inner padding, keeps x math below simple

    // Floating card
    Rectangle panel = {static_cast<float>(panelX + EXT_GAP), static_cast<float>(panelY + EXT_GAP),
                       static_cast<float>(EXT_RIGHT_PANEL_W - EXT_GAP * 2),
                       static_cast<float>(panelH - EXT_GAP * 2)};
    ExtDrawPanelFrame(panel);

    float yPos = panel.y + 14.0f;

    if (!unit->IsInModuleView())
    {
        // Unit overview mode - production rate controls
        DrawTextEx(headerFont, "PRODUCTION CONTROLS", {static_cast<float>(panelX + padding), yPos},
                   FS(14.0f), sp, EXT_HEADER_COLOR);
        yPos += 34.0f;

        const auto& modules = unit->GetModules();
        const auto& activeIndices = unit->GetActiveModuleIndices();

        if (activeIndices.empty())
        {
            DrawTextEx(bodyFont, "No active modules",
                       {static_cast<float>(panelX + padding), yPos}, FS(14.0f), sp, EXT_DIM_TEXT);
        }
        return;
    }

    int idx = unit->GetSelectedModuleIndex();
    const auto& modules = unit->GetModules();
    if (idx < 0 || idx >= static_cast<int>(modules.size())) return;
    const auto& mod = modules[idx];

    DrawTextEx(headerFont, "CONTROL PANEL", {static_cast<float>(panelX + padding), yPos},
               FS(14.0f), sp, EXT_HEADER_COLOR);
    yPos += 34.0f;

    float btnW = static_cast<float>(EXT_RIGHT_PANEL_W - padding * 2);
    float btnH = 40.0f;

    // Build button (if not built)
    if (!mod.isBuilt)
    {
        Rectangle buildBtn = {static_cast<float>(panelX + padding), yPos, btnW, btnH};
        bool canBuild = unit->PublicCanBuildModule(idx);
        bool isHovered = CheckCollisionPointRec(GetMousePosition(), buildBtn);

        Color btnColor = canBuild ? Color{14, 40, 70, 255} : Color{16, 22, 38, 255};
        if (isHovered && canBuild) btnColor = Color{20, 56, 96, 255};

        DrawRectangleRounded(buildBtn, 0.25f, 4, btnColor);
        DrawRectangleRoundedLinesEx(buildBtn, 0.25f, 4, 1.0f,
                                    canBuild ? EXT_ACCENT_CYAN : Fade(EXT_DIM_TEXT, 0.6f));

        const char* buildText = "BUILD MODULE";
        float textW = MeasureTextEx(headerFont, buildText, FS(16.0f), sp).x;
        DrawTextEx(headerFont, buildText,
                   {buildBtn.x + (btnW - textW) / 2.0f, buildBtn.y + 12.0f}, FS(16.0f), sp,
                   canBuild ? WHITE : EXT_DIM_TEXT);

        if (isHovered && canBuild && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            unit->PublicBuildModule(idx);
        }

        yPos += btnH + 10.0f;

        // Show build costs
        auto costIter = mod.upgradeCosts.find(1);
        if (costIter != mod.upgradeCosts.end())
        {
            DrawTextEx(bodyFont, "Build Cost:", {static_cast<float>(panelX + padding), yPos},
                       FS(13.0f), sp, EXT_DIM_TEXT);
            yPos += 18.0f;

            const auto& storage = unit->GetResourceStorage();
            for (const auto& [resource, amount] : costIter->second)
            {
                std::string resName = ResourceUtils::GetResourceName(resource);
                float stored = 0.0f;
                auto it = storage.find(resource);
                if (it != storage.end()) stored = it->second;

                Color costColor = (stored >= amount) ? EXT_ACCENT_GREEN : Color{255, 100, 100, 255};
                DrawTextEx(bodyFont, TextFormat("  %s: %.0f / %.0f", resName.c_str(), stored, amount),
                           {static_cast<float>(panelX + padding), yPos}, FS(12.0f), sp, costColor);
                yPos += 16.0f;
            }
        }
        return;
    }

    // Tier upgrade button
    if (mod.tier < 3)
    {
        Rectangle upgradeBtn = {static_cast<float>(panelX + padding), yPos, btnW, btnH};
        bool canUpgrade = unit->PublicCanUpgradeModule(idx);
        bool isHovered = CheckCollisionPointRec(GetMousePosition(), upgradeBtn);

        Color btnColor = canUpgrade ? Color{14, 40, 70, 255} : Color{16, 22, 38, 255};
        if (isHovered && canUpgrade) btnColor = Color{20, 56, 96, 255};

        DrawRectangleRounded(upgradeBtn, 0.25f, 4, btnColor);
        DrawRectangleRoundedLinesEx(upgradeBtn, 0.25f, 4, 1.0f,
                                    canUpgrade ? EXT_ACCENT_CYAN : Fade(EXT_DIM_TEXT, 0.6f));

        const char* upgradeText = TextFormat("UPGRADE TO TIER %d", mod.tier + 1);
        float textW = MeasureTextEx(headerFont, upgradeText, FS(13.0f), sp).x;
        DrawTextEx(headerFont, upgradeText,
                   {upgradeBtn.x + (btnW - textW) / 2.0f - 8.0f, upgradeBtn.y + 12.0f}, FS(13.0f), sp,
                   canUpgrade ? WHITE : EXT_DIM_TEXT);
        ExtDrawChevrons(upgradeBtn.x + (btnW + textW) / 2.0f + 6.0f, upgradeBtn.y + btnH / 2.0f,
                        5.0f, canUpgrade ? EXT_ACCENT_CYAN : Fade(EXT_DIM_TEXT, 0.6f));

        if (isHovered && canUpgrade && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            unit->UpgradeModuleTier(idx);
        }

        yPos += btnH + 10.0f;

        // Show upgrade costs
        auto costIter = mod.upgradeCosts.find(mod.tier + 1);
        if (costIter != mod.upgradeCosts.end())
        {
            DrawTextEx(bodyFont, "Upgrade Cost:", {static_cast<float>(panelX + padding), yPos},
                       FS(13.0f), sp, EXT_DIM_TEXT);
            yPos += 18.0f;

            const auto& storage = unit->GetResourceStorage();
            for (const auto& [resource, amount] : costIter->second)
            {
                std::string resName = ResourceUtils::GetResourceName(resource);
                float stored = 0.0f;
                auto it = storage.find(resource);
                if (it != storage.end()) stored = it->second;

                Color costColor = (stored >= amount) ? EXT_ACCENT_GREEN : Color{255, 100, 100, 255};
                DrawTextEx(bodyFont, TextFormat("  %s: %.0f / %.0f", resName.c_str(), stored, amount),
                           {static_cast<float>(panelX + padding), yPos}, FS(12.0f), sp, costColor);
                yPos += 16.0f;
            }
        }
    }
    else
    {
        ExtDrawIcon(ExtIcon::WARNING, panelX + padding + 8.0f, yPos + 8.0f, 8.0f, EXT_ACCENT_GOLD);
        DrawTextEx(headerFont, "MAX TIER REACHED",
                   {static_cast<float>(panelX + padding + 26), yPos}, FS(13.0f), sp, EXT_ACCENT_GOLD);
        yPos += 30.0f;
    }

    yPos += 12.0f;

    // Activate/Deactivate toggle: hazard-striped danger/confirm button
    Rectangle toggleBtn = {static_cast<float>(panelX + padding), yPos, btnW, btnH};
    bool isHovered = CheckCollisionPointRec(GetMousePosition(), toggleBtn);

    bool danger = mod.isActive;
    Color fillCol = danger ? Color{52, 12, 16, 255} : Color{12, 44, 26, 255};
    if (isHovered) fillCol = danger ? Color{72, 16, 22, 255} : Color{16, 58, 34, 255};
    Color edgeCol = danger ? EXT_ACCENT_RED : EXT_ACCENT_GREEN;

    DrawRectangleRounded(toggleBtn, 0.25f, 4, fillCol);
    ExtDrawHazardStripes({toggleBtn.x + 3.0f, toggleBtn.y + 3.0f, 26.0f, btnH - 6.0f},
                         Fade(edgeCol, 0.22f));
    ExtDrawHazardStripes({toggleBtn.x + btnW - 29.0f, toggleBtn.y + 3.0f, 26.0f, btnH - 6.0f},
                         Fade(edgeCol, 0.22f));
    DrawRectangleRoundedLinesEx(toggleBtn, 0.25f, 4, 1.5f, edgeCol);

    const char* toggleText = mod.isActive ? "DEACTIVATE" : "ACTIVATE";
    float textW = MeasureTextEx(headerFont, toggleText, FS(15.0f), sp).x;
    DrawTextEx(headerFont, toggleText,
               {toggleBtn.x + (btnW - textW) / 2.0f, toggleBtn.y + 12.0f}, FS(15.0f), sp, WHITE);

    if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        unit->PublicHandleModuleActivation(idx);
    }

    yPos += btnH + 20.0f;

    // Module status section
    DrawTextEx(headerFont, "MODULE STATUS", {static_cast<float>(panelX + padding), yPos},
               FS(13.0f), sp, EXT_ACCENT_CYAN);
    yPos += 24.0f;

    DrawTextEx(bodyFont, TextFormat("Tier: %d / 3", mod.tier),
               {static_cast<float>(panelX + padding), yPos}, FS(12.0f), sp, EXT_TEXT);
    yPos += 20.0f;

    DrawTextEx(bodyFont, TextFormat("Efficiency: %.0f%%", mod.efficiency * 100.0f),
               {static_cast<float>(panelX + padding), yPos}, FS(12.0f), sp, EXT_TEXT);
    yPos += 20.0f;

    if (mod.energyRequired > 0)
    {
        DrawTextEx(bodyFont, TextFormat("Energy: %.1f kW", mod.energyRequired),
                   {static_cast<float>(panelX + padding), yPos}, FS(12.0f), sp, EXT_TEXT);
        yPos += 20.0f;
    }

    // Tier dependencies
    if (!mod.tierDependencies.empty())
    {
        yPos += 10.0f;
        DrawTextEx(bodyFont, "Required Tech:",
                   {static_cast<float>(panelX + padding), yPos}, FS(12.0f), sp, EXT_DIM_TEXT);
        yPos += 16.0f;
        for (const auto& dep : mod.tierDependencies)
        {
            DrawTextEx(bodyFont, TextFormat("  - %s", dep.c_str()),
                       {static_cast<float>(panelX + padding), yPos}, FS(11.0f), sp, EXT_DIM_TEXT);
            yPos += 14.0f;
        }
    }

    // Decorative blueprint of the unit fills the remaining space
    float artTop = yPos + 10.0f;
    float artBottom = panel.y + panel.height - 48.0f;
    if (artBottom - artTop > 110.0f)
    {
        ExtDrawWireframeUnit({panel.x + 14.0f, artTop, panel.width - 28.0f, artBottom - artTop},
                             ExtUnitIdentity(unit->GetUnitType()).accent, unit->GetUnitType());

        float tagY = panel.y + panel.height - 40.0f;
        DrawTextEx(bodyFont, TextFormat("// %s //", ExtUnitBlueprintTag(unit->GetUnitType())),
                   {panel.x + 18.0f, tagY}, FS(10.0f), sp, Fade(EXT_DIM_TEXT, 0.8f));

        // Barcode strip
        float bx = panel.x + 18.0f;
        float by = tagY + 18.0f;
        for (int i = 0; i < 46; i++)
        {
            float lineW = ((i * 37) % 3 == 0) ? 2.0f : 1.0f;
            DrawRectangleRec({bx, by, lineW, 9.0f},
                             Fade(EXT_DIM_TEXT, ((i * 23) % 4 == 0) ? 0.85f : 0.4f));
            bx += lineW + 2.0f;
        }
    }
}

// ============================================================================
// CENTER PANELS (Module-Specific)
// ============================================================================

void RenderManager::DrawUnitResourceOverview(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;
    int padding = 15;

    float yPos = static_cast<float>(y + padding);
    float px = static_cast<float>(x + padding);

    DrawTextEx(headerFont, "RESOURCE OVERVIEW", {px, yPos}, FS(18.0f), sp, EXT_HEADER_COLOR);
    yPos += 30.0f;

    // Aggregate production/consumption from active modules
    const auto& modules = unit->GetModules();
    const auto& activeIndices = unit->GetActiveModuleIndices();

    std::map<ResourceType, float> totalProduction;
    std::map<ResourceType, float> totalConsumption;

    for (int moduleIndex : activeIndices)
    {
        const auto& mod = modules[moduleIndex];
        for (const auto& [type, rate] : mod.productionRates)
            totalProduction[type] += rate;
        for (const auto& [type, rate] : mod.consumptionRates)
            totalConsumption[type] += rate;
    }

    // Status line
    DrawTextEx(bodyFont, TextFormat("Active Modules: %d", static_cast<int>(activeIndices.size())),
               {px, yPos}, FS(14.0f), sp, LIGHTGRAY);
    yPos += 20.0f;

    const char* statusText = unit->IsActive() ? "ACTIVE" : "IDLE";
    Color statusColor = unit->IsActive() ? EXT_ACCENT_GREEN : YELLOW;
    DrawTextEx(bodyFont, TextFormat("Status: %s", statusText), {px, yPos}, FS(14.0f), sp, statusColor);
    yPos += 30.0f;

    // Resource rates table
    struct ResourceInfo {
        ResourceType type;
        const char* name;
    };
    ResourceInfo resources[] = {
        {ResourceType::ENERGY, "Energy"}, {ResourceType::H2, "Hydrogen"},
        {ResourceType::O2, "Oxygen"}, {ResourceType::C, "Carbon"},
        {ResourceType::Fe, "Iron"}, {ResourceType::Si, "Silicon"},
        {ResourceType::Ti, "Titanium"}, {ResourceType::Al, "Aluminum"},
        {ResourceType::Ca, "Calcium"}, {ResourceType::WATER, "Water"},
        {ResourceType::FOOD, "Food"}, {ResourceType::SCIENCE, "Science"},
        {ResourceType::MANPOWER, "Manpower"}
    };

    // Column headers
    DrawTextEx(bodyFont, "Resource", {px, yPos}, FS(13.0f), sp, EXT_DIM_TEXT);
    DrawTextEx(bodyFont, "Production", {px + 120.0f, yPos}, FS(13.0f), sp, EXT_DIM_TEXT);
    DrawTextEx(bodyFont, "Consumption", {px + 240.0f, yPos}, FS(13.0f), sp, EXT_DIM_TEXT);
    yPos += 20.0f;
    DrawLine(static_cast<int>(px), static_cast<int>(yPos),
             static_cast<int>(px + w - padding * 2), static_cast<int>(yPos), EXT_PANEL_BORDER);
    yPos += 5.0f;

    for (const auto& res : resources)
    {
        float prod = totalProduction[res.type];
        float cons = totalConsumption[res.type];
        if (prod <= 0 && cons <= 0) continue;

        DrawTextEx(bodyFont, res.name, {px, yPos}, FS(13.0f), sp, LIGHTGRAY);

        if (prod > 0)
            DrawTextEx(bodyFont, TextFormat("+%.2f/s", prod), {px + 120.0f, yPos}, FS(13.0f), sp, EXT_ACCENT_GREEN);
        else
            DrawTextEx(bodyFont, "-", {px + 120.0f, yPos}, FS(13.0f), sp, EXT_DIM_TEXT);

        if (cons > 0)
            DrawTextEx(bodyFont, TextFormat("-%.2f/s", cons), {px + 240.0f, yPos}, FS(13.0f), sp, Color{255, 100, 100, 255});
        else
            DrawTextEx(bodyFont, "-", {px + 240.0f, yPos}, FS(13.0f), sp, EXT_DIM_TEXT);

        yPos += 18.0f;
    }

    // Storage section
    yPos += 20.0f;
    DrawTextEx(headerFont, "STORAGE", {px, yPos}, FS(16.0f), sp, EXT_HEADER_COLOR);
    yPos += 25.0f;

    const auto& storage = unit->GetResourceStorage();
    const auto& capacity = unit->GetStorageCapacity();
    const auto& overflow = unit->GetOverflowBuffer();
    float barW = static_cast<float>(w - padding * 2 - 140);

    bool hasStorage = false;
    for (const auto& res : resources)
    {
        float stored = 0.0f;
        float cap = 0.0f;
        float buffered = 0.0f;
        auto sIt = storage.find(res.type);
        auto cIt = capacity.find(res.type);
        auto oIt = overflow.find(res.type);
        if (sIt != storage.end()) stored = sIt->second;
        if (cIt != capacity.end()) cap = cIt->second;
        if (oIt != overflow.end()) buffered = oIt->second;

        if (cap <= 0 && stored <= 0) continue;
        hasStorage = true;

        DrawTextEx(bodyFont, res.name, {px, yPos + 2.0f}, FS(12.0f), sp, LIGHTGRAY);

        float fillFraction = cap > 0 ? stored / cap : 0.0f;
        Color barColor;
        if (fillFraction > 0.9f) barColor = Color{255, 100, 100, 255};
        else if (fillFraction > 0.7f) barColor = YELLOW;
        else barColor = EXT_ACCENT_CYAN;

        DrawStyledBar(px + 90.0f, yPos, barW, 16.0f, fillFraction, barColor);

        // Show overflow indicator if buffer has content
        if (buffered > 0.0f)
        {
            DrawTextEx(bodyFont, TextFormat("%.0f/%.0f +%.0f", stored, cap, buffered),
                       {px + 90.0f + barW + 5.0f, yPos + 1.0f}, FS(11.0f), sp, Color{255, 180, 100, 255});
        }
        else
        {
            DrawTextEx(bodyFont, TextFormat("%.0f/%.0f", stored, cap),
                       {px + 90.0f + barW + 5.0f, yPos + 1.0f}, FS(11.0f), sp, LIGHTGRAY);
        }

        yPos += 22.0f;
    }

    if (!hasStorage)
    {
        DrawTextEx(bodyFont, "Storage is empty", {px, yPos}, FS(13.0f), sp, EXT_DIM_TEXT);
    }
}

// --- Prospecting UI color constants ---
static const Color PROS_CELL_BG         = {26, 26, 46, 255};
static const Color PROS_CELL_BORDER     = {51, 51, 85, 255};
static const Color PROS_HOVER_BORDER    = {102, 255, 255, 255};
static const Color PROS_SELECT_BORDER   = {102, 255, 255, 255};
static const Color PROS_TAB_ACTIVE      = {102, 255, 255, 60};
static const Color PROS_TAB_BORDER      = {51, 102, 119, 255};
static const Color PROS_TAB_ACTIVE_BDR  = {102, 255, 255, 255};
static const Color PROS_BTN_BORDER      = {51, 102, 119, 255};
static const Color PROS_BTN_HOVER       = {102, 255, 255, 38};
static const Color PROS_BTN_TEXT        = {170, 187, 204, 255};
static const Color PROS_BTN_TEXT_HOVER  = {238, 238, 255, 255};
static const Color PROS_BTN_DISABLED    = {64, 84, 106, 255};
static const Color PROS_MSG_STATUS      = {85, 85, 85, 255};
static const Color PROS_MSG_ALERT       = {204, 170, 68, 255};

static Color ProsSweepHeatColor(float signal)
{
    if (signal < 0.05f) return {13, 13, 59, 102};
    if (signal < 0.15f) return {27, 27, 107, 102};
    if (signal < 0.30f) return {34, 68, 170, 102};
    if (signal < 0.50f) return {0, 204, 221, 102};
    if (signal < 0.70f) return {0, 255, 170, 102};
    if (signal < 0.85f) return {255, 0, 170, 102};
    return {255, 102, 221, 102};
}

static Color ProsElementColor(ResourceType type)
{
    switch (type)
    {
        case ResourceType::Fe: return {181, 70, 60, 255};
        case ResourceType::Ti: return {160, 176, 192, 255};
        case ResourceType::Si: return {212, 168, 80, 255};
        case ResourceType::Al: return {192, 192, 200, 255};
        case ResourceType::Ca: return {232, 220, 192, 255};
        case ResourceType::H2: return {136, 204, 238, 255};
        case ResourceType::O2: return {85, 170, 153, 255};
        case ResourceType::C:  return {64, 64, 64, 255};
        default: return {150, 150, 150, 255};
    }
}

static const char* ProsDepthName(DepthLayer d)
{
    switch (d)
    {
        case DepthLayer::SURFACE: return "Regolith";
        case DepthLayer::SHALLOW: return "Megaregolith";
        case DepthLayer::MID: return "Fract. Bedrock";
        case DepthLayer::DEEP: return "Intact Bedrock";
        default: return "Unknown";
    }
}

static const char* ProsConfLabel(float conf)
{
    if (conf >= 0.80f) return "Certain";
    if (conf >= 0.60f) return "High";
    if (conf >= 0.40f) return "Moderate";
    if (conf >= 0.20f) return "Low";
    return "Very Low";
}

// --- Prospecting energy accounting -----------------------------------------
// Sweeps, drills, and lab work all cost energy from the unit's storage. The
// UI gates on affordability so costs are visible before committing, and
// charges only when the action actually succeeds.

static bool ProsCanAfford(const Unit* unit, float cost)
{
    if (cost <= 0.0f) return true;
    return unit->GetStoredResource(ResourceType::ENERGY) >= cost;
}

static bool ProsChargeEnergy(Unit* unit, float cost, const char* action)
{
    if (cost <= 0.0f) return true;

    if (!unit->ConsumeResource(ResourceType::ENERGY, cost))
    {
        unit->PublicShowMessage(
            TextFormat("Not enough energy for %s: need %.0f E, have %.0f E.",
                       action, cost, unit->GetStoredResource(ResourceType::ENERGY)));
        return false;
    }
    return true;
}

// Rounded grid cell base: state-driven fill, hover/selection borders.
static void ProsDrawCellBase(Rectangle r, Color fill, bool selected, bool hover)
{
    DrawRectangleRounded(r, 0.22f, 4, {12, 15, 28, 255});
    if (fill.a > 0)
    {
        DrawRectangleRounded(r, 0.22f, 4, fill);
    }

    if (selected)
    {
        DrawRectangleRoundedLinesEx(r, 0.22f, 4, 2.0f, PROS_SELECT_BORDER);
        DrawRectangleRoundedLinesEx({r.x - 2, r.y - 2, r.width + 4, r.height + 4}, 0.22f, 4,
                                    1.0f, Fade(PROS_SELECT_BORDER, 0.35f));
    }
    else if (hover)
    {
        DrawRectangleRoundedLinesEx(r, 0.22f, 4, 1.0f, Fade(PROS_HOVER_BORDER, 0.8f));
    }
    else
    {
        DrawRectangleRoundedLinesEx(r, 0.22f, 4, 1.0f, {32, 38, 60, 255});
    }
}

// Out-of-reach cell: dimmed, dashed border, small lock glyph. The cell is
// visible from tier 0 so the player can see the ground they will eventually
// reach, and how much of it is still out of range.
static void ProsDrawLockedCell(Rectangle r, bool hover)
{
    // Deliberately quiet: the reachable area must stay the focus, while the
    // surrounding ground is still visible as "yours, later". A lock glyph on
    // every cell would drown the panel, so it only appears on hover.
    DrawRectangleRounded(r, 0.22f, 4, {8, 10, 18, 255});

    Color edge = hover ? Color{70, 92, 120, 255} : Color{20, 25, 40, 255};
    const float dash = 4.0f;
    for (float x = r.x + 3; x < r.x + r.width - 3; x += dash * 2)
    {
        float w = std::min(dash, r.x + r.width - 3 - x);
        DrawRectangleRec({x, r.y, w, 1.0f}, edge);
        DrawRectangleRec({x, r.y + r.height - 1, w, 1.0f}, edge);
    }
    for (float y = r.y + 3; y < r.y + r.height - 3; y += dash * 2)
    {
        float h = std::min(dash, r.y + r.height - 3 - y);
        DrawRectangleRec({r.x, y, 1.0f, h}, edge);
        DrawRectangleRec({r.x + r.width - 1, y, 1.0f, h}, edge);
    }

    if (hover)
    {
        float cx = r.x + r.width / 2.0f;
        float cy = r.y + r.height / 2.0f;
        float s = std::min(r.width, r.height) * 0.16f;
        Color glyph = {120, 150, 190, 255};
        DrawRectangleRec({cx - s, cy - s * 0.1f, s * 2.0f, s * 1.5f}, glyph);
        DrawRing({cx, cy - s * 0.1f}, s * 0.62f, s * 0.92f, 180.0f, 360.0f, 16, glyph);
    }
}

// Sample/sweep marker in the cell center. Confidence drives the glyph:
// hollow ring (low) -> ring with core (moderate) -> solid bright dot (high).
static void ProsDrawCellMarker(Rectangle r, const SubCell& cell)
{
    Vector2 c = {r.x + r.width / 2.0f, r.y + r.height / 2.0f};
    float base = r.width * 0.18f;

    if (!cell.sampleIds.empty())
    {
        float conf = cell.aggregateConfidence;
        if (conf > 0.7f)
        {
            DrawCircleV(c, base * 0.85f, {120, 240, 255, 255});
            DrawCircleV(c, base * 1.6f, {120, 240, 255, 35});
        }
        else if (conf > 0.4f)
        {
            DrawRing(c, base * 0.8f, base * 1.05f, 0.0f, 360.0f, 28, {238, 234, 252, 210});
            DrawCircleV(c, base * 0.4f, {238, 234, 252, 130});
        }
        else
        {
            DrawRing(c, base * 0.8f, base * 1.05f, 0.0f, 360.0f, 28, {238, 234, 252, 180});
        }
    }
    else if (cell.hasBeenSwept)
    {
        DrawCircleV(c, 1.5f, {225, 218, 255, 130});
    }
}

void RenderManager::DrawProspectingPanel(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;
    int padding = EXT_GAP + 14;   // card inset + inner padding
    float px = static_cast<float>(x + padding);
    float pw = static_cast<float>(w - padding * 2);

    if (!unit->HasProspectingSystem())
    {
        DrawTextEx(headerFont, "No prospecting system.", {px, static_cast<float>(y + padding)},
                   FS(14.0f), sp, EXT_DIM_TEXT);
        return;
    }

    auto* ps = unit->GetProspectingSystem();
    ps->gameTime += GetFrameTime();
    Vector2 mouse = GetMousePosition();

    // --- Header: icon + title, calibration gauge on the right ---
    float yPos = static_cast<float>(y + padding);
    ExtDrawIcon(ExtIcon::RADAR, px + 10.0f, yPos + 10.0f, 10.0f, EXT_ACCENT_CYAN);
    DrawTextEx(headerFont, "PROSPECTING", {px + 28.0f, yPos + 1.0f}, FS(15.0f), sp, EXT_TEXT);

    float calQHeader = ps->GetSweep().GetCalibrationQuality();
    const char* calValue = TextFormat("%.0f%%", calQHeader * 100.0f);
    float calValueW = MeasureTextEx(headerFont, calValue, FS(12.0f), sp).x;
    float gaugeW = 90.0f;
    float gaugeX = px + pw - calValueW - gaugeW - 10.0f;
    const char* calLabel = "CALIBRATION";
    float calLabelW = MeasureTextEx(bodyFont, calLabel, FS(10.0f), sp).x;
    DrawTextEx(bodyFont, calLabel, {gaugeX - calLabelW - 10.0f, yPos + 5.0f},
               FS(10.0f), sp, EXT_DIM_TEXT);
    ExtDrawSegBar(gaugeX, yPos + 3.0f, gaugeW, 14.0f, calQHeader,
                  calQHeader >= 0.8f ? EXT_ACCENT_CYAN : EXT_ACCENT_GOLD);
    DrawTextEx(headerFont, calValue, {gaugeX + gaugeW + 10.0f, yPos + 2.0f},
               FS(12.0f), sp, EXT_TEXT);
    yPos += 32.0f;

    // --- Stage tabs ---
    float tabGap = 6.0f;
    float tabW = (pw - tabGap * 2.0f) / 3.0f;
    float tabH = 30.0f;
    const char* tabNames[] = {"SWEEP", "SAMPLES", "LAB"};
    ProspectingTab tabs[] = {ProspectingTab::SWEEP, ProspectingTab::SAMPLES, ProspectingTab::LAB};

    for (int i = 0; i < 3; i++)
    {
        Rectangle tabRect = {px + i * (tabW + tabGap), yPos, tabW, tabH};
        bool isActive = (ps->activeTab == tabs[i]);
        bool isHover = CheckCollisionPointRec(mouse, tabRect);

        if (isActive)
        {
            DrawRectangleRounded(tabRect, 0.3f, 4, {16, 48, 58, 255});
            DrawRectangleRoundedLinesEx(tabRect, 0.3f, 4, 1.5f, PROS_TAB_ACTIVE_BDR);
        }
        else
        {
            DrawRectangleRounded(tabRect, 0.3f, 4, isHover ? Color{16, 26, 44, 255} : EXT_PANEL_BG2);
            DrawRectangleRoundedLinesEx(tabRect, 0.3f, 4, 1.0f, PROS_TAB_BORDER);
        }

        Color textColor = isActive ? PROS_TAB_ACTIVE_BDR : (isHover ? PROS_BTN_TEXT_HOVER : PROS_BTN_TEXT);
        Vector2 textSize = MeasureTextEx(headerFont, tabNames[i], FS(11.0f), sp);
        DrawTextEx(headerFont, tabNames[i],
                   {tabRect.x + (tabW - textSize.x) / 2, tabRect.y + (tabH - textSize.y) / 2},
                   FS(11.0f), sp, textColor);

        if (isHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            ps->activeTab = tabs[i];
        }
    }
    yPos += tabH + 12.0f;

    // --- Content area ---
    float contentY = yPos;
    float contentH = static_cast<float>(y + h - padding) - yPos;

    auto& grid = ps->GetGrid();
    int gridSize = grid.GetGridSize();

    if (ps->activeTab == ProspectingTab::SWEEP)
    {
        // === SWEEP TAB ===
        float gridAreaW = std::min(pw * 0.58f, contentH - 10.0f);
        float cellSize = gridAreaW / gridSize;
        float gridX = px;
        float gridY = contentY;

        // Tracked while drawing so the out-of-range tooltip can be drawn last
        int lockedHoverX = -1;
        int lockedHoverY = -1;

        float cellGap = 5.0f;
        for (int gy = 0; gy < gridSize; gy++)
        {
            for (int gx = 0; gx < gridSize; gx++)
            {
                Rectangle cellRect = {gridX + gx * cellSize, gridY + gy * cellSize,
                                      cellSize - cellGap, cellSize - cellGap};

                bool hover = CheckCollisionPointRec(mouse, cellRect);

                if (!grid.IsInReach(gx, gy))
                {
                    ProsDrawLockedCell(cellRect, hover);
                    if (hover)
                    {
                        lockedHoverX = gx;
                        lockedHoverY = gy;
                    }
                    continue;
                }

                const SubCell& cell = grid.GetSubCell(gx, gy);
                Color fill = {0, 0, 0, 0};
                if (cell.hasBeenSwept)
                {
                    fill = ProsSweepHeatColor(cell.sweepSignal);
                    fill.a = 115;   // muted plum over the dark base, per the mock
                }

                bool selected = (ps->selectedCellX == gx && ps->selectedCellY == gy);
                ProsDrawCellBase(cellRect, fill, selected, hover);
                ProsDrawCellMarker(cellRect, cell);

                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    ps->selectedCellX = gx;
                    ps->selectedCellY = gy;
                }
            }
        }

        // Reach legend under the grid
        float legendY = gridY + gridSize * cellSize + 2.0f;
        DrawTextEx(bodyFont,
                   TextFormat("SURVEY RANGE  %dx%d of %dx%d",
                              grid.GetReach(), grid.GetReach(), gridSize, gridSize),
                   {gridX, legendY}, FS(9.0f), sp, EXT_DIM_TEXT);
        if (grid.GetReach() < gridSize)
        {
            const char* rangeHint = "Higher tiers extend range";
            float hintW = MeasureTextEx(bodyFont, rangeHint, FS(9.0f), sp).x;
            DrawTextEx(bodyFont, rangeHint,
                       {gridX + gridAreaW - cellGap - hintW, legendY}, FS(9.0f), sp,
                       Fade(EXT_DIM_TEXT, 0.7f));
        }

        // Out-of-range tooltip, drawn last so it sits above the grid
        if (lockedHoverX >= 0)
        {
            int needTier = TierRequiredForSubCell(lockedHoverX, lockedHoverY);
            const char* line1 = "OUT OF RANGE";
            const char* line2 = needTier >= 0
                ? TextFormat("Tier %d extends survey range here", needTier)
                : "Unreachable";

            float w = std::max(MeasureTextEx(headerFont, line1, FS(10.0f), sp).x,
                               MeasureTextEx(bodyFont, line2, FS(9.0f), sp).x) + 20.0f;
            float h = 38.0f;
            float tx = std::min(mouse.x + 14.0f, px + pw - w);
            float ty = std::min(mouse.y + 14.0f, static_cast<float>(y + h) - 48.0f);

            DrawRectangleRounded({tx, ty, w, h}, 0.2f, 4, {12, 18, 32, 245});
            DrawRectangleRoundedLinesEx({tx, ty, w, h}, 0.2f, 4, 1.0f, EXT_PANEL_BORDER);
            DrawTextEx(headerFont, line1, {tx + 10.0f, ty + 7.0f}, FS(10.0f), sp, EXT_ACCENT_GOLD);
            DrawTextEx(bodyFont, line2, {tx + 10.0f, ty + 21.0f}, FS(9.0f), sp, EXT_DIM_TEXT);
        }

        // Sweep controls (right of grid)
        float ctrlX = gridX + gridAreaW + 15.0f;
        float ctrlY = contentY;
        float ctrlW = pw - gridAreaW - 15.0f;

        DrawTextEx(headerFont, "FREQUENCY BAND", {ctrlX, ctrlY}, FS(12.0f), sp, EXT_HEADER_COLOR);
        ctrlY += 22.0f;

        for (int band = 0; band < SWEEP_FREQUENCY_BANDS; band++)
        {
            bool affordable = ProsCanAfford(unit, SWEEP_ENERGY_COST[band]);
            bool canSweep = ps->GetSweep().CanSweep(grid, band) && affordable;
            bool alreadySwept = grid.HasSweptFrequency(band);
            Rectangle bandBtn = {ctrlX, ctrlY, ctrlW - 10.0f, 26.0f};
            bool hover = CheckCollisionPointRec(mouse, bandBtn);
            bool selectedBand = (ps->selectedFrequencyBand == band);

            Color fillCol = selectedBand ? Color{14, 44, 56, 255}
                                         : (hover && canSweep ? Color{16, 26, 44, 255} : EXT_PANEL_BG2);
            DrawRectangleRounded(bandBtn, 0.35f, 4, fillCol);
            if (alreadySwept && !selectedBand)
                DrawRectangleRounded(bandBtn, 0.35f, 4, {0, 204, 221, 16});
            DrawRectangleRoundedLinesEx(bandBtn, 0.35f, 4, selectedBand ? 1.5f : 1.0f,
                                        selectedBand ? PROS_TAB_ACTIVE_BDR
                                                     : (canSweep ? PROS_BTN_BORDER : PROS_BTN_DISABLED));

            Color textCol = selectedBand ? EXT_TEXT
                                         : (!canSweep ? PROS_BTN_DISABLED
                                                      : (hover ? PROS_BTN_TEXT_HOVER : PROS_BTN_TEXT));
            DrawTextEx(headerFont, TextFormat("BAND %d", band),
                       {ctrlX + 10.0f, ctrlY + 6.0f}, FS(10.0f), sp, textCol);
            const char* costLabel = TextFormat("%.0f E", SWEEP_ENERGY_COST[band]);
            float costW = MeasureTextEx(bodyFont, costLabel, FS(10.0f), sp).x;
            DrawTextEx(bodyFont, costLabel,
                       {ctrlX + ctrlW - 20.0f - costW, ctrlY + 6.0f}, FS(10.0f), sp, textCol);

            if (hover && canSweep && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                ps->selectedFrequencyBand = band;
            }
            ctrlY += 32.0f;
        }

        ctrlY += 10.0f;
        Rectangle sweepBtn = {ctrlX, ctrlY, ctrlW - 10.0f, 34.0f};
        float sweepCost = SWEEP_ENERGY_COST[ps->selectedFrequencyBand];
        bool canSweepNow = ps->GetSweep().CanSweep(grid, ps->selectedFrequencyBand) &&
                           ProsCanAfford(unit, sweepCost);
        bool sweepHover = CheckCollisionPointRec(mouse, sweepBtn);

        Color runFill = !canSweepNow ? Color{16, 22, 38, 255}
                                     : (sweepHover ? Color{20, 56, 96, 255} : Color{14, 40, 70, 255});
        DrawRectangleRounded(sweepBtn, 0.3f, 4, runFill);
        DrawRectangleRoundedLinesEx(sweepBtn, 0.3f, 4, 1.5f,
                                    canSweepNow ? PROS_TAB_ACTIVE_BDR : PROS_BTN_DISABLED);

        const char* runLabel = "RUN SWEEP";
        float runW = MeasureTextEx(headerFont, runLabel, FS(12.0f), sp).x;
        Color runText = canSweepNow ? (sweepHover ? WHITE : EXT_ACCENT_CYAN) : PROS_BTN_DISABLED;
        DrawTextEx(headerFont, runLabel,
                   {sweepBtn.x + (sweepBtn.width - runW) / 2.0f - 10.0f, ctrlY + 9.0f},
                   FS(12.0f), sp, runText);
        ExtDrawChevrons(sweepBtn.x + (sweepBtn.width + runW) / 2.0f + 4.0f, ctrlY + 17.0f,
                        5.0f, runText);

        if (sweepHover && canSweepNow && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (ProsChargeEnergy(unit, sweepCost, "sweep"))
            {
                ps->GetSweep().ExecuteSweep(ps->GetGrid(), ps->selectedFrequencyBand, ps->gameTime);
            }
        }

        ctrlY += 46.0f;

        // Sweep history
        const auto& sweepHist = grid.GetSweepHistory();
        if (!sweepHist.empty())
        {
            DrawTextEx(bodyFont, TextFormat("Sweeps: %d", static_cast<int>(sweepHist.size())),
                       {ctrlX, ctrlY}, FS(10.0f), sp, EXT_DIM_TEXT);
            ctrlY += 16.0f;
        }

        float calQ = ps->GetSweep().GetCalibrationQuality();
        bool calibrating = ps->GetSweep().IsCalibrating();

        DrawTextEx(bodyFont, TextFormat("Calibration: %.0f%%", calQ * 100.0f),
                   {ctrlX, ctrlY}, FS(10.0f), sp, calQ >= 0.8f ? EXT_ACCENT_GREEN : PROS_MSG_ALERT);
        ctrlY += 18.0f;

        // CALIBRATE: restores quality to 100%, blocks sweeping while it runs
        Rectangle calBtn = {ctrlX, ctrlY, ctrlW - 10.0f, 26.0f};
        bool calHover = CheckCollisionPointRec(mouse, calBtn);
        bool calNeeded = calQ < 0.999f;
        bool calEnabled = calNeeded && !calibrating;

        Color calFill = calibrating ? Color{16, 40, 48, 255}
                                    : (!calEnabled ? Color{14, 20, 34, 255}
                                                   : (calHover ? Color{20, 50, 66, 255}
                                                               : EXT_PANEL_BG2));
        DrawRectangleRounded(calBtn, 0.3f, 4, calFill);

        if (calibrating)
        {
            // Progress fill
            float prog = ps->GetSweep().GetCalibrationProgress();
            if (prog > 0.01f)
            {
                DrawRectangleRounded({calBtn.x + 2.0f, calBtn.y + 2.0f,
                                      (calBtn.width - 4.0f) * prog, calBtn.height - 4.0f},
                                     0.3f, 4, Fade(EXT_ACCENT_CYAN, 0.25f));
            }
        }

        DrawRectangleRoundedLinesEx(calBtn, 0.3f, 4, calibrating ? 1.5f : 1.0f,
                                    calibrating ? EXT_ACCENT_CYAN
                                                : (calEnabled ? PROS_BTN_BORDER : PROS_BTN_DISABLED));

        const char* calLabel = calibrating
            ? TextFormat("CALIBRATING %.0f%%", ps->GetSweep().GetCalibrationProgress() * 100.0f)
            : (calNeeded ? "CALIBRATE" : "CALIBRATED");
        Color calText = calibrating ? EXT_ACCENT_CYAN
                                    : (calEnabled ? (calHover ? WHITE : PROS_BTN_TEXT_HOVER)
                                                  : PROS_BTN_DISABLED);
        float calLabelW = MeasureTextEx(headerFont, calLabel, FS(10.0f), sp).x;
        DrawTextEx(headerFont, calLabel,
                   {calBtn.x + (calBtn.width - calLabelW) / 2.0f, calBtn.y + 7.0f},
                   FS(10.0f), sp, calText);

        if (calHover && calEnabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            ps->GetSweep().StartCalibration();
            unit->PublicShowMessage("Calibrating sweep instrument...");
        }
        ctrlY += 32.0f;

        // Cell info tooltip
        if (ps->selectedCellX >= 0 && ps->selectedCellX < gridSize &&
            ps->selectedCellY >= 0 && ps->selectedCellY < gridSize)
        {
            ctrlY += 8.0f;
            DrawTextEx(headerFont, TextFormat("CELL (%d,%d)", ps->selectedCellX, ps->selectedCellY),
                       {ctrlX, ctrlY}, FS(11.0f), sp, EXT_ACCENT_CYAN);
            ctrlY += 18.0f;

            const SubCell& selCell = grid.GetSubCell(ps->selectedCellX, ps->selectedCellY);
            DrawTextEx(bodyFont, TextFormat("Signal: %.2f", selCell.sweepSignal),
                       {ctrlX, ctrlY}, FS(10.0f), sp, EXT_DIM_TEXT);
            ctrlY += 14.0f;
            DrawTextEx(bodyFont, TextFormat("Confidence: %s", ProsConfLabel(selCell.aggregateConfidence)),
                       {ctrlX, ctrlY}, FS(10.0f), sp, EXT_DIM_TEXT);
            ctrlY += 14.0f;
            DrawTextEx(bodyFont, TextFormat("Samples: %d", static_cast<int>(selCell.sampleIds.size())),
                       {ctrlX, ctrlY}, FS(10.0f), sp, EXT_DIM_TEXT);
        }
    }
    else if (ps->activeTab == ProspectingTab::SAMPLES)
    {
        // === SAMPLES TAB ===
        float gridAreaW = std::min(pw * 0.55f, contentH - 40.0f);
        float cellSize = gridAreaW / gridSize;
        float gridX = px;
        float gridY = contentY;

        // Draw grid with element tints for sampled cells
        for (int gy = 0; gy < gridSize; gy++)
        {
            for (int gx = 0; gx < gridSize; gx++)
            {
                Rectangle cellRect = {gridX + gx * cellSize, gridY + gy * cellSize,
                                      cellSize - 5.0f, cellSize - 5.0f};

                if (!grid.IsInReach(gx, gy))
                {
                    ProsDrawLockedCell(cellRect, CheckCollisionPointRec(mouse, cellRect));
                    continue;
                }

                const SubCell& cell = grid.GetSubCell(gx, gy);
                Color fill = {0, 0, 0, 0};
                if (cell.hasBeenSwept)
                {
                    fill = ProsSweepHeatColor(cell.sweepSignal);
                    fill.a = 55;
                }

                bool hover = CheckCollisionPointRec(mouse, cellRect);
                bool selected = (ps->selectedCellX == gx && ps->selectedCellY == gy);
                ProsDrawCellBase(cellRect, fill, selected, hover);

                if (!cell.sampleIds.empty())
                {
                    auto gt = grid.GetGroundTruth(gx, gy, DepthLayer::SURFACE);
                    ResourceType dominant = ResourceType::Fe;
                    float maxVal = 0.0f;
                    for (const auto& [t, v] : gt)
                    {
                        if (v > maxVal) { maxVal = v; dominant = t; }
                    }
                    Color elemColor = ProsElementColor(dominant);
                    elemColor.a = 60;
                    DrawRectangleRounded({cellRect.x + 2, cellRect.y + 2,
                                          cellRect.width - 4, cellRect.height - 4}, 0.22f, 4, elemColor);

                    float dotR = cellRect.width * 0.16f;
                    DrawCircle(static_cast<int>(cellRect.x + cellRect.width / 2),
                               static_cast<int>(cellRect.y + cellRect.height / 2),
                               dotR, ProsElementColor(dominant));
                }

                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    ps->selectedCellX = gx;
                    ps->selectedCellY = gy;
                }
            }
        }

        // Right side: depth selector + collect button + tray
        float ctrlX = gridX + gridAreaW + 15.0f;
        float ctrlY = contentY;
        float ctrlW = pw - gridAreaW - 15.0f;

        DrawTextEx(headerFont, "DEPTH LAYER", {ctrlX, ctrlY}, FS(12.0f), sp, EXT_HEADER_COLOR);
        ctrlY += 22.0f;

        DepthLayer depths[] = {DepthLayer::SURFACE, DepthLayer::SHALLOW, DepthLayer::MID, DepthLayer::DEEP};
        for (int d = 0; d < 4; d++)
        {
            bool tierAllows = ps->GetSampler().CanDrill(depths[d]);
            bool canDrill = tierAllows;   // selection stays legal; COLLECT gates on energy
            Rectangle depthBtn = {ctrlX, ctrlY, ctrlW - 10.0f, 22.0f};
            bool hover = CheckCollisionPointRec(mouse, depthBtn);
            bool selected = (ps->selectedDepth == depths[d]);

            // Radio-style rows: this is a selection list, not an action
            // button -- COLLECT below is the action.
            Color borderCol = canDrill ? (selected ? Fade(PROS_TAB_ACTIVE_BDR, 0.7f) : Fade(PROS_BTN_BORDER, 0.7f))
                                       : Fade(PROS_BTN_DISABLED, 0.7f);
            Color textCol = canDrill ? (selected ? EXT_TEXT
                                                 : (hover ? PROS_BTN_TEXT_HOVER : PROS_BTN_TEXT))
                                     : PROS_BTN_DISABLED;

            Color depthFill = (selected && canDrill) ? Color{13, 30, 40, 255}
                                                     : (hover && canDrill ? Color{14, 22, 38, 255}
                                                                          : Color{11, 16, 30, 255});
            DrawRectangleRounded(depthBtn, 0.35f, 4, depthFill);
            DrawRectangleRoundedLinesEx(depthBtn, 0.35f, 4, 1.0f, borderCol);

            // Radio indicator
            float rx = ctrlX + 12.0f;
            float ry = ctrlY + 11.0f;
            DrawCircleLines(static_cast<int>(rx), static_cast<int>(ry), 5.0f,
                            canDrill ? Fade(EXT_ACCENT_CYAN, selected ? 1.0f : 0.5f)
                                     : PROS_BTN_DISABLED);
            if (selected && canDrill)
            {
                DrawCircle(static_cast<int>(rx), static_cast<int>(ry), 2.5f, EXT_ACCENT_CYAN);
            }

            float cost = ps->GetSampler().GetDrillCost(depths[d]);
            const char* label = canDrill
                ? TextFormat("%s  %.0fE", ProsDepthName(depths[d]), cost)
                : TextFormat("%s  [LOCKED]", ProsDepthName(depths[d]));
            DrawTextEx(bodyFont, label, {ctrlX + 24, ctrlY + 3}, FS(9.0f), sp, textCol);

            if (hover && canDrill && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                ps->selectedDepth = depths[d];
            }
            ctrlY += 26.0f;
        }

        ctrlY += 8.0f;
        bool hasSelection = (ps->selectedCellX >= 0 && ps->selectedCellX < gridSize &&
                              ps->selectedCellY >= 0 && ps->selectedCellY < gridSize);
        bool trayFull = ps->GetTray().IsFull();
        float drillCost = ps->GetSampler().GetDrillCost(ps->selectedDepth);
        bool canAffordDrill = ProsCanAfford(unit, drillCost);
        bool canCollect = hasSelection && !trayFull && canAffordDrill &&
                          ps->GetSampler().CanDrill(ps->selectedDepth);

        Rectangle collectBtn = {ctrlX, ctrlY, ctrlW - 10.0f, 30.0f};
        bool collectHover = CheckCollisionPointRec(mouse, collectBtn);

        Color collectFill = !canCollect ? Color{16, 22, 38, 255}
                                        : (collectHover ? Color{20, 56, 96, 255} : Color{14, 40, 70, 255});
        DrawRectangleRounded(collectBtn, 0.3f, 4, collectFill);
        DrawRectangleRoundedLinesEx(collectBtn, 0.3f, 4, 1.5f,
                                    canCollect ? PROS_TAB_ACTIVE_BDR : PROS_BTN_DISABLED);
        const char* collectLabel = "COLLECT";
        float collectW = MeasureTextEx(headerFont, collectLabel, FS(12.0f), sp).x;
        Color collectText = canCollect ? (collectHover ? WHITE : EXT_ACCENT_CYAN) : PROS_BTN_DISABLED;
        DrawTextEx(headerFont, collectLabel,
                   {collectBtn.x + (collectBtn.width - collectW) / 2.0f - 8.0f, ctrlY + 7}, FS(12.0f), sp,
                   collectText);
        ExtDrawChevrons(collectBtn.x + (collectBtn.width + collectW) / 2.0f + 4.0f, ctrlY + 15.0f,
                        5.0f, collectText);

        if (collectHover && canCollect && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (ProsChargeEnergy(unit, drillCost, "drilling"))
            {
                if (!ps->GetSampler().CollectSample(ps->GetGrid(), ps->GetTray(),
                                                    ps->selectedCellX, ps->selectedCellY,
                                                    ps->selectedDepth))
                {
                    // Refund a drill that could not actually be taken
                    unit->AddResource(ResourceType::ENERGY, drillCost);
                }
            }
        }

        // DISCARD: frees a tray slot, otherwise a full tray is a dead end
        ctrlY += 34.0f;
        const Sample* discardTarget = (ps->selectedSampleIndex >= 0)
            ? ps->GetTray().GetSampleByIndex(ps->selectedSampleIndex) : nullptr;

        Rectangle discardBtn = {ctrlX, ctrlY, ctrlW - 10.0f, 26.0f};
        bool discardHover = CheckCollisionPointRec(mouse, discardBtn);
        bool canDiscard = (discardTarget != nullptr);

        DrawRectangleRounded(discardBtn, 0.3f, 4,
                             canDiscard && discardHover ? Color{54, 16, 22, 255}
                                                        : Color{14, 20, 34, 255});
        DrawRectangleRoundedLinesEx(discardBtn, 0.3f, 4, 1.0f,
                                    canDiscard ? Fade(EXT_ACCENT_RED, 0.8f) : PROS_BTN_DISABLED);

        const char* discardLabel = trayFull && !canDiscard ? "TRAY FULL - SELECT A SAMPLE" : "DISCARD SAMPLE";
        float discardW = MeasureTextEx(headerFont, discardLabel, FS(10.0f), sp).x;
        DrawTextEx(headerFont, discardLabel,
                   {discardBtn.x + (discardBtn.width - discardW) / 2.0f, discardBtn.y + 7.0f},
                   FS(10.0f), sp,
                   canDiscard ? (discardHover ? WHITE : Fade(EXT_ACCENT_RED, 0.9f)) : PROS_BTN_DISABLED);

        if (discardHover && canDiscard && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (ps->GetTray().RemoveSample(discardTarget->id))
            {
                ps->selectedSampleIndex = -1;
                unit->PublicShowMessage("Sample discarded.");
            }
        }

        // Sample tray display
        ctrlY += 32.0f;
        DrawTextEx(headerFont, TextFormat("TRAY (%d/%d)", ps->GetTray().GetCount(), ps->GetTray().GetCapacity()),
                   {ctrlX, ctrlY}, FS(11.0f), sp, EXT_HEADER_COLOR);
        ctrlY += 20.0f;

        float slotSize = 28.0f;
        float slotGap = 4.0f;
        int slotsPerRow = static_cast<int>((ctrlW - 10.0f) / (slotSize + slotGap));
        if (slotsPerRow < 1) slotsPerRow = 1;

        for (int i = 0; i < ps->GetTray().GetCapacity(); i++)
        {
            int row = i / slotsPerRow;
            int col = i % slotsPerRow;
            Rectangle slot = {ctrlX + col * (slotSize + slotGap), ctrlY + row * (slotSize + slotGap),
                              slotSize, slotSize};

            const Sample* s = ps->GetTray().GetSampleByIndex(i);
            bool slotHover = CheckCollisionPointRec(mouse, slot);
            bool slotSelected = (ps->selectedSampleIndex == i && s != nullptr);

            if (s)
            {
                DrawRectangleRounded(slot, 0.25f, 4,
                                     slotSelected ? Color{16, 38, 54, 255} : EXT_PANEL_BG2);

                // The sample's actual crystal sprite
                DrawCrystalSprite(s->visual, {slot.x + 2, slot.y + 2,
                                              slot.width - 4, slot.height - 4});

                // Depth indicator badge
                const char* depthChar =
                    s->depthLayer == DepthLayer::SURFACE ? "S" :
                    s->depthLayer == DepthLayer::SHALLOW ? "R" :
                    s->depthLayer == DepthLayer::MID     ? "M" : "B";
                DrawTextEx(bodyFont, depthChar,
                           {slot.x + 2, slot.y + slot.height - 12}, FS(8.0f), sp,
                           {255, 255, 255, 160});
            }
            else
            {
                DrawRectangleRounded(slot, 0.25f, 4, {12, 15, 28, 255});
            }

            Color slotBorder = PROS_CELL_BORDER;
            float slotBorderThick = 1.0f;
            if (slotSelected)
            {
                slotBorder = PROS_SELECT_BORDER;
                slotBorderThick = 2.0f;
            }
            else if (slotHover)
            {
                slotBorder = PROS_HOVER_BORDER;
            }
            else if (s && s->state == SampleState::COMPLETED)
            {
                slotBorder = Fade(EXT_ACCENT_GREEN, 0.6f);
            }
            else if (s && s->state == SampleState::PROCESSING)
            {
                slotBorder = PROS_MSG_ALERT;
            }
            DrawRectangleRoundedLinesEx(slot, 0.25f, 4, slotBorderThick, slotBorder);

            if (slotHover && s && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                ps->selectedSampleIndex = i;
            }
        }

        // Selected sample details
        const Sample* selSample = (ps->selectedSampleIndex >= 0)
            ? ps->GetTray().GetSampleByIndex(ps->selectedSampleIndex) : nullptr;
        if (selSample)
        {
            int trayRows = (ps->GetTray().GetCapacity() + slotsPerRow - 1) / slotsPerRow;
            float detailY = ctrlY + trayRows * (slotSize + slotGap) + 8.0f;

            DrawTextEx(bodyFont, TextFormat("Depth: %s", ProsDepthName(selSample->depthLayer)),
                       {ctrlX, detailY}, FS(9.0f), sp, EXT_DIM_TEXT);
            detailY += 14.0f;
            DrawTextEx(bodyFont, TextFormat("Richness: %.0f%%", selSample->richness * 100.0f),
                       {ctrlX, detailY}, FS(9.0f), sp, EXT_DIM_TEXT);
            detailY += 14.0f;

            const char* stateStr = selSample->state == SampleState::IN_TRAY ? "In Tray" :
                                   selSample->state == SampleState::PROCESSING ? "Processing" : "Completed";
            DrawTextEx(bodyFont, TextFormat("State: %s", stateStr),
                       {ctrlX, detailY}, FS(9.0f), sp, EXT_DIM_TEXT);
            detailY += 14.0f;

            for (const auto& [type, conf] : selSample->elementConfidence)
            {
                if (conf > 0.0f)
                {
                    float abund = 0.0f;
                    auto ait = selSample->trueComposition.find(type);
                    if (ait != selSample->trueComposition.end()) abund = ait->second;

                    const char* valText = conf < 0.3f ?
                        TextFormat("~%.0f%%", abund * 100.0f) :
                        TextFormat("%.1f%%", abund * 100.0f);

                    DrawTextEx(bodyFont, TextFormat("  %s: %s  conf:%s",
                        ResourceTypeToString(type), valText, ProsConfLabel(conf)),
                        {ctrlX, detailY}, FS(8.0f), sp, ProsElementColor(type));
                    detailY += 12.0f;
                }
            }
        }
    }
    else if (ps->activeTab == ProspectingTab::LAB)
    {
        // === LAB TAB ===
        float leftW = pw * 0.45f;
        float rightX = px + leftW + 10.0f;
        float rightW = pw - leftW - 10.0f;

        // Left: sample selection (mini tray) with crystal sprites
        DrawTextEx(headerFont, "SELECT SAMPLE", {px, contentY}, FS(12.0f), sp, EXT_HEADER_COLOR);
        float trayY = contentY + 20.0f;
        float slotSize = 42.0f;
        float slotGap = 5.0f;
        int slotsPerRow = static_cast<int>(leftW / (slotSize + slotGap));
        if (slotsPerRow < 1) slotsPerRow = 1;

        for (int i = 0; i < ps->GetTray().GetCount(); i++)
        {
            int row = i / slotsPerRow;
            int col = i % slotsPerRow;
            Rectangle slot = {px + col * (slotSize + slotGap), trayY + row * (slotSize + slotGap),
                              slotSize, slotSize};

            const Sample* s = ps->GetTray().GetSampleByIndex(i);
            if (!s) continue;

            bool hover = CheckCollisionPointRec(mouse, slot);
            bool selected = (ps->selectedSampleIndex == i);

            DrawRectangleRounded(slot, 0.2f, 4, selected ? Color{16, 38, 54, 255} : EXT_PANEL_BG2);
            DrawCrystalSprite(s->visual, {slot.x + 3, slot.y + 3, slot.width - 6, slot.height - 6});

            Color borderCol = PROS_CELL_BORDER;
            float borderThick = 1.0f;
            if (selected)
            {
                borderCol = PROS_SELECT_BORDER;
                borderThick = 2.0f;
            }
            else if (hover)
            {
                borderCol = PROS_HOVER_BORDER;
            }
            else if (s->state == SampleState::COMPLETED)
            {
                borderCol = Fade(EXT_ACCENT_GREEN, 0.6f);
            }
            else if (s->state == SampleState::PROCESSING)
            {
                borderCol = PROS_MSG_ALERT;
            }
            DrawRectangleRoundedLinesEx(slot, 0.2f, 4, borderThick, borderCol);

            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                ps->selectedSampleIndex = i;
            }
        }

        // Tool buttons (below sample tray)
        int trayRows = (ps->GetTray().GetCount() + slotsPerRow - 1) / slotsPerRow;
        float toolY = trayY + trayRows * (slotSize + slotGap) + 12.0f;

        Sample* selSample = (ps->selectedSampleIndex >= 0)
            ? ps->GetTray().GetSampleByIndex(ps->selectedSampleIndex) : nullptr;

        // --- Preset pipelines: one tap runs a separation + tool combo ---
        DrawTextEx(headerFont, "PRESETS", {px, toolY}, FS(12.0f), sp, EXT_HEADER_COLOR);
        toolY += 20.0f;

        const std::vector<LabPreset>& presets = LabEngine::GetPresets();
        float presetColW = (leftW - 11.0f) / 2.0f;
        for (size_t p = 0; p < presets.size(); p++)
        {
            // A preset runs a separation plus its tools -- charge the sum
            float presetCost = LabEngine::GetSeparationCost(presets[p].separation);
            for (AnalysisTool tool : presets[p].tools)
            {
                presetCost += LabEngine::GetToolCost(tool);
            }

            bool canApply = selSample && ps->GetLab().CanApplyPreset(*selSample, static_cast<int>(p)) &&
                            ProsCanAfford(unit, presetCost);
            Rectangle presetBtn = {px + (p % 2) * (presetColW + 6.0f),
                                   toolY + (p / 2) * 27.0f, presetColW, 23.0f};
            bool hover = CheckCollisionPointRec(mouse, presetBtn);
            bool flash = (ps->lastLabActionKind == 2 &&
                          ps->lastLabActionIndex == static_cast<int>(p) &&
                          ps->gameTime - ps->lastLabActionTime < 0.4f);

            Color fill = flash ? Color{24, 70, 90, 255}
                               : (hover && canApply ? Color{16, 32, 52, 255} : Color{13, 22, 40, 255});
            DrawRectangleRounded(presetBtn, 0.3f, 4, fill);
            DrawRectangleRoundedLinesEx(presetBtn, 0.3f, 4, flash ? 2.0f : 1.0f,
                                        flash ? EXT_ACCENT_CYAN
                                              : (canApply ? Fade(EXT_ACCENT_VIOLET, 0.8f)
                                                          : PROS_BTN_DISABLED));

            Color textCol = canApply ? (hover ? WHITE : Fade(EXT_ACCENT_VIOLET, 0.95f))
                                     : PROS_BTN_DISABLED;
            bool tierLocked = ps->GetTier() < presets[p].requiredTier;
            const char* presetLabel = tierLocked
                ? TextFormat("%s  T%d", presets[p].name.c_str(), presets[p].requiredTier)
                : TextFormat("%s  %.0fE", presets[p].name.c_str(), presetCost);
            DrawTextEx(bodyFont, presetLabel,
                       {presetBtn.x + 8.0f, presetBtn.y + 5.0f}, FS(9.0f), sp, textCol);

            if (hover && canApply && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (ProsChargeEnergy(unit, presetCost, presets[p].name.c_str()) &&
                    ps->GetLab().ApplyPreset(*selSample, static_cast<int>(p), ps->gameTime))
                {
                    ps->lastLabActionKind = 2;
                    ps->lastLabActionIndex = static_cast<int>(p);
                    ps->lastLabActionTime = ps->gameTime;
                    unit->PublicShowMessage(presets[p].name + " pipeline applied.");
                }
            }
        }
        toolY += ((presets.size() + 1) / 2) * 27.0f + 6.0f;

        DrawTextEx(headerFont, "ANALYSIS TOOLS", {px, toolY}, FS(12.0f), sp, EXT_HEADER_COLOR);
        toolY += 20.0f;

        struct ToolEntry { AnalysisTool tool; const char* name; };
        ToolEntry tools[] = {
            {AnalysisTool::VISUAL_INSPECTION, "Visual"},
            {AnalysisTool::XRF, "XRF"},
            {AnalysisTool::OPTICAL_MICROSCOPY, "Optical"},
            {AnalysisTool::MAGNETIC_SUSCEPTIBILITY, "Magnetic"},
            {AnalysisTool::LIBS_PULSE, "LIBS"},
            {AnalysisTool::FIRE_ASSAY, "Fire Assay"},
        };

        // Two-column grid keeps the tool list inside the card
        float toolColW = (leftW - 11.0f) / 2.0f;
        for (int t = 0; t < 6; t++)
        {
            auto& te = tools[t];
            float cost = LabEngine::GetToolCost(te.tool);
            bool canApply = selSample && ps->GetLab().CanApplyTool(*selSample, te.tool) &&
                            ProsCanAfford(unit, cost);
            Rectangle toolBtn = {px + (t % 2) * (toolColW + 6.0f), toolY + (t / 2) * 27.0f,
                                 toolColW, 23.0f};
            bool hover = CheckCollisionPointRec(mouse, toolBtn);
            bool pressed = hover && canApply && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

            // Already applied to the selected sample?
            bool applied = false;
            if (selSample)
            {
                for (const auto& step : selSample->analysisHistory)
                {
                    if (step.tool == te.tool) { applied = true; break; }
                }
            }

            // Brief flash right after a successful tap (touch has no hover)
            bool flash = (ps->lastLabActionKind == 0 && ps->lastLabActionIndex == t &&
                          ps->gameTime - ps->lastLabActionTime < 0.4f);

            Color fill = EXT_PANEL_BG2;
            if (pressed || flash) fill = {24, 70, 90, 255};
            else if (hover && canApply) fill = {16, 26, 44, 255};
            else if (applied) fill = {13, 30, 34, 255};

            DrawRectangleRounded(toolBtn, 0.3f, 4, fill);
            DrawRectangleRoundedLinesEx(toolBtn, 0.3f, 4, flash ? 2.0f : 1.0f,
                                        flash ? EXT_ACCENT_CYAN
                                              : (applied ? Fade(EXT_ACCENT_GREEN, 0.7f)
                                                         : (canApply ? PROS_BTN_BORDER : PROS_BTN_DISABLED)));

            Color textCol = canApply ? (hover ? PROS_BTN_TEXT_HOVER : PROS_BTN_TEXT)
                                     : (applied ? Fade(EXT_ACCENT_GREEN, 0.8f) : PROS_BTN_DISABLED);
            DrawTextEx(bodyFont, TextFormat("%s  %.0fE", te.name, cost),
                       {toolBtn.x + 8.0f, toolBtn.y + 5.0f}, FS(9.0f), sp, textCol);

            if (applied)
            {
                // Checkmark on the right edge
                float cxm = toolBtn.x + toolBtn.width - 14.0f;
                float cym = toolBtn.y + 12.0f;
                DrawLineEx({cxm - 4.0f, cym}, {cxm - 1.0f, cym + 3.5f}, 2.0f, EXT_ACCENT_GREEN);
                DrawLineEx({cxm - 1.0f, cym + 3.5f}, {cxm + 4.5f, cym - 3.5f}, 2.0f, EXT_ACCENT_GREEN);
            }

            if (hover && canApply && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (ProsChargeEnergy(unit, cost, te.name) &&
                    ps->GetLab().ApplyTool(*selSample, te.tool, ps->gameTime))
                {
                    ps->lastLabActionKind = 0;
                    ps->lastLabActionIndex = t;
                    ps->lastLabActionTime = ps->gameTime;
                }
            }
        }
        toolY += 3 * 27.0f;

        // Separation methods
        toolY += 8.0f;
        DrawTextEx(headerFont, "SEPARATION", {px, toolY}, FS(12.0f), sp, EXT_HEADER_COLOR);
        toolY += 20.0f;

        struct SepEntry { SeparationMethod method; const char* name; };
        SepEntry seps[] = {
            {SeparationMethod::MAGNETIC, "Magnetic"},
            {SeparationMethod::HEAVY_MINERAL, "Heavy Liquid"},
            {SeparationMethod::VOLATILE_EXTRACTION, "Volatile"},
        };

        for (int t = 0; t < 3; t++)
        {
            auto& se = seps[t];
            float cost = LabEngine::GetSeparationCost(se.method);
            bool canApply = selSample && ps->GetLab().CanApplySeparation(*selSample, se.method) &&
                            ProsCanAfford(unit, cost);
            Rectangle sepBtn = {px + (t % 2) * (toolColW + 6.0f), toolY + (t / 2) * 27.0f,
                                toolColW, 23.0f};
            bool hover = CheckCollisionPointRec(mouse, sepBtn);
            bool pressed = hover && canApply && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            bool applied = selSample && selSample->separationApplied == se.method;
            bool flash = (ps->lastLabActionKind == 1 && ps->lastLabActionIndex == t &&
                          ps->gameTime - ps->lastLabActionTime < 0.4f);

            Color fill = EXT_PANEL_BG2;
            if (pressed || flash) fill = {24, 70, 90, 255};
            else if (hover && canApply) fill = {16, 26, 44, 255};
            else if (applied) fill = {13, 30, 34, 255};

            DrawRectangleRounded(sepBtn, 0.3f, 4, fill);
            DrawRectangleRoundedLinesEx(sepBtn, 0.3f, 4, flash ? 2.0f : 1.0f,
                                        flash ? EXT_ACCENT_CYAN
                                              : (applied ? Fade(EXT_ACCENT_GREEN, 0.7f)
                                                         : (canApply ? PROS_BTN_BORDER : PROS_BTN_DISABLED)));

            Color textCol = canApply ? (hover ? PROS_BTN_TEXT_HOVER : PROS_BTN_TEXT)
                                     : (applied ? Fade(EXT_ACCENT_GREEN, 0.8f) : PROS_BTN_DISABLED);
            DrawTextEx(bodyFont, TextFormat("%s  %.0fE", se.name, cost),
                       {sepBtn.x + 8.0f, sepBtn.y + 5.0f}, FS(9.0f), sp, textCol);

            if (applied)
            {
                float cxm = sepBtn.x + sepBtn.width - 14.0f;
                float cym = sepBtn.y + 12.0f;
                DrawLineEx({cxm - 4.0f, cym}, {cxm - 1.0f, cym + 3.5f}, 2.0f, EXT_ACCENT_GREEN);
                DrawLineEx({cxm - 1.0f, cym + 3.5f}, {cxm + 4.5f, cym - 3.5f}, 2.0f, EXT_ACCENT_GREEN);
            }

            if (hover && canApply && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (ProsChargeEnergy(unit, cost, se.name) &&
                    ps->GetLab().ApplySeparation(*selSample, se.method, ps->gameTime))
                {
                    ps->lastLabActionKind = 1;
                    ps->lastLabActionIndex = t;
                    ps->lastLabActionTime = ps->gameTime;
                }
            }
        }

        // Right side: selected sample detail + analysis results
        DrawTextEx(headerFont, "RESULTS", {rightX, contentY}, FS(12.0f), sp, EXT_HEADER_COLOR);
        float resY = contentY + 22.0f;

        if (selSample)
        {
            // Large crystal preview on a glow disc, like the kit's orbs
            float previewSize = 96.0f;
            Rectangle preview = {rightX + rightW - previewSize - 12.0f, contentY + 6.0f,
                                 previewSize, previewSize};
            Vector2 previewCenter = {preview.x + previewSize / 2.0f, preview.y + previewSize / 2.0f};
            DrawCircleV(previewCenter, previewSize * 0.52f,
                        Fade(selSample->visual.elementColor, 0.10f));
            DrawCircleLines(static_cast<int>(previewCenter.x), static_cast<int>(previewCenter.y),
                            previewSize * 0.52f, Fade(selSample->visual.elementColor, 0.35f));
            DrawCrystalSprite(selSample->visual, preview);

            const char* stateStr = selSample->state == SampleState::IN_TRAY ? "In Tray" :
                                   selSample->state == SampleState::PROCESSING ? "Processing" : "Completed";
            DrawTextEx(bodyFont, TextFormat("State: %s", stateStr),
                       {rightX, resY}, FS(10.0f), sp, EXT_DIM_TEXT);
            resY += 16.0f;
            DrawTextEx(bodyFont, TextFormat("Depth: %s", ProsDepthName(selSample->depthLayer)),
                       {rightX, resY}, FS(10.0f), sp, EXT_DIM_TEXT);
            resY += 16.0f;
            DrawTextEx(bodyFont, TextFormat("Richness: %.0f%%", selSample->richness * 100.0f),
                       {rightX, resY}, FS(10.0f), sp, EXT_DIM_TEXT);
            resY += 16.0f;
            DrawTextEx(bodyFont, TextFormat("Analyses: %d", static_cast<int>(selSample->analysisHistory.size())),
                       {rightX, resY}, FS(10.0f), sp, EXT_DIM_TEXT);
            resY += 20.0f;

            // Element composition with confidence
            DrawTextEx(headerFont, "COMPOSITION", {rightX, resY}, FS(11.0f), sp, EXT_HEADER_COLOR);
            resY += 18.0f;

            for (const auto& [type, abundance] : selSample->trueComposition)
            {
                if (abundance < 0.01f) continue;
                float conf = 0.0f;
                auto cit = selSample->elementConfidence.find(type);
                if (cit != selSample->elementConfidence.end()) conf = cit->second;

                Color elemCol = ProsElementColor(type);
                if (conf < 0.01f)
                {
                    DrawTextEx(bodyFont, "  ???",
                               {rightX, resY}, FS(9.0f), sp, {80, 80, 100, 255});
                }
                else
                {
                    float barW = rightW - 130.0f;
                    float barH = 10.0f;
                    float barX = rightX + 60.0f;

                    DrawTextEx(bodyFont, ResourceTypeToString(type),
                               {rightX, resY}, FS(9.0f), sp, elemCol);

                    // Background bar
                    DrawRectangle(static_cast<int>(barX), static_cast<int>(resY + 2),
                                  static_cast<int>(barW), static_cast<int>(barH), {40, 40, 60, 255});

                    // Fill bar shows true abundance, alpha scales with confidence
                    Color fillCol = elemCol;
                    fillCol.a = static_cast<unsigned char>(80 + 175 * conf);
                    DrawRectangle(static_cast<int>(barX), static_cast<int>(resY + 2),
                                  static_cast<int>(barW * abundance), static_cast<int>(barH), fillCol);

                    // Show value with precision based on confidence. Low
                    // confidence reads as a coarser number in dimmer text
                    // rather than a "~" prefix: Exo 2 draws the tilde as a
                    // flat mid-height stroke set off from the digits, which
                    // at this size reads as a minus sign on a value that can
                    // never be negative.
                    const char* valText;
                    Color valCol;
                    if (conf < 0.3f)
                    {
                        // Nearest 5% -- the number itself carries the coarseness
                        float coarse = roundf(abundance * 100.0f / 5.0f) * 5.0f;
                        valText = TextFormat("%.0f%%", coarse);
                        valCol = EXT_DIM_TEXT;
                    }
                    else if (conf < 0.7f)
                    {
                        valText = TextFormat("%.0f%%", abundance * 100.0f);
                        valCol = {190, 195, 215, 255};
                    }
                    else
                    {
                        valText = TextFormat("%.1f%%", abundance * 100.0f);
                        valCol = WHITE;
                    }

                    DrawTextEx(bodyFont, valText,
                               {barX + barW + 4, resY}, FS(9.0f), sp, valCol);
                    DrawTextEx(bodyFont, TextFormat("[%.0f%%]", conf * 100.0f),
                               {barX + barW + 50.0f, resY}, FS(8.0f), sp, EXT_DIM_TEXT);
                }
                resY += 16.0f;
            }

            // Analysis history
            if (!selSample->analysisHistory.empty())
            {
                resY += 8.0f;
                DrawTextEx(headerFont, "HISTORY", {rightX, resY}, FS(10.0f), sp, EXT_HEADER_COLOR);
                resY += 16.0f;

                for (const auto& entry : selSample->analysisHistory)
                {
                    const char* toolName = "?";
                    switch (entry.tool)
                    {
                        case AnalysisTool::VISUAL_INSPECTION: toolName = "Visual"; break;
                        case AnalysisTool::XRF: toolName = "XRF"; break;
                        case AnalysisTool::OPTICAL_MICROSCOPY: toolName = "Optical"; break;
                        case AnalysisTool::MAGNETIC_SUSCEPTIBILITY: toolName = "Magnetic"; break;
                        case AnalysisTool::LIBS_PULSE: toolName = "LIBS"; break;
                        case AnalysisTool::FIRE_ASSAY: toolName = "Fire Assay"; break;
                        default: break;
                    }

                    DrawTextEx(bodyFont, TextFormat("  %s", toolName),
                               {rightX, resY}, FS(8.0f), sp, EXT_DIM_TEXT);
                    resY += 12.0f;

                    if (resY > static_cast<float>(y + h) - 90.0f) break;
                }
            }
        }
        else
        {
            DrawTextEx(bodyFont, "Select a sample to analyze.",
                       {rightX, resY}, FS(10.0f), sp, EXT_DIM_TEXT);
        }
    }

    // (Survey progress summary now lives in the shared bottom status bar.)
}

void RenderManager::DrawExcavationPanel(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;
    int padding = 15;

    float yPos = static_cast<float>(y + padding);
    float px = static_cast<float>(x + padding);
    Vector2 mousePos = GetMousePosition();

    DrawTextEx(headerFont, "EXCAVATION FLEET", {px, yPos}, FS(18.0f), sp, EXT_HEADER_COLOR);
    yPos += 28.0f;

    // Total stats
    DrawTextEx(bodyFont, TextFormat("Total Regolith Extracted: %.1f kg", unit->GetTotalRegolithExtracted()),
               {px, yPos}, FS(14.0f), sp, EXT_ACCENT_CYAN);
    yPos += 25.0f;

    // Fleet table
    const auto& excavators = unit->GetExcavators();
    if (excavators.empty())
    {
        DrawTextEx(bodyFont, "No excavators deployed", {px, yPos}, FS(13.0f), sp, EXT_DIM_TEXT);
        return;
    }

    // Get excavation tier for depth step and max depth
    int excTier = 0;
    float maxDepth = 10.0f;
    for (const auto& mod : unit->GetModules())
    {
        if (mod.moduleType == "EXCAVATION")
        {
            excTier = mod.tier;
            float tierMaxDepths[] = {10.0f, 30.0f, 100.0f, 300.0f};
            maxDepth = tierMaxDepths[std::min(excTier, 3)];
            break;
        }
    }
    float depthSteps[] = {1.0f, 5.0f, 10.0f, 30.0f};
    float depthStep = depthSteps[std::min(excTier, 3)];
    float rateStep = 5.0f;

    // Table header
    DrawTextEx(bodyFont, "ID", {px, yPos}, FS(12.0f), sp, EXT_DIM_TEXT);
    DrawTextEx(bodyFont, "Method", {px + 40.0f, yPos}, FS(12.0f), sp, EXT_DIM_TEXT);
    DrawTextEx(bodyFont, "Depth", {px + 140.0f, yPos}, FS(12.0f), sp, EXT_DIM_TEXT);
    DrawTextEx(bodyFont, "Rate", {px + 270.0f, yPos}, FS(12.0f), sp, EXT_DIM_TEXT);
    DrawTextEx(bodyFont, "Wear", {px + 380.0f, yPos}, FS(12.0f), sp, EXT_DIM_TEXT);
    yPos += 18.0f;

    DrawLine(static_cast<int>(px), static_cast<int>(yPos),
             static_cast<int>(px + w - padding * 2), static_cast<int>(yPos), EXT_PANEL_BORDER);
    yPos += 5.0f;

    float totalRate = 0.0f;
    float btnW = 20.0f;
    float btnH = 18.0f;

    for (const auto& exc : excavators)
    {
        DrawTextEx(bodyFont, TextFormat("#%d", exc.id), {px, yPos}, FS(12.0f), sp, LIGHTGRAY);
        DrawTextEx(bodyFont, exc.method.c_str(), {px + 40.0f, yPos}, FS(12.0f), sp, LIGHTGRAY);

        // --- Depth [-] value [+] ---
        float depthX = px + 140.0f;
        Rectangle depthMinus = {depthX, yPos - 1.0f, btnW, btnH};
        Rectangle depthPlus = {depthX + 90.0f, yPos - 1.0f, btnW, btnH};

        // [-] button
        Color minusBg = CheckCollisionPointRec(mousePos, depthMinus) ? Color{24, 38, 60, 255} : Color{16, 24, 42, 255};
        DrawRectangleRec(depthMinus, minusBg);
        DrawRectangleLinesEx(depthMinus, 1.0f, EXT_PANEL_BORDER);
        DrawTextEx(bodyFont, "-", {depthX + 6.0f, yPos}, FS(12.0f), sp, WHITE);

        if (CheckCollisionPointRec(mousePos, depthMinus) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            unit->SetExcavatorDepth(exc.id, exc.depth - depthStep);
        }

        // Value
        DrawTextEx(bodyFont, TextFormat("%.0f cm", exc.depth), {depthX + 24.0f, yPos}, FS(12.0f), sp, LIGHTGRAY);

        // [+] button
        Color plusBg = CheckCollisionPointRec(mousePos, depthPlus) ? Color{24, 38, 60, 255} : Color{16, 24, 42, 255};
        DrawRectangleRec(depthPlus, plusBg);
        DrawRectangleLinesEx(depthPlus, 1.0f, EXT_PANEL_BORDER);
        DrawTextEx(bodyFont, "+", {depthX + 96.0f, yPos}, FS(12.0f), sp, WHITE);

        if (CheckCollisionPointRec(mousePos, depthPlus) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            unit->SetExcavatorDepth(exc.id, exc.depth + depthStep);
        }

        // Max depth label
        DrawTextEx(bodyFont, TextFormat("/ %.0f", maxDepth), {depthX + 114.0f, yPos}, FS(10.0f), sp, EXT_DIM_TEXT);

        // --- Rate [-] value [+] ---
        float rateX = px + 270.0f;
        Rectangle rateMinus = {rateX, yPos - 1.0f, btnW, btnH};
        Rectangle ratePlus = {rateX + 85.0f, yPos - 1.0f, btnW, btnH};

        // [-] button
        Color rMinBg = CheckCollisionPointRec(mousePos, rateMinus) ? Color{24, 38, 60, 255} : Color{16, 24, 42, 255};
        DrawRectangleRec(rateMinus, rMinBg);
        DrawRectangleLinesEx(rateMinus, 1.0f, EXT_PANEL_BORDER);
        DrawTextEx(bodyFont, "-", {rateX + 6.0f, yPos}, FS(12.0f), sp, WHITE);

        if (CheckCollisionPointRec(mousePos, rateMinus) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            unit->SetExcavatorRate(exc.id, exc.rate - rateStep);
        }

        // Value
        DrawTextEx(bodyFont, TextFormat("%.0f", exc.rate), {rateX + 24.0f, yPos}, FS(12.0f), sp, LIGHTGRAY);

        // [+] button
        Color rPlsBg = CheckCollisionPointRec(mousePos, ratePlus) ? Color{24, 38, 60, 255} : Color{16, 24, 42, 255};
        DrawRectangleRec(ratePlus, rPlsBg);
        DrawRectangleLinesEx(ratePlus, 1.0f, EXT_PANEL_BORDER);
        DrawTextEx(bodyFont, "+", {rateX + 91.0f, yPos}, FS(12.0f), sp, WHITE);

        if (CheckCollisionPointRec(mousePos, ratePlus) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            unit->SetExcavatorRate(exc.id, exc.rate + rateStep);
        }

        // Wear bar
        DrawWearBar(px + 380.0f, yPos + 1.0f, 60.0f, 12.0f, exc.wear);

        totalRate += exc.rate;
        yPos += 24.0f;
    }

    yPos += 10.0f;
    DrawLine(static_cast<int>(px), static_cast<int>(yPos),
             static_cast<int>(px + w - padding * 2), static_cast<int>(yPos), EXT_PANEL_BORDER);
    yPos += 8.0f;

    DrawTextEx(headerFont, TextFormat("Total Rate: %.1f kg/hr", totalRate),
               {px, yPos}, FS(14.0f), sp, EXT_ACCENT_GREEN);
    DrawTextEx(bodyFont, TextFormat("Fleet Size: %d", static_cast<int>(excavators.size())),
               {px + 250.0f, yPos}, FS(13.0f), sp, LIGHTGRAY);
}

void RenderManager::DrawBeneficiationPanel(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;
    int padding = 15;

    float yPos = static_cast<float>(y + padding);
    float px = static_cast<float>(x + padding);
    Vector2 mousePos = GetMousePosition();

    DrawTextEx(headerFont, "SEPARATION CHAIN", {px, yPos}, FS(18.0f), sp, EXT_HEADER_COLOR);
    yPos += 28.0f;

    const auto& chain = unit->GetSeparationChain();
    if (chain.empty())
    {
        DrawTextEx(bodyFont, "No separation nodes configured", {px, yPos}, FS(13.0f), sp, EXT_DIM_TEXT);
        return;
    }

    float nodeW = static_cast<float>(w - padding * 2);
    float nodeH = 70.0f;
    float arrowBtnW = 30.0f;
    float arrowBtnH = 25.0f;

    for (size_t i = 0; i < chain.size(); i++)
    {
        const auto& node = chain[i];

        // Node background
        Color nodeBg = node.isActive ? Color{30, 40, 60, 255} : Color{30, 30, 40, 200};
        DrawRectangle(static_cast<int>(px), static_cast<int>(yPos),
                      static_cast<int>(nodeW), static_cast<int>(nodeH), nodeBg);
        DrawRectangleLines(static_cast<int>(px), static_cast<int>(yPos),
                           static_cast<int>(nodeW), static_cast<int>(nodeH), EXT_PANEL_BORDER);

        // --- Reorder buttons (left side) ---
        float reorderX = px + nodeW - 80.0f;

        // Up arrow (if not first)
        if (i > 0)
        {
            Rectangle upBtn = {reorderX, yPos + 3.0f, arrowBtnW, arrowBtnH};
            Color upBg = CheckCollisionPointRec(mousePos, upBtn) ? Color{60, 70, 90, 255} : Color{24, 38, 60, 255};
            DrawRectangleRec(upBtn, upBg);
            DrawRectangleLinesEx(upBtn, 1.0f, EXT_PANEL_BORDER);
            // Up arrow triangle
            DrawTriangle(
                {reorderX + arrowBtnW/2.0f, yPos + 6.0f},
                {reorderX + 5.0f, yPos + 22.0f},
                {reorderX + arrowBtnW - 5.0f, yPos + 22.0f},
                EXT_ACCENT_CYAN);

            if (CheckCollisionPointRec(mousePos, upBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                unit->SwapSeparationNodes(static_cast<int>(i), static_cast<int>(i) - 1);
            }
        }

        // Down arrow (if not last)
        if (i < chain.size() - 1)
        {
            Rectangle downBtn = {reorderX + arrowBtnW + 4.0f, yPos + 3.0f, arrowBtnW, arrowBtnH};
            Color downBg = CheckCollisionPointRec(mousePos, downBtn) ? Color{60, 70, 90, 255} : Color{24, 38, 60, 255};
            DrawRectangleRec(downBtn, downBg);
            DrawRectangleLinesEx(downBtn, 1.0f, EXT_PANEL_BORDER);
            // Down arrow triangle
            float dx2 = reorderX + arrowBtnW + 4.0f;
            DrawTriangle(
                {dx2 + 5.0f, yPos + 6.0f},
                {dx2 + arrowBtnW - 5.0f, yPos + 6.0f},
                {dx2 + arrowBtnW/2.0f, yPos + 22.0f},
                EXT_ACCENT_CYAN);

            if (CheckCollisionPointRec(mousePos, downBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                unit->SwapSeparationNodes(static_cast<int>(i), static_cast<int>(i) + 1);
            }
        }

        // Node type name
        const char* typeNames[] = {"SIZE SORT", "MAGNETIC", "ELECTROSTATIC", "THERMAL", "CHEMICAL", "MRE", "DIRECT OUTPUT"};
        int typeIdx = static_cast<int>(node.type);
        DrawTextEx(bodyFont, typeNames[typeIdx], {px + 10.0f, yPos + 5.0f}, FS(14.0f), sp, WHITE);

        // --- Clickable ON/OFF toggle ---
        Color activeColor = node.isActive ? EXT_ACCENT_GREEN : EXT_DIM_TEXT;
        const char* statusText = node.isActive ? "ON" : "OFF";
        float statusX = px + nodeW - 130.0f;
        Rectangle toggleBtn = {statusX, yPos + 3.0f, 35.0f, 18.0f};
        Color toggleBg = CheckCollisionPointRec(mousePos, toggleBtn) ? Color{24, 38, 60, 255} : Color{14, 21, 38, 255};
        DrawRectangleRounded(toggleBtn, 0.4f, 4, toggleBg);
        DrawRectangleRoundedLinesEx(toggleBtn, 0.4f, 4, 1.0f, activeColor);
        DrawTextEx(bodyFont, statusText, {statusX + 5.0f, yPos + 5.0f}, FS(12.0f), sp, activeColor);

        if (CheckCollisionPointRec(mousePos, toggleBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            unit->ToggleSeparationNodeActive(static_cast<int>(i));
        }

        // Efficiency bar
        DrawTextEx(bodyFont, "Eff:", {px + 10.0f, yPos + 25.0f}, FS(11.0f), sp, EXT_DIM_TEXT);
        DrawStyledBar(px + 40.0f, yPos + 25.0f, 120.0f, 12.0f, node.efficiency, EXT_ACCENT_CYAN);
        DrawTextEx(bodyFont, TextFormat("%.0f%%", node.efficiency * 100.0f),
                   {px + 165.0f, yPos + 24.0f}, FS(11.0f), sp, LIGHTGRAY);

        // Wear bar
        DrawTextEx(bodyFont, "Wear:", {px + 210.0f, yPos + 25.0f}, FS(11.0f), sp, EXT_DIM_TEXT);
        DrawWearBar(px + 250.0f, yPos + 25.0f, 80.0f, 12.0f, node.wear);
        DrawTextEx(bodyFont, TextFormat("%.0f%%", node.wear * 100.0f),
                   {px + 335.0f, yPos + 24.0f}, FS(11.0f), sp, LIGHTGRAY);

        // Energy & temperature
        DrawTextEx(bodyFont, TextFormat("Energy: %.0f kW", node.energyConsumption),
                   {px + 10.0f, yPos + 45.0f}, FS(11.0f), sp, LIGHTGRAY);

        if (node.temperature > 25.0f)
        {
            Color tempColor = node.temperature > 500.0f ? Color{255, 150, 50, 255} : LIGHTGRAY;
            DrawTextEx(bodyFont, TextFormat("Temp: %.0f C", node.temperature),
                       {px + 150.0f, yPos + 45.0f}, FS(11.0f), sp, tempColor);
        }

        // Waste ratio
        if (node.wasteRatio > 0)
        {
            DrawTextEx(bodyFont, TextFormat("Waste: %.0f%%", node.wasteRatio * 100.0f),
                       {px + 300.0f, yPos + 45.0f}, FS(11.0f), sp, Color{255, 100, 100, 255});
        }

        yPos += nodeH + 5.0f;

        // Arrow connector between nodes
        if (i < chain.size() - 1)
        {
            float arrowX = px + nodeW / 2.0f;
            DrawLine(static_cast<int>(arrowX), static_cast<int>(yPos - 5.0f),
                     static_cast<int>(arrowX), static_cast<int>(yPos + 8.0f), EXT_ACCENT_CYAN);
            DrawTriangle(
                {arrowX, yPos + 12.0f},
                {arrowX - 5.0f, yPos + 5.0f},
                {arrowX + 5.0f, yPos + 5.0f},
                EXT_ACCENT_CYAN);
            yPos += 15.0f;
        }
    }
}

void RenderManager::DrawOperationsPanel(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;
    int padding = 15;

    float yPos = static_cast<float>(y + padding);
    float px = static_cast<float>(x + padding);

    DrawTextEx(headerFont, "OPERATIONS MANAGEMENT", {px, yPos}, FS(18.0f), sp, EXT_HEADER_COLOR);
    yPos += 35.0f;

    // Efficiency modifier display
    float effMod = unit->GetOperationsEfficiencyModifier();
    bool isOpsActive = unit->IsOperationsActive();

    DrawTextEx(bodyFont, "Operations Status:", {px, yPos}, FS(14.0f), sp, LIGHTGRAY);
    yPos += 22.0f;

    Color activeColor = isOpsActive ? EXT_ACCENT_GREEN : EXT_DIM_TEXT;
    DrawTextEx(headerFont, isOpsActive ? "ACTIVE" : "INACTIVE", {px, yPos}, FS(20.0f), sp, activeColor);
    yPos += 35.0f;

    // Large efficiency display
    DrawTextEx(bodyFont, "Efficiency Modifier:", {px, yPos}, FS(14.0f), sp, LIGHTGRAY);
    yPos += 22.0f;

    Color effColor;
    if (effMod < 1.0f) effColor = YELLOW;
    else if (effMod > 1.0f) effColor = EXT_ACCENT_GREEN;
    else effColor = WHITE;

    DrawTextEx(headerFont, TextFormat("x%.2f", effMod), {px, yPos}, FS(36.0f), sp, effColor);
    yPos += 50.0f;

    // Tier description
    int idx = unit->GetSelectedModuleIndex();
    const auto& mod = unit->GetModules()[idx];

    const char* tierDescs[] = {
        "Tier 0: Basic operations\n  -15% efficiency penalty",
        "Tier 1: Standard operations\n  No efficiency bonus",
        "Tier 2: Optimized operations\n  +10% efficiency bonus",
        "Tier 3: AI scheduling\n  +20% efficiency bonus"
    };

    DrawTextEx(bodyFont, tierDescs[std::min(mod.tier, 3)], {px, yPos}, FS(13.0f), sp, LIGHTGRAY);
    yPos += 40.0f;

    // Survey coverage bonus from prospecting system
    float confidence = unit->HasProspectingSystem()
        ? unit->GetProspectingSystem()->GetSurveyProgress() : 0.0f;
    float confBonus = confidence * 0.10f;
    Color confColor = confidence >= 0.8f ? EXT_ACCENT_GREEN :
                      confidence >= 0.4f ? YELLOW : RED;

    DrawTextEx(bodyFont, "Survey Coverage Bonus:", {px, yPos}, FS(13.0f), sp, LIGHTGRAY);
    yPos += 20.0f;
    DrawTextEx(headerFont, TextFormat("+%.1f%%  (%.0f%% surveyed)",
               confBonus * 100.0f, confidence * 100.0f),
               {px, yPos}, FS(16.0f), sp, confColor);
}

void RenderManager::DrawDirectivesPanel(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;
    int padding = 15;

    float yPos = static_cast<float>(y + padding);
    float px = static_cast<float>(x + padding);
    Vector2 mousePos = GetMousePosition();

    DrawTextEx(headerFont, "ACTIVE DIRECTIVES", {px, yPos}, FS(18.0f), sp, EXT_HEADER_COLOR);
    yPos += 28.0f;

    const auto& directive = unit->GetDirective();

    const char* directiveNames[] = {
        "NONE", "PRIORITIZE", "MAXIMIZE", "CONSERVE",
        "EXPLORATION MODE", "EMERGENCY HARVEST", "THERMAL SYNC"
    };
    const char* directiveDescs[] = {
        "Normal operation.",
        "Focus on specific resource (+40%).",
        "Max output (+25%), more energy.",
        "Conserve energy (-30% output).",
        "Prospecting focus (-50% extraction).",
        "Emergency harvest (+50%, +wear).",
        "Sync with thermal cycles."
    };

    // Current directive display
    int dirIdx = static_cast<int>(directive.type);
    DrawTextEx(bodyFont, "Current:", {px, yPos}, FS(13.0f), sp, LIGHTGRAY);
    Color dirColor = (dirIdx == 0) ? EXT_DIM_TEXT : EXT_ACCENT_CYAN;
    DrawTextEx(headerFont, directiveNames[dirIdx], {px + 65.0f, yPos - 2.0f}, FS(16.0f), sp, dirColor);
    yPos += 22.0f;

    // Target resource for PRIORITIZE
    if (directive.type == Unit::DirectiveType::PRIORITIZE)
    {
        std::string resName = ResourceUtils::GetResourceName(directive.targetResource);
        Color resColor = ResourceUtils::GetResourceColor(directive.targetResource);
        DrawTextEx(bodyFont, TextFormat("Target: %s", resName.c_str()), {px, yPos}, FS(13.0f), sp, resColor);
        yPos += 18.0f;
    }

    yPos += 10.0f;

    // --- Get directives tier for gating ---
    int directivesTier = 0;
    bool directivesActive = false;
    for (const auto& mod : unit->GetModules())
    {
        if (mod.moduleType == "DIRECTIVES" && mod.isActive)
        {
            directivesTier = mod.tier;
            directivesActive = true;
            break;
        }
    }

    DrawTextEx(headerFont, "AVAILABLE DIRECTIVES", {px, yPos}, FS(14.0f), sp, EXT_HEADER_COLOR);
    yPos += 20.0f;

    // Directive cards
    float cardH = 42.0f;
    float cardW = static_cast<float>(w - padding * 2);

    // Tier requirements: Tier 0 = NONE only, Tier 1 = +PRIORITIZE/MAXIMIZE/CONSERVE, Tier 2+ = all
    int minTierRequired[] = {0, 1, 1, 1, 2, 2, 2};

    for (int d = 0; d < 7; d++)
    {
        Unit::DirectiveType dType = static_cast<Unit::DirectiveType>(d);
        bool isCurrentDirective = (directive.type == dType);
        bool isUnlocked = directivesActive && directivesTier >= minTierRequired[d];
        // NONE is always available
        if (d == 0) isUnlocked = true;

        Rectangle card = {px, yPos, cardW, cardH};

        // Card background
        Color cardBg;
        if (isCurrentDirective)
        {
            cardBg = {16, 44, 56, 255};  // Teal-tinted active, matches selection scheme
        }
        else if (isUnlocked)
        {
            cardBg = CheckCollisionPointRec(mousePos, card) ? Color{24, 38, 60, 255} : Color{14, 21, 38, 255};
        }
        else
        {
            cardBg = {11, 16, 30, 200};  // Dim locked
        }
        DrawRectangleRounded(card, 0.2f, 4, cardBg);

        // Border
        if (isCurrentDirective)
        {
            DrawRectangleRoundedLinesEx(card, 0.2f, 4, 1.5f, EXT_ACCENT_CYAN);
        }
        else
        {
            DrawRectangleRoundedLinesEx(card, 0.2f, 4, 1.0f, EXT_PANEL_BORDER);
        }

        // Directive name
        Color nameColor = isUnlocked ? WHITE : EXT_DIM_TEXT;
        DrawTextEx(bodyFont, directiveNames[d], {px + 10.0f, yPos + 5.0f}, FS(13.0f), sp, nameColor);

        // Description
        Color descColor = isUnlocked ? LIGHTGRAY : Color{80, 80, 90, 255};
        DrawTextEx(bodyFont, directiveDescs[d], {px + 10.0f, yPos + 22.0f}, FS(10.0f), sp, descColor);

        // Tier requirement label for locked
        if (!isUnlocked)
        {
            DrawTextEx(bodyFont, TextFormat("Tier %d", minTierRequired[d]),
                       {px + cardW - 50.0f, yPos + 12.0f}, FS(11.0f), sp, EXT_DIM_TEXT);
        }

        // Click to select unlocked directive
        if (isUnlocked && !isCurrentDirective &&
            CheckCollisionPointRec(mousePos, card) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            Unit::ActiveDirective newDir;
            newDir.type = dType;
            newDir.targetResource = ResourceType::Fe;
            newDir.strength = 1.0f;
            unit->SetDirective(newDir);
        }

        yPos += cardH + 3.0f;
    }

    // --- PRIORITIZE resource selector ---
    if (directive.type == Unit::DirectiveType::PRIORITIZE)
    {
        yPos += 8.0f;
        DrawTextEx(headerFont, "TARGET RESOURCE", {px, yPos}, FS(13.0f), sp, EXT_HEADER_COLOR);
        yPos += 18.0f;

        ResourceType resources[] = {
            ResourceType::Fe, ResourceType::Ti, ResourceType::Si, ResourceType::Al,
            ResourceType::Ca, ResourceType::H2, ResourceType::O2, ResourceType::C
        };
        const char* resLabels[] = {"Fe", "Ti", "Si", "Al", "Ca", "H2", "O2", "C"};

        float chipW = 42.0f;
        float chipH = 24.0f;
        float chipX = px;

        for (int r = 0; r < 8; r++)
        {
            // Wrap to next row if needed
            if (chipX + chipW > px + cardW)
            {
                chipX = px;
                yPos += chipH + 4.0f;
            }

            Rectangle chip = {chipX, yPos, chipW, chipH};
            bool isSelected = (directive.targetResource == resources[r]);
            Color chipBg = isSelected ? Color{16, 44, 56, 255} : Color{14, 21, 38, 255};
            if (!isSelected && CheckCollisionPointRec(mousePos, chip))
            {
                chipBg = {24, 38, 60, 255};
            }

            DrawRectangleRounded(chip, 0.4f, 4, chipBg);
            Color chipBorder = isSelected ? EXT_ACCENT_CYAN :
                               ResourceUtils::GetResourceColor(resources[r]);
            DrawRectangleRoundedLinesEx(chip, 0.4f, 4, isSelected ? 1.5f : 1.0f, chipBorder);

            Color labelColor = isSelected ? EXT_ACCENT_CYAN :
                               ResourceUtils::GetResourceColor(resources[r]);
            DrawTextEx(bodyFont, resLabels[r], {chipX + 8.0f, yPos + 5.0f}, FS(12.0f), sp, labelColor);

            if (CheckCollisionPointRec(mousePos, chip) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                Unit::ActiveDirective newDir;
                newDir.type = Unit::DirectiveType::PRIORITIZE;
                newDir.targetResource = resources[r];
                newDir.strength = 1.0f;
                unit->SetDirective(newDir);
            }

            chipX += chipW + 4.0f;
        }
    }
}


// --- Orbital view ---------------------------------------------------------

void RenderManager::LoadOrbitalAssets() {
    if (orbitalAssetsLoaded) return;
    orbitalNearTexture = LoadTexture("src/assets/planet/orbital_near.png");
    orbitalFarTexture  = LoadTexture("src/assets/planet/orbital_far.png");
    if (orbitalNearTexture.id == 0) {
        std::cout << "WARNING: failed to load orbital_near.png" << std::endl;
    }
    if (orbitalFarTexture.id == 0) {
        std::cout << "WARNING: failed to load orbital_far.png" << std::endl;
    }
    orbitalAssetsLoaded = true;
}

void RenderManager::UnloadOrbitalAssets() {
    if (!orbitalAssetsLoaded) return;
    if (orbitalNearTexture.id != 0) UnloadTexture(orbitalNearTexture);
    if (orbitalFarTexture.id  != 0) UnloadTexture(orbitalFarTexture);
    orbitalAssetsLoaded = false;
}

void RenderManager::DrawOrbitalView() {
    if (!orbitalAssetsLoaded) {
        LoadOrbitalAssets();
    }

    Color spaceBg = {6, 7, 12, 255};
    ClearBackground(spaceBg);

    // Centre the disc in the screen. The texture is square (1200x1200).
    // If the screen is smaller in either dimension, the texture overflows
    // the visible area — that's fine, the disc is still centred.
    if (orbitalNearTexture.id != 0) {
        int x = (GetScreenWidth()  - orbitalNearTexture.width)  / 2;
        int y = (GetScreenHeight() - orbitalNearTexture.height) / 2;
        DrawTexture(orbitalNearTexture, x, y, WHITE);
    }

    DrawText("Lunar Orbit",            20, 20, 26, RAYWHITE);
    DrawText("ENTER  descend to surface",
             20, GetScreenHeight() - 60, 18, LIGHTGRAY);
    DrawText("ESC    return to menu",
             20, GetScreenHeight() - 36, 18, LIGHTGRAY);
}
