#include "rendermanager.h"
#include <algorithm>
#include <iostream>

RenderManager::RenderManager(int screenWidth, int screenHeight)
    : screenWidth(screenWidth),
      screenHeight(screenHeight)
{
}

RenderManager::~RenderManager() {
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
    Texture2D image = LoadTexture("assets/Logo.png");

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
        // Draw grid
        for (int i = 0; i <= PLANET_SIZE; i++) {
            float linePos = i * SECT_CORE_RADIUS * 2;
            DrawLineV({linePos, 0}, {linePos, PLANET_HEIGHT}, LIGHTGRAY);
            DrawLineV({0, linePos}, {PLANET_WIDTH, linePos}, LIGHTGRAY);
        }

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
                                   TimeManager& timeManager) {
    // Start drawing with camera transformation
    BeginMode2D(camera);

    if (colony) {
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

        // Draw all sects in the current colony
        for (const auto& sect : colony->GetSects()) {
            sect->DrawInColonyView(sect->GetPosition(), camera.zoom);
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

    if (inputManager.IsCommandPressed()) {
        Vector2 mousePos = inputManager.GetMousePosition();
        DrawCellInfo(mousePos, camera, planet, colonies);
        DrawPlusIndicator(mousePos, View::Colony);
    }

    // Draw UI elements including time
    timeManager.Draw(screenWidth, screenHeight);
    DrawText("Colony View", 10, 10, 20, BLACK);
    DrawText("Press S for Sect View", 10, 40, 20, GRAY);
    DrawText("Press P for Planet View", 10, 70, 20, GRAY);

    DrawText(TextFormat("Zoom: %.2f", camera.zoom), 10, screenHeight - 20, 20, GRAY);
    DrawText("Double-click to select", 10, GetScreenHeight() - 40, 20, DARKGRAY);
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

void RenderManager::DrawUnitView(Unit* unit) {
    if (unit) {
        unit->DrawInUnitView();
    }

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
