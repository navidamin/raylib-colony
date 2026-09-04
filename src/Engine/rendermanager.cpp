#include "rendermanager.h"
#include "rlgl.h"
#include "web_mouse.h"
#include "resource_manager.h"
#include "resource_types.h"
#include "survey_progress_engine.h"
#include "excavation_constants.h"
#include "block_pick.h"
#include "rock_texture.h"
#include <algorithm>
#include <iostream>
#include <cmath>

RenderManager::RenderManager(int screenWidth, int screenHeight)
    : screenWidth(screenWidth),
      screenHeight(screenHeight),
      fontsLoaded(false),
      tilesLoaded(false)
{
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
    UnloadStrataTextures();

    // Unload cached crystal sample sprites
    for (auto& [path, texture] : crystalTextures)
    {
        UnloadTexture(texture);
    }
    crystalTextures.clear();
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
    Texture2D image = LoadTexture("src/assets/Logo.png");

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

        // Load and render moon tiles if not already loaded
        if (!tilesLoaded) {
            LoadMoonTiles();
            GenerateTilePattern();
            tilesLoaded = true;
        }

        // Render the tiled moon surface
        RenderMoonSurface();
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
        // Load and render moon tiles if not already loaded
        if (!tilesLoaded) {
            LoadMoonTiles();
            GenerateTilePattern();
            tilesLoaded = true;
        }

        // Render the tiled moon surface
        RenderMoonSurface();

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
                Vector2 mouse = ColonyGetMousePosition();
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

void RenderManager::DrawSectView(Sect* sect, TimeManager& timeManager) {
    if (sect) {
        sect->DrawInSectView(Vector2{screenWidth/2.0f, screenHeight/2.0f});
    }

    // Draw UI elements including time
    timeManager.Draw(screenWidth, screenHeight);
    DrawText("Sect View", 10, 10, 20, BLACK);
    DrawText("Press U for Unit View", 10, 40, 20, GRAY);
    DrawText("Press C for Colony View", 10, 70, 20, GRAY);

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
                Vector2 mouse = ColonyGetMousePosition();
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

    // Route extraction units to the new dark-themed UI
    if (unit->GetUnitType() == "Extraction")
    {
        DrawExtractionUnitView(unit, timeManager);
        return;
    }

    // Non-extraction units use the old rendering path
    unit->DrawInUnitView();
    DrawText("Unit View", 10, 10, 20, BLACK);
    DrawText("Press S for Sect View", 10, 40, 20, GRAY);
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

// The strata textures: four procedural rocks, built once. Power-of-two and
// REPEAT-wrapped, because the strip tiles them down a band of any height and
// WebGL only repeats POT textures (a clamped tile shows its edges at once).
void RenderManager::LoadStrataTextures() {
    for (int L = 0; L < 4; L++) {
        std::vector<unsigned char> px = RockTexture::Generate(L, RockTexture::SIZE);
        Image img = { px.data(), RockTexture::SIZE, RockTexture::SIZE, 1,
                      PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        strataTex[L] = LoadTextureFromImage(img);
        if (strataTex[L].id != 0) {
            SetTextureFilter(strataTex[L], TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(strataTex[L], TEXTURE_WRAP_REPEAT);
        }
    }
    strataLoaded = true;
}

void RenderManager::UnloadStrataTextures() {
    for (int L = 0; L < 4; L++) {
        if (strataTex[L].id != 0) {
            UnloadTexture(strataTex[L]);
            strataTex[L].id = 0;
        }
    }
    strataLoaded = false;
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

// Inferred needs its own token. EXT_ACCENT_VIOLET {170,110,255} cannot be
// reused -- it is within a few units of EXT_HEADER_COLOR {168,130,255}, so
// section headings and Inferred ground would read as the same thing. The
// muted violet is deliberate: Inferred is the class the eye should settle
// on least.
static const Color EXT_CLASS_INFERRED = {124, 143, 214, 255};

// One colour key for the three named classes, used identically by the grid,
// the readout and the resource ring. Green is minable now, violet is not.
static Color ExtClassColor(ResourceClass cls)
{
    switch (cls)
    {
        case ResourceClass::MEASURED:  return EXT_ACCENT_GREEN;
        case ResourceClass::INDICATED: return EXT_ACCENT_GOLD;
        case ResourceClass::INFERRED:  return EXT_CLASS_INFERRED;
        default:                       return EXT_DIM_TEXT;
    }
}
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
    FLASK, SLIDERS, BOLT, WARNING, OVERVIEW, HAMBURGER
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
    }
}

// Per-module icon lookup for the module list and status segments.
static ExtIcon ExtModuleIcon(const std::string& moduleType)
{
    if (moduleType == "PROSPECTING") return ExtIcon::RADAR;
    if (moduleType == "EXCAVATION") return ExtIcon::EXCAVATOR;
    if (moduleType == "BENEFICIATION") return ExtIcon::NODES;
    if (moduleType == "OPERATIONS") return ExtIcon::GEAR;
    if (moduleType == "DIRECTIVES") return ExtIcon::CROSSHAIR;
    return ExtIcon::OVERVIEW;
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

// Scissor rects are framebuffer-pixel, not matrix-transformed: under a
// supersampled web build every BeginScissorMode multiplies by this.
static float gPixelScale = 1.0f;
void RenderManager::SetPixelScale(float scale)
{
    gPixelScale = scale > 0.0f ? scale : 1.0f;
}

// Diagonal hazard stripes clipped to a rectangle (danger button edges).
static void ExtDrawHazardStripes(Rectangle r, Color c)
{
    const float stride = 14.0f;
    BeginScissorMode(static_cast<int>(r.x * gPixelScale),
                     static_cast<int>(r.y * gPixelScale),
                     static_cast<int>(r.width * gPixelScale),
                     static_cast<int>(r.height * gPixelScale));
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

// Decorative blueprint drawing of the extraction unit (control panel art).
static void ExtDrawWireframeUnit(Rectangle area, Color c)
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

// ============================================================================

void RenderManager::DrawExtractionUnitView(Unit* unit, TimeManager& timeManager)
{
    // Full dark background
    DrawRectangle(0, 0, screenWidth, screenHeight, EXT_SCREEN_BG);

    DrawExtractionTopBar(unit, timeManager);
    DrawExtractionBottomBar(unit);
    DrawExtractionModuleList(unit);
    DrawExtractionModuleCenter(unit);
    DrawExtractionControlPanel(unit);

    // Update message fade (done in unit since it owns the state)
    // The unit still handles UpdateMessage in its own Update/Draw cycle
}

void RenderManager::DrawExtractionTopBar(Unit* unit, TimeManager& timeManager)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;

    DrawRectangle(0, 0, screenWidth, EXT_TOP_BAR_H, EXT_PANEL_BG);
    DrawLine(0, EXT_TOP_BAR_H, screenWidth, EXT_TOP_BAR_H, EXT_PANEL_BORDER);

    float midY = EXT_TOP_BAR_H / 2.0f;

    // Unit icon chip
    Rectangle chip = {14.0f, midY - 16.0f, 32.0f, 32.0f};
    DrawRectangleRounded(chip, 0.3f, 4, EXT_PANEL_BG2);
    DrawRectangleRoundedLinesEx(chip, 0.3f, 4, 1.0f, EXT_PANEL_BORDER);
    ExtDrawIcon(ExtIcon::EXCAVATOR, chip.x + 16.0f, chip.y + 16.0f, 9.0f, EXT_ACCENT_CYAN);

    // Unit title + status, measured so they never collide
    float titleSize = FS(18.0f);
    const char* title = "EXTRACTION UNIT";
    float titleX = chip.x + chip.width + 14.0f;
    Vector2 titleDim = MeasureTextEx(headerFont, title, titleSize, sp);
    DrawTextEx(headerFont, title, {titleX, midY - titleDim.y / 2.0f}, titleSize, sp, EXT_TEXT);

    bool isActive = unit->IsActive();
    Color statusColor = isActive ? EXT_ACCENT_CYAN : EXT_ACCENT_RED;
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

void RenderManager::DrawExtractionBottomBar(Unit* unit)
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
        segments.push_back({ExtIcon::FLASK, "CORES",
                            TextFormat("%.0f%%", sr.sampleConfidence * 100.0f),
                            TextFormat("CAL: %.0f%%", calQ * 100.0f),
                            calQ >= 0.8f ? EXT_ACCENT_GREEN : EXT_ACCENT_GOLD});
        // TESTING is gone with the lab. The slot shows the survey total
        // instead, which is the number the extraction pipeline multiplies by.
        segments.push_back({ExtIcon::SLIDERS, "SURVEY",
                            TextFormat("%.0f%%", sr.surveyProgress * 100.0f),
                            TextFormat("TIER %d", ps->GetTier()),
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

void RenderManager::DrawExtractionModuleList(Unit* unit)
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
    bool overviewHovered = CheckCollisionPointRec(ColonyGetMousePosition(), overviewBtn);
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
        bool isHovered = CheckCollisionPointRec(ColonyGetMousePosition(), btn);
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

void RenderManager::DrawExtractionModuleCenter(Unit* unit)
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
        DrawExtractionResourceOverview(unit, panelX, panelY, panelW, panelH);
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
        DrawExtractionResourceOverview(unit, panelX, panelY, panelW, panelH);
}

// --- Right Panel: Controls ---

void RenderManager::DrawExtractionControlPanel(Unit* unit)
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
        bool isHovered = CheckCollisionPointRec(ColonyGetMousePosition(), buildBtn);

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
        bool isHovered = CheckCollisionPointRec(ColonyGetMousePosition(), upgradeBtn);

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
    bool isHovered = CheckCollisionPointRec(ColonyGetMousePosition(), toggleBtn);

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
                             EXT_ACCENT_CYAN);

        float tagY = panel.y + panel.height - 40.0f;
        DrawTextEx(bodyFont, "// UE-3 //", {panel.x + 18.0f, tagY}, FS(10.0f), sp,
                   Fade(EXT_DIM_TEXT, 0.8f));

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

void RenderManager::DrawExtractionResourceOverview(Unit* unit, int x, int y, int w, int h)
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

// Sweep signal, as a single-hue luminance ramp.
//
// It used to run navy -> cyan -> green -> magenta, which collided head-on
// with the class ring drawn over it: green meant both "strong signal" and
// "Measured", and cyan is already the selection accent. Two variables on the
// same cell need two different channels, so signal is now INTENSITY along one
// plum family and class is HUE on the ring. Per the implementation plan, when
// shade and class clash, class wins -- the signal number is on the readout
// anyway, and confidence has no other home.
static Color ProsSweepHeatColor(float signal)
{
    if (signal < 0.05f) return {16,  14,  42, 102};
    if (signal < 0.15f) return {38,  22,  72, 102};
    if (signal < 0.30f) return {68,  30, 104, 102};
    if (signal < 0.50f) return {108, 38, 132, 102};
    if (signal < 0.70f) return {154, 46, 152, 102};
    if (signal < 0.85f) return {200, 58, 168, 102};
    return {242, 86, 190, 102};
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
// The class ring.
//
// The cell's FILL already carries sweep signal, so class goes on the border
// instead of competing for the same pixels: how much is there is the shade,
// how well you know it is the ring. Unclassified ground draws no ring at all,
// which is what makes surveyed ground stand out from blind ground at a
// glance across the whole lattice.
static void ProsDrawClassRing(Rectangle r, ResourceClass cls)
{
    if (cls == ResourceClass::UNCLASSIFIED) return;

    Color c = ExtClassColor(cls);
    float thickness = (cls == ResourceClass::MEASURED) ? 2.2f : 1.6f;
    DrawRectangleRoundedLinesEx({r.x + 1.0f, r.y + 1.0f, r.width - 2.0f, r.height - 2.0f},
                                0.22f, 4, thickness, c);
}

// ---------------------------------------------------------------------------
// The block model: four depth layers as exploded isometric plates.
//
// Replaces the flat one-depth-at-a-time grid. Three channels carry three
// variables and none of them fight:
//
//     height of the surface  = grade      how good
//     colour of the surface  = class      how sure
//     position in the stack  = depth      how hard to reach
//
// Height for grade is what makes it legible at a glance: an ore body becomes
// a hill, and a hill has an obvious peak, shape and extent. Sixty-four shaded
// squares do not. See docs/design/prospecting/block-model-design.md.
// ---------------------------------------------------------------------------

struct BlockCell
{
    float grade = 0.0f;          // targeted-resource yield, normalised 0-1
    ResourceClass cls = ResourceClass::UNCLASSIFIED;
};

// Screen geometry for the stack. GAP is DERIVED, never hand-tuned: an
// isometric diamond of an NxN lattice is 2*N*tileY tall, and relief lifts its
// surface up to reliefMax above its own base plane, so the next plate can only
// begin below both plus a visible clearance. Hand-tuning this is exactly how
// plates end up silently overlapping.
struct BlockModelGeom
{
    float tileX = 0.0f, tileY = 0.0f, relief = 0.0f, gap = 0.0f;
    float originX = 0.0f, originY = 0.0f;
    int   size = PROSPECTING_GRID_SIZE;
    // How far this plate is pushed down so that its HIGHEST point lands on
    // its own slot, whatever its data. Filled in once the layers are built.
    //
    // Without it a plate's position depends on how rich it is relative to the
    // rest of the stack: lift is normalised against the max grade across ALL
    // four layers, so on an unsurveyed field -- every cell holding its own
    // layer mean -- the richest layer floats to the full relief and the
    // poorer ones sit lower, by more the poorer they are. Measured on the
    // playtest: plate-to-line gaps of 24 / 27 / 35 / 40 px going down, read
    // as the stack drifting away from its own borders.
    //
    // Amplitude still comes from the shared scale, so a barren layer is still
    // visibly flatter than the ore -- it is only the plate's PLACEMENT that
    // is now its own business.
    float plateDrop[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    Vector2 Iso(float gx, float gy, int layer, float lift) const
    {
        int L = layer < 0 ? 0 : (layer > 3 ? 3 : layer);
        return { originX + (gx - gy) * tileX,
                 originY + (gx + gy) * tileY + layer * gap + plateDrop[L] - lift };
    }
};

static BlockModelGeom MakeBlockGeom(int gridSize, float x, float y, float w, float h)
{
    BlockModelGeom g;
    g.size = gridSize;

    // Fit the whole stack inside the area given, then derive the gap from it.
    g.tileX = (w - 84.0f) / (2.0f * gridSize);
    // Flatter than a 2:1 isometric on purpose, for two reasons. Four exploded
    // plates plus their relief have to fit one panel, and every degree of
    // extra tilt costs vertical budget that then gets scaled back out of the
    // width -- at 0.46 the stack shrank 42% and the model used less than half
    // the space it had. And the flatter the plate, the more plainly it reads
    // as a HORIZONTAL PLANE at one depth: tilt invites the axis running away
    // from the viewer to be read as depth, which is exactly the misreading
    // PlateDepthM exists to kill. 0.28 -> 0.22 on that second count.
    g.tileY = g.tileX * 0.22f;
    float diamondH = 2.0f * gridSize * g.tileY;
    // Relief 0.30 -> 0.45 -> 0.60 across two playtest rounds ("the curvature
    // is not enough to be clearly visible"). Height alone never reads well
    // from a flat-lit iso plate, so DrawBlockLayer also hill-shades the
    // surface by slope -- which is what finally makes the shape legible --
    // and the gap below is derived from this, so plates never overlap.
    g.relief = std::max(24.0f, diamondH * 0.60f);
    const float clearance = 7.0f;
    g.gap = diamondH + g.relief + clearance;

    float stackH = 3.0f * g.gap + diamondH + g.relief;
    float scale = std::min(1.0f, (h - 14.0f) / stackH);
    g.tileX *= scale; g.tileY *= scale; g.relief *= scale; g.gap *= scale;

    g.originX = x + 74.0f + gridSize * g.tileX;
    g.originY = y + g.relief * scale + 6.0f;
    return g;
}

// One plate. Painter's algorithm front-to-back within the plate; corner
// heights are averaged from the blocks that touch them, so the surface reads
// as ground rather than as data resolution.
// The strata palette, shared by the borehole strip (full strength) and the
// plates' resting tone (dimmed) -- declared ahead of both users.
// Basalt was {39,42,48}: so dark that everything drawn on it -- vesicles,
// joints, the whole texture -- was multiplied down into a flat smudge, and
// the band read as dim rather than as dense. Lifted enough to carry detail,
// and NEUTRAL rather than blue: the first lift went to {52,56,64} and put it
// in the fractured layer's cool grey, so the two deepest bands stopped
// telling apart. Blue is now layer 2's alone, and basalt is still plainly
// the darkest rock in the column.
static const Color DP_ROCK_COL[4]  = {{58,52,43,255},{69,62,52,255},{57,66,77,255},{53,52,55,255}};
// How the one texture per stratum is laid into each projection. In the strip
// it is EXACTLY 1:1 -- one texel per screen pixel. This was 118, a 0.92
// minification, and that alone was enough to average the finest grain into a
// wash: anything a pixel or two across arrived blurred, so regolith read as
// mush next to basalt's chunkier vesicles. A texture whose smallest feature
// is a pixel has to be sampled at the size it was drawn.
// On a plate, which is far wider, one stretched tile would magnify the grain
// past recognition, so it repeats: at x2 a clast on the plate is about the
// size of the same clast in the band.
static constexpr float DP_ROCK_TEX_PX = static_cast<float>(RockTexture::SIZE);
static constexpr float BLOCK_TEX_REPEAT = 2.0f;
// How far a plate's DRAWN surface hangs below its stratum's top boundary,
// as a fraction of the plate diamond's half-height (see DockFromBlock).
static constexpr float BLOCK_PLATE_TUCK = 0.50f;

// The y of a stratum's top boundary -- the line the plate hangs under, the
// line the strip's band starts at, and the line the depth label names.
// Derived in ONE place so those three cannot drift apart; they did, the first
// time the plates moved and the labels stayed anchored to the plate instead.
static float BlockPlateLineY(const BlockModelGeom& g, int layer)
{
    // The plate's own ceiling sits at its slot (see plateDrop), so the line is
    // simply a little above that -- no relief term, and the same offset for
    // every plate however rich or poor its layer is.
    float slot = g.originY + layer * g.gap + g.size * g.tileY;
    return slot - g.size * g.tileY * BLOCK_PLATE_TUCK;
}


// The one lift law. Everything that has to sit ON a plate's surface -- the
// plate itself, the hover outline, the pick, the ends of the prescribed line
// -- goes through these, so none of them can drift from the drawn ground
// when the relief or the law changes (the x1.5 relief pass put the rim a
// plate-height under its plate; the x2 lattice would have done it again).
// Corner heights average the blocks that touch them, so the surface reads
// as ground rather than as data resolution; the 0.8 power lifts the middle
// grades, which is where most of a mound's flank lives.
static float BlockCornerLift(const std::vector<BlockCell>& cells, int N, float maxGrade,
                            float relief, int i, int j)
{
    float sum = 0.0f; int n = 0;
    for (int dj = -1; dj <= 0; dj++)
        for (int di = -1; di <= 0; di++)
        {
            int a = i + di, b = j + dj;
            if (a < 0 || b < 0 || a >= N || b >= N) continue;
            sum += cells[b * N + a].grade; n++;
        }
    if (n == 0) return 0.0f;
    float t = std::min((sum / n) / std::max(maxGrade, 0.0001f), 1.0f);
    return powf(t, 0.8f) * relief;
}
// The surface at a cell's centre: the mean of its four drawn corners.
static float BlockCellLift(const std::vector<BlockCell>& cells, int N, float maxGrade,
                          float relief, int i, int j)
{
    i = std::clamp(i, 0, N - 1); j = std::clamp(j, 0, N - 1);
    return 0.25f * (BlockCornerLift(cells, N, maxGrade, relief, i,     j)
                  + BlockCornerLift(cells, N, maxGrade, relief, i + 1, j)
                  + BlockCornerLift(cells, N, maxGrade, relief, i + 1, j + 1)
                  + BlockCornerLift(cells, N, maxGrade, relief, i,     j + 1));
}
// What the plates were drawn with this frame, handed to whatever draws on them.
struct BlockPlateLift
{
    const std::vector<std::vector<BlockCell>>* layers = nullptr;
    float maxGrade = 0.0001f;
    float At(const BlockModelGeom& g, int L, int i, int j) const
    {
        if (!layers || L < 0 || L >= static_cast<int>(layers->size())) return 0.0f;
        return BlockCellLift((*layers)[L], g.size, maxGrade, g.relief, i, j);
    }
};

static void DrawBlockLayer(const BlockModelGeom& g, const std::vector<BlockCell>& cells,
                               int layer, float maxGrade, float plateLight,
                               Font labelFont, float sp,
                               const char* depthLabel, const char* levelLabel,
                               std::vector<Rectangle>* hitBoxes, std::vector<int>* hitIndex,
                               const Texture2D* tex = nullptr, float rimPulse = 0.0f)
{
    const int N = g.size;
    // How lit this plate is, decided by the focus law on the facade and
    // eased there. It used to be powf(0.84, layer) -- depth alone, so all
    // four competed for attention at once and the deep ones were never fully
    // readable however long you looked at them.
    const float fade = plateLight;
    // The stratum's generated rock (rock_texture.h), laid over the plate so
    // a clast spans about a cell -- the same ground the borehole strip shows
    // in section, read here as a surface instead of as data. Modulated by the
    // cell's fill, so class colour and stratum tone survive underneath it.
    // The gain is exactly 2 against the texture's mean of exactly 128: a
    // textured plate averages the tone the flat fill would have had, which is
    // why texture could be added without re-tuning a single palette entry.
    const bool textured = tex != nullptr && tex->id != 0;
    const float texGain = 2.0f;

    auto cornerLift = [&](int i, int j) -> float
    {
        return BlockCornerLift(cells, N, maxGrade, g.relief, i, j);
    };
    auto clamp255 = [](float v) {
        return static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
    };
    if (textured) rlSetTexture(tex->id);

    for (int j = 0; j < N; j++)
    {
        for (int i = 0; i < N; i++)
        {
            const BlockCell& c = cells[j * N + i];
            float l00 = cornerLift(i, j),     l10 = cornerLift(i + 1, j);
            float l11 = cornerLift(i + 1, j + 1), l01 = cornerLift(i, j + 1);
            Vector2 q[4] = {
                g.Iso(static_cast<float>(i),     static_cast<float>(j),     layer, l00),
                g.Iso(static_cast<float>(i + 1), static_cast<float>(j),     layer, l10),
                g.Iso(static_cast<float>(i + 1), static_cast<float>(j + 1), layer, l11),
                g.Iso(static_cast<float>(i),     static_cast<float>(j + 1), layer, l01)
            };

            // A plate's resting tone IS its stratum: the same DP_ROCK_COL
            // the borehole strip paints full-strength, dimmed to a quieter
            // twin. Colour = class still holds, but the class fades toward
            // the rock as certainty falls -- MEASURED speaks in full class
            // colour, INFERRED is mostly a guess so it is mostly rock, and
            // UNCLASSIFIED ground simply looks like its stratum. Relief
            // lightness keeps tracking believed grade.
            Color rock = DP_ROCK_COL[layer];
            Color ground = {
                static_cast<unsigned char>(EXT_PANEL_BG.r + (rock.r - EXT_PANEL_BG.r) * 0.80f),
                static_cast<unsigned char>(EXT_PANEL_BG.g + (rock.g - EXT_PANEL_BG.g) * 0.80f),
                static_cast<unsigned char>(EXT_PANEL_BG.b + (rock.b - EXT_PANEL_BG.b) * 0.80f),
                255 };
            Color base = ExtClassColor(c.cls);
            float classW = c.cls == ResourceClass::MEASURED   ? 0.90f
                         : c.cls == ResourceClass::INDICATED  ? 0.75f
                         : c.cls == ResourceClass::INFERRED   ? 0.40f : 0.12f;
            Color tone = {
                static_cast<unsigned char>(ground.r + (base.r - ground.r) * classW),
                static_cast<unsigned char>(ground.g + (base.g - ground.g) * classW),
                static_cast<unsigned char>(ground.b + (base.b - ground.b) * classW),
                255 };
            float lit = (0.65f + 0.35f * std::min(c.grade / std::max(maxGrade, 0.0001f), 1.0f)) * fade;
            Color fill = {
                static_cast<unsigned char>(EXT_PANEL_BG.r + (tone.r - EXT_PANEL_BG.r) * lit),
                static_cast<unsigned char>(EXT_PANEL_BG.g + (tone.g - EXT_PANEL_BG.g) * lit),
                static_cast<unsigned char>(EXT_PANEL_BG.b + (tone.b - EXT_PANEL_BG.b) * lit),
                255 };

            // Hill-shading (Dark Plating: light implied from the upper-left).
            // A face tilting toward the viewer (front corner above the back)
            // and toward the left catches light; one falling away loses it.
            // Slope, not height, is what the eye reads as shape -- this is
            // what makes the curvature legible at any relief. Slope is
            // measured in relief-per-plate-width, so a mound keeps the same
            // shading whether the lattice is 8 or 32 cells across (a denser
            // lattice halves the per-cell rise; it must not halve the light).
            // tanh keeps the gradation: steep flanks saturate gently instead
            // of clipping to a flat two-tone.
            float perCell = static_cast<float>(N) / std::max(g.relief, 1.0f);
            float toward  = (l11 - l00) * perCell;
            float left    = (l01 - l10) * perCell;
            // The lit side swings further than the shadowed side: a face in
            // shadow still has to show its craters and its class colour.
            float slope   = tanhf(0.45f * toward + 0.25f * left);
            float shade   = 1.0f + (slope >= 0.0f ? 0.70f : 0.50f) * slope;
            Color lit3 = { clamp255(fill.r * shade), clamp255(fill.g * shade),
                           clamp255(fill.b * shade), 255 };

            if (textured)
            {
                // One quad per cell, all on the one texture, so the whole
                // plate is a single batch. Same winding as the triangles.
                Color m = { clamp255(lit3.r * texGain), clamp255(lit3.g * texGain),
                            clamp255(lit3.b * texGain), 255 };
                const float tr = BLOCK_TEX_REPEAT;
                float u0 = static_cast<float>(i) * tr / N,     v0 = static_cast<float>(j) * tr / N;
                float u1 = static_cast<float>(i + 1) * tr / N, v1 = static_cast<float>(j + 1) * tr / N;
                rlCheckRenderBatchLimit(4);
                rlBegin(RL_QUADS);
                rlColor4ub(m.r, m.g, m.b, m.a);
                rlNormal3f(0.0f, 0.0f, 1.0f);
                rlTexCoord2f(u0, v0); rlVertex2f(q[0].x, q[0].y);
                rlTexCoord2f(u0, v1); rlVertex2f(q[3].x, q[3].y);
                rlTexCoord2f(u1, v1); rlVertex2f(q[2].x, q[2].y);
                rlTexCoord2f(u1, v0); rlVertex2f(q[1].x, q[1].y);
                rlEnd();
            }
            else
            {
                // Two triangles rather than a quad -- raylib fills triangles
                // only, and the winding has to be consistent or faces drop out.
                //
                // This is the FALLBACK, and it is brutally slow: measured at
                // 216 ms/frame against 27 for the textured path above, because
                // each DrawTriangle goes through the generic batch while the
                // textured path binds once and pushes 4096 quads into a single
                // batch. Do not "simplify" the textured path into this one --
                // it is 8x faster, not a decoration.
                DrawTriangle(q[0], q[3], q[2], lit3);
                DrawTriangle(q[0], q[2], q[1], lit3);
                DrawLineEx(q[0], q[1], 0.6f, lit3);
                DrawLineEx(q[1], q[2], 0.6f, lit3);
            }

            if (hitBoxes && hitIndex)
            {
                float minX = std::min(std::min(q[0].x, q[1].x), std::min(q[2].x, q[3].x));
                float maxX = std::max(std::max(q[0].x, q[1].x), std::max(q[2].x, q[3].x));
                float minY = std::min(std::min(q[0].y, q[1].y), std::min(q[2].y, q[3].y));
                float maxY = std::max(std::max(q[0].y, q[1].y), std::max(q[2].y, q[3].y));
                hitBoxes->push_back({minX, minY, maxX - minX, maxY - minY});
                hitIndex->push_back(j * N + i);
            }
        }
    }

    if (textured) rlSetTexture(0);

    // The active-plate rim (section 9.4: the stratum being cut rim-lights its
    // plate). Drawn HERE, not with the trace, because only this function knows
    // cornerLift -- the plate's surface floats up to g.relief above its base
    // plane, so a rim computed at lift 0 sat below the plate it was meant to
    // hug, and the x1.5 relief pass made that gap plain. Tracing the boundary
    // through the same cornerLift the surface uses cannot drift by
    // construction, whatever the relief.
    if (rimPulse > 0.0f)
    {
        Color rim = Fade({244, 198, 106, 255}, rimPulse);
        auto edge = [&](int i0, int j0, int i1, int j1)
        {
            int steps = std::max(std::abs(i1 - i0), std::abs(j1 - j0));
            Vector2 prev = g.Iso(static_cast<float>(i0), static_cast<float>(j0),
                                 layer, cornerLift(i0, j0));
            for (int t = 1; t <= steps; t++)
            {
                int i = i0 + (i1 - i0) * t / steps;
                int j = j0 + (j1 - j0) * t / steps;
                Vector2 cur = g.Iso(static_cast<float>(i), static_cast<float>(j),
                                    layer, cornerLift(i, j));
                DrawLineEx(prev, cur, 2.0f, rim);
                prev = cur;
            }
        };
        edge(0, 0, N, 0);   edge(N, 0, N, N);
        edge(N, N, 0, N);   edge(0, N, 0, 0);
    }

    // Depth ruling out to the left edge of this plate. Anchored to the
    // stratum's BOUNDARY, which is the depth the label names -- not to the
    // plate, which hangs below it by design (BlockPlateLineY).
    Vector2 leftCorner = g.Iso(0.0f, static_cast<float>(N), layer, 0.0f);
    leftCorner.y = BlockPlateLineY(g, layer);
    float labelX = g.originX - N * g.tileX - 68.0f;
    for (float dx = labelX + 44.0f; dx < leftCorner.x - 5.0f; dx += 6.0f)
        DrawLineEx({dx, leftCorner.y}, {dx + 2.5f, leftCorner.y}, 1.0f,
                   Fade(EXT_ACCENT_CYAN, 0.12f + 0.20f * std::clamp(fade, 0.0f, 1.0f)));
    // The label belongs to the plate, so it recedes with it -- never all the
    // way out, since it is also the depth scale of the whole stack.
    float labelA = 0.40f + 0.60f * std::clamp(fade, 0.0f, 1.0f);
    DrawTextEx(labelFont, depthLabel, {labelX, leftCorner.y - 11.0f}, 11.0f, sp,
               Fade(EXT_ACCENT_CYAN, labelA));
    DrawTextEx(labelFont, levelLabel, {labelX, leftCorner.y + 1.0f}, 8.0f, sp,
               Fade(EXT_DIM_TEXT, labelA));
}


// ===========================================================================
// THE BOREHOLE DOCK -- the drill-dock port (variant b: seam-aligned).
// One ground across the dock strip and the block model; the plate slots sit
// at the strip's band centres so both share one depth mapping, which is what
// lets the prescribed line render dead straight and keeps its cursor level
// with the mud panel's. Recipes per docs/design/graphics/dark-plating.md
// (sections 4-6, 9); reference docs/design/prospecting/prototypes/drill-dock.html.
// ===========================================================================

static const Color DP_OUT          = {10, 14, 20, 255};
// DP_ROCK_COL lives above DrawBlockLayer, which shares it.
static const Color DP_ROCK_EDGE[4] = {{25,21,16,255},{28,23,18,255},{22,28,35,255},{16,18,22,255}};
static const Color DP_ROCK_GRAIN[4]= {{76,68,55,255},{91,81,64,255},{77,90,103,255},{52,56,65,255}};
static const Color DP_ICE_FLECK    = {160, 225, 245, 255};
static const Color PROS_STRING_MID   = {119, 135, 154, 255};
static const Color PROS_STRING_LIT   = {232, 240, 248, 255};
static const Color PROS_SHADOW_HAIR  = {160, 190, 215, 255};

// Deterministic speckle: fixed seed per drawing pass, so the ground never
// shimmers between frames (Dark Plating section 5).
static unsigned dpGrain = 1;
static float DpRnd()
{
    dpGrain = (dpGrain * 16807u) % 2147483647u;
    return static_cast<float>(dpGrain) / 2147483647.0f;
}

// The strip's depth axis, shared by both vertical instruments -- prospecting's
// borehole strip and excavation's shaft. Pure geometry: bands in, depth<->y
// out, and nothing in it knows which module is drawing.
struct DockGeom
{
    float x = 0.0f, w = 0.0f, cx = 0.0f;
    float bandTop[5] = {0, 0, 0, 0, 0};

    float YOf(float m) const
    {
        int d = LayerOfDepthM(std::min(m, FULL_COLUMN_M - 0.01f));
        float t = (m - LayerTopM(d)) / LAYER_THICKNESS_M[d];
        return bandTop[d] + (bandTop[d + 1] - bandTop[d]) * t;
    }
    // ...and back again. The strata bands run the full width of the panel, so
    // any height in it IS a depth -- which is what lets the pointer read a
    // depth off the empty ground between the plates.
    float DepthAtY(float y) const
    {
        if (y <= bandTop[0]) return 0.0f;
        for (int d = 0; d < 4; d++)
        {
            if (y >= bandTop[d + 1]) continue;
            float span = std::max(1.0f, bandTop[d + 1] - bandTop[d]);
            return LayerTopM(d) + LAYER_THICKNESS_M[d] * (y - bandTop[d]) / span;
        }
        return FULL_COLUMN_M;
    }
};

static DockGeom DockFromBlock(const BlockModelGeom& g, float stripX, float stripW)
{
    DockGeom d;
    d.x = stripX; d.w = stripW; d.cx = stripX + stripW * 0.5f;
    float slot[4];
    for (int L = 0; L < 4; L++)
        slot[L] = g.originY + L * g.gap + g.size * g.tileY;
    // Each band STARTS at its own plate: a plate is the top face of its rock,
    // so the rock hangs below it. That makes YOf(PlatePlaneM(L)) land on
    // plate L by construction, at any layout -- the plate and the line that
    // names it cannot drift apart. (The bands used to be CENTRED on the
    // plates, which put a plate in the middle of rock that was half above it.)
    //
    // The boundary sits above the plate so the plate hangs UNDER its own
    // ceiling. Two terms, and the first one is the whole lesson:
    //
    //   g.relief -- because a plate is drawn LIFTED, not at its base plane.
    //     cornerLift raises each corner by (grade/maxGrade)^0.8 * relief, so
    //     a field with nothing surveyed yet -- every cell holding the same
    //     layer mean -- lifts EVERY corner the full relief. Positioning
    //     against the base plane therefore put the visible plate 22 px above
    //     a line that was, on paper, 5 px above its base. The eye sees the
    //     lifted surface; the layout has to be told about it.
    //   the fraction -- the "a bit below" itself, applied to the drawn
    //     surface: a fully lifted plate rests half a half-height under its
    //     line.
    //
    // Referenced to the FULL lift on purpose. The layout must not move as
    // survey data arrives (the bands are the depth scale), so it is pinned to
    // the plate's ceiling: the richest cells rise toward the boundary and
    // poorer ground hangs further below it, which is the right reading.
    (void)slot;
    for (int L = 0; L < 4; L++) d.bandTop[L] = BlockPlateLineY(g, L);
    d.bandTop[4] = d.bandTop[3] + g.gap;
    return d;
}

// Winding per the block-layer convention: quads defined clockwise, filled as
// (0,3,2)+(0,2,1) or faces drop out.
static void DpFillQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col)
{
    DrawTriangle(a, d, c, col);
    DrawTriangle(a, c, b, col);
}
static void DpFillDiamond(float cx, float cy, float s, Color col)
{
    Vector2 t = {cx, cy - s}, r = {cx + s, cy}, b = {cx, cy + s}, l = {cx - s, cy};
    DpFillQuad(t, r, b, l, col);
}

// Dashed line with a phase, so a dash train can march (the advance) or spin
// (the string). raylib has no line dash; this is the whole of it.
static void DpDashed(Vector2 a, Vector2 b, float dash, float gap,
                       float phase, float th, Color c)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) return;
    dx /= len; dy /= len;
    float period = dash + gap;
    float s = fmodf(phase, period);
    if (s > 0.0f) s -= period;
    for (; s < len; s += period)
    {
        float s0 = std::max(0.0f, s), s1 = std::min(len, s + dash);
        if (s1 <= s0) continue;
        DrawLineEx({a.x + dx * s0, a.y + dy * s0},
                   {a.x + dx * s1, a.y + dy * s1}, th, c);
    }
}

// ---- metal (Dark Plating section 4) ---------------------------------------
// Heat context for the frame being drawn: a Gaussian around the bit, fed to
// every steel call so the glow spreads up the machine from the working point.
static float prosHeatAmt = 0.0f, prosHeatBitY = 0.0f;
static float ProsHeatAt(float y)
{
    float d = y - prosHeatBitY;
    return prosHeatAmt * expf(-(d * d) / (2.0f * 70.0f * 70.0f));
}
static Color DpSteel(float sh, float heat = 0.0f)
{
    sh = std::clamp(sh, 0.0f, 1.0f);
    float r = 96.0f + 142.0f * sh, g = 104.0f + 140.0f * sh, b = 118.0f + 134.0f * sh;
    float t = std::clamp(heat * 1.15f, 0.0f, 1.0f);
    r += (255.0f - r) * t;
    g += ((55.0f + 110.0f * sh) - g) * t;
    b += (25.0f - b) * t;
    return { static_cast<unsigned char>(r), static_cast<unsigned char>(g),
             static_cast<unsigned char>(b), 255 };
}
// One slice of a turned cylinder: five flat bands, specular off-centre left.
//
// Heat is a PARAMETER, not a lookup. It used to call ProsHeatAt, which reads
// the borehole dock's own globals -- so every banded part of excavation's rig
// (rod, joints, lower works, neck) was shaded by the auger's heat field, at
// the auger's sigma, centred on the auger's bit depth. Two machines cannot
// share one hotspot; a helper that reaches for module state is not shared, it
// is borrowed.
static void DpBandedSlice(float cx, float y, float hw, float hh,
                            const float tones[5][2], float heat)
{
    float x = cx - hw, wTot = hw * 2.0f, t = 0.0f;
    for (int k = 0; k < 5; k++)
    {
        DrawRectangleRec({x + wTot * t, y, wTot * tones[k][0] + 0.7f, hh},
                         DpSteel(tones[k][1], heat));
        t += tones[k][0];
    }
}
static const float DP_ROD_TONES[5][2]   = {{0.15f,0.11f},{0.17f,0.98f},{0.21f,0.58f},{0.27f,0.30f},{0.20f,0.07f}};
static const float DP_JOINT_TONES[5][2] = {{0.15f,0.16f},{0.18f,0.94f},{0.22f,0.56f},{0.26f,0.28f},{0.19f,0.10f}};
static const float DP_CHUCK_TONES[5][2] = {{0.17f,0.06f},{0.16f,0.62f},{0.22f,0.34f},{0.26f,0.18f},{0.19f,0.04f}};

// ---- drill string geometry (pass 6 constants, px) -------------------------
static const float PROS_DRILL_R = 15.0f, PROS_DRILL_RS = 8.6f, PROS_DRILL_RSB = 7.2f;
static const float PROS_ROD_TOP = 11.5f, PROS_PITCH = 19.0f;
static const float PROS_TILT = 1.7f, PROS_TH_T = 5.0f, PROS_TH_C = 1.9f;
static const float PROS_TAPER = PROS_PITCH * 1.5f, PROS_CONE = 18.0f;
static const float PROS_THREAD_LEN = PROS_PITCH * 6.2f;

static float prosSpin = 0.0f;          // the string's spin accumulator
struct ProsChip { float y, a, up; int layer; };
static std::vector<ProsChip> prosChips;

struct ProsRig
{
    float cx, surfY, bitY, coneApex, coneTop, threadTop;
};
static float ProsRodHalf(const ProsRig& r, float y)
{
    if (y >= r.coneTop)
        return std::max(0.5f, PROS_DRILL_RSB * (1.0f - (y - r.coneTop) / (r.coneApex - r.coneTop)));
    if (y >= r.threadTop)
        return PROS_DRILL_RS + (PROS_DRILL_RSB - PROS_DRILL_RS)
               * (y - r.threadTop) / std::max(1.0f, r.coneTop - r.threadTop);
    // above the thread: stepped sections handled by the shaft drawing; the
    // envelope just widens toward the chuck
    float t = std::clamp((r.threadTop - y) / 150.0f, 0.0f, 1.0f);
    return PROS_DRILL_RS + 0.5f + (PROS_ROD_TOP - PROS_DRILL_RS - 0.5f) * t;
}
static float ProsCrest(const ProsRig& r, float y)
{
    float start = r.coneTop - PROS_TAPER;
    if (y <= start) return PROS_DRILL_R;
    if (y >= r.coneTop) return ProsRodHalf(r, y);
    float t = std::clamp((y - start) / PROS_TAPER, 0.0f, 1.0f);
    return PROS_DRILL_R + (ProsRodHalf(r, y) - PROS_DRILL_R) * t;
}
static float ProsRadAt(const ProsRig& r, float y)
{
    return (y >= r.threadTop && y <= r.coneTop) ? ProsCrest(r, y) : ProsRodHalf(r, y);
}

// The helicoid, front or back sweep (Dark Plating section 6.1): each theta
// step owns a radial quad from root to crest; the sawtooth is the projection.
static void ProsDrawThread(const ProsRig& r, bool front)
{
    float span = r.coneTop - r.threadTop;
    if (span <= 6.0f) return;
    float thMax = (span / PROS_PITCH) * 2.0f * PI;
    const float step = 0.11f;

    struct Seg { Vector2 r0, c0, r1, c1; float e0x, e1x, c, s; };
    std::vector<Seg> segs;
    segs.reserve(static_cast<size_t>(thMax / step) + 4);
    for (float th = 0.0f; th < thMax; th += step)
    {
        float a0 = th + prosSpin, a1 = th + step + prosSpin;
        float c0 = cosf(a0), c1 = cosf(a1), cm = (c0 + c1) * 0.5f;
        if (front != (cm > 0.0f)) continue;
        float yb0 = r.threadTop + PROS_PITCH * th / (2.0f * PI);
        float yb1 = r.threadTop + PROS_PITCH * (th + step) / (2.0f * PI);
        float rc0 = ProsCrest(r, yb0), rc1 = ProsCrest(r, yb1);
        float rr0 = ProsRodHalf(r, yb0), rr1 = ProsRodHalf(r, yb1);
        if (rc0 - rr0 < 1.2f) continue;
        float s0 = -sinf(a0), s1 = -sinf(a1);        // left-hand helix
        Seg sg;
        sg.r0 = {r.cx + rr0 * s0, yb0 + PROS_TILT * c0 * (rr0 / PROS_DRILL_R)};
        sg.c0 = {r.cx + rc0 * s0, yb0 + PROS_TILT * c0 * (rc0 / PROS_DRILL_R)};
        sg.r1 = {r.cx + rr1 * s1, yb1 + PROS_TILT * c1 * (rr1 / PROS_DRILL_R)};
        sg.c1 = {r.cx + rc1 * s1, yb1 + PROS_TILT * c1 * (rc1 / PROS_DRILL_R)};
        sg.e0x = r.cx + (rc0 + 1.7f) * s0;
        sg.e1x = r.cx + (rc1 + 1.7f) * s1;
        sg.c = cm; sg.s = (s0 + s1) * 0.5f;
        segs.push_back(sg);
    }
    std::sort(segs.begin(), segs.end(), [](const Seg& p, const Seg& q){ return p.c < q.c; });

    // one flood outline pass under the whole sweep, then the faces --
    // per-segment strokes rib the surface (Dark Plating section 2)
    const float M = 1.4f;
    Color outCol = front ? DP_OUT : Color{16, 24, 32, 255};
    for (const Seg& s : segs)
    {
        DpFillQuad({s.r0.x, s.r0.y - M}, {s.e0x, s.c0.y - M},
                     {s.e1x, s.c1.y - M}, {s.r1.x, s.r1.y - M}, outCol);
        DpFillQuad({s.r0.x, s.r0.y - M}, {s.r1.x, s.r1.y - M},
                     {s.r1.x, s.r1.y + PROS_TH_T + M}, {s.r0.x, s.r0.y + PROS_TH_T + M}, outCol);
        DpFillQuad({s.e0x, s.c0.y - M}, {s.e1x, s.c1.y - M},
                     {s.e1x, s.c1.y + PROS_TH_C + M}, {s.e0x, s.c0.y + PROS_TH_C + M}, outCol);
    }
    auto band = [](float v){ return roundf(std::clamp(v, 0.0f, 1.0f) * 7.0f) / 7.0f; };
    auto dim  = [&](float v){ return front ? v : 0.15f + v * 0.44f; };
    for (const Seg& s : segs)
    {
        float c = std::max(0.0f, s.c), lt = (1.0f - s.s) * 0.5f;
        float heat = ProsHeatAt(s.r0.y);
        float shB = dim(band(0.40f + 0.30f * c + 0.18f * lt));
        float shU = dim(band(0.10f + 0.16f * c));
        float shR = dim(band(0.38f + 0.34f * c + 0.22f * lt));
        float shF = dim(band(0.33f + 0.46f * c + 0.14f * lt));
        // body slab (root thickness tapering to crest = the V that sharpens teeth)
        DpFillQuad(s.r0, s.c0, {s.c0.x, s.c0.y + PROS_TH_C}, {s.r0.x, s.r0.y + PROS_TH_T}, DpSteel(shB, heat));
        DpFillQuad(s.r0, s.r1, {s.r1.x, s.r1.y + PROS_TH_T}, {s.r0.x, s.r0.y + PROS_TH_T}, DpSteel(shB, heat));
        // underside in shadow
        DpFillQuad({s.r0.x, s.r0.y + PROS_TH_T * 0.55f}, {s.c0.x, s.c0.y + PROS_TH_C * 0.55f},
                     {s.c1.x, s.c1.y + PROS_TH_C}, {s.r1.x, s.r1.y + PROS_TH_T}, DpSteel(shU, heat));
        // crest rim
        DpFillQuad(s.c0, s.c1, {s.c1.x, s.c1.y + PROS_TH_C}, {s.c0.x, s.c0.y + PROS_TH_C}, DpSteel(shR, heat));
        // the ramp face itself
        DpFillQuad(s.r0, s.c0, s.c1, s.r1, DpSteel(shF, heat));
    }
    if (front)
    {
        for (const Seg& s : segs)
        {
            float g = roundf(std::clamp(0.54f + 0.30f * std::max(0.0f, s.c) + 0.06f, 0.0f, 1.0f) * 3.0f) / 3.0f;
            DrawLineEx({s.c0.x, s.c0.y + 0.8f}, {s.c1.x, s.c1.y + 0.8f}, 1.1f,
                       DpSteel(g, ProsHeatAt(s.c0.y)));
        }
    }
}

static void ProsDrawJoint(float cx, float y, float rr, bool big)
{
    float hh = big ? 5.6f : 4.6f, w = rr + (big ? 3.6f : 2.7f);
    DrawRectangleRec({cx - w - 1.8f, y - hh - 1.8f, (w + 1.8f) * 2.0f, hh * 2.0f + 3.6f}, DP_OUT);
    DpBandedSlice(cx, y - hh, w, hh * 2.0f, DP_JOINT_TONES, ProsHeatAt(y));
    DrawRectangleRec({cx - w, y - hh, w * 2.0f, 1.4f}, Fade(WHITE, 0.34f));
    DrawRectangleRec({cx - w, y + hh - 1.8f, w * 2.0f, 1.8f}, Fade(BLACK, 0.45f));
}

// The rig: powerhead, chuck, jointed rod, threaded stem, carbide cone.
static void ProsDrawString(const ProsRig& r, float topY)
{
    // rod silhouette + banded body, in thin slices so the taper stays banded
    for (float y = topY; y < r.coneApex; y += 1.4f)
    {
        float w = ProsRodHalf(r, y) + 1.8f;
        DrawRectangleRec({r.cx - w, y, w * 2.0f, 2.1f}, DP_OUT);
    }
    for (float y = topY; y < r.coneApex - 1.0f; y += 1.4f)
    {
        float w = ProsRodHalf(r, y);
        if (w < 0.7f) continue;
        DpBandedSlice(r.cx, y, w, 1.8f, DP_ROD_TONES, ProsHeatAt(y));
    }
    // chuck under the head, then joints where sections step
    float chuckTop = r.surfY - 46.0f;
    DrawRectangleRec({r.cx - PROS_ROD_TOP - 5.2f, chuckTop - 1.8f,
                      (PROS_ROD_TOP + 5.2f) * 2.0f, 15.6f}, DP_OUT);
    DpBandedSlice(r.cx, chuckTop, PROS_ROD_TOP + 3.6f, 12.0f, DP_CHUCK_TONES,
                    ProsHeatAt(chuckTop));
    DrawRectangleRec({r.cx - PROS_ROD_TOP - 3.6f, chuckTop, (PROS_ROD_TOP + 3.6f) * 2.0f, 2.0f}, Fade(WHITE, 0.22f));

    float run = r.threadTop - (r.surfY - 30.0f);
    int nj = run > 190.0f ? 2 : (run > 70.0f ? 1 : 0);
    for (int k = 1; k <= nj; k++)
        ProsDrawJoint(r.cx, (r.surfY - 30.0f) + run * k / (nj + 1),
                      ProsRodHalf(r, (r.surfY - 30.0f) + run * k / (nj + 1)), false);
    ProsDrawJoint(r.cx, r.threadTop, PROS_DRILL_RS + 0.8f, true);   // threads begin

    // carbide point: flat facets, brightest left
    float sh = r.coneApex - PROS_CONE + 1.4f, cw = PROS_DRILL_RSB * 0.9f;
    const float fac[3][3] = {{-1.0f, -0.34f, 0.90f}, {-0.34f, 0.28f, 0.52f}, {0.28f, 1.0f, 0.22f}};
    for (int k = 0; k < 3; k++)
    {
        Vector2 a = {r.cx + cw * fac[k][0], sh}, b = {r.cx + cw * fac[k][1], sh};
        DrawTriangle(a, {r.cx, r.coneApex - 1.2f}, b,
                     DpSteel(fac[k][2], std::min(1.0f, ProsHeatAt(sh) * 1.3f)));
    }
    DrawRectangleRec({r.cx - cw - 1.0f, sh - 1.3f, (cw + 1.0f) * 2.0f, 1.5f}, Fade(BLACK, 0.45f));
}

// The powerhead -- amber housing, vents, side pod with the state lamp,
// bolts, collar clamps. Anchored just above the surface and clamped to the
// clip, so it can NEVER be scissored away by a tight layout again.
static void ProsDrawHead(float cx, float surfY, float clipTop,
                         bool turning, bool cooling)
{
    float top = std::max(clipTop + 10.0f, surfY - 55.0f);
    auto box = [](float x, float y, float w, float h, Color fill, bool bev)
    {
        DrawRectangleRec({x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f}, DP_OUT);
        DrawRectangleRec({x, y, w, h}, fill);
        if (bev)
        {
            DrawRectangleRec({x, y, w, 2.6f}, Fade(WHITE, 0.30f));
            DrawRectangleRec({x, y, 2.6f, h}, Fade(WHITE, 0.14f));
            DrawRectangleRec({x, y + h - 2.6f, w, 2.6f}, Fade(BLACK, 0.30f));
            DrawRectangleRec({x + w - 2.6f, y, 2.6f, h}, Fade(BLACK, 0.22f));
        }
    };
    box(cx - 4.5f, top - 8.0f, 9.0f, 8.0f, {57, 66, 78, 255}, false);   // mast strap
    box(cx - 25.0f, top, 50.0f, 27.0f, {217, 150, 47, 255}, true);      // amber housing
    for (int i = 0; i < 3; i++)
    {
        DrawRectangleRec({cx - 15.0f, top + 6.0f + i * 7.0f, 30.0f, 3.4f}, {122, 81, 21, 255});
        DrawRectangleRec({cx - 15.0f, top + 6.0f + i * 7.0f + 2.3f, 30.0f, 1.1f}, Fade(BLACK, 0.5f));
    }
    DrawRectangleRec({cx - 22.0f, top + 2.5f, 3.0f, 3.0f}, {244, 198, 106, 255});  // bolts
    DrawRectangleRec({cx + 19.0f, top + 2.5f, 3.0f, 3.0f}, {244, 198, 106, 255});
    DrawRectangleRec({cx - 22.0f, top + 21.5f, 3.0f, 3.0f}, {244, 198, 106, 255});
    DrawRectangleRec({cx + 19.0f, top + 21.5f, 3.0f, 3.0f}, {244, 198, 106, 255});
    box(cx + 25.0f, top + 6.0f, 16.0f, 13.0f, {57, 66, 78, 255}, true); // motor pod
    DrawRectangleRec({cx + 29.5f, top + 9.5f, 4.0f, 4.0f},
                     cooling ? Color{255, 90, 40, 255}
                             : turning ? Color{255, 200, 77, 255}
                                       : Color{80, 225, 255, 255});
    box(cx - 10.0f, top + 27.0f, 20.0f, 9.0f, {74, 84, 95, 255}, true); // collar clamp
    box(cx - 7.0f, top + 36.0f, 14.0f, 8.0f, {57, 66, 78, 255}, true);
}

// The whole strip. Returns nothing; draws rock, hole, string, mud trace.
// During a trip the string is OUT of the hole: the drawn depth runs the true
// depth out and back on a half-sine (redline's lift) while the sim depth
// holds. Everything visual that follows the bit reads this, never depthM.
// Where the bit rests with no hole under it: just below the collar, so the
// rig reads as parked rather than as drilling nothing. The end-of-hole hoist
// lands EXACTLY here, which is why the handover from RETRACTING to DONE does
// not jump.
static constexpr float PROS_IDLE_DEPTH_M = 1.5f;

static float ProsShownDepthM(const LineHole& hole)
{
    // The hoist at the end of a hole: monotonic, back to the idle pose, on a
    // smoothstep -- a winch takes up, runs, and eases the last rods in. (The
    // trip below is the other motion: out AND BACK on a half-sine, because a
    // trip resumes the same hole.)
    if (hole.state == LineHoleState::RETRACTING)
    {
        float f = hole.pullDur > 0.0f
                ? std::clamp(hole.pullT / hole.pullDur, 0.0f, 1.0f) : 1.0f;
        float s = f * f * (3.0f - 2.0f * f);
        return PROS_IDLE_DEPTH_M + (hole.depthM - PROS_IDLE_DEPTH_M) * (1.0f - s);
    }
    if (!hole.tripping || hole.tripDur <= 0.0f) return hole.depthM;
    float f = std::clamp(hole.tripT / hole.tripDur, 0.0f, 1.0f);
    return hole.depthM * (1.0f - sinf(f * PI));
}

static void ProsDrawBoreholeDock(Unit* unit, ProspectingSystem* ps,
                                 const DockGeom& dg, float clipTop, float clipBot,
                                 float hoverM, float hoverU, float hoverAlpha,
                                 const Font& bodyFont, float sp, float fsSmall,
                                 const Texture2D* strata)
{
    (void)unit;
    const LineHole& hole = ps->lineHole;
    bool turning = hole.state == LineHoleState::DRILLING && !hole.tripping;
    bool cooling = turning && hole.dwelling;
    float surfY = dg.bandTop[0];
    float botY = dg.bandTop[4];

    // The spin is SCREW-TRUE in ground soft enough to bite: rotation rate is
    // derived so one turn descends one thread pitch on screen, which is what
    // locks the auger illusion to the actual advance (campaign: the old fixed
    // rate was HALF screw-true in regolith -- the drill looked pushed, not
    // screwed). Hard rock floors at a grind rate: turning faster than it
    // bites, which is honest. A dwell winds the spin down to a creep; a trip
    // backs the rods off in slow reverse.
    float dt = GetFrameTime();
    if (turning)
    {
        int L = LayerOfDepthM(hole.depthM);
        float pxPerM = (dg.bandTop[L + 1] - dg.bandTop[L]) / LAYER_THICKNESS_M[L];
        float screwTrue = DrillAdvanceAtM(hole.depthM) * pxPerM * 2.0f * PI / PROS_PITCH;
        prosSpin -= (cooling ? 1.1f : std::max(screwTrue, 6.0f) * hole.rpm) * dt;
    }
    else if (hole.tripping)
    {
        prosSpin += 2.4f * dt;
    }

    BeginScissorMode(static_cast<int>(dg.x * gPixelScale),
                     static_cast<int>(clipTop * gPixelScale),
                     static_cast<int>(dg.w * gPixelScale),
                     static_cast<int>((clipBot - clipTop) * gPixelScale));

    // The stage shakes with the work (redline): a rumble scaling with the
    // spindle, a jolt when the bit lets go. The whole dock translates as one
    // rigid console -- rock, string and cursors together -- inside the fixed
    // scissor and frame, so the console rattles in its mount.
    {
        float rpmN0 = hole.rpm / DRILL_RPM_MAX;
        float burst = std::max(0.0f, 1.0f - (ps->gameTime - hole.fracturedTime) / 0.6f);
        // The rumble scales with the GROUND: soft regolith hums, basalt
        // rattles the mount. Base is a whisper -- an idle crawl is near
        // still, and only drive x hardness earns real shake.
        float hardSh = DrillHardnessAtM(hole.depthM);
        float amp = (turning ? rpmN0 * rpmN0 * (0.25f + 1.2f * hardSh) : 0.0f)
                  + burst * 5.0f;
        float sx = amp > 0.01f ? (static_cast<float>(GetRandomValue(-100, 100)) / 100.0f) * amp : 0.0f;
        float sy = amp > 0.01f ? (static_cast<float>(GetRandomValue(-100, 100)) / 100.0f) * amp : 0.0f;
        rlPushMatrix();
        rlTranslatef(sx, sy, 0.0f);
    }

    // sky
    DrawRectangleRec({dg.x, clipTop, dg.w, surfY - clipTop}, {10, 16, 24, 255});
    dpGrain = 3;
    for (int i = 0; i < 8; i++)
        DrawRectangleRec({dg.x + DpRnd() * dg.w, clipTop + DpRnd() * (surfY - clipTop - 8.0f),
                          1.4f, 1.4f}, Fade({200, 220, 240, 255}, 0.5f));

    // The rock, full strength: this is the SAME ground the plates float on --
    // and now literally the same image. One generated texture per stratum,
    // tiled down the band here (a section) and stretched over the plate there
    // (a plan view), so the band and the plate beside it are one rock. The
    // hand-scattered speckle this replaces could not be: it was drawn from a
    // seeded LCG that only this strip ran.
    for (int L = 0; L < 4; L++)
    {
        float y0 = dg.bandTop[L], y1 = dg.bandTop[L + 1];
        if (strata != nullptr && strata[L].id != 0)
        {
            // x2 against the texture's mean of exactly 128 (rock_texture.h):
            // a textured band holds the mean tone the flat fill had, so the
            // stratum palette did not have to be re-tuned for texture.
            Color tint = { static_cast<unsigned char>(std::min(255, DP_ROCK_COL[L].r * 2)),
                           static_cast<unsigned char>(std::min(255, DP_ROCK_COL[L].g * 2)),
                           static_cast<unsigned char>(std::min(255, DP_ROCK_COL[L].b * 2)),
                           255 };
            float k = static_cast<float>(RockTexture::SIZE) / DP_ROCK_TEX_PX;
            // Each band enters the tile at a different row, so four bands of
            // similar height cannot line up into a visible repeat.
            float off = static_cast<float>(L) * 41.0f;
            DrawTexturePro(strata[L], {0.0f, off, dg.w * k, (y1 - y0) * k},
                           {dg.x, y0, dg.w, y1 - y0}, {0.0f, 0.0f}, 0.0f, tint);
        }
        else
        {
            DrawRectangleRec({dg.x, y0, dg.w, y1 - y0}, DP_ROCK_COL[L]);
        }
        DrawRectangleRec({dg.x, y0, dg.w, 2.0f}, DP_ROCK_EDGE[L]);
    }
    DrawRectangleRec({dg.x, surfY - 1.8f, dg.w, 2.0f}, {74, 85, 96, 255});

    // depth figures down the right edge
    for (int L = 0; L <= 4; L++)
    {
        const char* t = TextFormat("%d", static_cast<int>(L == 4 ? FULL_COLUMN_M : LayerTopM(L)));
        float tw = MeasureTextEx(bodyFont, t, fsSmall, sp).x;
        DrawTextEx(bodyFont, t, {dg.x + dg.w - tw - 4.0f, dg.bandTop[L] + 3.0f},
                   fsSmall, sp, Fade(EXT_DIM_TEXT, 0.9f));
    }

    ProsRig rig;
    rig.cx = dg.cx; rig.surfY = surfY;
    prosHeatAmt = hole.heat;
    // The string is only in the ground while it is cutting or coming out; a
    // DONE hole is a hole with the rig parked over it.
    bool stringInGround = hole.state == LineHoleState::DRILLING ||
                          hole.state == LineHoleState::RETRACTING;
    float depthShown = stringInGround ? ProsShownDepthM(hole) : PROS_IDLE_DEPTH_M;
    rig.bitY = dg.YOf(depthShown);
    rig.coneApex = rig.bitY;                    // the tip IS the depth
    rig.coneTop = rig.coneApex - PROS_CONE;
    rig.threadTop = std::max(std::min(rig.bitY - PROS_THREAD_LEN,
                                      rig.coneTop - 3.0f * PROS_PITCH), surfY + 10.0f);
    prosHeatBitY = rig.bitY;

    // borehole with ragged walls, cut to the DEEPEST point the bit reached
    // -- the hole stays a hole while a trip lifts the string out of it
    // The HOLE outlives the string in it: once cut, it stays cut to the
    // deepest point the bit reached, through the hoist and after it.
    float cutY = dg.YOf(hole.state == LineHoleState::NONE ||
                        hole.state == LineHoleState::AIMING
                        ? PROS_IDLE_DEPTH_M : hole.depthM);
    float bw = PROS_DRILL_R * 2.0f + 9.0f;
    DrawRectangleRec({rig.cx - bw / 2.0f, surfY, bw, cutY - surfY + 4.0f}, {14, 11, 8, 255});
    dpGrain = 29;
    for (float y = surfY; y < cutY + 2.0f; y += 7.0f)
    {
        DrawRectangleRec({rig.cx - bw / 2.0f - 2.0f - DpRnd() * 2.6f, y, 3.4f + DpRnd() * 2.6f, 6.0f}, Fade(BLACK, 0.8f));
        DrawRectangleRec({rig.cx + bw / 2.0f - 1.4f + DpRnd() * 2.6f, y, 3.4f + DpRnd() * 2.6f, 6.0f}, Fade(BLACK, 0.8f));
    }
    DrawRectangleRec({rig.cx - bw / 2.0f, surfY, 5.0f, cutY - surfY + 4.0f}, Fade(BLACK, 0.45f));
    DrawRectangleRec({rig.cx + bw / 2.0f - 5.0f, surfY, 5.0f, cutY - surfY + 4.0f}, Fade(BLACK, 0.45f));

    // casing at the collar, spoil mounds either side
    DrawEllipse(static_cast<int>(rig.cx - 34.0f), static_cast<int>(surfY - 1.0f),
                16.0f, 6.0f, {70, 62, 49, 255});
    DrawEllipse(static_cast<int>(rig.cx + 34.0f), static_cast<int>(surfY - 1.0f),
                16.0f, 6.0f, {70, 62, 49, 255});
    DrawEllipse(static_cast<int>(rig.cx - 38.0f), static_cast<int>(surfY - 3.0f),
                6.0f, 2.2f, Fade(WHITE, 0.08f));
    DrawEllipse(static_cast<int>(rig.cx + 30.0f), static_cast<int>(surfY - 3.0f),
                6.0f, 2.2f, Fade(WHITE, 0.08f));
    DrawRectangleRec({rig.cx - 24.0f, surfY - 10.0f, 48.0f, 12.0f}, DP_OUT);
    DrawRectangleRec({rig.cx - 22.0f, surfY - 8.5f, 44.0f, 9.0f}, {28, 37, 48, 255});
    DrawRectangleRec({rig.cx - 22.0f, surfY - 8.5f, 44.0f, 1.8f}, Fade(WHITE, 0.18f));

    // the string
    ProsDrawThread(rig, false);
    ProsDrawString(rig, clipTop + 4.0f);
    ProsDrawThread(rig, true);

    // Cracks climb the string as the bit wears (redline's recipe, Dark
    // Plating hairline over OUT): a 3px near-black jag, a heat-orange glow
    // pass, a white catch-light offset right. They appear past 0.55 wear and
    // multiply toward the fracture; fixed per-crack seeds keep each one a
    // stable feature rather than per-frame noise.
    if (hole.state == LineHoleState::DRILLING && !hole.tripping && hole.wear > 0.55f)
    {
        int n = 1 + static_cast<int>((hole.wear - 0.55f) / 0.45f * 5.0f);
        for (int i = 0; i < n; i++)
        {
            unsigned sd = 48271u * static_cast<unsigned>(i + 3);
            auto crnd = [&sd]() {
                sd = sd * 48271u % 2147483647u;
                return static_cast<float>(sd % 1000u) / 1000.0f;
            };
            float y = rig.bitY - 14.0f - crnd() * 110.0f;
            if (y < surfY + 12.0f) continue;
            Vector2 p = {rig.cx - PROS_DRILL_R + 1.5f + crnd() * 9.0f, y};
            for (int s = 0; s < 6; s++)
            {
                Vector2 q = {p.x + (crnd() - 0.48f) * 11.0f, p.y + 2.5f + crnd() * 4.5f};
                DrawLineEx(p, q, 3.0f, {13, 8, 5, 255});
                DrawLineEx(p, q, 1.2f,
                           Fade({255, 130, 35, 255},
                                std::clamp(0.25f + hole.heat, 0.0f, 1.0f)));
                DrawLineEx({p.x + 1.6f, p.y}, {q.x + 1.6f, q.y}, 1.0f,
                           Fade(WHITE, 0.28f));
                p = q;
            }
        }
    }

    // heat glow pooling at the working point
    if (hole.heat > 0.04f)
    {
        DrawCircleGradient({rig.cx, rig.bitY}, 58.0f,
                           Fade({255, 110, 30, 255}, 0.42f * hole.heat), BLANK);
    }

    // cuttings ride the flights while the string actually cuts -- more of
    // them the harder it is driven
    float rpmN = hole.rpm / DRILL_RPM_MAX;
    if (turning && !cooling &&
        prosChips.size() < static_cast<size_t>(20.0f + 60.0f * rpmN))
        for (int i = 0; i < (rpmN > 0.6f ? 3 : 2); i++)
            prosChips.push_back({rig.bitY - 6.0f, DpRnd() * 6.28f,
                                 22.0f + DpRnd() * 22.0f, LayerOfDepthM(hole.depthM)});
    for (int i = static_cast<int>(prosChips.size()) - 1; i >= 0; i--)
    {
        ProsChip& c = prosChips[i];
        c.y -= c.up * dt * (turning ? (0.4f + 1.2f * rpmN) : 0.2f);
        c.a -= (2.0f + 8.0f * rpmN) * dt;
        if (c.y < surfY - 2.0f) { prosChips.erase(prosChips.begin() + i); continue; }
        Color cc = DP_ROCK_GRAIN[c.layer];
        DrawRectangleRec({rig.cx + (ProsRadAt(rig, c.y) + 2.2f) * sinf(c.a), c.y, 2.8f, 2.1f},
                         Fade(cc, cosf(c.a) > 0.0f ? 0.95f : 0.45f));
    }
    // debris tumbling in the annulus just above the bit -- never below it
    if (turning)
        for (int i = 0; i < 6; i++)
        {
            float a = fmodf(ps->gameTime * 0.7f + i * 1.7f, 6.28f);
            float dy = rig.bitY - 9.0f - fmodf(i * 15.0f + ps->gameTime * 11.0f, 40.0f);
            if (dy < surfY + 4.0f) continue;
            DrawRectangleRec({rig.cx + sinf(a) * (ProsRadAt(rig, dy) + 2.0f), dy, 2.6f, 2.6f},
                             Fade({120, 104, 84, 255}, 0.7f));
        }
    // sparks where hard rock is being cut
    float hardNow = DrillHardnessAtM(hole.depthM);
    if (turning && !cooling && hardNow > 0.5f)
        for (int i = 0; i < static_cast<int>(8.0f * hardNow * (0.3f + rpmN)); i++)
        {
            float a = static_cast<float>(GetRandomValue(0, 314)) / 100.0f;
            float r2 = 6.0f + static_cast<float>(GetRandomValue(0, 220)) / 10.0f;
            DrawRectangleRec({rig.cx + cosf(a) * r2,
                              rig.bitY + 6.0f - static_cast<float>(GetRandomValue(0, 60)) / 10.0f
                              + sinf(a) * 5.0f, 2.2f, 2.2f},
                             Fade({255, static_cast<unsigned char>(170 + GetRandomValue(0, 70)), 60, 255},
                                  0.4f + static_cast<float>(GetRandomValue(0, 50)) / 100.0f));
        }

    // the prescribed line, mud projection: shadow + advance below the bit
    if (hole.state != LineHoleState::NONE)
    {
        float endY = dg.YOf(hole.endM);
        Vector2 tip = {rig.cx, dg.YOf(std::min(ProsShownDepthM(hole), hole.endM)) + 3.0f};
        if (hole.depthM < hole.endM - 0.5f)
        {
            DrawLineEx(tip, {rig.cx, endY}, 6.0f, Fade(EXT_ACCENT_CYAN, 0.10f));
            DrawLineEx(tip, {rig.cx, endY}, 1.0f, Fade(PROS_SHADOW_HAIR, 0.30f));
            DpDashed(tip, {rig.cx, endY}, 4.0f, 8.0f, -ps->gameTime * 26.0f,
                       1.3f, Fade(EXT_ACCENT_CYAN, 0.55f));
            DrawRectangleRec({rig.cx - 5.0f, endY, 10.0f, 1.6f}, EXT_ACCENT_CYAN);
        }
        // assay ticks accrue down the drilled wall, Measured green
        for (float m = 6.0f; m < hole.depthM; m += 6.0f)
            DrawRectangleRec({rig.cx + PROS_DRILL_R + 6.0f, dg.YOf(m), 3.4f, 1.5f}, EXT_ACCENT_GREEN);
        // Twin cursor -- one dot per projection, so it exists only while
        // there is a bit for it to mark (Dark Plating section 9.3).
        if (stringInGround)
        {
            float pulse = 3.9f + 1.1f * sinf(ps->gameTime * 12.6f);
            DpFillDiamond(tip.x, tip.y, pulse + 1.4f, DP_OUT);
            DpFillDiamond(tip.x, tip.y, pulse, {244, 198, 106, 255});
        }
    }

    // past the redline the whole face warms -- felt before the gauge is read
    if (hole.heat > 0.78f)
        DrawRectangleRec({dg.x, clipTop, dg.w, clipBot - clipTop},
                         Fade({255, 60, 20, 255}, 0.10f * (hole.heat - 0.78f) / 0.22f));

    ProsDrawHead(rig.cx, surfY, clipTop, turning, cooling);

    // The core log (redline's third panel, folded into the dock): one lane
    // down the left edge, each stick graded by the thermal dose it was cut
    // under, in redline's own log legend. Sticks are counted PER STRATUM,
    // so they come out equal height on screen AND level with the string --
    // a fixed metre length could not be both, because the strip gives every
    // stratum an equal band while they hold 12/22/34/52 m.
    if (hole.state != LineHoleState::NONE)
    {
        float lx = dg.x + 4.0f, lw = 7.0f;
        float laneH = botY - surfY;
        // One ground joins the panels (Dark Plating section 9.1): the lane
        // reads depth through the SAME mapping as the strip and the string.
        DrawRectangleRec({lx - 1.5f, surfY - 1.5f, lw + 3.0f, laneH + 3.0f},
                         DP_OUT);
        for (int iv = 0; iv < PROS_LOG_INTERVALS; iv++)
        {
            float m0 = ProsLogTopM(iv);
            float m1 = ProsLogBottomM(iv);
            // Drawn through the STRIP's mapping, so the record stands level
            // with the string beside it. Equal sticks per stratum keep every
            // bar the same height even though the mapping is per-band.
            // Pixel-snapped: fractional stick edges rasterised to uneven
            // 0/1/2 px gaps, which read as random breaks in the record.
            float y0 = floorf(dg.YOf(m0)) + 1.0f;
            float y1 = floorf(dg.YOf(m1));
            if (y1 <= y0) continue;
            Color fill = {15, 24, 33, 255};                      // uncut
            switch (hole.logQ[iv])
            {
                case 3: fill = {147, 167, 184, 255}; break;      // intact
                case 2: fill = {92, 102, 117, 255};  break;      // partial
                case 1: fill = {58, 30, 22, 255};    break;      // lost: hot, not a hole
                default: break;
            }
            DrawRectangleRec({lx, y0, lw, y1 - y0}, fill);
            if (hole.logQ[iv] == 1)
                DrawRectangleLinesEx({lx, y0, lw, y1 - y0}, 1.0f, {150, 62, 34, 255});
            // volatiles tick on cut sticks of the icy stratum. Sticks are
            // per-stratum now, so this is a layer test, not a straddle test.
            if (hole.logQ[iv] != 0 && iv / PROS_LOG_PER_LAYER == 2)
                DrawRectangleRec({lx, y0, 2.0f, y1 - y0}, Fade(DP_ICE_FLECK, 0.85f));
        }
        // stratum seams, so the lane's linear scale stays tied to the rock
        for (int L = 1; L < 4; L++)
            DrawRectangleRec({lx - 1.5f, dg.YOf(LayerTopM(L)) - 0.5f,
                              lw + 3.0f, 1.2f}, Fade(DP_ROCK_EDGE[L], 0.9f));
        // the core-landing flash still pings the whole stratum it came from
        for (int L = 0; L < 4; L++)
        {
            if (!hole.cored[L]) continue;
            float flash = 1.0f - (ps->gameTime - hole.coredTime[L]) / 0.5f;
            if (flash > 0.0f)
                DrawRectangleRec({lx, dg.YOf(LayerTopM(L)), lw,
                                  dg.YOf(LayerBottomM(L)) - dg.YOf(LayerTopM(L))},
                                 Fade(WHITE, 0.6f * flash));
        }
        // The bit's own tick rides the lane as a third cursor, centred on the
        // SAME point as the strip's amber diamond (which sits +3 px, on the
        // cone tip) -- twin cursors share one ground, so they must be level to
        // the pixel, not merely close.
        DrawRectangleRec({lx - 1.5f, dg.YOf(std::min(depthShown, hole.endM)) + 2.0f,
                          lw + 3.0f, 2.0f}, {244, 198, 106, 255});

        // The lane's legend, redline's own chip, bottom-right of the box:
        // a small backed console plate so it reads on any rock behind it.
        {
            float gw = 42.0f, gh = 42.0f;
            float gx = dg.x + dg.w - gw - 5.0f;
            float gy = botY - gh - 5.0f;
            DrawRectangleRec({gx - 1.5f, gy - 1.5f, gw + 3.0f, gh + 3.0f}, DP_OUT);
            DrawRectangleRec({gx, gy, gw, gh}, {13, 21, 30, 235});
            struct { Color c; const char* n; } items[4] = {
                {{147, 167, 184, 255}, "INTCT"},
                {{92, 102, 117, 255},  "PART"},
                {{58, 30, 22, 255},    "LOST"},
                {DP_ICE_FLECK,       "ICE"},
            };
            for (int k = 0; k < 4; k++)
            {
                float ry = gy + 3.0f + k * 10.0f;
                DrawRectangleRec({gx + 4.0f, ry + 1.0f, 6.0f, 6.0f}, items[k].c);
                if (k == 2)
                    DrawRectangleLinesEx({gx + 4.0f, ry + 1.0f, 6.0f, 6.0f},
                                         1.0f, {150, 62, 34, 255});
                DrawTextEx(bodyFont, items[k].n, {gx + 14.0f, ry},
                           fsSmall, sp, EXT_DIM_TEXT);
            }
        }
    }

    // The other half of the hover twin cursor. The plane's depth is ONE
    // number, so the guide sits level and only the dot travels -- sideways,
    // tracking where across the section the pointer is. A cursor that walked
    // up and down as the pointer crossed a plane was the whole misreading:
    // it made the screen axis running away from the viewer look like depth.
    if (hoverM >= 0.0f)
    {
        float hy = dg.YOf(hoverM);
        DrawLineEx({dg.x + 14.0f, hy}, {dg.x + dg.w - 24.0f, hy}, 1.0f,
                   Fade(EXT_ACCENT_CYAN, 0.30f * hoverAlpha));
        const char* dm = TextFormat("%.0f", hoverM);
        DrawTextEx(bodyFont, dm, {dg.x + dg.w - 21.0f, hy - 4.0f},
                   fsSmall, sp, Fade(EXT_ACCENT_CYAN, 0.85f * hoverAlpha));
        if (hoverU >= 0.0f)
        {
            float hx = dg.x + 8.0f + std::clamp(hoverU, 0.0f, 1.0f) * (dg.w - 34.0f);
            DrawCircleV({hx, hy}, 3.4f, Fade(DP_OUT, 0.85f * hoverAlpha));
            DrawCircleV({hx, hy}, 2.0f, Fade(EXT_ACCENT_CYAN, hoverAlpha));
        }
    }

    // tag, bottom-right
    {
        const char* tag = "BOREHOLE";
        float tw = MeasureTextEx(bodyFont, tag, fsSmall, sp).x;
        (void)tw;
        DrawTextEx(bodyFont, tag, {dg.x + 5.0f, botY + 5.0f},
                   fsSmall, sp, EXT_DIM_TEXT);
    }
    rlPopMatrix();
    EndScissorMode();

    // border: solid except the model-facing edge, which is a dashed cut mark
    DrawLineEx({dg.x, clipTop}, {dg.x + dg.w, clipTop}, 1.0f, EXT_PANEL_BORDER);
    DrawLineEx({dg.x, clipBot}, {dg.x + dg.w, clipBot}, 1.0f, EXT_PANEL_BORDER);
    DrawLineEx({dg.x + dg.w, clipTop}, {dg.x + dg.w, clipBot}, 1.0f, EXT_PANEL_BORDER);
    DpDashed({dg.x, clipTop}, {dg.x, clipBot}, 4.0f, 5.0f, 0.0f, 1.0f,
               Fade(EXT_PANEL_BORDER, 0.9f));
}

// One point of the prescribed line in BLOCK space: the point sits ON ITS
// PLATE, at the drawn position of the cell the line passes through there --
// on the plate's SURFACE, at the height the cell was drawn with, which is
// also what the pick hit when the player clicked it. (Lift 0 put the collar
// up to a full relief under the mound it was clicked on.)
// The earlier band-space form (dg.YOf(m) + iso row offset) double-counted
// the row -- the row IS the depth under the plate-slab mapping -- so both
// ends drifted off the clicked cells, worst at the plate corners.
static Vector2 ProsLinePoint(ProspectingSystem* ps, const BlockModelGeom& g,
                             const BlockPlateLift& pl, float m)
{
    float fx = 0.0f, fy = 0.0f;
    ps->GetLineCell(m, fx, fy);
    int L = LayerOfDepthM(std::min(m, ps->lineHole.endM));
    int ci = static_cast<int>(floorf(fx)), cj = static_cast<int>(floorf(fy));
    return g.Iso(fx + 0.5f, fy + 0.5f, L, pl.At(g, L, ci, cj));
}
static Vector2 ProsTraceAt(ProspectingSystem* ps, const BlockModelGeom& g,
                           const DockGeom& dg, const BlockPlateLift& pl, float m)
{
    const LineHole& hole = ps->lineHole;
    Vector2 P0 = ProsLinePoint(ps, g, pl, 0.0f);
    Vector2 P1 = ProsLinePoint(ps, g, pl, hole.endM);
    float u = (dg.YOf(m) - dg.YOf(0.0f)) / std::max(1.0f, dg.YOf(hole.endM) - dg.YOf(0.0f));
    return {P0.x + (P1.x - P0.x) * u, P0.y + (P1.y - P0.y) * u};
}

// The prescribed line over the stack: shadow, string, advance, twin cursor,
// crossing rings, flip flash, active-plate rim (Dark Plating section 9).
static void ProsDrawTraceBlock(ProspectingSystem* ps, const BlockModelGeom& g,
                               const DockGeom& dg, const BlockPlateLift& pl)
{
    const LineHole& hole = ps->lineHole;
    // The line over the plates is the LIVE OPERATION, not a record: a hole
    // being aimed, cut, or hoisted out of. When the string clears the collar
    // the line goes with it, leaving the plates showing only what the hole
    // taught them (cored cells, flipped classes). The record of the hole
    // itself lives in the dock -- the borehole and the core log lane.
    if (hole.state == LineHoleState::NONE ||
        hole.state == LineHoleState::DONE) return;

    Vector2 P0 = ProsTraceAt(ps, g, dg, pl, 0.0f);
    Vector2 P1 = ProsTraceAt(ps, g, dg, pl, hole.endM);
    Vector2 Pn = ProsTraceAt(ps, g, dg, pl, std::min(ProsShownDepthM(hole), hole.endM));

    // shadow of the full prescribed path -- always there, quiet
    DrawLineEx(P0, P1, 6.0f, Fade(EXT_ACCENT_CYAN, 0.10f));
    DrawLineEx(P0, P1, 1.0f, Fade(PROS_SHADOW_HAIR, 0.30f));
    // the advance, progressing along the shadow
    if (hole.depthM < hole.endM - 0.5f)
        DpDashed(Pn, P1, 4.0f, 8.0f, -ps->gameTime * 26.0f, 1.3f,
                   Fade(EXT_ACCENT_CYAN, 0.55f));
    // the string: barber-pole banding on the drill's own spin
    if (hole.depthM > 0.5f)
    {
        DrawLineEx(P0, Pn, 5.0f, DP_OUT);
        DrawLineEx(P0, Pn, 3.0f, PROS_STRING_MID);
        DpDashed(P0, Pn, 6.0f, 10.0f, prosSpin * 6.0f, 3.0f, PROS_STRING_LIT);
    }

    // crossing rings + flip flash on the cells the line cored
    for (int L = 0; L <= hole.targetLayer; L++)
    {
        if (!hole.cored[L]) continue;
        int cx = 0, cy = 0;
        ps->GetCrossingCell(L, cx, cy);
        Vector2 c = g.Iso(cx + 0.5f, cy + 0.5f, L, 0.0f);
        DrawCircleV(c, 2.6f, DP_OUT);
        DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 2.6f, EXT_TEXT);
        float pop = 1.0f - (ps->gameTime - hole.coredTime[L]) / 0.5f;
        if (pop > 0.0f)
        {
            Vector2 q[4] = { g.Iso(static_cast<float>(cx),     static_cast<float>(cy),     L, 0.0f),
                             g.Iso(static_cast<float>(cx + 1), static_cast<float>(cy),     L, 0.0f),
                             g.Iso(static_cast<float>(cx + 1), static_cast<float>(cy + 1), L, 0.0f),
                             g.Iso(static_cast<float>(cx),     static_cast<float>(cy + 1), L, 0.0f) };
            DpFillQuad(q[0], q[1], q[2], q[3], Fade(WHITE, 0.55f * pop));
        }
    }

    // The active-plate rim lives in DrawBlockLayer, which knows the
    // per-corner lifts it has to hug. Its pulse rides this same clock.

    // twin cursor, same clock as the mud panel's
    float pulse = 3.9f + 1.1f * sinf(ps->gameTime * 12.6f);
    DpFillDiamond(Pn.x, Pn.y, pulse + 1.4f, DP_OUT);
    DpFillDiamond(Pn.x, Pn.y, pulse, {244, 198, 106, 255});
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
    if (ps->UpdateLineHole(GetFrameTime()))
    {
        unit->PublicShowMessage("Line complete - every layer it crossed is cored");
    }
    Vector2 mouse = ColonyGetMousePosition();

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

    // --- Content area ---
    float contentY = yPos;
    float contentH = static_cast<float>(y + h - padding) - yPos;

    auto& grid = ps->GetGrid();
    int gridSize = grid.GetGridSize();

    // =======================================================================
    // ONE SCREEN. No tabs.
    //
    // The tabs were SWEEP / SAMPLES / LAB. All three are gone:
    //
    //   SWEEP   was one action with one parameter, and never needed a screen.
    //   SAMPLES held the real decision -- where and how deep -- parked next to
    //           a tray of crystals, which is what made it feel like inventory.
    //   LAB     asked you to assay a core you had already paid to drill. That
    //           is not a choice, it is a delay with a UI: a recovered core is
    //           rock you are holding, so what it cuts is known.
    //
    // What is left is the model, and two things you can do to it: sweep the
    // surface, or drill a spot. See docs/design/prospecting/block-model-design.md
    // =======================================================================

    float dockW = 104.0f;
    float modelW = pw * 0.60f - dockW;
    float modelH = contentH - 30.0f;
    float gridX = px;
    float gridY = contentY;

    // Which element the relief is showing. Confidence -- and so the class
    // envelopes -- are the same for every element, because one core is assayed
    // for all of them; only the height field and the tonnage change. Until the
    // switcher exists, show the cell's richest.
    ResourceType shown = ResourceType::Fe;
    {
        float best = -1.0f;
        for (const auto& kv : grid.GetGroundTruth(gridSize / 2, gridSize / 2,
                                                  DepthLayer::SURFACE))
        {
            if (kv.second > best) { best = kv.second; shown = kv.first; }
        }
    }

    BlockModelGeom geom = MakeBlockGeom(gridSize, gridX, gridY, modelW, modelH);
    DockGeom dock = DockFromBlock(geom, gridX + modelW + 6.0f, dockW);

    // The powerhead needs sky. If the stack starts too close to the panel
    // top (it did on the playtest layout, and the head was scissored away),
    // push it down and rebuild -- the strip's bands are derived from the
    // plate slots, so both move together and stay aligned.
    {
        float sky = dock.bandTop[0] - contentY;
        if (sky < 64.0f)
        {
            float push = 64.0f - sky;
            geom = MakeBlockGeom(gridSize, gridX, gridY + push, modelW, modelH - push);
            dock = DockFromBlock(geom, gridX + modelW + 6.0f, dockW);
        }
    }

    // One ground, both panels (Dark Plating section 9.1): the strata bands run
    // dim under the whole stack and full-strength inside the dock, and the
    // boundary rules cross unbroken through the explosion gaps. Same rock as
    // the dock wears, same tiling, just quieter -- these bands are the ground
    // the plates float in, and the plates have to stay the loudest thing in
    // their own half of the panel.
    if (!strataLoaded) LoadStrataTextures();
    for (int L = 0; L < 4; L++)
    {
        Rectangle band = {gridX, dock.bandTop[L], dock.x - gridX,
                          dock.bandTop[L + 1] - dock.bandTop[L]};
        if (strataLoaded && strataTex[L].id != 0)
        {
            float k = static_cast<float>(RockTexture::SIZE) / DP_ROCK_TEX_PX;
            Color tint = { static_cast<unsigned char>(std::min(255, DP_ROCK_COL[L].r * 2)),
                           static_cast<unsigned char>(std::min(255, DP_ROCK_COL[L].g * 2)),
                           static_cast<unsigned char>(std::min(255, DP_ROCK_COL[L].b * 2)),
                           255 };
            // Far dimmer than a RESTING plate, not just dimmer than a lit
            // one: at 0.34 this camouflaged the plates it was supposed to sit
            // behind -- the dim plates rest at 0.38-0.50 of full, so the
            // ground behind them has to be a fraction of THAT, or the panel
            // reads as one texture with diamonds faintly in it.
            DrawTexturePro(strataTex[L], {0.0f, L * 41.0f, band.width * k, band.height * k},
                           band, {0.0f, 0.0f}, 0.0f, Fade(tint, 0.20f));
        }
        else
        {
            DrawRectangleRec(band, Fade(DP_ROCK_COL[L], 0.18f));
        }
        DrawRectangleRec({gridX, dock.bandTop[L], dock.x - gridX, 1.6f},
                         Fade(DP_ROCK_EDGE[L], 0.85f));
    }
    DrawRectangleRec({gridX, dock.bandTop[4] - 1.0f, dock.x - gridX, 1.6f},
                     Fade(DP_ROCK_EDGE[3], 0.85f));

    // Build all four layers first so one grade scale covers the stack --
    // per-layer normalisation would make a barren layer look as rich as the
    // ore, which is the whole thing the relief exists to show.
    std::vector<std::vector<BlockCell>> layers(4);
    float maxGrade = 0.0001f;
    // BELIEVED grade, never ground truth. An undrilled layer is a flat plate
    // at the layer mean -- the relief is what you have learned, and it grows
    // as cores go in. Drawing truth here was the review's first finding: the
    // model was the answer key. Built as ONE field per frame: the per-cell
    // scalar calls were O(N^4) at 16x16 and cost 55 ms/frame on their own.
    EstimateField field = BuildEstimateField(grid, shown);
    for (int L = 0; L < 4; L++)
    {
        layers[L].resize(gridSize * gridSize);
        for (int gy = 0; gy < gridSize; gy++)
        {
            for (int gx = 0; gx < gridSize; gx++)
            {
                BlockCell& c = layers[L][gy * gridSize + gx];
                // Drained by what has already been taken out, so the relief
                // reads as what is LEFT rather than what was once there. That
                // is the whole difference from prospecting's stack, which
                // asks what is in the ground; this one asks what is still
                // worth sending a machine to.
                //
                // Ground that has been dug does not read as ground that was
                // always poor, and it does not need a marker to say so:
                // digging sets confidence to 1.0 at that spot and depth, so a
                // worked-out cell is a MEASURED cell with no relief, while
                // barren unsurveyed ground is UNCLASSIFIED with no relief.
                // The class colour already carries it.
                float left = 1.0f - std::clamp(
                    grid.GetSubCell(gx, gy).workedFraction[L], 0.0f, 1.0f);
                c.grade = field.GradeAt(gx, gy, L) * left;
                c.cls = GetResourceClass(field.ConfidenceAt(gx, gy, L));
                maxGrade = std::max(maxGrade, c.grade);
            }
        }
    }

    // Hang every plate from its own ceiling: push it down by its tallest
    // corner, so that corner lands on the plate's slot and the plate's top is
    // in the same place whatever its layer holds. Taken over CORNERS, not
    // cells, because a corner averages the four blocks that touch it and is
    // what the surface is actually drawn through.
    for (int L = 0; L < 4; L++)
    {
        float top = 0.0f;
        for (int j = 0; j <= gridSize; j++)
            for (int i = 0; i <= gridSize; i++)
            {
                float lift = BlockCornerLift(layers[L], gridSize, maxGrade,
                                            geom.relief, i, j);
                if (lift > top) top = lift;
            }
        geom.plateDrop[L] = top;
    }

    // DEPTH-LEVEL names (the DepthLayer enum), not geology names. The strata
    // are named on the borehole strip, where the rock actually is; naming them
    // here too put "INTACT" (intact basalt) one column from the core log's
    // "INTCT" (intact core recovered), so a rock label read as a core grade.
    // One vocabulary per instrument.
    static const char* depthLabels[4] = {"0 m", "12 m", "34 m", "68 m"};
    static const char* levelLabels[4] = {"SURFACE", "SHALLOW", "MID", "DEEP"};

    int focusDepth = static_cast<int>(ps->selectedDepth);
    // The stratum being cut rim-lights its plate, pulsing on the SAME clock as
    // the twin cursors (Dark Plating section 9.3 -- an unsynced phase breaks
    // the illusion that the two dots are one object).
    int rimLayer = ps->lineHole.state == LineHoleState::DRILLING
                 ? LayerOfDepthM(ps->lineHole.depthM) : -1;
    float rimPulse = 0.45f + 0.25f * sinf(ps->gameTime * 12.6f);
    // ---- Which plate is under the pointer, decided BEFORE anything is
    // drawn: the plates' brightness now depends on it (a hovered plate comes
    // up to full), and a hover computed after the draw would light the plate
    // one frame late -- which on a fast pointer is a visible smear of the
    // wrong layer.
    //
    // Analytic pick: invert the iso transform per plate, so every pixel of a
    // plate maps to its nearest cell -- at 16x16 per-cell hit rects would be
    // smaller than any finger. Inverted against the LIFTED surface the player
    // can see, not each plate's base plane: the lift reaches g.relief while a
    // tile is only ~3.9 px tall, so a flat inversion picked several rows off
    // the block under the cursor, and the x1.5 relief pass widened that.
    // See src/Prospecting/block_pick.h; the round trip is under test.
    BlockPickGeom pick;
    pick.originX = geom.originX; pick.originY = geom.originY;
    pick.tileX = geom.tileX;     pick.tileY = geom.tileY;
    pick.gap = geom.gap;         pick.size = gridSize;

    auto LiftAt = [&](int L, int i, int j) {
        // The same height the surface is drawn at (the one lift law).
        return BlockCellLift(layers[L], gridSize, maxGrade, geom.relief, i, j);
    };
    int hovL = -1, hovX = -1, hovY = -1;
    for (int L = 0; L < 4 && hovL < 0; L++)
    {
        int pi = 0, pj = 0;
        // The plate is drawn plateDrop lower than its slot, so the pointer has
        // to be read in the plate's own frame -- otherwise picking answers for
        // where the plate would have been.
        if (!BlockPickCell(pick, L, mouse.x, mouse.y - geom.plateDrop[L],
                           [&](int i, int j) { return LiftAt(L, i, j); }, pi, pj)) continue;
        hovL = L; hovX = pi; hovY = pj;
    }
    if (hovL < 0 && ps->previewHoverLayer >= 0)
    {
        hovL = std::clamp(ps->previewHoverLayer, 0, 3);
        hovX = gridSize / 2; hovY = gridSize / 2;
    }
    // Four plates of data is more than anyone reads at once: the surface
    // stays lit, the three below rest dim and the one under the pointer comes
    // up. Eased on the facade, which is where state that outlives a frame
    // belongs (prospecting_constants.h has the table).
    ps->UpdatePlateLight(hovL, rimLayer, GetFrameTime());

    for (int L = 0; L < 4; L++)
    {
        // The stratum's own rock -- four textures for four layers, not one
        // world tile reused; the strip's band at this depth wears the same.
        const Texture2D* tile = (strataLoaded && strataTex[L].id != 0)
                              ? &strataTex[L] : nullptr;
        DrawBlockLayer(geom, layers[L], L, maxGrade, ps->plateLight[L],
                           bodyFont, sp,
                           depthLabels[L], levelLabels[L], nullptr, nullptr,
                           tile, L == rimLayer ? rimPulse : 0.0f);
    }

    // The hovered cell's outline and its cursor DOT, drawn after the plates so
    // they sit on top of the one they mark rather than under the plate below.
    // The dot is half of a twin cursor (Dark Plating 9.3): its other half sits
    // in the borehole strip at the same depth, and the two move together --
    // which is what says "this point on this plane IS that point in the rock".
    if (hovL >= 0)
    {
        float lift = LiftAt(hovL, hovX, hovY);
        Vector2 dot = geom.Iso(hovX + 0.5f, hovY + 0.5f, hovL, lift);
        Vector2 q0 = geom.Iso(static_cast<float>(hovX), static_cast<float>(hovY), hovL, lift);
        Vector2 q1 = geom.Iso(static_cast<float>(hovX + 1), static_cast<float>(hovY), hovL, lift);
        Vector2 q2 = geom.Iso(static_cast<float>(hovX + 1), static_cast<float>(hovY + 1), hovL, lift);
        Vector2 q3 = geom.Iso(static_cast<float>(hovX), static_cast<float>(hovY + 1), hovL, lift);
        DrawLineEx(q0, q1, 1.2f, Fade(PROS_HOVER_BORDER, 0.9f));
        DrawLineEx(q1, q2, 1.2f, Fade(PROS_HOVER_BORDER, 0.9f));
        DrawLineEx(q2, q3, 1.2f, Fade(PROS_HOVER_BORDER, 0.9f));
        DrawLineEx(q3, q0, 1.2f, Fade(PROS_HOVER_BORDER, 0.9f));
        DrawCircleV(dot, 3.4f, Fade(DP_OUT, 0.85f));
        DrawCircleV(dot, 2.0f, EXT_ACCENT_CYAN);

    }

    // ---- The line is drawn with two CLICKS, not a drag: click a SURFACE
    // block to collar it, then click a block on the layer the hole should
    // reach -- the string starts on that second click (and charges energy).
    // While aiming, the dashed preview follows the pointer. Clicking the
    // collar block again cancels; a click anywhere else just selects.
    bool stringDown = ps->lineHole.state == LineHoleState::DRILLING;
    bool aiming = ps->lineHole.state == LineHoleState::AIMING;

    if (aiming && hovL > 0) ps->AimAt(hovL, hovX, hovY);   // preview tracks the pointer

    if (hovL >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (aiming && hovL == 0 &&
            hovX == ps->lineHole.collarX && hovY == ps->lineHole.collarY)
        {
            ps->CancelAim();
        }
        else if (hovL == 0 && !stringDown)
        {
            ps->StartAim(hovX, hovY);
            ps->selectedCellX = hovX; ps->selectedCellY = hovY;
            ps->selectedDepth = DepthLayer::SURFACE;
        }
        else if (aiming && hovL > 0)
        {
            ps->AimAt(hovL, hovX, hovY);
            float lineCost = DrillEnergyToDepthMetres(ps->lineHole.endM);
            if (unit->ConsumeResource(ResourceType::ENERGY, lineCost))
            {
                ps->CommitHole();
                unit->PublicShowMessage(TextFormat(
                    "String down - drilling the line (%.0f E)", lineCost));
            }
            else
            {
                unit->PublicShowMessage(TextFormat(
                    "The line needs %.0f E - aim shallower or wait for energy", lineCost));
            }
            ps->selectedCellX = hovX; ps->selectedCellY = hovY;
            ps->selectedDepth = static_cast<DepthLayer>(hovL);
        }
        else
        {
            ps->selectedCellX = hovX; ps->selectedCellY = hovY;
            ps->selectedDepth = static_cast<DepthLayer>(hovL);
        }
    }

    // Redline's clicking, in the game: while the string is down, clicking
    // the borehole strip drives the spindle -- more advance, more heat.
    if (stringDown && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, {dock.x, contentY, dock.w,
                                       dock.bandTop[4] + 18.0f - contentY}))
    {
        ps->KickString();
    }

    // the line over the stack, after the plates so it reads as through them
    BlockPlateLift plateLift; plateLift.layers = &layers; plateLift.maxGrade = maxGrade;
    ProsDrawTraceBlock(ps, geom, dock, plateLift);
    // ONE depth for the whole plane. Moving across a plate slides the strip's
    // cursor sideways, never up or down -- depth is the axis between plates.
    float hoverM = (hovL >= 0) ? PlatePlaneM(hovL) : -1.0f;
    // Off the plates, the pointer still has a height, and the strata bands run
    // the full width of the panel -- so bare ground between the plates is a
    // depth too. Reading it out is what ties the two panels together: sweep
    // the pointer down the empty rock and the strip's cursor tracks it, which
    // says "these two views are the same column" more directly than any
    // static rule can. Paler than the cell cursor, because it marks a height
    // and not a block you could drill.
    bool groundHover = false;
    if (hovL < 0 && mouse.x >= gridX && mouse.x < dock.x &&
        mouse.y >= dock.bandTop[0] && mouse.y <= dock.bandTop[4])
    {
        groundHover = true;
        hoverM = dock.DepthAtY(mouse.y);
    }
    // Where across the section that cell sits. The strip is a vertical slice,
    // so its horizontal axis is the same left-right the plates are drawn with:
    // iso screen x is (gx - gy), so this is that, normalised.
    float hoverU = (hovL >= 0)
        ? (static_cast<float>(hovX - hovY) + gridSize) / (2.0f * gridSize)
        : groundHover
        ? std::clamp((mouse.x - gridX) / std::max(1.0f, dock.x - gridX), 0.0f, 1.0f)
        : -1.0f;
    if (groundHover)
    {
        // the pointer's own mark, on the ground it is reading
        DrawCircleV(mouse, 3.4f, Fade(DP_OUT, 0.30f));
        DrawCircleV(mouse, 2.0f, Fade(Color{198, 232, 250, 255}, 0.60f));
    }
    ProsDrawBoreholeDock(unit, ps, dock, contentY, dock.bandTop[4] + 18.0f,
                         hoverM, hoverU, groundHover ? 0.55f : 1.0f,
                         bodyFont, sp, FS(7.5f),
                         strataLoaded ? strataTex : nullptr);

    // ONE marker, on the layer it was selected on. The line's own points are
    // drawn by the trace: the collar ring while aiming, crossing rings as the
    // bit cores them.
    if (ps->selectedCellX >= 0 && ps->selectedCellY >= 0 &&
        ps->lineHole.state == LineHoleState::NONE)
    {
        Vector2 c = geom.Iso(ps->selectedCellX + 0.5f, ps->selectedCellY + 0.5f,
                             focusDepth, 0.0f);
        DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 4.5f,
                        EXT_ACCENT_CYAN);
    }
    if (ps->lineHole.state == LineHoleState::AIMING)
    {
        Vector2 c = geom.Iso(ps->lineHole.collarX + 0.5f,
                             ps->lineHole.collarY + 0.5f, 0, 0.0f);
        DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 4.5f,
                        Color{244, 198, 106, 255});
    }

    // --- Legend, two rows so the element line and the swatches cannot collide
    float legendY = gridY + modelH + 2.0f;
    DrawTextEx(bodyFont, TextFormat("%s   height = grade   colour = class",
                                    ResourceTypeToString(shown)),
               {gridX, legendY}, FS(8.5f), sp, EXT_DIM_TEXT);
    {
        float swX = gridX;
        float swY = legendY + 12.0f;
        const ResourceClass legendCls[3] = { ResourceClass::MEASURED,
                                             ResourceClass::INDICATED,
                                             ResourceClass::INFERRED };
        for (int k = 0; k < 3; k++)
        {
            DrawRectangleRounded({swX, swY + 1.0f, 7.0f, 7.0f}, 0.3f, 4,
                                 ExtClassColor(legendCls[k]));
            const char* nm = ResourceClassName(legendCls[k]);
            DrawTextEx(bodyFont, nm, {swX + 10.0f, swY - 1.0f}, FS(8.0f), sp, EXT_DIM_TEXT);
            swX += 10.0f + MeasureTextEx(bodyFont, nm, FS(8.0f), sp).x + 10.0f;
        }
        DrawTextEx(bodyFont, TextFormat("REACH %dx%d", grid.GetReach(), grid.GetReach()),
                   {swX + 4.0f, swY - 1.0f}, FS(8.0f), sp, Fade(EXT_DIM_TEXT, 0.7f));
    }

    // =========================== the control rail ===========================
    float ctrlX = dock.x + dock.w + 15.0f;
    float ctrlY = contentY;
    float ctrlW = px + pw - ctrlX;

    bool hasSelection = (ps->selectedCellX >= 0 && ps->selectedCellX < gridSize &&
                         ps->selectedCellY >= 0 && ps->selectedCellY < gridSize);

    // --- Resource statement: the number the whole loop is trying to grow ---
    DrawTextEx(headerFont, "RESOURCE", {ctrlX, ctrlY}, FS(11.0f), sp, EXT_HEADER_COLOR);
    ctrlY += 18.0f;
    {
        ClassSplit split = GetClassSplit(grid, ps->GetTray(), shown, grid.GetTier());
        const ResourceClass rows[3] = { ResourceClass::MEASURED,
                                        ResourceClass::INDICATED,
                                        ResourceClass::INFERRED };
        for (int k = 0; k < 3; k++)
        {
            float v = split.Get(rows[k]);
            Color c = ExtClassColor(rows[k]);
            DrawRectangleRounded({ctrlX, ctrlY + 3.0f, 7.0f, 7.0f}, 0.3f, 4,
                                 v > 0.0f ? c : Fade(c, 0.3f));
            DrawTextEx(bodyFont, ResourceClassName(rows[k]), {ctrlX + 12.0f, ctrlY},
                       FS(9.5f), sp, v > 0.0f ? EXT_TEXT : Fade(EXT_DIM_TEXT, 0.6f));
            const char* amount = v > 0.0f ? TextFormat("%.0f", v) : "-";
            float aw = MeasureTextEx(bodyFont, amount, FS(9.5f), sp).x;
            DrawTextEx(bodyFont, amount, {ctrlX + ctrlW - 12.0f - aw, ctrlY},
                       FS(9.5f), sp, v > 0.0f ? EXT_TEXT : Fade(EXT_DIM_TEXT, 0.6f));
            ctrlY += 13.0f;
        }
        ctrlY += 3.0f;
        DrawLineEx({ctrlX, ctrlY}, {ctrlX + ctrlW - 12.0f, ctrlY}, 1.0f, EXT_PANEL_BORDER);
        ctrlY += 6.0f;
        DrawTextEx(bodyFont, "Committable", {ctrlX, ctrlY}, FS(9.5f), sp, EXT_DIM_TEXT);
        const char* cm = TextFormat("%.0f", split.Committable());
        float cmw = MeasureTextEx(headerFont, cm, FS(13.0f), sp).x;
        DrawTextEx(headerFont, cm, {ctrlX + ctrlW - 12.0f - cmw, ctrlY - 3.0f},
                   FS(13.0f), sp, EXT_ACCENT_GREEN);
        ctrlY += 20.0f;
    }

    // --- Wide survey: one instrument, one button ---------------------------
    // LIBS reads SURFACE chemistry -- element by element, fast and cheap, and
    // blind to everything below the regolith. It shapes where you drill; it
    // never classifies, because you cannot put tonnage in a statement on the
    // strength of a surface reading.
    DrawTextEx(headerFont, "SURFACE SWEEP", {ctrlX, ctrlY}, FS(11.0f), sp, EXT_HEADER_COLOR);
    ctrlY += 17.0f;
    {
        bool sweptAlready = grid.HasSweptFrequency(0);
        bool affordable = ProsCanAfford(unit, SWEEP_ENERGY_COST[0]);
        bool canSweep = ps->GetSweep().CanSweep(grid, 0) && affordable;
        Rectangle btn = {ctrlX, ctrlY, ctrlW - 12.0f, 26.0f};
        bool hover = CheckCollisionPointRec(mouse, btn);

        DrawRectangleRounded(btn, 0.3f, 4, canSweep && hover ? Color{16, 40, 60, 255}
                                                             : EXT_PANEL_BG2);
        DrawRectangleRoundedLinesEx(btn, 0.3f, 4, 1.0f,
                                    canSweep ? PROS_TAB_ACTIVE_BDR : PROS_BTN_DISABLED);
        const char* label = sweptAlready ? "LIBS  -  SWEPT" : "LIBS ROVER SWEEP";
        Vector2 ls = MeasureTextEx(headerFont, label, FS(10.5f), sp);
        DrawTextEx(headerFont, label,
                   {btn.x + (btn.width - ls.x) / 2.0f, btn.y + (26.0f - ls.y) / 2.0f},
                   FS(10.5f), sp,
                   sweptAlready ? EXT_ACCENT_GREEN
                                : (canSweep ? EXT_ACCENT_CYAN : PROS_BTN_DISABLED));
        // Caption BELOW the button. Inside it, the two lines collided.
        DrawTextEx(bodyFont, TextFormat("%.0f E   surface chemistry only, never classifies",
                                        SWEEP_ENERGY_COST[0]),
                   {btn.x + 1.0f, btn.y + 28.0f}, FS(8.0f), sp, EXT_DIM_TEXT);

        if (hover && canSweep && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            float cost = ps->GetSweep().GetSweepCost(0);
            if (unit->ConsumeResource(ResourceType::ENERGY, cost))
            {
                ps->GetSweep().ExecuteSweep(grid, 0, ps->gameTime);
                unit->PublicShowMessage("LIBS sweep complete - surface chemistry mapped");
            }
        }
        ctrlY += 44.0f;
    }

    // --- Drill: the line, its cost, its progress ---------------------------
    DrawTextEx(headerFont, "DRILL - AUGER", {ctrlX, ctrlY}, FS(11.0f), sp, EXT_HEADER_COLOR);
    ctrlY += 17.0f;

    const LineHole& lh = ps->lineHole;
    if (lh.state == LineHoleState::AIMING && lh.targetLayer > 0)
    {
        float lineCost = DrillEnergyToDepthMetres(lh.endM);
        bool affordable = ProsCanAfford(unit, lineCost);
        static const char* layerNames[4] = {"REGOLITH", "MEGAREGOLITH", "FRACTURED", "BASALT"};
        DrawTextEx(bodyFont, TextFormat("line to %.0f m (%s) - %.0f E",
                                        lh.endM, layerNames[lh.targetLayer], lineCost),
                   {ctrlX, ctrlY}, FS(9.5f), sp,
                   affordable ? EXT_ACCENT_CYAN : EXT_ACCENT_GOLD);
        ctrlY += 13.0f;
        DrawTextEx(bodyFont, "click to drill - the collar block cancels",
                   {ctrlX, ctrlY}, FS(8.0f), sp, EXT_DIM_TEXT);
        ctrlY += 16.0f;
    }
    else if (lh.state == LineHoleState::DRILLING && lh.tripping)
    {
        DrawTextEx(bodyFont, TextFormat("bit fractured at %.0f m - tripping  %.0f / %.0f s",
                                        lh.depthM, lh.tripT, lh.tripDur),
                   {ctrlX, ctrlY}, FS(9.5f), sp, EXT_ACCENT_RED);
        ctrlY += 13.0f;
        DrawTextEx(bodyFont, "out rod by rod, and back - depth is the price",
                   {ctrlX, ctrlY}, FS(8.0f), sp, EXT_DIM_TEXT);
        ctrlY += 16.0f;
    }
    else if (lh.state == LineHoleState::DRILLING)
    {
        DrawTextEx(bodyFont, TextFormat("string down  %.0f / %.0f m%s",
                                        lh.depthM, lh.endM,
                                        lh.dwelling ? "  -  COOLING" : ""),
                   {ctrlX, ctrlY}, FS(9.5f), sp,
                   lh.dwelling ? EXT_ACCENT_GOLD : EXT_TEXT);
        ctrlY += 13.0f;
        DrawTextEx(bodyFont, "click the borehole to drive the string",
                   {ctrlX, ctrlY}, FS(8.0f), sp, EXT_DIM_TEXT);
        ctrlY += 15.0f;
        DrawTextEx(bodyFont, "SPINDLE", {ctrlX, ctrlY + 1.0f}, FS(8.0f), sp, EXT_DIM_TEXT);
        ExtDrawSegBar(ctrlX + 54.0f, ctrlY, ctrlW - 66.0f, 10.0f,
                      lh.rpm / DRILL_RPM_MAX,
                      lh.rpm > 0.9f ? EXT_ACCENT_GOLD : EXT_ACCENT_CYAN);
        ctrlY += 15.0f;
    }
    else if (lh.state == LineHoleState::RETRACTING)
    {
        DrawTextEx(bodyFont, TextFormat("line complete - hoisting  %.0f m",
                                        ProsShownDepthM(lh)),
                   {ctrlX, ctrlY}, FS(9.5f), sp, EXT_ACCENT_GREEN);
        ctrlY += 13.0f;
        DrawTextEx(bodyFont, "the string comes out; the hole and its log stay",
                   {ctrlX, ctrlY}, FS(8.0f), sp, EXT_DIM_TEXT);
        ctrlY += 16.0f;
    }
    else if (lh.state == LineHoleState::DONE)
    {
        DrawTextEx(bodyFont, TextFormat("line complete - %.0f m cored", lh.endM),
                   {ctrlX, ctrlY}, FS(9.5f), sp, EXT_ACCENT_GREEN);
        ctrlY += 13.0f;
        DrawTextEx(bodyFont, "string racked - click a block to line the next",
                   {ctrlX, ctrlY}, FS(8.0f), sp, EXT_DIM_TEXT);
        ctrlY += 16.0f;
    }
    else
    {
        DrawTextEx(bodyFont, "click a surface block, then a block on",
                   {ctrlX, ctrlY}, FS(9.0f), sp, EXT_DIM_TEXT);
        ctrlY += 12.0f;
        DrawTextEx(bodyFont, "the layer the hole should reach",
                   {ctrlX, ctrlY}, FS(9.0f), sp, EXT_DIM_TEXT);
        ctrlY += 16.0f;
    }

    // Bit temperature: the price hard rock charges in time (auto-peck at max)
    if (lh.state == LineHoleState::DRILLING || lh.heat > 0.03f)
    {
        DrawTextEx(bodyFont, "BIT TEMP", {ctrlX, ctrlY + 1.0f}, FS(8.0f), sp, EXT_DIM_TEXT);
        Color hc = lh.heat > 0.8f ? EXT_ACCENT_RED
                 : lh.heat > 0.5f ? EXT_ACCENT_GOLD : EXT_ACCENT_CYAN;
        ExtDrawSegBar(ctrlX + 54.0f, ctrlY, ctrlW - 66.0f, 10.0f, lh.heat, hc);
        ctrlY += 15.0f;
    }
    // Bit wear: time-at-temperature plus metres cut. At full it fractures,
    // and a fracture buys a TRIP -- time scaled by depth, never the run.
    if (lh.state == LineHoleState::DRILLING || lh.wear > 0.02f)
    {
        DrawTextEx(bodyFont, "BIT WEAR", {ctrlX, ctrlY + 1.0f}, FS(8.0f), sp, EXT_DIM_TEXT);
        Color wc = lh.wear > 0.8f ? EXT_ACCENT_RED
                 : lh.wear > 0.55f ? EXT_ACCENT_GOLD : EXT_ACCENT_CYAN;
        ExtDrawSegBar(ctrlX + 54.0f, ctrlY, ctrlW - 66.0f, 10.0f, lh.wear, wc);
        ctrlY += 17.0f;
    }
    ctrlY += 6.0f;

    // --- What is known about the selected spot -----------------------------
    if (hasSelection)
    {
        DrawLineEx({ctrlX, ctrlY - 6.0f}, {ctrlX + ctrlW - 12.0f, ctrlY - 6.0f},
                   1.0f, EXT_PANEL_BORDER);

        const SubCell& selCell = grid.GetSubCell(ps->selectedCellX, ps->selectedCellY);
        float selConf = GetDepthConfidence(grid, ps->GetTray(),
                                           ps->selectedCellX, ps->selectedCellY,
                                           ps->selectedDepth);
        ResourceClass selClass = GetResourceClass(selConf);

        DrawTextEx(headerFont, ResourceClassName(selClass), {ctrlX, ctrlY},
                   FS(11.0f), sp, ExtClassColor(selClass));
        if (!IsCommittable(selClass))
        {
            float nw = MeasureTextEx(headerFont, ResourceClassName(selClass), FS(11.0f), sp).x;
            DrawTextEx(bodyFont, "not minable", {ctrlX + nw + 8.0f, ctrlY + 1.0f},
                       FS(8.5f), sp, Fade(EXT_DIM_TEXT, 0.85f));
        }
        ctrlY += 17.0f;

        {
            bool known = selCell.HasCore(static_cast<int>(ps->selectedDepth)) ||
                         selCell.HasBeenDug(static_cast<int>(ps->selectedDepth));
            DrawTextEx(bodyFont, TextFormat("%s %s  %.0f",
                                            ResourceTypeToString(shown),
                                            known ? "assay" : "estimate",
                                            GetEstimatedYield(grid, ps->selectedCellX,
                                                              ps->selectedCellY,
                                                              ps->selectedDepth, shown)),
                       {ctrlX, ctrlY}, FS(9.0f), sp,
                       known ? EXT_TEXT : EXT_DIM_TEXT);
        }
        ctrlY += 13.0f;
        DrawTextEx(bodyFont, TextFormat("cores here  %d",
                                        static_cast<int>(selCell.sampleIds.size())),
                   {ctrlX, ctrlY}, FS(9.0f), sp, EXT_DIM_TEXT);
        ctrlY += 13.0f;

        // Class per depth. Confidence is per depth, so a spot can be Measured
        // at the surface and Unclassified below it -- which is exactly what
        // decides whether a deep dig is a plan or a gamble.
        DrawTextEx(bodyFont, "CLASS BY DEPTH", {ctrlX, ctrlY}, FS(8.0f), sp,
                   Fade(EXT_DIM_TEXT, 0.8f));
        ctrlY += 12.0f;
        for (int d = 0; d < 4; d++)
        {
            Rectangle chip = {ctrlX + d * 26.0f, ctrlY, 22.0f, 14.0f};
            if (false)
            {
                DrawRectangleRounded(chip, 0.3f, 4, Color{18, 22, 34, 255});
                DrawRectangleRoundedLinesEx(chip, 0.3f, 4, 1.0f, Color{34, 40, 58, 255});
                continue;
            }
            float c = GetDepthConfidence(grid, ps->GetTray(), ps->selectedCellX,
                                         ps->selectedCellY, static_cast<DepthLayer>(d));
            Color col = ExtClassColor(GetResourceClass(c));
            DrawRectangleRounded(chip, 0.3f, 4, Fade(col, 0.22f));
            DrawRectangleRoundedLinesEx(chip, 0.3f, 4, d == focusDepth ? 1.6f : 1.0f, col);
            const char* initial = (d == 0) ? "S" : (d == 1) ? "H" : (d == 2) ? "M" : "D";
            float iw = MeasureTextEx(bodyFont, initial, FS(8.5f), sp).x;
            DrawTextEx(bodyFont, initial, {chip.x + (22.0f - iw) / 2.0f, chip.y + 2.5f},
                       FS(8.5f), sp, col);
        }
        ctrlY += 20.0f;

        int dugLayers = 0;
        for (int d = 0; d < 4; d++) if (selCell.HasBeenDug(d)) dugLayers++;
        if (dugLayers > 0)
        {
            DrawTextEx(bodyFont, TextFormat("Excavated: %d/4 layers", dugLayers),
                       {ctrlX, ctrlY}, FS(9.0f), sp, Color{228, 164, 74, 255});
        }
    }


    // (Survey progress summary now lives in the shared bottom status bar.)
}


// ===========================================================================
// Excavation panel helpers
// ===========================================================================

// Yield heat for the excavation grid. Green = rich, slate = poor. Deliberately
// a different ramp from the sweep heat map: that one shows what the radar

// ===========================================================================
// THE SHAFT DOCK -- excavation's vertical instrument
// ===========================================================================
// Excavation's answer to the borehole strip, and deliberately not a copy of
// it. A drill hole is a NEEDLE: prospecting aims a slanted line anywhere it
// likes and records what it crossed. A working face is a VOLUME, and it has
// to connect to the surface -- which is the access rule this module has and
// prospecting does not (docs/design/excavation/excavation-design.md, Access).
//
// So this strip draws a SHAFT: wide, square-shouldered, timbered, with the
// ground it has opened standing empty above the face and the diamond rotary
// rig working at the bottom of it. Beside prospecting's ragged 30 px bore,
// it should read as a thing you could lower a machine down.
//
// Phase 1 of docs/design/excavation/rebuild-plan.md: drawing only. The face
// follows the depth the player has selected; sinking becomes an action that
// costs energy and time in phase 4.
//
// The material this rig is drawn from -- DpSteel, DpBandedSlice, DP_OUT,
// DP_*_TONES -- is Dark Plating's shared layer (style guide sections 2, 4 and
// 5), and the plate stack beside it is the shared block model. Neither belongs
// to prospecting; both are called here rather than duplicated.

// Excavation's own heat field. Same Gaussian law as the drill (style guide
// 4.5), its own hotspot: heat belongs to the machine that made it, and two
// modules can be on screen in the same frame.
static float excHeatAmt = 0.0f, excHeatBitY = 0.0f;
static float ExcHeatAt(float y)
{
    float d = y - excHeatBitY;
    return excHeatAmt * expf(-(d * d) / (2.0f * 30.0f * 30.0f));
}

// Where the machine is. Excavation needs three numbers where the auger needed
// six -- there is no thread and no cone to anchor.
struct ExcRig
{
    float cx, surfY, bitY;
};
static float excSpin = 0.0f;          // the bit's rotation accumulator

// ---- drill string geometry (diamond rotary, px) ---------------------------
// The rig is the concept sheet's diamond rotary drill: bevelled-lid motor box,
// amber collar, slotted neck, rod, the variant's lower works, and a cone of
// diamond-tipped cutters. Dark Plating section 6.5.
//
// EXC_DRILL_R is the widest thing on the string, so it still sizes the hole.
static const float EXC_DRILL_R = 15.0f;      // the bit, and so the bore
static const float EXC_ROD_TOP = 10.4f, EXC_ROD_BOT = 8.8f;
static const float EXC_NECK_R  = 12.6f, EXC_NECK_H = 17.0f;
static const float EXC_BIT_LEN = 22.0f;
// A rotary bit does not screw itself in the way an auger does, so its spin is
// not geometrically locked to advance. What replaces the auger's pitch is the
// bit's DEPTH OF CUT PER REVOLUTION: how far one turn of the teeth takes it
// down. Keeping the coupling means the teeth still visibly turn faster in soft
// ground and grind in hard, which is the read the pitch used to buy.
static const float EXC_BITE_PX = 6.5f;

// One tooth ring: an elliptical annulus at its own height and radius, with n
// teeth marching around it. SQ is how far above the rings the eye sits.
static const float EXC_SQ = 0.36f;
struct ExcRing { float r, y, len, w, pull; int n; };

// The two variants of the sheet. Tier buys the heavier rig: below T2 the
// module is running the compact, at T2 and up the heavy duty -- so an upgrade
// is a thing you SEE in the bar, not only a number in the panel.
struct ExcVariant
{
    const char* name;
    float boxW, boxH, lidH, collarW, collarH;
    bool  litVents;                 // heavy wears three lit vents, compact a slot
    ExcRing ring[2];
    float tipY, tipLen, tipW;
    int   nStack;
    struct { int kind; float h, r; } stack[5];   // 0 collar 1 twotone 2 ventbox
};                                               // 3 amber   4 hex     5 fluted
static const ExcVariant EXC_VARIANTS[2] =
{
    { "COMPACT",
      42.0f, 21.0f, 6.0f, 50.0f, 10.0f, false,
      { {13.4f, 0.0f, 8.6f, 4.2f, 0.20f, 8}, {8.6f, 7.2f, 7.2f, 3.8f, 0.36f, 6} },
      13.0f, 7.8f, 4.6f,
      2, { {0, 6.0f, 11.2f}, {1, 15.0f, 9.6f} } },
    { "HEAVY DUTY",
      50.0f, 27.0f, 7.0f, 58.0f, 11.0f, true,
      { {15.0f, 0.0f, 9.4f, 4.7f, 0.20f, 10}, {9.8f, 8.5f, 8.1f, 4.1f, 0.36f, 7} },
      15.2f, 8.8f, 5.0f,
      4, { {0, 7.0f, 12.0f}, {2, 16.0f, 13.4f}, {3, 13.0f, 15.2f},
           {4, 7.0f, 12.2f} } },
};
static int excVariantIdx = 1;
static const ExcVariant& ExcVar() { return EXC_VARIANTS[excVariantIdx]; }

static float ExcStackH()
{
    const ExcVariant& v = ExcVar();
    float h = 0.0f;
    for (int i = 0; i < v.nStack; i++) h += v.stack[i].h;
    return h;
}
// The outer envelope, so chips ride the machine and never a constant.
static float ExcRadAt(const ExcRig& r, float y)
{
    const ExcVariant& v = ExcVar();
    float bitTop = r.bitY - EXC_BIT_LEN;
    if (y >= bitTop) return EXC_DRILL_R;
    float y1 = bitTop;
    for (int i = 0; i < v.nStack; i++)
    {
        float y0 = y1 - v.stack[i].h;
        if (y >= y0) return v.stack[i].r;
        y1 = y0;
    }
    float t = std::clamp((y - r.surfY) / std::max(1.0f, y1 - r.surfY), 0.0f, 1.0f);
    return EXC_ROD_TOP + (EXC_ROD_BOT - EXC_ROD_TOP) * t;
}

// One cutter: a stubby cone, base at (bx,by), tip pulled toward the axis by

// inx. Four points, so the tip reads as a chisel rather than a needle.
static void ExcTooth(float bx, float by, float w, float len, float inx, Color fill)
{
    Vector2 a = {bx - w * 0.5f, by}, b = {bx + w * 0.5f, by};
    Vector2 c = {bx + inx + w * 0.16f, by + len * 0.86f};
    Vector2 d = {bx + inx - w * 0.10f, by + len};
    DrawTriangle(a, d, c, fill);
    DrawTriangle(a, c, b, fill);
}
static void ExcToothOutline(float bx, float by, float w, float len, float inx)
{
    Vector2 a = {bx - w * 0.5f, by}, b = {bx + w * 0.5f, by};
    Vector2 c = {bx + inx + w * 0.16f, by + len * 0.86f};
    Vector2 d = {bx + inx - w * 0.10f, by + len};
    DrawLineEx(a, d, 2.4f, DP_OUT); DrawLineEx(d, c, 2.4f, DP_OUT);
    DrawLineEx(c, b, 2.4f, DP_OUT); DrawLineEx(a, b, 2.4f, DP_OUT);
}

// The bit (Dark Plating section 6.5). Two rings of diamond-tipped cutters and
// a centre point. Each ring is an elliptical annulus; every tooth owns an
// angle on it, and rotation is the teeth marching around -- the same
// projection the auger's helicoid used, with segments instead of a sweep.
// The cone is a CONSEQUENCE: the outer ring splays (tips pull only 0.20 of
// the ring radius toward the axis), the inner converges at 0.36, the centre
// point sits on the axis. All rings share ONE painter sort -- sorting per
// ring lets a front tooth of the outer ring vanish under a back tooth of the
// inner one.
static void ExcDrawBit(const ExcRig& r)
{
    const ExcVariant& v = ExcVar();
    float bT = r.bitY - EXC_BIT_LEN;
    float heat = ExcHeatAt(r.bitY);

    // the dark body the teeth are set in, so they read against something
    Vector2 o0 = {r.cx - EXC_DRILL_R - 1.6f, bT - 1.6f};
    Vector2 o1 = {r.cx + EXC_DRILL_R + 1.6f, bT - 1.6f};
    Vector2 o2 = {r.cx + EXC_DRILL_R * 0.22f, r.bitY + 1.2f};
    Vector2 o3 = {r.cx - EXC_DRILL_R * 0.22f, r.bitY + 1.2f};
    DrawTriangle(o0, o3, o2, DP_OUT); DrawTriangle(o0, o2, o1, DP_OUT);
    Color body = DpSteel(0.16f, heat);
    Vector2 b0 = {r.cx - EXC_DRILL_R + 1.2f, bT};
    Vector2 b1 = {r.cx + EXC_DRILL_R - 1.2f, bT};
    Vector2 b2 = {r.cx + EXC_DRILL_R * 0.18f, r.bitY - 1.6f};
    Vector2 b3 = {r.cx - EXC_DRILL_R * 0.18f, r.bitY - 1.6f};
    DrawTriangle(b0, b3, b2, body); DrawTriangle(b0, b2, b1, body);

    struct Tooth { float a, f; int ring, idx; };
    std::vector<Tooth> teeth;
    for (int k = 0; k < 2; k++)
    {
        const ExcRing& rg = v.ring[k];
        float step = 2.0f * PI / static_cast<float>(rg.n);
        for (int i = 0; i < rg.n; i++)
        {
            float a = excSpin + k * step * 0.5f + i * step;
            teeth.push_back({a, -cosf(a), k, i});
        }
    }
    std::sort(teeth.begin(), teeth.end(),
              [](const Tooth& p, const Tooth& q) { return p.f < q.f; });

    for (const Tooth& t : teeth)
    {
        const ExcRing& rg = v.ring[t.ring];
        bool front = t.f > 0.0f;
        float bx = r.cx + rg.r * sinf(t.a);
        float by = (bT + rg.y) - EXC_SQ * rg.r * cosf(t.a);
        float sc = front ? 1.0f : 0.78f;
        float inx = -sinf(t.a) * rg.r * rg.pull * sc;
        float lt = 0.5f - 0.5f * sinf(t.a);              // light from upper-left
        // Back teeth are REMAPPED brighter, not merely darkened: a plain dim
        // sinks them into the body they stand on.
        float shade = front ? 0.50f + 0.32f * lt + 0.12f * t.f
                            : 0.15f + (0.48f + 0.30f * lt) * 0.44f;
        if (front) ExcToothOutline(bx, by, rg.w * sc, rg.len * sc, inx);
        ExcTooth(bx, by, rg.w * sc, rg.len * sc, inx,
                  DpSteel(shade, front ? heat : heat * 0.6f));
        if (front)
        {
            DrawLineEx({bx - rg.w * 0.30f, by + 1.0f},
                       {bx + inx - rg.w * 0.06f, by + rg.len * 0.82f},
                       1.0f, DpSteel(0.82f + 0.18f * lt, heat * 0.4f));
            // one diamond speck per tooth, seeded so the grit rides its tooth
            unsigned gs = (2654435761u * static_cast<unsigned>(t.ring * 31 + t.idx)) % 97u;
            DrawRectangleRec({bx - 1.4f + (gs % 3u), by + 2.0f + (gs % 4u), 1.7f, 1.7f},
                             DpSteel(0.94f, heat * 0.3f));
        }
    }
    // the centre point: angle-free, always front, painted last
    ExcToothOutline(r.cx, bT + v.tipY, v.tipW, v.tipLen, 0.0f);
    ExcTooth(r.cx, bT + v.tipY, v.tipW, v.tipLen, 0.0f, DpSteel(0.56f, heat));
    DrawLineEx({r.cx - v.tipW * 0.22f, bT + v.tipY + 1.4f},
               {r.cx - v.tipW * 0.06f, bT + v.tipY + v.tipLen * 0.8f},
               1.0f, DpSteel(0.92f, heat * 0.4f));
}



// variant is a list and never a branch.
static void ExcDrawStackSeg(const ExcRig& r, int kind, float y0, float y1, float rad)
{
    float h = ExcHeatAt((y0 + y1) * 0.5f), hh = (y1 - y0);
    if (kind == 0 || kind == 4)                       // step collar / hex joint
    {
        DrawRectangleRec({r.cx - rad - 2.0f, y0 - 1.5f, (rad + 2.0f) * 2.0f, hh + 3.0f}, DP_OUT);
        for (float y = y0; y < y1; y += 1.4f)
            DpBandedSlice(r.cx, y, rad, 1.8f, DP_JOINT_TONES, ExcHeatAt(y));
        DrawRectangleRec({r.cx - rad, y0, rad * 2.0f, 1.6f}, Fade(WHITE, 0.34f));
        DrawRectangleRec({r.cx - rad, y1 - 2.0f, rad * 2.0f, 2.0f}, Fade(BLACK, 0.45f));
        if (kind == 4)                                // side facets say hexagonal
        {
            DrawTriangle({r.cx - rad, y0}, {r.cx - rad, y1}, {r.cx - rad + 4.0f, (y0 + y1) * 0.5f},
                         Fade(BLACK, 0.35f));
            DrawTriangle({r.cx + rad, y0}, {r.cx + rad - 4.0f, (y0 + y1) * 0.5f}, {r.cx + rad, y1},
                         Fade(BLACK, 0.35f));
        }
    }
    else if (kind == 1)                               // the compact's lower barrel
    {
        DrawRectangleRec({r.cx - rad - 2.0f, y0 - 1.5f, (rad + 2.0f) * 2.0f, hh + 3.0f}, DP_OUT);
        for (float y = y0; y < y1; y += 1.4f)
            DpBandedSlice(r.cx, y, rad, 1.8f, DP_CHUCK_TONES, ExcHeatAt(y));
        DrawRectangleRec({r.cx - rad + 3.0f, y0 + 3.0f, 1.8f, hh - 6.0f}, Fade(BLACK, 0.30f));
        DrawRectangleRec({r.cx + rad - 5.0f, y0 + 3.0f, 1.8f, hh - 6.0f}, Fade(BLACK, 0.30f));
        DrawRectangleRec({r.cx - rad, (y0 + y1) * 0.5f - 0.8f, rad * 2.0f, 1.6f}, Fade(BLACK, 0.38f));
    }
    else if (kind == 2)                               // the vented grey housing
    {
        DrawRectangleRec({r.cx - rad - 2.0f, y0 - 2.0f, (rad + 2.0f) * 2.0f, hh + 4.0f}, DP_OUT);
        DrawRectangleRec({r.cx - rad, y0, rad * 2.0f, hh}, DpSteel(0.34f, h * 0.5f));
        DrawRectangleRec({r.cx - rad, y0, rad * 2.0f, 2.6f}, Fade(WHITE, 0.30f));
        DrawRectangleRec({r.cx - rad, y1 - 2.6f, rad * 2.0f, 2.6f}, Fade(BLACK, 0.30f));
        DrawRectangleRec({r.cx - rad * 0.52f - 1.6f, y0 + 4.0f, 3.2f, 6.0f}, Fade(BLACK, 0.5f));
        DrawRectangleRec({r.cx + rad * 0.52f - 1.6f, y0 + 4.0f, 3.2f, 6.0f}, Fade(BLACK, 0.5f));
    }
    else if (kind == 3)                               // the amber stabiliser
    {
        DrawRectangleRec({r.cx - rad - 2.0f, y0 - 2.0f, (rad + 2.0f) * 2.0f, hh + 4.0f}, DP_OUT);
        DrawRectangleRec({r.cx - rad, y0, rad * 2.0f, hh}, {217, 150, 47, 255});
        DrawRectangleRec({r.cx - rad, y0, rad * 2.0f, 2.6f}, Fade(WHITE, 0.30f));
        DrawRectangleRec({r.cx - rad, y1 - 2.6f, rad * 2.0f, 2.6f}, Fade(BLACK, 0.30f));
        DrawRectangleRec({r.cx - rad * 0.42f - 1.8f, y0 + 3.0f, 3.6f, hh - 6.0f}, Fade(BLACK, 0.46f));
        DrawRectangleRec({r.cx + rad * 0.42f - 1.8f, y0 + 3.0f, 3.6f, hh - 6.0f}, Fade(BLACK, 0.46f));
        DrawRectangleRec({r.cx - rad + 2.5f, y0 + 2.0f, 2.4f, 2.4f}, {244, 198, 106, 255});
        DrawRectangleRec({r.cx + rad - 4.9f, y0 + 2.0f, 2.4f, 2.4f}, {244, 198, 106, 255});
        if (h > 0.02f)
            DrawRectangleRec({r.cx - rad, y0, rad * 2.0f, hh},
                             Fade(Color{255, 110, 30, 255}, std::min(0.5f, h * 0.5f)));
    }
    else                                              // the bundled-column section
    {
        DrawRectangleRec({r.cx - rad - 2.0f, y0 - 1.5f, (rad + 2.0f) * 2.0f, hh + 3.0f}, DP_OUT);
        DrawRectangleRec({r.cx - rad, y0, rad * 2.0f, hh}, DpSteel(0.10f, h * 0.4f));
        float cw = rad * 0.30f;
        for (int k = -1; k <= 1; k++)
            for (float y = y0 + 4.0f; y < y1 - 4.0f; y += 1.4f)
                DpBandedSlice(r.cx + k * rad * 0.60f, y, cw, 1.8f, DP_ROD_TONES,
                                ExcHeatAt(y));
        for (float y = y0; y < y0 + 4.0f; y += 1.4f)
            DpBandedSlice(r.cx, y, rad, 1.8f, DP_JOINT_TONES, ExcHeatAt(y));
        for (float y = y1 - 4.0f; y < y1; y += 1.4f)
            DpBandedSlice(r.cx, y, rad, 1.8f, DP_JOINT_TONES, ExcHeatAt(y));
        DrawRectangleRec({r.cx - rad, y0, rad * 2.0f, 1.3f}, Fade(WHITE, 0.30f));
        DrawRectangleRec({r.cx - rad, y1 - 1.6f, rad * 2.0f, 1.6f}, Fade(BLACK, 0.40f));
    }
}

// The rig below the collar: slotted neck, rod with its joints, the variant's

// fixed offsets above the lower works -- what is bolted together stays
// bolted together as the hole deepens.
static void ExcDrawString(const ExcRig& r, float topY)
{
    const ExcVariant& v = ExcVar();
    float bitTop = r.bitY - EXC_BIT_LEN;
    float stackTop = bitTop - ExcStackH();

    // rod: silhouette then banded body, in thin slices so the taper stays
    // banded. It runs from under the collar to the lower works.
    float rodTopY = std::max(topY, r.surfY - 6.0f);
    if (stackTop > rodTopY)
    {
        for (float y = rodTopY; y < stackTop; y += 1.4f)
        {
            float w = ExcRadAt(r, std::max(y, r.surfY + 0.1f)) + 1.8f;
            DrawRectangleRec({r.cx - w, y, w * 2.0f, 2.1f}, DP_OUT);
        }
        for (float y = rodTopY; y < stackTop - 1.0f; y += 1.4f)
        {
            float w = ExcRadAt(r, std::max(y, r.surfY + 0.1f));
            if (w < 0.7f) continue;
            DpBandedSlice(r.cx, y, w, 1.8f, DP_ROD_TONES, ExcHeatAt(y));
        }
        for (int k = 1; k <= 3; k++)
        {
            float jy = stackTop - k * 78.0f;
            if (jy < r.surfY + 10.0f) break;
            ProsDrawJoint(r.cx, jy, ExcRadAt(r, jy) + 0.8f, false);
        }
    }

    // the lower works, bottom-up from the bit
    float y1 = bitTop;
    for (int i = 0; i < v.nStack; i++)
    {
        float y0 = y1 - v.stack[i].h;
        if (y1 > r.surfY + 2.0f) ExcDrawStackSeg(r, v.stack[i].kind, y0, y1, v.stack[i].r);
        y1 = y0;
    }
    ExcDrawBit(r);
}

// The top works, following the concept sheet: a bevelled-lid motor box on the
// sled, its mouth slot and state lamp, then the amber collar -- one dark mouth
// slot on the compact, three lit vents on the heavy. Anchored just above the
// surface and clamped to the clip, so it can NEVER be scissored away by a

static void ExcDrawHead(float cx, float surfY, float clipTop,
                         bool turning, bool cooling)
{
    const ExcVariant& v = ExcVar();
    auto box = [](float x, float y, float w, float h, Color fill, bool bev)
    {
        DrawRectangleRec({x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f}, DP_OUT);
        DrawRectangleRec({x, y, w, h}, fill);
        if (bev)
        {
            DrawRectangleRec({x, y, w, 2.6f}, Fade(WHITE, 0.30f));
            DrawRectangleRec({x, y, 2.6f, h}, Fade(WHITE, 0.14f));
            DrawRectangleRec({x, y + h - 2.6f, w, 2.6f}, Fade(BLACK, 0.30f));
            DrawRectangleRec({x + w - 2.6f, y, 2.6f, h}, Fade(BLACK, 0.22f));
        }
    };
    float neckY = std::max(clipTop + v.boxH + v.lidH + v.collarH + 8.0f,
                           surfY - 9.0f - EXC_NECK_H);
    float collarY = neckY - v.collarH;
    float boxY = collarY - v.boxH, lidY = boxY - v.lidH;

    // base sled: the rig stands ON the ground rather than hovering over it
    DrawRectangleRec({cx - v.boxW * 0.5f - 12.0f, surfY - 11.0f, v.boxW + 24.0f, 11.0f}, DP_OUT);
    DrawRectangleRec({cx - v.boxW * 0.5f - 10.0f, surfY - 10.0f, v.boxW + 20.0f, 9.0f}, {28, 37, 48, 255});
    DrawRectangleRec({cx - v.boxW * 0.5f - 10.0f, surfY - 10.0f, v.boxW + 20.0f, 2.2f}, Fade(WHITE, 0.18f));

    // the bevelled lid: a trapezoid, so the box reads as a cast housing
    Vector2 l0 = {cx - v.boxW * 0.34f - 2.0f, lidY - 2.0f};
    Vector2 l1 = {cx + v.boxW * 0.34f + 2.0f, lidY - 2.0f};
    Vector2 l2 = {cx + v.boxW * 0.5f + 4.0f, boxY + 1.0f};
    Vector2 l3 = {cx - v.boxW * 0.5f - 4.0f, boxY + 1.0f};
    DrawTriangle(l0, l3, l2, DP_OUT); DrawTriangle(l0, l2, l1, DP_OUT);
    Vector2 m0 = {cx - v.boxW * 0.34f, lidY}, m1 = {cx + v.boxW * 0.34f, lidY};
    Vector2 m2 = {cx + v.boxW * 0.5f + 2.0f, boxY}, m3 = {cx - v.boxW * 0.5f - 2.0f, boxY};
    Color lid = DpSteel(0.62f);
    DrawTriangle(m0, m3, m2, lid); DrawTriangle(m0, m2, m1, lid);
    DrawRectangleRec({cx - v.boxW * 0.34f, lidY, v.boxW * 0.68f, 2.2f}, Fade(WHITE, 0.30f));

    box(cx - v.boxW * 0.5f, boxY, v.boxW, v.boxH, {57, 66, 78, 255}, true);
    DrawRectangleRec({cx - v.boxW * 0.5f + 6.0f, boxY + 5.0f, v.boxW - 12.0f, 1.4f}, Fade(BLACK, 0.35f));
    DrawRectangleRec({cx - 0.7f, boxY + 5.0f, 1.4f, v.boxH - 10.0f}, Fade(BLACK, 0.35f));
    DrawRectangleRec({cx - 7.0f, boxY + v.boxH - 8.0f, 14.0f, 4.6f}, {20, 26, 33, 255});
    DrawRectangleRec({cx - 7.0f, boxY + v.boxH - 8.0f, 14.0f, 1.2f}, Fade(WHITE, 0.14f));
    // the state lamp keeps its semantics: hot cooling, amber turning, cyan idle
    DrawRectangleRec({cx + v.boxW * 0.5f - 10.2f, boxY + v.boxH - 9.2f, 7.4f, 7.4f}, DP_OUT);
    DrawRectangleRec({cx + v.boxW * 0.5f - 9.0f, boxY + v.boxH - 8.0f, 5.0f, 5.0f},
                     cooling ? Color{255, 90, 40, 255}
                             : turning ? Color{255, 200, 77, 255}
                                       : Color{80, 225, 255, 255});

    // the amber collar -- the machine's signature colour (section 1.2)
    box(cx - v.collarW * 0.5f, collarY, v.collarW, v.collarH, {217, 150, 47, 255}, true);
    if (v.litVents)
    {
        for (int k = -1; k <= 1; k++)
        {
            DrawRectangleRec({cx + k * v.collarW * 0.16f - 2.4f, collarY + 2.5f, 4.8f, v.collarH - 5.0f},
                             {122, 81, 21, 255});
            DrawRectangleRec({cx + k * v.collarW * 0.16f - 1.2f, collarY + 3.5f, 2.4f, v.collarH - 7.0f},
                             {244, 198, 106, 255});
        }
    }
    else
    {
        DrawRectangleRec({cx - v.collarW * 0.30f, collarY + v.collarH * 0.5f - 1.8f,
                          v.collarW * 0.60f, 3.6f}, {122, 81, 21, 255});
        DrawRectangleRec({cx - v.collarW * 0.30f, collarY + v.collarH * 0.5f + 0.8f,
                          v.collarW * 0.60f, 1.0f}, Fade(BLACK, 0.4f));
    }
    DrawRectangleRec({cx - v.collarW * 0.5f - 2.5f, collarY + 2.0f, 2.6f, 2.6f}, {244, 198, 106, 255});
    DrawRectangleRec({cx + v.collarW * 0.5f - 0.1f, collarY + 2.0f, 2.6f, 2.6f}, {244, 198, 106, 255});

    // the slotted box the string runs down through, standing on the sled
    DrawRectangleRec({cx - EXC_NECK_R - 2.5f, neckY - 2.5f,
                      (EXC_NECK_R + 2.5f) * 2.0f, EXC_NECK_H + 5.0f}, DP_OUT);
    for (float y = neckY; y < neckY + EXC_NECK_H; y += 1.4f)
        DpBandedSlice(cx, y, EXC_NECK_R, 1.8f, DP_CHUCK_TONES, ExcHeatAt(y));
    DrawRectangleRec({cx - EXC_NECK_R, neckY, EXC_NECK_R * 2.0f, 2.2f}, Fade(WHITE, 0.24f));
    DrawRectangleRec({cx - EXC_NECK_R, neckY + EXC_NECK_H - 2.6f,
                      EXC_NECK_R * 2.0f, 2.6f}, Fade(BLACK, 0.40f));
    int ns = v.litVents ? 3 : 4;
    for (int i = 0; i < ns; i++)
    {
        float t = (i + 0.5f) / ns;
        float sx = cx - EXC_NECK_R + t * EXC_NECK_R * 2.0f - 1.3f;
        DrawRectangleRec({sx, neckY + 3.0f, 2.6f, EXC_NECK_H - 7.0f}, Fade(BLACK, 0.5f));
        if (v.litVents)
            DrawRectangleRec({sx + 0.4f, neckY + 4.0f, 1.8f, EXC_NECK_H - 9.0f},
                             {244, 198, 106, 255});
    }
}

// ---- the shaft itself ------------------------------------------------------
// Sized so it reads as a volume rather than a bore: the rig is 30 px across
// and the shaft is 64, in a strip 16 px wider than prospecting's -- a hole you
// sink a machine down, not one you push a needle through. The extra width is
// spent on ROCK, because the strata either side are what make a depth legible.
static const float EXC_SHAFT_HALF = 32.0f;
static const float EXC_SET_SPACING = 36.0f;   // timber sets down the walls

static void ExcDrawShaft(const DockGeom& dg, float surfY, float faceY)
{
    float x0 = dg.cx - EXC_SHAFT_HALF, x1 = dg.cx + EXC_SHAFT_HALF;
    float h = faceY - surfY;
    if (h < 1.0f) return;

    // the void
    DrawRectangleRec({x0, surfY, x1 - x0, h}, {11, 9, 7, 255});

    // Dressed walls, not ragged ones. A borehole's wall is broken rock and is
    // drawn as jitter; a shaft's is cut to a line and held there, so it gets a
    // straight face with a lit inner edge. That difference is most of what
    // makes this read as built rather than drilled.
    for (int s = -1; s <= 1; s += 2)
    {
        float wx = (s < 0) ? x0 : x1 - 7.0f;
        DrawRectangleRec({wx, surfY, 7.0f, h}, {31, 27, 22, 255});
        DrawRectangleRec({(s < 0) ? x0 + 6.0f : x1 - 7.0f, surfY, 1.4f, h},
                         Fade(Color{198, 214, 232, 255}, 0.13f));
    }
    // the far wall darkens with depth, which is what gives the void a floor
    for (int i = 0; i < 5; i++)
    {
        float t0 = i / 5.0f;
        DrawRectangleRec({x0 + 7.0f, surfY + h * t0, (x1 - x0) - 14.0f, h / 5.0f + 1.0f},
                         Fade(BLACK, 0.10f + 0.14f * t0));
    }

    // Sets: the timbering that holds a shaft open. Spaced by a fixed pixel
    // pitch rather than by depth, because they are structure, not scale.
    for (float y = surfY + EXC_SET_SPACING; y < faceY - 6.0f; y += EXC_SET_SPACING)
    {
        DrawRectangleRec({x0 + 5.0f, y, (x1 - x0) - 10.0f, 4.6f}, {24, 31, 40, 255});
        DrawRectangleRec({x0 + 5.0f, y, (x1 - x0) - 10.0f, 1.3f}, Fade(WHITE, 0.16f));
        DrawRectangleRec({x0 + 5.0f, y + 3.6f, (x1 - x0) - 10.0f, 1.0f}, Fade(BLACK, 0.45f));
    }

    // the face: the floor the rig is standing on and cutting into
    DrawRectangleRec({x0 + 7.0f, faceY - 3.0f, (x1 - x0) - 14.0f, 3.0f}, {46, 40, 32, 255});
    DrawRectangleRec({x0 + 7.0f, faceY - 3.0f, (x1 - x0) - 14.0f, 1.2f}, Fade(WHITE, 0.10f));
}

// ---- the dock's depth axis -------------------------------------------------
// Phase 1 builds it standalone; phase 3 swaps this for DockFromBlock once the
// block model arrives. The bands are spaced EVENLY on purpose -- that is what
// the plate stack will hand over, so the swap changes no pixels. YOf then
// interpolates within a band by true metre fraction, exactly as it does for
// prospecting, which is what keeps a depth in one panel level with the same
// depth in the other.
static DockGeom ExcDockEven(float x, float w, float top, float bottom)
{
    DockGeom d;
    d.x = x; d.w = w; d.cx = x + w * 0.5f;
    float span = (bottom - top) / 4.0f;
    for (int L = 0; L < 5; L++) d.bandTop[L] = top + span * L;
    return d;
}

// ---- the whole strip -------------------------------------------------------
static void ExcDrawShaftDock(ExcavationSystem* es, const DockGeom& dg,
                             float clipTop, float clipBot,
                             const Font& bodyFont, float sp, float fsSmall,
                             const Texture2D* strata)
{
    float surfY = dg.bandTop[0];
    int dIdx = std::clamp(static_cast<int>(es->selectedDepth), 0, 3);

    // The face sits at the CENTRE of the worked layer, matching prospecting's
    // PlateTargetM: aiming at a layer means working inside it, never on the
    // boundary, where the depth would belong to the layer above.
    float faceM = LAYER_CENTRE_M[dIdx];
    float faceY = dg.YOf(faceM);

    // Tier buys the heavier rig, so an upgrade is a thing you SEE in the bar.
    excVariantIdx = es->GetTier() >= 2 ? 1 : 0;

    // Drive the machine from the work it actually did last tick.
    const DigResult& lr = es->GetLastResult();
    float pace = std::max(0.0f, lr.effectivePace);
    bool working = pace > 0.001f;
    bool throttled = lr.throttledByPower;
    float dt = GetFrameTime();

    excSpin -= (working ? (2.0f + 7.0f * pace) : 0.0f) * dt;
    // Heat is integrated on the dig tick, not here: it is a consequence of
    // work, and the renderer only gets two frames in a headless preview.
    excHeatAmt = es->bitHeat;
    excHeatBitY = faceY;

    BeginScissorMode(static_cast<int>(dg.x * gPixelScale),
                     static_cast<int>(clipTop * gPixelScale),
                     static_cast<int>(dg.w * gPixelScale),
                     static_cast<int>((clipBot - clipTop) * gPixelScale));

    // sky
    DrawRectangleRec({dg.x, clipTop, dg.w, surfY - clipTop}, {10, 16, 24, 255});
    dpGrain = 3;
    for (int i = 0; i < 8; i++)
        DrawRectangleRec({dg.x + DpRnd() * dg.w,
                          clipTop + DpRnd() * std::max(1.0f, surfY - clipTop - 8.0f),
                          1.4f, 1.4f}, Fade(Color{200, 220, 240, 255}, 0.5f));

    // The rock: the same four generated textures the plates wear, so the band
    // here and the plate beside it are one rock rather than two palettes.
    for (int L = 0; L < 4; L++)
    {
        float y0 = dg.bandTop[L], y1 = dg.bandTop[L + 1];
        if (strata != nullptr && strata[L].id != 0)
        {
            Color tint = { static_cast<unsigned char>(std::min(255, DP_ROCK_COL[L].r * 2)),
                           static_cast<unsigned char>(std::min(255, DP_ROCK_COL[L].g * 2)),
                           static_cast<unsigned char>(std::min(255, DP_ROCK_COL[L].b * 2)),
                           255 };
            float k = static_cast<float>(RockTexture::SIZE) / DP_ROCK_TEX_PX;
            float off = static_cast<float>(L) * 41.0f;
            DrawTexturePro(strata[L], {0.0f, off, dg.w * k, (y1 - y0) * k},
                           {dg.x, y0, dg.w, y1 - y0}, {0.0f, 0.0f}, 0.0f, tint);
        }
        else
        {
            DrawRectangleRec({dg.x, y0, dg.w, y1 - y0}, DP_ROCK_COL[L]);
        }
        DrawRectangleRec({dg.x, y0, dg.w, 2.0f}, DP_ROCK_EDGE[L]);
    }
    DrawRectangleRec({dg.x, surfY - 1.8f, dg.w, 2.0f}, {74, 85, 96, 255});

    // depth figures down the right edge, as the borehole strip has them
    for (int L = 0; L < 4; L++)
    {
        const char* t = TextFormat("%d", static_cast<int>(LayerTopM(L)));
        float tw = MeasureTextEx(bodyFont, t, fsSmall, sp).x;
        DrawTextEx(bodyFont, t, {dg.x + dg.w - tw - 4.0f, dg.bandTop[L] + 3.0f},
                   fsSmall, sp, Fade(Color{198, 214, 232, 255}, 0.40f));
    }

    ExcDrawShaft(dg, surfY, faceY);

    // spoil at the collar: what the shaft took out has to be somewhere
    DrawEllipse(static_cast<int>(dg.cx - EXC_SHAFT_HALF - 12.0f),
                static_cast<int>(surfY - 1.0f), 15.0f, 6.0f, {70, 62, 49, 255});
    DrawEllipse(static_cast<int>(dg.cx + EXC_SHAFT_HALF + 12.0f),
                static_cast<int>(surfY - 1.0f), 15.0f, 6.0f, {70, 62, 49, 255});

    ExcRig rig;
    rig.cx = dg.cx; rig.surfY = surfY; rig.bitY = faceY;
    ExcDrawString(rig, clipTop + 4.0f);
    ExcDrawHead(dg.cx, surfY, clipTop, working, throttled);

    // the glow at the face -- the one overlay this style allows (3.3)
    if (excHeatAmt > 0.02f)
    {
        DrawCircleGradient({dg.cx, faceY}, 46.0f,
                           Fade(Color{255, 110, 30, 255}, 0.34f * excHeatAmt),
                           Fade(Color{255, 110, 30, 255}, 0.0f));
    }

    EndScissorMode();

    // The border: solid on three sides, DASHED on the edge that faces the
    // model, which is the cut mark the rock passes under (style guide 9.1).
    DrawLineEx({dg.x, clipTop}, {dg.x, clipBot}, 1.0f, DP_OUT);
    DrawLineEx({dg.x + dg.w, clipTop}, {dg.x + dg.w, clipBot}, 1.0f, DP_OUT);
    DrawLineEx({dg.x, clipBot}, {dg.x + dg.w, clipBot}, 1.0f, DP_OUT);
    DpDashed({dg.x, clipTop}, {dg.x + dg.w, clipTop}, 5.0f, 4.0f, 0.0f, 1.0f,
               Fade(Color{140, 165, 190, 255}, 0.45f));

    DrawTextEx(bodyFont, "SHAFT", {dg.x + 5.0f, clipBot - 13.0f}, fsSmall, sp,
               Fade(Color{198, 214, 232, 255}, 0.45f));
}



// A horizontal slider. Returns true while being dragged, and writes through
// `value`. IMGUI-style: drawn and hit-tested in one pass.
static bool ExcSlider(Rectangle track, float& value, float minValue, float maxValue,
                      Vector2 mouse, Color accent, bool enabled)
{
    float span = maxValue - minValue;
    if (span <= 0.0f) span = 1.0f;

    float t = (value - minValue) / span;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

    bool hover = CheckCollisionPointRec(mouse, {track.x - 6.0f, track.y - 9.0f,
                                                track.width + 12.0f, track.height + 18.0f});
    bool dragging = enabled && hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (dragging)
    {
        t = (mouse.x - track.x) / track.width;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        value = minValue + t * span;
    }

    Color line = enabled ? Fade(accent, 0.30f) : Fade(EXT_DIM_TEXT, 0.25f);
    Color fill = enabled ? accent : EXT_DIM_TEXT;

    DrawRectangleRounded(track, 1.0f, 4, line);
    DrawRectangleRounded({track.x, track.y, track.width * t, track.height}, 1.0f, 4,
                         Fade(fill, 0.85f));

    // Knob: bigger while held, so a touch press is visibly acknowledged.
    float knobR = dragging ? 7.0f : (hover && enabled ? 6.0f : 5.0f);
    Vector2 knob = {track.x + track.width * t, track.y + track.height * 0.5f};
    DrawCircleV(knob, knobR, enabled ? fill : EXT_DIM_TEXT);
    DrawCircleV(knob, knobR * 0.45f, EXT_PANEL_BG);

    return dragging;
}

void RenderManager::DrawExcavationPanel(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;
    int padding = EXT_GAP + 14;
    float px = static_cast<float>(x + padding);
    float pw = static_cast<float>(w - padding * 2);
    Vector2 mouse = ColonyGetMousePosition();

    if (!unit->HasExcavationSystem() || !unit->HasProspectingSystem())
    {
        DrawTextEx(headerFont, "No excavation system.", {px, static_cast<float>(y + padding)},
                   FS(14.0f), sp, EXT_DIM_TEXT);
        return;
    }

    ExcavationSystem* es = unit->GetExcavationSystem();
    ProspectingSystem* ps = unit->GetProspectingSystem();

    // Make the displayed state coherent before drawing any of it: a target this
    // ground actually holds, and the machine AUTO would really pick. Without
    // this the panel shows the constructor's defaults until the first dig tick.
    es->SyncToGround(*ps);
    const ProspectingGrid& grid = ps->GetGrid();
    const SiteView& site = es->GetSite();
    const EstimateEngine& estimator = es->GetEstimator();
    const DigSite& worked = es->GetWorked();

    // --- Header ---
    float yPos = static_cast<float>(y + padding);
    ExtDrawIcon(ExtIcon::EXCAVATOR, px + 10.0f, yPos + 10.0f, 10.0f, EXT_ACCENT_CYAN);
    DrawTextEx(headerFont, "EXCAVATION", {px + 28.0f, yPos + 1.0f}, FS(15.0f), sp, EXT_TEXT);

    // Machine name, right-aligned, so the active tool is readable at a glance
    // even when the bay is scrolled out of the eye's path.
    const Machine& active = es->GetActiveMachine();
    const char* machineLabel = TextFormat("%s%s", es->autoMachine ? "AUTO  " : "", active.displayName);
    float mlW = MeasureTextEx(bodyFont, machineLabel, FS(10.0f), sp).x;
    DrawTextEx(bodyFont, machineLabel, {px + pw - mlW, yPos + 4.0f}, FS(10.0f), sp,
               es->autoMachine ? EXT_DIM_TEXT : EXT_ACCENT_CYAN);

    yPos += 30.0f;
    float contentY = yPos;
    float contentH = static_cast<float>(y + h - padding) - yPos - 34.0f;   // leave the readout strip

    int gridSize = grid.GetGridSize();

    // =======================================================================
    // Left: the ground, as the block model
    // =======================================================================
    // The same instrument prospecting reads its survey off, asked a different
    // question: the plates are shaded by the estimated yield of the resource
    // being TARGETED, not the cell's richest element. Same stack, same lift
    // law, same four rocks -- so a depth here is the same depth there.
    //
    // Two overlays prospecting has no reason to draw: excavation's OWN reach
    // ring (its tier, never prospecting's -- hauling distance is not
    // instrument range, and that asymmetry is where the module's gamble
    // lives), and the ground already worked out.
    if (!strataLoaded) LoadStrataTextures();

    float dockW = 120.0f;                 // wider than the borehole strip: a shaft
    float modelW = pw * 0.60f - dockW;
    float modelH = contentH - 30.0f;
    float gridX = px;
    float gridY = contentY;

    BlockModelGeom geom = MakeBlockGeom(gridSize, gridX, gridY, modelW, modelH);
    DockGeom dock = DockFromBlock(geom, gridX + modelW + 6.0f, dockW);
    {
        // The rig's head needs sky, for the same reason the auger's does.
        float sky = dock.bandTop[0] - contentY;
        if (sky < 64.0f)
        {
            float push = 64.0f - sky;
            geom = MakeBlockGeom(gridSize, gridX, gridY + push, modelW, modelH - push);
            dock = DockFromBlock(geom, gridX + modelW + 6.0f, dockW);
        }
    }

    // ONE estimate field for the whole stack. The per-cell scalar path is
    // O(N^4) and cost 55 ms/frame at 16x16 -- at 32 it would be 16x worse.
    // One grade scale across all four plates, so a barren layer stays visibly
    // flatter than the ore instead of being normalised up to match it.
    std::vector<std::vector<BlockCell>> layers(4);
    float maxGrade = 0.0001f;
    EstimateField field = BuildEstimateField(grid, es->targetResource);
    for (int L = 0; L < 4; L++)
    {
        layers[L].resize(gridSize * gridSize);
        for (int gy = 0; gy < gridSize; gy++)
        {
            for (int gx = 0; gx < gridSize; gx++)
            {
                BlockCell& c = layers[L][gy * gridSize + gx];
                c.grade = field.GradeAt(gx, gy, L);
                c.cls = GetResourceClass(field.ConfidenceAt(gx, gy, L));
                maxGrade = std::max(maxGrade, c.grade);
            }
        }
    }
    for (int L = 0; L < 4; L++)
    {
        float top = 0.0f;
        for (int j = 0; j <= gridSize; j++)
            for (int i = 0; i <= gridSize; i++)
                top = std::max(top, BlockCornerLift(layers[L], gridSize, maxGrade,
                                                   geom.relief, i, j));
        geom.plateDrop[L] = top;
    }

    auto LiftAt = [&](int L, int i, int j)
    {
        return BlockCellLift(layers[L], gridSize, maxGrade, geom.relief, i, j);
    };

    // Pick BEFORE drawing: plate brightness depends on the hover, and reading
    // it afterwards lights the wrong layer for a frame.
    BlockPickGeom pick;
    pick.originX = geom.originX; pick.originY = geom.originY;
    pick.tileX = geom.tileX;     pick.tileY = geom.tileY;
    pick.gap = geom.gap;         pick.size = gridSize;
    int hovL = -1, hovX = -1, hovY = -1;
    for (int L = 0; L < 4 && hovL < 0; L++)
    {
        int pi = 0, pj = 0;
        if (!BlockPickCell(pick, L, mouse.x, mouse.y - geom.plateDrop[L],
                           [&](int i, int j) { return LiftAt(L, i, j); }, pi, pj)) continue;
        hovL = L; hovX = pi; hovY = pj;
    }
    if (hovL < 0 && es->previewHoverLayer >= 0)
    {
        hovL = std::clamp(es->previewHoverLayer, 0, 3);
        hovX = gridSize / 2; hovY = gridSize / 2;
    }

    int workedDepth = std::clamp(static_cast<int>(es->selectedDepth), 0, 3);
    es->UpdatePlateLight(hovL, workedDepth, GetFrameTime());

    static const char* excDepthLabels[4] = {"0 m", "12 m", "34 m", "68 m"};
    static const char* excLevelLabels[4] = {"SURFACE", "SHALLOW", "MID", "DEEP"};

    // The plate being worked rim-lights, on the same 12.6 rad/s clock the
    // other instruments pulse on -- an unsynced phase reads as two panels.
    // The SAME clock prospecting pulses on. Wall-clock here meant the two
    // block models drifted apart, and a headless preview could not be diffed
    // between builds because every run drew a different phase.
    float rimPulse = 0.45f + 0.25f * sinf(ps->gameTime * 12.6f);
    for (int L = 0; L < 4; L++)
    {
        const Texture2D* tile = (strataLoaded && strataTex[L].id != 0)
                              ? &strataTex[L] : nullptr;
        bool canWork = site.CanWorkDepth(static_cast<DepthLayer>(L));
        // A depth this tier cannot work is still DRAWN, just held back: the
        // player should see the ground waiting for them, not a gap.
        float light = es->plateLight[L] * (canWork ? 1.0f : 0.42f);
        DrawBlockLayer(geom, layers[L], L, maxGrade, light, bodyFont, sp,
                           excDepthLabels[L],
                           canWork ? excLevelLabels[L]
                                   : TextFormat("%s  LOCKED", excLevelLabels[L]),
                           nullptr, nullptr, tile,
                           L == workedDepth ? rimPulse : 0.0f);
    }

    // ---- excavation's reach ---------------------------------------------
    // A centred square that excavation's OWN tier sizes. Drawn as a polyline
    // along the boundary so it rides the lifted surface instead of cutting
    // through it, the same way the active-plate rim does.
    //
    // DASHED, because the active-plate rim is already a solid amber square on
    // the worked plate and two solid amber squares on one plate read as one
    // shape with a mistake in it. A dash says "limit" where a solid line says
    // "this one" -- same colour, different grammar.
    {
        int reach = GetReachForTier(es->GetTier());
        int r0 = (gridSize - reach) / 2, r1 = r0 + reach;
        for (int L = 0; L < 4; L++)
        {
            Color ring = Fade(Color{228, 164, 74, 255},
                              L == workedDepth ? 0.55f : 0.16f);
            auto Edge = [&](int ax, int ay, int bx, int by)
            {
                int steps = std::max(std::abs(bx - ax), std::abs(by - ay));
                Vector2 prev = {0.0f, 0.0f};
                for (int k = 0; k <= steps; k++)
                {
                    float t = static_cast<float>(k) / std::max(1, steps);
                    float fx = ax + (bx - ax) * t, fy = ay + (by - ay) * t;
                    float lift = BlockCornerLift(layers[L], gridSize, maxGrade, geom.relief,
                                                static_cast<int>(fx), static_cast<int>(fy));
                    Vector2 q = geom.Iso(fx, fy, L, lift);
                    if (k > 0 && (k % 2) == 1) DrawLineEx(prev, q, 1.2f, ring);
                    prev = q;
                }
            };
            Edge(r0, r0, r1, r0); Edge(r1, r0, r1, r1);
            Edge(r1, r1, r0, r1); Edge(r0, r1, r0, r0);
        }
    }

    // ---- the spot being worked, and the one under the pointer -------------
    auto CellOutline = [&](int L, int i, int j, Color col, float th)
    {
        float lift = LiftAt(L, i, j);
        Vector2 a = geom.Iso(static_cast<float>(i),     static_cast<float>(j),     L, lift);
        Vector2 b = geom.Iso(static_cast<float>(i + 1), static_cast<float>(j),     L, lift);
        Vector2 c = geom.Iso(static_cast<float>(i + 1), static_cast<float>(j + 1), L, lift);
        Vector2 d = geom.Iso(static_cast<float>(i),     static_cast<float>(j + 1), L, lift);
        DrawLineEx(a, b, th, col); DrawLineEx(b, c, th, col);
        DrawLineEx(c, d, th, col); DrawLineEx(d, a, th, col);
    };
    {
        int sx = std::clamp(es->selectedSpotX, 0, gridSize - 1);
        int sy = std::clamp(es->selectedSpotY, 0, gridSize - 1);
        CellOutline(workedDepth, sx, sy, EXT_ACCENT_CYAN, 1.8f);
        Vector2 dot = geom.Iso(sx + 0.5f, sy + 0.5f, workedDepth,
                               LiftAt(workedDepth, sx, sy));
        DrawCircleV(dot, 3.6f, Fade(DP_OUT, 0.85f));
        DrawCircleV(dot, 2.2f, EXT_ACCENT_CYAN);
    }
    int lockedHoverX = -1, lockedHoverY = -1;
    if (hovL >= 0)
    {
        bool inReach = site.IsInReach(hovX, hovY);
        bool ok = inReach && site.CanWorkDepth(static_cast<DepthLayer>(hovL));
        // The tooltip at the foot of the panel names the tier that would
        // reach here, so out-of-range ground explains itself rather than
        // just refusing the click.
        if (!inReach) { lockedHoverX = hovX; lockedHoverY = hovY; }
        CellOutline(hovL, hovX, hovY, Fade(ok ? PROS_HOVER_BORDER
                                              : Color{255, 90, 40, 255}, 0.9f), 1.2f);
        // Clicking a plate picks BOTH the spot and the depth. The plate is the
        // depth -- which is what let the separate depth row go.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ok)
        {
            es->selectedSpotX = hovX;
            es->selectedSpotY = hovY;
            es->selectedDepth = static_cast<DepthLayer>(hovL);
        }
    }

    // ---- legend ----------------------------------------------------------
    {
        const char* targetLabel = ResourceTypeToString(es->targetResource);
        float legendY = gridY + modelH + 2.0f;
        // Right group first, then the left text is given whatever is left --
        // at this width a fixed split collided the two.
        const char* reachTxt = TextFormat("REACH %d", site.GetReach());
        float reachW = MeasureTextEx(bodyFont, reachTxt, FS(8.0f), sp).x;
        float rx = gridX + modelW - reachW;
        DrawTextEx(bodyFont, reachTxt, {rx, legendY}, FS(8.0f), sp,
                   Fade(Color{228, 164, 74, 255}, 0.85f));
        DrawTextEx(bodyFont, TextFormat("relief = est. %s left", targetLabel),
                   {gridX, legendY}, FS(8.5f), sp, EXT_DIM_TEXT);
    }

    // =======================================================================
    // Centre: the shaft
    // =======================================================================
    ExcDrawShaftDock(es, dock, contentY, dock.bandTop[4] + 18.0f,
                     bodyFont, sp, FS(7.5f), strataLoaded ? strataTex : nullptr);

    // =======================================================================
    // Right: the controls
    // =======================================================================
    float ctrlX = dock.x + dock.w + 15.0f;
    float ctrlW = px + pw - ctrlX;
    float cy = contentY;

    // --- Target ---
    DrawTextEx(bodyFont, "TARGET", {ctrlX, cy}, FS(9.0f), sp, EXT_DIM_TEXT);
    cy += 14.0f;

    // The resources this ground actually holds, so the row is never a list of
    // things that are not there.
    std::vector<ResourceType> targets;
    for (const auto& [type, fraction] : grid.GetGroundTruth(es->selectedSpotX,
                                                            es->selectedSpotY,
                                                            es->selectedDepth))
    {
        if (fraction > 0.02f) targets.push_back(type);
    }
    if (targets.empty()) targets.push_back(es->targetResource);

    float tbW = std::min(58.0f, (ctrlW - 6.0f) / std::max<size_t>(1, targets.size()) - 4.0f);
    for (size_t i = 0; i < targets.size(); i++)
    {
        Rectangle tb = {ctrlX + i * (tbW + 4.0f), cy, tbW, 20.0f};
        bool isSelected = (targets[i] == es->targetResource);
        bool hover = CheckCollisionPointRec(mouse, tb);

        DrawRectangleRounded(tb, 0.3f, 4,
                             isSelected ? Fade(EXT_ACCENT_CYAN, 0.16f) : EXT_PANEL_BG2);
        if (isSelected)
        {
            DrawRectangleRoundedLinesEx(tb, 0.3f, 4, 1.0f, Fade(EXT_ACCENT_CYAN, 0.8f));
        }

        const char* name = ResourceTypeToString(targets[i]);
        float nw = MeasureTextEx(bodyFont, name, FS(9.0f), sp).x;
        DrawTextEx(bodyFont, name, {tb.x + (tb.width - nw) * 0.5f, tb.y + 5.0f},
                   FS(9.0f), sp, isSelected ? EXT_ACCENT_CYAN : EXT_DIM_TEXT);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            es->targetResource = targets[i];
        }
    }
    cy += 30.0f;

    // --- Pace ---
    DrawTextEx(bodyFont, "PACE", {ctrlX, cy}, FS(9.0f), sp, EXT_DIM_TEXT);
    const char* paceValue = TextFormat("%.2f / %.2f", es->pace, active.paceCeiling);
    float pvW = MeasureTextEx(bodyFont, paceValue, FS(9.0f), sp).x;
    DrawTextEx(bodyFont, paceValue, {ctrlX + ctrlW - pvW, cy}, FS(9.0f), sp, EXT_TEXT);
    cy += 14.0f;
    ExcSlider({ctrlX, cy, ctrlW, 6.0f}, es->pace, 0.0f, active.paceCeiling,
              mouse, EXT_ACCENT_CYAN, true);
    cy += 12.0f;
    DrawTextEx(bodyFont, "harder digs more, and dirtier", {ctrlX, cy}, FS(8.0f), sp,
               Fade(EXT_DIM_TEXT, 0.7f));
    cy += 20.0f;

    // --- Power cap ---
    DrawTextEx(bodyFont, "POWER CAP", {ctrlX, cy}, FS(9.0f), sp, EXT_DIM_TEXT);
    const char* capValue = es->powerCap <= 0.0f ? "uncapped"
                                                : TextFormat("%.1f kW", es->powerCap);
    float cvW = MeasureTextEx(bodyFont, capValue, FS(9.0f), sp).x;
    DrawTextEx(bodyFont, capValue, {ctrlX + ctrlW - cvW, cy}, FS(9.0f), sp,
               es->powerCap > 0.0f ? EXT_ACCENT_GOLD : EXT_TEXT);
    cy += 14.0f;
    ExcSlider({ctrlX, cy, ctrlW, 6.0f}, es->powerCap, 0.0f, 40.0f,
              mouse, EXT_ACCENT_GOLD, true);
    cy += 22.0f;

    // --- Automation ---
    DrawTextEx(bodyFont, "AUTOMATION", {ctrlX, cy}, FS(9.0f), sp, EXT_HEADER_COLOR);
    cy += 14.0f;

    AiLevel maxLevel = es->MaxAiLevel();
    float aiW = (ctrlW - 9.0f) / 4.0f;
    for (int i = 0; i < 4; i++)
    {
        AiLevel level = static_cast<AiLevel>(i);
        Rectangle ab = {ctrlX + i * (aiW + 3.0f), cy, aiW, 19.0f};

        // OFF is always available -- taking control back is never gated.
        bool available = (level == AiLevel::OFF) || (i <= static_cast<int>(maxLevel));
        bool isSelected = (es->aiLevel == level);
        bool hover = CheckCollisionPointRec(mouse, ab);

        Color fill = isSelected ? Fade(EXT_ACCENT_GREEN, 0.16f) : EXT_PANEL_BG2;
        if (!available) fill = Fade(EXT_PANEL_BG2, 0.4f);
        DrawRectangleRounded(ab, 0.3f, 4, fill);
        if (isSelected)
        {
            DrawRectangleRoundedLinesEx(ab, 0.3f, 4, 1.0f, Fade(EXT_ACCENT_GREEN, 0.85f));
        }

        const char* name = AutoPilot::LevelName(level);
        float nw = MeasureTextEx(bodyFont, name, FS(7.5f), sp).x;
        Color textColor = !available ? Fade(EXT_DIM_TEXT, 0.4f)
                                     : (isSelected ? EXT_ACCENT_GREEN : EXT_DIM_TEXT);
        DrawTextEx(bodyFont, name, {ab.x + (ab.width - nw) * 0.5f, ab.y + 5.0f},
                   FS(7.5f), sp, textColor);

        if (hover && available && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            es->aiLevel = level;
        }
    }
    cy += 22.0f;

    // What the current level does, and what handing over costs.
    DrawTextEx(bodyFont, AutoPilot::LevelDescription(es->aiLevel), {ctrlX, cy},
               FS(8.0f), sp, Fade(EXT_DIM_TEXT, 0.8f));
    cy += 11.0f;

    float aiEff = AutoPilot::EfficiencyFor(es->aiLevel);
    if (aiEff < 1.0f)
    {
        DrawTextEx(bodyFont, TextFormat("costs %.0f%% output vs deciding yourself",
                                        (1.0f - aiEff) * 100.0f),
                   {ctrlX, cy}, FS(8.0f), sp, Fade(EXT_ACCENT_GOLD, 0.8f));
    }
    cy += 18.0f;

    // --- Machine bay ---
    DrawTextEx(bodyFont, "MACHINE BAY", {ctrlX, cy}, FS(9.0f), sp, EXT_HEADER_COLOR);

    Rectangle autoChip = {ctrlX + ctrlW - 52.0f, cy - 4.0f, 52.0f, 17.0f};
    bool autoHover = CheckCollisionPointRec(mouse, autoChip);
    DrawRectangleRounded(autoChip, 0.4f, 4,
                         es->autoMachine ? Fade(EXT_ACCENT_GREEN, 0.18f) : EXT_PANEL_BG2);
    DrawRectangleRoundedLinesEx(autoChip, 0.4f, 4, 1.0f,
                                es->autoMachine ? Fade(EXT_ACCENT_GREEN, 0.8f)
                                                : Fade(EXT_DIM_TEXT, 0.5f));
    const char* autoLabel = es->autoMachine ? "AUTO ON" : "AUTO OFF";
    float alW = MeasureTextEx(bodyFont, autoLabel, FS(7.5f), sp).x;
    DrawTextEx(bodyFont, autoLabel,
               {autoChip.x + (autoChip.width - alW) * 0.5f, autoChip.y + 4.0f}, FS(7.5f), sp,
               es->autoMachine ? EXT_ACCENT_GREEN : EXT_DIM_TEXT);
    if (autoHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        es->autoMachine = !es->autoMachine;
    }
    cy += 20.0f;

    // Two columns -- six machines in one column overflows the card.
    float cardW = (ctrlW - 6.0f) * 0.5f;
    for (int i = 0; i < EXC_MACHINE_TABLE_SIZE; i++)
    {
        MachineId id = static_cast<MachineId>(i);
        const Machine& m = DigEngine::GetMachine(id);

        int col = i % 2;
        int row = i / 2;
        Rectangle card = {ctrlX + col * (cardW + 6.0f), cy + row * 34.0f, cardW, 30.0f};

        bool available = es->IsMachineAvailable(id);
        bool isActive = (es->activeMachine == id);
        bool hover = CheckCollisionPointRec(mouse, card);
        bool pressed = hover && available && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        Color fill = EXT_PANEL_BG2;
        if (!available) fill = Fade(EXT_PANEL_BG2, 0.4f);
        else if (pressed) fill = Fade(EXT_ACCENT_CYAN, 0.28f);
        else if (isActive) fill = Fade(EXT_ACCENT_CYAN, 0.16f);

        DrawRectangleRounded(card, 0.25f, 4, fill);
        if (isActive)
        {
            DrawRectangleRoundedLinesEx(card, 0.25f, 4, 1.0f, Fade(EXT_ACCENT_CYAN, 0.85f));
        }

        Color nameColor = !available ? Fade(EXT_DIM_TEXT, 0.4f)
                                     : (isActive ? EXT_ACCENT_CYAN : EXT_TEXT);
        DrawTextEx(bodyFont, m.displayName, {card.x + 6.0f, card.y + 3.0f},
                   FS(8.5f), sp, nameColor);

        // The two stats that decide the choice: how tightly it digs, and how
        // hard it can be pushed.
        DrawTextEx(bodyFont, TextFormat("aim %.0f%%   pace %.1f", m.precision * 100.0f, m.paceCeiling),
                   {card.x + 6.0f, card.y + 16.0f}, FS(7.0f), sp,
                   available ? Fade(EXT_DIM_TEXT, 0.85f) : Fade(EXT_DIM_TEXT, 0.35f));

        if (!available)
        {
            // Name the tier that unlocks it, not a bare number -- "T2" alone
            // reads as a rating rather than a requirement.
            const char* lock = TextFormat("TIER %d", m.requiredTier);
            float lw = MeasureTextEx(bodyFont, lock, FS(7.0f), sp).x;
            DrawTextEx(bodyFont, lock, {card.x + card.width - lw - 5.0f, card.y + 4.0f},
                       FS(7.0f), sp, Fade(EXT_ACCENT_GOLD, 0.65f));
        }

        if (hover && available && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            es->activeMachine = id;
            es->autoMachine = false;   // picking a machine means taking control
        }
    }

    // =======================================================================
    // The readout -- what you thought was there, against what is arriving.
    // This one line is the module.
    // =======================================================================
    float readY = static_cast<float>(y + h - padding) - 26.0f;
    DrawLineEx({px, readY - 8.0f}, {px + pw, readY - 8.0f}, 1.0f, Fade(EXT_PANEL_BORDER, 0.6f));

    SpotEstimate sel = es->EstimateSelected(*ps);
    const DigResult& last = es->GetLastResult();
    const char* targetName = ResourceTypeToString(es->targetResource);

    const char* expectation = sel.isCertain
        ? TextFormat("SPOT %d,%d   %s  %.0f  (known)", es->selectedSpotX, es->selectedSpotY,
                     targetName, sel.shown)
        : TextFormat("SPOT %d,%d   %s  %.0f  (%.0f-%.0f, %s)", es->selectedSpotX, es->selectedSpotY,
                     targetName, sel.shown, sel.low, sel.high,
                     ProsConfLabel(sel.confidence));

    DrawTextEx(bodyFont, expectation, {px, readY}, FS(10.0f), sp,
               sel.isCertain ? EXT_TEXT : EXT_DIM_TEXT);

    // Right side: what actually came up last tick, and the share of it that
    // was the thing being aimed at.
    if (last.totalMass > 0.0f)
    {
        float share = last.targetMass / last.totalMass;

        // Per-DAY rate, not the per-frame mass. A frame moves a fraction of a
        // unit, so the raw figure rounded to 0.0 and the panel looked broken
        // while the module was working perfectly well.
        float frame = GetFrameTime();
        float perDay = frame > 0.0f ? (TICKS_PER_DAY / frame) : 0.0f;

        const char* got = TextFormat("GETTING  %.0f %s/day  of  %.0f moved   (%.0f%% useful)",
                                     last.targetMass * perDay, targetName,
                                     last.totalMass * perDay, share * 100.0f);
        float gw = MeasureTextEx(bodyFont, got, FS(10.0f), sp).x;
        Color gotColor = share > 0.6f ? EXT_ACCENT_GREEN
                                      : (share > 0.3f ? EXT_TEXT : EXT_ACCENT_GOLD);
        DrawTextEx(bodyFont, got, {px + pw - gw, readY}, FS(10.0f), sp, gotColor);
    }
    else
    {
        const char* idle = last.throttledByPower ? "THROTTLED BY POWER CAP" : "IDLE";
        float iw = MeasureTextEx(bodyFont, idle, FS(10.0f), sp).x;
        DrawTextEx(bodyFont, idle, {px + pw - iw, readY}, FS(10.0f), sp,
                   last.throttledByPower ? EXT_ACCENT_GOLD : EXT_DIM_TEXT);
    }

    // Expert's survey hint, under the readout. This is the level's entire
    // contribution: it cannot survey, but it can say where looking would pay.
    const AutoDecision& decision = es->lastDecision;
    if (decision.surveyHintX >= 0)
    {
        const char* hint = TextFormat("SURVEY %d,%d  -  could be %.0f more %s than it reads",
                                      decision.surveyHintX, decision.surveyHintY,
                                      decision.surveyGain, targetName);
        DrawTextEx(bodyFont, hint, {px, readY + 12.0f}, FS(8.5f), sp,
                   Fade(EXT_ACCENT_VIOLET, 0.9f));
    }

    // Out-of-range tooltip last, so it sits above the grid.
    if (lockedHoverX >= 0)
    {
        int needTier = TierRequiredForSubCell(lockedHoverX, lockedHoverY);
        const char* line1 = "OUT OF REACH";
        const char* line2 = needTier >= 0
            ? TextFormat("Excavation tier %d reaches here", needTier)
            : "Unreachable";

        float tw = std::max(MeasureTextEx(headerFont, line1, FS(10.0f), sp).x,
                            MeasureTextEx(bodyFont, line2, FS(9.0f), sp).x) + 20.0f;
        float th = 38.0f;
        Rectangle tip = {mouse.x + 12.0f, mouse.y - th - 6.0f, tw, th};
        if (tip.x + tip.width > px + pw) tip.x = px + pw - tip.width;

        DrawRectangleRounded(tip, 0.2f, 4, {8, 12, 24, 240});
        DrawRectangleRoundedLinesEx(tip, 0.2f, 4, 1.0f, Fade(EXT_ACCENT_GOLD, 0.7f));
        DrawTextEx(headerFont, line1, {tip.x + 10.0f, tip.y + 7.0f}, FS(10.0f), sp, EXT_ACCENT_GOLD);
        DrawTextEx(bodyFont, line2, {tip.x + 10.0f, tip.y + 21.0f}, FS(9.0f), sp, EXT_DIM_TEXT);
    }
}

void RenderManager::DrawBeneficiationPanel(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;
    int padding = 15;

    float yPos = static_cast<float>(y + padding);
    float px = static_cast<float>(x + padding);
    Vector2 mousePos = ColonyGetMousePosition();

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
    Vector2 mousePos = ColonyGetMousePosition();

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
