#include "rendermanager.h"
#include "resource_manager.h"
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

// Color constants for extraction UI (reuse site selection aesthetic)
static const Color EXT_PANEL_BG      = {20, 20, 40, 220};
static const Color EXT_PANEL_BORDER  = {100, 100, 200, 200};
static const Color EXT_SCREEN_BG     = {10, 10, 25, 255};
static const Color EXT_HEADER_COLOR  = {150, 150, 255, 255};
static const Color EXT_ACCENT_CYAN   = {100, 220, 255, 255};
static const Color EXT_ACCENT_GREEN  = {100, 255, 150, 255};
static const Color EXT_ACCENT_GOLD   = {255, 220, 100, 255};
static const Color EXT_DIM_TEXT      = {140, 140, 160, 255};

// Layout constants
static const int EXT_TOP_BAR_H    = 50;
static const int EXT_BOTTOM_BAR_H = 40;
static const int EXT_LEFT_PANEL_W  = 280;
static const int EXT_RIGHT_PANEL_W = 300;

float RenderManager::FS(float baseSize)
{
    return baseSize * 1.30f;
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

    // Unit title
    DrawTextEx(headerFont, "EXTRACTION UNIT", {20.0f, 14.0f}, FS(22.0f), sp, WHITE);

    // Status indicator
    bool isActive = unit->IsActive();
    Color statusColor = isActive ? EXT_ACCENT_GREEN : Color{255, 100, 100, 255};
    const char* statusText = isActive ? "ONLINE" : "OFFLINE";
    float statusX = 220.0f;
    DrawCircle(static_cast<int>(statusX), 25, 5, statusColor);
    DrawTextEx(bodyFont, statusText, {statusX + 12.0f, 16.0f}, FS(16.0f), sp, statusColor);

    // Day counter
    const char* dayText = TextFormat("Day %d", timeManager.GetCurrentDay());
    float dayWidth = MeasureTextEx(bodyFont, dayText, FS(16.0f), sp).x;
    DrawTextEx(bodyFont, dayText, {screenWidth - dayWidth - 20.0f, 16.0f}, FS(16.0f), sp, WHITE);

    // Navigation hint
    DrawTextEx(bodyFont, "Press S for Sect View",
               {screenWidth - dayWidth - 200.0f, 16.0f}, FS(14.0f), sp, EXT_DIM_TEXT);
}

void RenderManager::DrawExtractionBottomBar(Unit* unit)
{
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;

    int startY = screenHeight - EXT_BOTTOM_BAR_H;
    DrawRectangle(0, startY, screenWidth, EXT_BOTTOM_BAR_H, EXT_PANEL_BG);
    DrawLine(0, startY, screenWidth, startY, EXT_PANEL_BORDER);

    // Fade message
    const UIMessage& msg = unit->GetCurrentMessage();
    if (msg.opacity > 0)
    {
        Color msgColor = {255, 200, 50, static_cast<unsigned char>(255 * msg.opacity)};
        DrawTextEx(bodyFont, msg.text.c_str(), {20.0f, startY + 8.0f}, FS(18.0f), sp, msgColor);
    }
}

// --- Shared Helpers ---

void RenderManager::DrawStyledBar(float x, float y, float w, float h, float value, Color fillColor)
{
    value = Clamp(value, 0.0f, 1.0f);
    DrawRectangle(static_cast<int>(x), static_cast<int>(y),
                  static_cast<int>(w), static_cast<int>(h), {40, 40, 60, 200});
    DrawRectangle(static_cast<int>(x), static_cast<int>(y),
                  static_cast<int>(w * value), static_cast<int>(h), fillColor);
    DrawRectangleLines(static_cast<int>(x), static_cast<int>(y),
                       static_cast<int>(w), static_cast<int>(h), EXT_PANEL_BORDER);
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
    Color tierColors[] = {GRAY, GREEN, BLUE, EXT_ACCENT_GOLD};
    for (int i = 0; i <= maxTier; i++)
    {
        float cx = x + i * 18.0f;
        float cy = y;
        if (i <= tier)
        {
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 6, tierColors[std::min(i, 3)]);
        }
        else
        {
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 6, EXT_DIM_TEXT);
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

    // Panel background
    DrawRectangle(0, panelY, EXT_LEFT_PANEL_W, panelH, EXT_PANEL_BG);
    DrawLine(EXT_LEFT_PANEL_W, panelY, EXT_LEFT_PANEL_W, panelY + panelH, EXT_PANEL_BORDER);

    int padding = 10;
    float yPos = static_cast<float>(panelY + padding);

    // "Unit Overview" button
    Rectangle overviewBtn = {
        static_cast<float>(padding),
        yPos,
        static_cast<float>(EXT_LEFT_PANEL_W - padding * 2),
        40.0f
    };

    bool overviewHovered = CheckCollisionPointRec(GetMousePosition(), overviewBtn);
    bool overviewSelected = !unit->IsInModuleView();

    Color overviewBg = overviewSelected ? Color{40, 60, 100, 255} : Color{30, 30, 50, 255};
    if (overviewHovered) overviewBg = Color{50, 70, 120, 255};

    DrawRectangleRec(overviewBtn, overviewBg);
    if (overviewSelected)
        DrawRectangleLinesEx(overviewBtn, 2.0f, EXT_ACCENT_CYAN);
    else
        DrawRectangleLinesEx(overviewBtn, 1.0f, EXT_PANEL_BORDER);

    DrawTextEx(headerFont, "UNIT OVERVIEW", {overviewBtn.x + 10.0f, overviewBtn.y + 10.0f},
               FS(16.0f), sp, overviewSelected ? WHITE : LIGHTGRAY);

    if (overviewHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        unit->SetIsInModuleView(false);
        unit->SetShowingStats(false);
        unit->PublicShowMessage("Switched to unit overview");
    }

    yPos += 55.0f;

    // Section label
    DrawTextEx(bodyFont, "MODULES", {static_cast<float>(padding), yPos}, FS(12.0f), sp, EXT_DIM_TEXT);
    yPos += 20.0f;

    // Module buttons
    const auto& modules = unit->GetModules();
    int selectedIdx = unit->GetSelectedModuleIndex();

    for (size_t i = 0; i < modules.size(); i++)
    {
        const auto& mod = modules[i];

        Rectangle btn = {
            static_cast<float>(padding),
            yPos,
            static_cast<float>(EXT_LEFT_PANEL_W - padding * 2),
            50.0f
        };

        bool isHovered = CheckCollisionPointRec(GetMousePosition(), btn);
        bool isSelected = unit->IsInModuleView() && selectedIdx == static_cast<int>(i);

        // Background color based on state
        Color btnBg;
        if (!mod.isBuilt)
            btnBg = {25, 25, 35, 255};
        else if (isSelected)
            btnBg = {40, 60, 100, 255};
        else
            btnBg = {30, 30, 50, 255};

        if (isHovered) btnBg = Color{static_cast<unsigned char>(std::min(btnBg.r + 15, 255)),
                                      static_cast<unsigned char>(std::min(btnBg.g + 15, 255)),
                                      static_cast<unsigned char>(std::min(btnBg.b + 20, 255)),
                                      255};

        DrawRectangleRec(btn, btnBg);

        // Border
        if (isSelected)
            DrawRectangleLinesEx(btn, 2.0f, EXT_ACCENT_CYAN);
        else if (mod.isBuilt && mod.isActive)
            DrawRectangleLinesEx(btn, 1.0f, EXT_ACCENT_GREEN);
        else if (mod.isBuilt)
            DrawRectangleLinesEx(btn, 1.0f, EXT_DIM_TEXT);
        else
            DrawRectangleLinesEx(btn, 1.0f, Color{60, 60, 80, 150});

        // Module name
        Color nameColor = mod.isBuilt ? WHITE : EXT_DIM_TEXT;
        DrawTextEx(bodyFont, mod.name.c_str(), {btn.x + 10.0f, btn.y + 6.0f}, FS(15.0f), sp, nameColor);

        // Tier indicator
        DrawTierIndicator(btn.x + 10.0f, btn.y + 32.0f, mod.tier);

        // Status text
        const char* statusText = !mod.isBuilt ? "NOT BUILT" : (mod.isActive ? "ACTIVE" : "INACTIVE");
        Color statusColor = !mod.isBuilt ? EXT_DIM_TEXT : (mod.isActive ? EXT_ACCENT_GREEN : YELLOW);
        float statusWidth = MeasureTextEx(bodyFont, statusText, FS(11.0f), sp).x;
        DrawTextEx(bodyFont, statusText,
                   {btn.x + btn.width - statusWidth - 10.0f, btn.y + 32.0f}, FS(11.0f), sp, statusColor);

        // Click handling
        if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            unit->SetSelectedModuleIndex(static_cast<int>(i));
            unit->SetIsInModuleView(true);
            unit->SetShowingStats(false);
            unit->PublicShowMessage("Viewing " + mod.name);
        }

        yPos += 55.0f;
    }
}

// --- Center Panel Router ---

void RenderManager::DrawExtractionModuleCenter(Unit* unit)
{
    int panelX = EXT_LEFT_PANEL_W;
    int panelY = EXT_TOP_BAR_H;
    int panelW = screenWidth - EXT_LEFT_PANEL_W - EXT_RIGHT_PANEL_W;
    int panelH = screenHeight - EXT_TOP_BAR_H - EXT_BOTTOM_BAR_H;

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
    int padding = 12;

    // Panel background
    DrawRectangle(panelX, panelY, EXT_RIGHT_PANEL_W, panelH, EXT_PANEL_BG);
    DrawLine(panelX, panelY, panelX, panelY + panelH, EXT_PANEL_BORDER);

    float yPos = static_cast<float>(panelY + padding);

    if (!unit->IsInModuleView())
    {
        // Unit overview mode - production rate controls
        DrawTextEx(headerFont, "PRODUCTION CONTROLS", {static_cast<float>(panelX + padding), yPos},
                   FS(16.0f), sp, EXT_HEADER_COLOR);
        yPos += 30.0f;

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

    DrawTextEx(headerFont, "CONTROLS", {static_cast<float>(panelX + padding), yPos},
               FS(16.0f), sp, EXT_HEADER_COLOR);
    yPos += 30.0f;

    float btnW = static_cast<float>(EXT_RIGHT_PANEL_W - padding * 2);
    float btnH = 40.0f;

    // Build button (if not built)
    if (!mod.isBuilt)
    {
        Rectangle buildBtn = {static_cast<float>(panelX + padding), yPos, btnW, btnH};
        bool canBuild = unit->PublicCanBuildModule(idx);
        bool isHovered = CheckCollisionPointRec(GetMousePosition(), buildBtn);

        Color btnColor = canBuild ? Color{40, 80, 180, 255} : Color{50, 50, 70, 255};
        if (isHovered && canBuild) btnColor = Color{60, 100, 220, 255};

        DrawRectangleRec(buildBtn, btnColor);
        DrawRectangleLinesEx(buildBtn, 1.0f, canBuild ? EXT_ACCENT_CYAN : EXT_DIM_TEXT);

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

        Color btnColor = canUpgrade ? Color{40, 80, 180, 255} : Color{50, 50, 70, 255};
        if (isHovered && canUpgrade) btnColor = Color{60, 100, 220, 255};

        DrawRectangleRec(upgradeBtn, btnColor);
        DrawRectangleLinesEx(upgradeBtn, 1.0f, canUpgrade ? EXT_ACCENT_CYAN : EXT_DIM_TEXT);

        const char* upgradeText = TextFormat("UPGRADE TO TIER %d", mod.tier + 1);
        float textW = MeasureTextEx(headerFont, upgradeText, FS(14.0f), sp).x;
        DrawTextEx(headerFont, upgradeText,
                   {upgradeBtn.x + (btnW - textW) / 2.0f, upgradeBtn.y + 12.0f}, FS(14.0f), sp,
                   canUpgrade ? WHITE : EXT_DIM_TEXT);

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
        DrawTextEx(bodyFont, "MAX TIER REACHED", {static_cast<float>(panelX + padding), yPos},
                   FS(14.0f), sp, EXT_ACCENT_GOLD);
        yPos += 25.0f;
    }

    yPos += 15.0f;

    // Activate/Deactivate toggle
    Rectangle toggleBtn = {static_cast<float>(panelX + padding), yPos, btnW, btnH};
    bool isHovered = CheckCollisionPointRec(GetMousePosition(), toggleBtn);

    Color toggleColor = mod.isActive ? Color{150, 40, 40, 255} : Color{40, 120, 60, 255};
    if (isHovered) toggleColor = mod.isActive ? Color{180, 60, 60, 255} : Color{60, 150, 80, 255};

    DrawRectangleRec(toggleBtn, toggleColor);
    DrawRectangleLinesEx(toggleBtn, 1.0f, mod.isActive ? Color{255, 100, 100, 200} : EXT_ACCENT_GREEN);

    const char* toggleText = mod.isActive ? "DEACTIVATE" : "ACTIVATE";
    float textW = MeasureTextEx(headerFont, toggleText, FS(16.0f), sp).x;
    DrawTextEx(headerFont, toggleText,
               {toggleBtn.x + (btnW - textW) / 2.0f, toggleBtn.y + 12.0f}, FS(16.0f), sp, WHITE);

    if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        unit->PublicHandleModuleActivation(idx);
    }

    yPos += btnH + 20.0f;

    // Module info section
    DrawTextEx(headerFont, "MODULE INFO", {static_cast<float>(panelX + padding), yPos},
               FS(14.0f), sp, EXT_HEADER_COLOR);
    yPos += 22.0f;

    DrawTextEx(bodyFont, TextFormat("Tier: %d / 3", mod.tier),
               {static_cast<float>(panelX + padding), yPos}, FS(13.0f), sp, LIGHTGRAY);
    yPos += 18.0f;

    DrawTextEx(bodyFont, TextFormat("Efficiency: %.0f%%", mod.efficiency * 100.0f),
               {static_cast<float>(panelX + padding), yPos}, FS(13.0f), sp, LIGHTGRAY);
    yPos += 18.0f;

    if (mod.energyRequired > 0)
    {
        DrawTextEx(bodyFont, TextFormat("Energy: %.1f kW", mod.energyRequired),
                   {static_cast<float>(panelX + padding), yPos}, FS(13.0f), sp, LIGHTGRAY);
        yPos += 18.0f;
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
    float barW = static_cast<float>(w - padding * 2 - 140);

    bool hasStorage = false;
    for (const auto& res : resources)
    {
        float stored = 0.0f;
        float cap = 0.0f;
        auto sIt = storage.find(res.type);
        auto cIt = capacity.find(res.type);
        if (sIt != storage.end()) stored = sIt->second;
        if (cIt != capacity.end()) cap = cIt->second;

        if (cap <= 0 && stored <= 0) continue;
        hasStorage = true;

        DrawTextEx(bodyFont, res.name, {px, yPos + 2.0f}, FS(12.0f), sp, LIGHTGRAY);

        float fillFraction = cap > 0 ? stored / cap : 0.0f;
        Color barColor;
        if (fillFraction > 0.9f) barColor = Color{255, 100, 100, 255};
        else if (fillFraction > 0.7f) barColor = YELLOW;
        else barColor = EXT_ACCENT_CYAN;

        DrawStyledBar(px + 90.0f, yPos, barW, 16.0f, fillFraction, barColor);
        DrawTextEx(bodyFont, TextFormat("%.0f/%.0f", stored, cap),
                   {px + 90.0f + barW + 5.0f, yPos + 1.0f}, FS(11.0f), sp, LIGHTGRAY);

        yPos += 22.0f;
    }

    if (!hasStorage)
    {
        DrawTextEx(bodyFont, "Storage is empty", {px, yPos}, FS(13.0f), sp, EXT_DIM_TEXT);
    }
}

void RenderManager::DrawProspectingPanel(Unit* unit, int x, int y, int w, int h)
{
    const Font& headerFont = fontsLoaded ? uiHeaderFont : GetFontDefault();
    const Font& bodyFont = fontsLoaded ? uiFont : GetFontDefault();
    float sp = 1.0f;
    int padding = 15;

    float yPos = static_cast<float>(y + padding);
    float px = static_cast<float>(x + padding);
    float panelW = static_cast<float>(w - padding * 2);

    const char* scanTitles[] = {"VISUAL ESTIMATION", "LIBS SCANNER", "MULTI-SPECTRAL SUITE", "DEEP SURVEY ARRAY"};
    int idx = unit->GetSelectedModuleIndex();
    const auto& mod = unit->GetModules()[idx];
    int tier = std::min(mod.tier, 3);

    DrawTextEx(headerFont, scanTitles[tier], {px, yPos}, FS(18.0f), sp, EXT_HEADER_COLOR);
    yPos += 28.0f;

    // Accuracy based on tier
    const char* accLabels[] = {"Categories only", "+/-15% noise", "+/-5% noise", "Exact readings"};
    DrawTextEx(bodyFont, TextFormat("Accuracy: %s", accLabels[tier]), {px, yPos}, FS(14.0f), sp, EXT_ACCENT_CYAN);
    yPos += 20.0f;

    // Geological confidence
    float confidence = unit->GetGeologicalConfidence();
    Color confColor = confidence >= 0.8f ? EXT_ACCENT_GREEN :
                      confidence >= 0.4f ? YELLOW : RED;
    DrawTextEx(bodyFont, TextFormat("Geological Confidence: %.0f%%", confidence * 100.0f),
               {px, yPos}, FS(13.0f), sp, confColor);
    yPos += 18.0f;

    // --- CALIBRATION gauge (T1+) ---
    if (tier >= 1)
    {
        float calQ = unit->GetCalibrationQuality();
        Color calColor = calQ >= 0.9f ? EXT_ACCENT_GREEN :
                         calQ >= 0.7f ? YELLOW : RED;
        DrawTextEx(bodyFont, "Calibration:", {px, yPos}, FS(12.0f), sp, LIGHTGRAY);

        float barX = px + 90.0f;
        float barW = 100.0f;
        float barH = 10.0f;
        DrawRectangle(static_cast<int>(barX), static_cast<int>(yPos + 2.0f),
                      static_cast<int>(barW), static_cast<int>(barH), {30, 30, 40, 255});
        DrawRectangle(static_cast<int>(barX), static_cast<int>(yPos + 2.0f),
                      static_cast<int>(barW * calQ), static_cast<int>(barH), calColor);
        DrawRectangleLines(static_cast<int>(barX), static_cast<int>(yPos + 2.0f),
                           static_cast<int>(barW), static_cast<int>(barH), EXT_PANEL_BORDER);
        DrawTextEx(bodyFont, TextFormat("%.0f%%", calQ * 100.0f),
                   {barX + barW + 5.0f, yPos}, FS(11.0f), sp, calColor);

        // Calibrate button
        if (tier >= 1 && tier < 3)
        {
            Vector2 mousePos0 = GetMousePosition();
            float btnX = barX + barW + 45.0f;
            Rectangle calBtn = {btnX, yPos - 1.0f, 70.0f, 16.0f};

            if (unit->IsCalibrating())
            {
                // Show progress
                float progress = 1.0f - (unit->GetCalibrationTimer() / CALIBRATION_DURATION);
                DrawRectangleRec(calBtn, {30, 40, 60, 255});
                DrawRectangle(static_cast<int>(btnX), static_cast<int>(yPos - 1.0f),
                              static_cast<int>(70.0f * progress), 16, EXT_ACCENT_CYAN);
                DrawRectangleLinesEx(calBtn, 1.0f, EXT_PANEL_BORDER);
                DrawTextEx(bodyFont, "CAL...", {btnX + 14.0f, yPos}, FS(11.0f), sp, WHITE);
            }
            else
            {
                Color btnBg = CheckCollisionPointRec(mousePos0, calBtn) ?
                    Color{60, 70, 90, 255} : Color{40, 45, 60, 255};
                DrawRectangleRec(calBtn, btnBg);
                DrawRectangleLinesEx(calBtn, 1.0f, EXT_PANEL_BORDER);
                DrawTextEx(bodyFont, "CALIBRATE", {btnX + 4.0f, yPos}, FS(10.0f), sp, EXT_ACCENT_CYAN);

                if (CheckCollisionPointRec(mousePos0, calBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    unit->StartCalibration();
                }
            }
        }
        else if (tier >= 3)
        {
            DrawTextEx(bodyFont, "(auto)", {px + 200.0f, yPos}, FS(10.0f), sp, EXT_ACCENT_GREEN);
        }
        yPos += 18.0f;
    }

    // Scan history count
    const auto& scanHistory = unit->GetScanHistory();
    DrawTextEx(bodyFont, TextFormat("Scans Completed: %d", static_cast<int>(scanHistory.size())),
               {px, yPos}, FS(13.0f), sp, LIGHTGRAY);
    yPos += 22.0f;

    // --- SCAN PROFILE section (T1+) ---
    if (tier >= 1)
    {
        DrawTextEx(headerFont, "SCAN PROFILE", {px, yPos}, FS(13.0f), sp, EXT_HEADER_COLOR);
        yPos += 18.0f;

        const auto& profiles = unit->GetAvailableProfiles();
        int activeIdx = unit->GetActiveScanProfileIndex();
        Vector2 mousePos1 = GetMousePosition();
        float btnWidth = 55.0f;
        float btnHeight = 20.0f;

        for (size_t i = 0; i < profiles.size(); i++)
        {
            float bx = px + static_cast<float>(i) * (btnWidth + 5.0f);
            Rectangle profBtn = {bx, yPos, btnWidth, btnHeight};

            bool isActive = (static_cast<int>(i) == activeIdx);
            bool isHovered = CheckCollisionPointRec(mousePos1, profBtn);

            Color btnBg = isActive ? Color{60, 50, 20, 255} :
                          isHovered ? Color{50, 55, 70, 255} : Color{35, 40, 50, 255};
            DrawRectangleRec(profBtn, btnBg);
            Color borderColor = isActive ? EXT_ACCENT_GOLD : EXT_PANEL_BORDER;
            DrawRectangleLinesEx(profBtn, isActive ? 2.0f : 1.0f, borderColor);
            Color textColor = isActive ? EXT_ACCENT_GOLD : LIGHTGRAY;
            DrawTextEx(bodyFont, profiles[i].name.c_str(),
                       {bx + 4.0f, yPos + 3.0f}, FS(10.0f), sp, textColor);

            if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                unit->SetActiveScanProfile(static_cast<int>(i));
            }

            // Tooltip on hover
            if (isHovered)
            {
                float ttX = bx;
                float ttY = yPos + btnHeight + 2.0f;
                DrawRectangle(static_cast<int>(ttX), static_cast<int>(ttY), 150, 48, {20, 25, 35, 240});
                DrawRectangleLines(static_cast<int>(ttX), static_cast<int>(ttY), 150, 48, EXT_PANEL_BORDER);
                DrawTextEx(bodyFont, TextFormat("Pwr: %.1fx  Pulses: %d",
                    profiles[i].powerMultiplier, profiles[i].pulseCount),
                    {ttX + 4.0f, ttY + 3.0f}, FS(9.0f), sp, LIGHTGRAY);
                DrawTextEx(bodyFont, TextFormat("Cooldown: %.0fs  Cost: %.0f",
                    profiles[i].cooldownTime, profiles[i].energyCost),
                    {ttX + 4.0f, ttY + 16.0f}, FS(9.0f), sp, LIGHTGRAY);
                float noiseMult = 1.0f / (profiles[i].powerMultiplier *
                    std::sqrt(static_cast<float>(profiles[i].pulseCount) / 15.0f));
                DrawTextEx(bodyFont, TextFormat("Noise: %.1fx", noiseMult),
                    {ttX + 4.0f, ttY + 29.0f}, FS(9.0f), sp, EXT_ACCENT_CYAN);
            }
        }

        // AI profile indicator
        const auto& ai = unit->GetProspectingAI();
        if (ai.autoSelectProfile)
        {
            float aiX = px + profiles.size() * (btnWidth + 5.0f) + 5.0f;
            DrawTextEx(bodyFont, "[AI]", {aiX, yPos + 3.0f}, FS(10.0f), sp, EXT_ACCENT_CYAN);
        }
        yPos += btnHeight + 4.0f;
    }

    // --- Interactive 5x5 Scan Grid ---
    Vector2 unitGrid = unit->GetGridPosition();
    int centerGX = static_cast<int>(unitGrid.x);
    int centerGY = static_cast<int>(unitGrid.y);
    float cellSz = 38.0f;
    float gridStartX = px;
    float gridStartY = yPos;
    Vector2 mousePos = GetMousePosition();
    bool canScan = true;
    float cooldown = unit->GetScanCooldown();
    bool campaignMode = unit->IsCampaignActive();

    DrawTextEx(headerFont, "SCAN GRID", {px, yPos - 2.0f}, FS(14.0f), sp, EXT_HEADER_COLOR);
    DrawTextEx(bodyFont, "(5x5 around unit)", {px + 85.0f, yPos}, FS(11.0f), sp, EXT_DIM_TEXT);
    yPos += 18.0f;
    gridStartY = yPos;

    for (int dy = -2; dy <= 2; dy++)
    {
        for (int dx = -2; dx <= 2; dx++)
        {
            int cellGX = centerGX + dx;
            int cellGY = centerGY + dy;
            float cx = gridStartX + static_cast<float>(dx + 2) * cellSz;
            float cy = gridStartY + static_cast<float>(dy + 2) * cellSz;
            Rectangle cellRect = {cx, cy, cellSz - 2.0f, cellSz - 2.0f};

            // Background: scanned cells tinted by quality, unscanned dark
            auto scanIt = scanHistory.find({cellGX, cellGY});
            bool isScanned = (scanIt != scanHistory.end() && scanIt->second.isScanned);

            Color bgColor = {20, 25, 35, 255};
            if (isScanned)
            {
                int q = scanIt->second.qualityRating;
                bgColor = {static_cast<unsigned char>(30 + q * 10),
                           static_cast<unsigned char>(40 + q * 15),
                           static_cast<unsigned char>(50 + q * 10), 255};
            }
            DrawRectangleRec(cellRect, bgColor);

            // Unit's own cell: cyan border
            if (dx == 0 && dy == 0)
            {
                DrawRectangleLinesEx(cellRect, 2.0f, EXT_ACCENT_CYAN);
            }
            else
            {
                DrawRectangleLinesEx(cellRect, 1.0f, EXT_PANEL_BORDER);
            }

            // Marked sites: small green dot
            const auto& markedSites = unit->GetMarkedSites();
            for (const auto& site : markedSites)
            {
                if (site.first == cellGX && site.second == cellGY)
                {
                    DrawCircle(static_cast<int>(cx + cellSz/2.0f - 1.0f),
                               static_cast<int>(cy + cellSz/2.0f - 1.0f), 4.0f, EXT_ACCENT_GREEN);
                    break;
                }
            }

            // Campaign queue indicator
            const auto& campaign = unit->GetCampaign();
            for (size_t ci = 0; ci < campaign.size(); ci++)
            {
                if (campaign[ci].gridX == cellGX && campaign[ci].gridY == cellGY)
                {
                    Color cColor = campaign[ci].completed ? Color{60, 100, 60, 200} : EXT_ACCENT_GOLD;
                    DrawRectangleLinesEx(cellRect, 2.0f, cColor);
                    if (!campaign[ci].completed)
                    {
                        DrawTextEx(bodyFont, TextFormat("%d", static_cast<int>(ci + 1)),
                                   {cx + cellSz - 14.0f, cy + cellSz - 16.0f}, FS(9.0f), sp, EXT_ACCENT_GOLD);
                    }
                    break;
                }
            }

            // Scan count badge (top-right corner)
            if (isScanned && scanIt->second.scanCount > 0)
            {
                int sc = scanIt->second.scanCount;
                Color badgeColor = sc >= 3 ? EXT_ACCENT_GREEN :
                                   sc >= 2 ? YELLOW : LIGHTGRAY;
                DrawTextEx(bodyFont, TextFormat("%dx", sc),
                           {cx + cellSz - 18.0f, cy + 2.0f}, FS(8.0f), sp, badgeColor);
            }

            // Coordinate label
            DrawTextEx(bodyFont, TextFormat("%d,%d", cellGX, cellGY),
                       {cx + 2.0f, cy + 2.0f}, FS(9.0f), sp, EXT_DIM_TEXT);

            // Hover highlight
            if (canScan && cooldown <= 0.0f && !unit->IsCalibrating() &&
                CheckCollisionPointRec(mousePos, cellRect))
            {
                DrawRectangleRec(cellRect, {255, 255, 255, 30});

                // Left-click: perform scan (or add to campaign if T2+ campaign mode)
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    unit->PerformLIBSScan(cellGX, cellGY);
                }
                // Right-click: toggle mark/unmark
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
                {
                    bool alreadyMarked = false;
                    for (const auto& site : markedSites)
                    {
                        if (site.first == cellGX && site.second == cellGY)
                        {
                            alreadyMarked = true;
                            break;
                        }
                    }
                    if (alreadyMarked)
                        unit->UnmarkSite(cellGX, cellGY);
                    else
                        unit->MarkSiteForExcavation(cellGX, cellGY);
                }
                // Middle-click: add to campaign queue (T2+)
                if (tier >= 2 && IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
                {
                    unit->AddToCampaign(cellGX, cellGY);
                }

                // Hover tooltip with confidence interval
                if (isScanned && scanIt->second.scanCount > 0 && tier >= 1)
                {
                    float ttX = cx + cellSz;
                    float ttY = cy;
                    if (ttX + 160.0f > static_cast<float>(x + w)) ttX = cx - 162.0f;
                    int ttH = 12 + static_cast<int>(scanIt->second.elements.size()) * 13;
                    DrawRectangle(static_cast<int>(ttX), static_cast<int>(ttY),
                                  160, ttH, {15, 20, 30, 240});
                    DrawRectangleLines(static_cast<int>(ttX), static_cast<int>(ttY),
                                      160, ttH, EXT_PANEL_BORDER);

                    float ttYPos = ttY + 3.0f;
                    int sc = scanIt->second.scanCount;
                    for (const auto& [resType, amount] : scanIt->second.elements)
                    {
                        // Confidence interval: ±(noise% / sqrt(scanCount))
                        float basePct = (tier == 1) ? 15.0f : (tier == 2) ? 5.0f : 0.0f;
                        float interval = basePct / std::sqrt(static_cast<float>(sc));
                        float absInterval = amount * interval / 100.0f;
                        std::string resName = ResourceUtils::GetResourceName(resType);
                        DrawTextEx(bodyFont, TextFormat("%s: %.0f +/-%.0f (%dx)",
                            resName.c_str(), amount, absInterval, sc),
                            {ttX + 4.0f, ttYPos}, FS(9.0f), sp, LIGHTGRAY);
                        ttYPos += 13.0f;
                    }
                }
            }
        }
    }

    // Cooldown / Calibrating overlay
    if (cooldown > 0.0f || unit->IsCalibrating())
    {
        float gridW = 5.0f * cellSz;
        float gridH = 5.0f * cellSz;
        DrawRectangle(static_cast<int>(gridStartX), static_cast<int>(gridStartY),
                      static_cast<int>(gridW), static_cast<int>(gridH), {0, 0, 0, 150});

        if (unit->IsCalibrating())
        {
            DrawTextEx(headerFont, "CALIBRATING",
                       {gridStartX + gridW/2.0f - 50.0f, gridStartY + gridH/2.0f - 12.0f},
                       FS(16.0f), sp, EXT_ACCENT_CYAN);
            float barY2 = gridStartY + gridH/2.0f + 10.0f;
            float calProgress = 1.0f - (unit->GetCalibrationTimer() / CALIBRATION_DURATION);
            DrawRectangle(static_cast<int>(gridStartX + 20.0f), static_cast<int>(barY2),
                          static_cast<int>((gridW - 40.0f) * calProgress), 6, EXT_ACCENT_CYAN);
            DrawRectangleLines(static_cast<int>(gridStartX + 20.0f), static_cast<int>(barY2),
                               static_cast<int>(gridW - 40.0f), 6, EXT_PANEL_BORDER);
        }
        else
        {
            DrawTextEx(headerFont, "COOLDOWN",
                       {gridStartX + gridW/2.0f - 40.0f, gridStartY + gridH/2.0f - 12.0f},
                       FS(16.0f), sp, EXT_ACCENT_GOLD);
            float barY2 = gridStartY + gridH/2.0f + 10.0f;
            float maxCooldown = unit->GetActiveScanProfile().cooldownTime;
            if (maxCooldown < 0.1f) maxCooldown = 3.0f;
            float progress = 1.0f - (cooldown / maxCooldown);
            DrawRectangle(static_cast<int>(gridStartX + 20.0f), static_cast<int>(barY2),
                          static_cast<int>((gridW - 40.0f) * progress), 6, EXT_ACCENT_CYAN);
            DrawRectangleLines(static_cast<int>(gridStartX + 20.0f), static_cast<int>(barY2),
                               static_cast<int>(gridW - 40.0f), 6, EXT_PANEL_BORDER);
        }
    }

    yPos = gridStartY + 5.0f * cellSz + 8.0f;

    // Interaction hints
    const char* hints = (tier >= 2) ?
        "L-click: Scan  R-click: Mark  M-click: Queue" :
        "Left-click: Scan cell   Right-click: Mark/unmark site";
    DrawTextEx(bodyFont, hints, {px, yPos}, FS(10.0f), sp, EXT_DIM_TEXT);
    yPos += 16.0f;

    // --- Campaign Controls (T2+) ---
    if (tier >= 2)
    {
        const auto& campaign = unit->GetCampaign();
        DrawTextEx(headerFont, TextFormat("CAMPAIGN (%d queued)", static_cast<int>(campaign.size())),
                   {px, yPos}, FS(13.0f), sp, EXT_HEADER_COLOR);
        yPos += 18.0f;

        Vector2 mousePos2 = GetMousePosition();
        float cbtnW = 50.0f;
        float cbtnH = 18.0f;

        if (!campaign.empty())
        {
            if (unit->IsCampaignActive())
            {
                // PAUSE button
                Rectangle pauseBtn = {px, yPos, cbtnW, cbtnH};
                Color pbg = CheckCollisionPointRec(mousePos2, pauseBtn) ?
                    Color{60, 60, 80, 255} : Color{40, 40, 55, 255};
                DrawRectangleRec(pauseBtn, pbg);
                DrawRectangleLinesEx(pauseBtn, 1.0f, EXT_PANEL_BORDER);
                DrawTextEx(bodyFont, "PAUSE", {px + 6.0f, yPos + 2.0f}, FS(10.0f), sp, YELLOW);
                if (CheckCollisionPointRec(mousePos2, pauseBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    unit->PauseCampaign();
                }

                // Progress
                DrawTextEx(bodyFont, TextFormat("Scanning %d/%d...",
                    static_cast<int>(std::count_if(campaign.begin(), campaign.end(),
                        [](const Unit::CampaignEntry& e) { return e.completed; })),
                    static_cast<int>(campaign.size())),
                    {px + cbtnW + 10.0f, yPos + 2.0f}, FS(10.0f), sp, EXT_ACCENT_CYAN);
            }
            else
            {
                // START button
                Rectangle startBtn = {px, yPos, cbtnW, cbtnH};
                Color sbg = CheckCollisionPointRec(mousePos2, startBtn) ?
                    Color{40, 70, 40, 255} : Color{30, 50, 30, 255};
                DrawRectangleRec(startBtn, sbg);
                DrawRectangleLinesEx(startBtn, 1.0f, EXT_PANEL_BORDER);
                DrawTextEx(bodyFont, "START", {px + 8.0f, yPos + 2.0f}, FS(10.0f), sp, EXT_ACCENT_GREEN);
                if (CheckCollisionPointRec(mousePos2, startBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    unit->StartCampaign();
                }
            }

            // CLEAR button
            Rectangle clearBtn = {px + cbtnW + 60.0f + (unit->IsCampaignActive() ? 70.0f : 0.0f),
                                  yPos, 48.0f, cbtnH};
            Color clbg = CheckCollisionPointRec(mousePos2, clearBtn) ?
                Color{80, 40, 40, 255} : Color{55, 30, 30, 255};
            DrawRectangleRec(clearBtn, clbg);
            DrawRectangleLinesEx(clearBtn, 1.0f, EXT_PANEL_BORDER);
            DrawTextEx(bodyFont, "CLEAR", {clearBtn.x + 6.0f, yPos + 2.0f}, FS(10.0f), sp, RED);
            if (CheckCollisionPointRec(mousePos2, clearBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                unit->ClearCampaign();
            }
        }
        else
        {
            DrawTextEx(bodyFont, "Middle-click grid cells to queue", {px, yPos}, FS(10.0f), sp, EXT_DIM_TEXT);
        }
        yPos += 22.0f;
    }

    // --- Scan History (compact) ---
    if (!scanHistory.empty())
    {
        DrawTextEx(headerFont, "RECENT SCANS", {px, yPos}, FS(14.0f), sp, EXT_HEADER_COLOR);
        yPos += 20.0f;

        float maxBarW = static_cast<float>(w - padding * 2 - 140);

        // Sort by scan order (most recent first)
        std::vector<std::pair<std::pair<int,int>, Unit::ScanResult>> sortedScans(
            scanHistory.begin(), scanHistory.end());
        std::sort(sortedScans.begin(), sortedScans.end(),
                  [](const auto& a, const auto& b) { return a.second.scanOrder > b.second.scanOrder; });

        int count = 0;
        for (const auto& [coords, scan] : sortedScans)
        {
            if (count >= 4) break;
            DrawTextEx(bodyFont, TextFormat("(%d,%d)", coords.first, coords.second),
                       {px, yPos}, FS(12.0f), sp, LIGHTGRAY);

            // Quality rating + scan count
            DrawTextEx(bodyFont, TextFormat("Q%d %dx", scan.qualityRating, scan.scanCount),
                       {px + 55.0f, yPos}, FS(11.0f), sp, EXT_ACCENT_GOLD);

            float barH = 16.0f;

            if (scan.scanTier == 0)
            {
                // Tier 0: Show categories only (no bars)
                float catX = px + 100.0f;
                for (const auto& [resType, category] : scan.categories)
                {
                    std::string resName = ResourceUtils::GetResourceName(resType);
                    Color catColor = (category == "HIGH") ? GREEN :
                                     (category == "MED") ? YELLOW : RED;
                    std::string label = resName.substr(0, 2) + ":" + category;
                    DrawTextEx(bodyFont, label.c_str(), {catX, yPos + 1.0f},
                               FS(9.0f), sp, catColor);
                    catX += MeasureTextEx(bodyFont, label.c_str(), FS(9.0f), sp).x + 6.0f;
                }
            }
            else
            {
                // Tier 1+: Show composition bars with values
                float totalAmount = 0.0f;
                for (const auto& [resType, amount] : scan.elements)
                {
                    totalAmount += amount;
                }
                float barX = px + 100.0f;
                float availBarW = maxBarW;
                if (totalAmount > 0.0f)
                {
                    for (const auto& [resType, amount] : scan.elements)
                    {
                        float fraction = amount / totalAmount;
                        float pct = fraction * 100.0f;
                        float barW2 = fraction * availBarW;
                        if (barW2 < 2.0f) barW2 = 2.0f;
                        Color barColor = ResourceUtils::GetResourceColor(resType);
                        DrawRectangle(static_cast<int>(barX), static_cast<int>(yPos),
                                      static_cast<int>(barW2), static_cast<int>(barH), barColor);

                        std::string resName = ResourceUtils::GetResourceName(resType);
                        std::string label = TextFormat("%s %.0f%%", resName.c_str(), pct);
                        float labelW = MeasureTextEx(bodyFont, label.c_str(), FS(10.0f), sp).x;
                        if (barW2 > labelW + 4.0f)
                        {
                            DrawTextEx(bodyFont, label.c_str(),
                                       {barX + 2.0f, yPos + 1.0f}, FS(10.0f), sp, {0, 0, 0, 200});
                        }
                        else if (barW2 > 18.0f)
                        {
                            DrawTextEx(bodyFont, resName.c_str(),
                                       {barX + 2.0f, yPos + 1.0f}, FS(10.0f), sp, {0, 0, 0, 200});
                        }
                        barX += barW2 + 1.0f;
                    }
                }
            }

            yPos += barH + 6.0f;
            count++;
        }
    }

    // --- Marked Sites ---
    yPos += 8.0f;
    const auto& markedSites = unit->GetMarkedSites();
    DrawTextEx(headerFont, TextFormat("MARKED SITES (%d)", static_cast<int>(markedSites.size())),
               {px, yPos}, FS(14.0f), sp, EXT_HEADER_COLOR);
    yPos += 20.0f;

    for (size_t i = 0; i < markedSites.size() && i < 4; i++)
    {
        DrawTextEx(bodyFont, TextFormat("  Site %d: (%d, %d)", static_cast<int>(i + 1),
                   markedSites[i].first, markedSites[i].second),
                   {px, yPos}, FS(12.0f), sp, EXT_ACCENT_GREEN);
        yPos += 16.0f;
    }

    // --- Objectives Section (T1+) ---
    if (tier >= 1)
    {
        yPos += 8.0f;
        const auto& objectives = unit->GetActiveObjectives();
        DrawTextEx(headerFont, TextFormat("OBJECTIVES (%d)", static_cast<int>(objectives.size())),
                   {px, yPos}, FS(13.0f), sp, EXT_HEADER_COLOR);
        yPos += 18.0f;

        for (size_t i = 0; i < objectives.size() && i < 3; i++)
        {
            const auto& obj = objectives[i];
            if (obj.revealed)
            {
                Color objColor = obj.completed ? EXT_ACCENT_GOLD : LIGHTGRAY;
                DrawTextEx(bodyFont, obj.description.c_str(),
                           {px + 5.0f, yPos}, FS(10.0f), sp, objColor);

                // Reward preview
                const char* rewardLabels[] = {"extraction bonus", "confidence bonus", "resource grant"};
                DrawTextEx(bodyFont, TextFormat("  -> +%.0f%% %s",
                    obj.rewardValue * 100.0f, rewardLabels[obj.rewardType]),
                    {px + 5.0f, yPos + 12.0f}, FS(9.0f), sp, EXT_DIM_TEXT);
            }
            else
            {
                DrawTextEx(bodyFont, "??? (scan nearby to reveal)",
                           {px + 5.0f, yPos}, FS(10.0f), sp, EXT_DIM_TEXT);
            }
            yPos += 26.0f;
        }

        // Objective bonus active indicator
        if (unit->GetObjectiveBonusMultiplier() > 1.0f)
        {
            DrawTextEx(bodyFont, TextFormat("Active bonus: +%.0f%% extraction",
                (unit->GetObjectiveBonusMultiplier() - 1.0f) * 100.0f),
                {px, yPos}, FS(10.0f), sp, EXT_ACCENT_GOLD);
            yPos += 14.0f;
        }

        // Discovery log (compact)
        const auto& completed = unit->GetCompletedObjectives();
        if (!completed.empty())
        {
            DrawTextEx(bodyFont, TextFormat("Discovery Log: %d completed", static_cast<int>(completed.size())),
                       {px, yPos}, FS(10.0f), sp, EXT_DIM_TEXT);
            yPos += 14.0f;
        }
    }

    // --- Depth Profile (when hovering a scanned cell with layer data, T1+) ---
    // This section draws on the right side of the grid when applicable
    if (tier >= 1)
    {
        // Find if any scanned cell is being hovered that has layer data
        for (int dy = -2; dy <= 2; dy++)
        {
            for (int dx = -2; dx <= 2; dx++)
            {
                int cellGX = centerGX + dx;
                int cellGY = centerGY + dy;
                float cx = gridStartX + static_cast<float>(dx + 2) * cellSz;
                float cy = gridStartY + static_cast<float>(dy + 2) * cellSz;
                Rectangle cellRect = {cx, cy, cellSz - 2.0f, cellSz - 2.0f};

                if (CheckCollisionPointRec(mousePos, cellRect))
                {
                    auto scanIt2 = scanHistory.find({cellGX, cellGY});
                    if (scanIt2 != scanHistory.end() && scanIt2->second.isScanned &&
                        !scanIt2->second.layerElements.empty())
                    {
                        // Draw depth profile on the right side
                        float dpX = gridStartX + 5.0f * cellSz + 10.0f;
                        float dpY = gridStartY;
                        float dpW = panelW - (5.0f * cellSz + 10.0f);
                        if (dpW < 80.0f) break;

                        DrawTextEx(bodyFont, "DEPTH PROFILE", {dpX, dpY}, FS(11.0f), sp, EXT_HEADER_COLOR);
                        dpY += 16.0f;

                        const char* layerNames[] = {"Surface", "Shallow", "Mid", "Deep"};
                        const char* layerDepths[] = {"0-10cm", "10-30cm", "30-100cm", "100-300cm"};
                        int maxLayer = scanIt2->second.maxScannedDepthLayer;

                        float bandH = 28.0f;
                        for (int li = 0; li < 4; li++)
                        {
                            DepthLayer dl = static_cast<DepthLayer>(li);
                            float bandY = dpY + li * (bandH + 2.0f);

                            if (li <= maxLayer)
                            {
                                // Scanned layer — show resource bars
                                DrawRectangle(static_cast<int>(dpX), static_cast<int>(bandY),
                                              static_cast<int>(dpW), static_cast<int>(bandH),
                                              {30, static_cast<unsigned char>(35 + li * 5), 45, 255});
                                DrawTextEx(bodyFont, layerNames[li],
                                           {dpX + 2.0f, bandY + 1.0f}, FS(8.0f), sp, LIGHTGRAY);
                                DrawTextEx(bodyFont, layerDepths[li],
                                           {dpX + 2.0f, bandY + 10.0f}, FS(7.0f), sp, EXT_DIM_TEXT);

                                auto layerIt = scanIt2->second.layerElements.find(dl);
                                if (layerIt != scanIt2->second.layerElements.end())
                                {
                                    float totalL = 0.0f;
                                    for (const auto& [t, v] : layerIt->second)
                                        totalL += v;

                                    float barX2 = dpX + 2.0f;
                                    float barY2 = bandY + 18.0f;
                                    float barAvail = dpW - 4.0f;
                                    if (totalL > 0.0f)
                                    {
                                        for (const auto& [t, v] : layerIt->second)
                                        {
                                            float frac = v / totalL;
                                            float bw = frac * barAvail;
                                            if (bw < 1.0f) bw = 1.0f;
                                            DrawRectangle(static_cast<int>(barX2), static_cast<int>(barY2),
                                                          static_cast<int>(bw), 8,
                                                          ResourceUtils::GetResourceColor(t));
                                            barX2 += bw;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                // Unscanned layer
                                DrawRectangle(static_cast<int>(dpX), static_cast<int>(bandY),
                                              static_cast<int>(dpW), static_cast<int>(bandH),
                                              {15, 15, 20, 255});
                                DrawTextEx(bodyFont, TextFormat("%s ???", layerNames[li]),
                                           {dpX + 2.0f, bandY + 8.0f}, FS(9.0f), sp, EXT_DIM_TEXT);
                            }

                            DrawRectangleLines(static_cast<int>(dpX), static_cast<int>(bandY),
                                               static_cast<int>(dpW), static_cast<int>(bandH), EXT_PANEL_BORDER);
                        }
                    }
                    goto depthProfileDone;  // Only draw for one cell
                }
            }
        }
        depthProfileDone:;
    }

    // --- AI Auto-Management Toggle (T1+) ---
    if (tier >= 1)
    {
        yPos += 8.0f;
        DrawTextEx(headerFont, "AI MANAGEMENT", {px, yPos}, FS(12.0f), sp, EXT_HEADER_COLOR);
        yPos += 16.0f;

        auto& ai = unit->GetProspectingAI();
        Vector2 mousePos3 = GetMousePosition();
        float checkX = px;
        float checkW = panelW * 0.5f;
        float checkH = 16.0f;

        // Auto Profile checkbox
        {
            Rectangle checkRect = {checkX, yPos, checkW, checkH};
            bool hovered = CheckCollisionPointRec(mousePos3, checkRect);
            Color textCol = ai.autoSelectProfile ? EXT_ACCENT_CYAN : EXT_DIM_TEXT;
            const char* checkmark = ai.autoSelectProfile ? "[x]" : "[ ]";
            DrawTextEx(bodyFont, TextFormat("%s Auto Profile", checkmark),
                       {checkX, yPos + 1.0f}, FS(10.0f), sp, textCol);
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                ai.autoSelectProfile = !ai.autoSelectProfile;
            }
        }
        yPos += checkH;

        // Auto Calibrate checkbox
        {
            Rectangle checkRect = {checkX, yPos, checkW, checkH};
            bool hovered = CheckCollisionPointRec(mousePos3, checkRect);
            Color textCol = ai.autoCalibrate ? EXT_ACCENT_CYAN : EXT_DIM_TEXT;
            const char* checkmark = ai.autoCalibrate ? "[x]" : "[ ]";
            DrawTextEx(bodyFont, TextFormat("%s Auto Calibrate", checkmark),
                       {checkX, yPos + 1.0f}, FS(10.0f), sp, textCol);
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                ai.autoCalibrate = !ai.autoCalibrate;
            }
        }
        yPos += checkH;

        // Auto Campaign checkbox (T3 only)
        if (tier >= 3)
        {
            Rectangle checkRect = {checkX, yPos, checkW, checkH};
            bool hovered = CheckCollisionPointRec(mousePos3, checkRect);
            Color textCol = ai.autoCampaign ? EXT_ACCENT_CYAN : EXT_DIM_TEXT;
            const char* checkmark = ai.autoCampaign ? "[x]" : "[ ]";
            DrawTextEx(bodyFont, TextFormat("%s Auto Campaign", checkmark),
                       {checkX, yPos + 1.0f}, FS(10.0f), sp, textCol);
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                ai.autoCampaign = !ai.autoCampaign;
            }
            yPos += checkH;
        }

        // AI last action message
        const auto& lastAction = unit->GetAILastAction();
        if (!lastAction.empty())
        {
            DrawTextEx(bodyFont, lastAction.c_str(), {px, yPos + 2.0f}, FS(9.0f), sp, EXT_ACCENT_CYAN);
        }
    }
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
        Color minusBg = CheckCollisionPointRec(mousePos, depthMinus) ? Color{60, 60, 80, 255} : Color{40, 40, 55, 255};
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
        Color plusBg = CheckCollisionPointRec(mousePos, depthPlus) ? Color{60, 60, 80, 255} : Color{40, 40, 55, 255};
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
        Color rMinBg = CheckCollisionPointRec(mousePos, rateMinus) ? Color{60, 60, 80, 255} : Color{40, 40, 55, 255};
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
        Color rPlsBg = CheckCollisionPointRec(mousePos, ratePlus) ? Color{60, 60, 80, 255} : Color{40, 40, 55, 255};
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
            Color upBg = CheckCollisionPointRec(mousePos, upBtn) ? Color{60, 70, 90, 255} : Color{40, 45, 60, 255};
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
            Color downBg = CheckCollisionPointRec(mousePos, downBtn) ? Color{60, 70, 90, 255} : Color{40, 45, 60, 255};
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
        Color toggleBg = CheckCollisionPointRec(mousePos, toggleBtn) ? Color{50, 55, 70, 255} : Color{30, 35, 50, 255};
        DrawRectangleRec(toggleBtn, toggleBg);
        DrawRectangleLinesEx(toggleBtn, 1.0f, activeColor);
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

    // Geological confidence bonus
    float confidence = unit->GetGeologicalConfidence();
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
    Color dirColor = (dirIdx == 0) ? EXT_DIM_TEXT : EXT_ACCENT_GOLD;
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
            cardBg = {50, 45, 20, 255};  // Gold-tinted active
        }
        else if (isUnlocked)
        {
            cardBg = CheckCollisionPointRec(mousePos, card) ? Color{40, 45, 60, 255} : Color{30, 35, 50, 255};
        }
        else
        {
            cardBg = {20, 20, 25, 200};  // Dim locked
        }
        DrawRectangleRec(card, cardBg);

        // Border
        if (isCurrentDirective)
        {
            DrawRectangleLinesEx(card, 2.0f, EXT_ACCENT_GOLD);
        }
        else
        {
            DrawRectangleLinesEx(card, 1.0f, EXT_PANEL_BORDER);
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
            Color chipBg = isSelected ? Color{60, 50, 20, 255} : Color{35, 40, 55, 255};
            if (!isSelected && CheckCollisionPointRec(mousePos, chip))
            {
                chipBg = {50, 55, 70, 255};
            }

            DrawRectangleRec(chip, chipBg);
            Color chipBorder = isSelected ? EXT_ACCENT_GOLD :
                               ResourceUtils::GetResourceColor(resources[r]);
            DrawRectangleLinesEx(chip, isSelected ? 2.0f : 1.0f, chipBorder);

            Color labelColor = isSelected ? EXT_ACCENT_GOLD :
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
