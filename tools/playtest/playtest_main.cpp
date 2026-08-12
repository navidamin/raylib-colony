// Prospecting playtest sandbox.
//
// Boots straight into the extraction unit's prospecting module with a real
// game loop and live input -- no menu, no colony building, no navigation.
// Sweep the grid, drill samples, run the lab, and watch survey progress and
// extraction efficiency respond.
//
// Controls (also shown in-game):
//   Mouse  - operate the panel (tabs, grid cells, bands, tools)
//   T      - upgrade prospecting tier (0 -> 3)
//   R      - reset the run (fresh grid, tier 0)
//   ESC    - quit
//
// Build & run:
//   cmake -B build && cmake --build build --target colony_playtest
//   ./build/src/colony_playtest

#include "raylib.h"

#include "rendermanager.h"
#include "unit.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "game_constants.h"

#include <map>
#include <memory>
#include <string>

static int FindProspectingModule(Unit& unit)
{
    const auto& modules = unit.GetModules();
    for (size_t i = 0; i < modules.size(); i++)
    {
        if (modules[i].moduleType == "PROSPECTING") return static_cast<int>(i);
    }
    return -1;
}

static std::unique_ptr<Unit> MakeUnit(ResourceManager& resourceManager,
                                      TimeManager& timeManager,
                                      std::map<ResourceType, float>& storage,
                                      std::map<ResourceType, float>& capacity)
{
    // Mid-grid position so the unit sits on a populated resource cell
    Vector2 position = {SECT_CORE_RADIUS * 2.0f * 5.0f, SECT_CORE_RADIUS * 2.0f * 5.0f};

    auto unit = std::make_unique<Unit>("Extraction", position, resourceManager,
                                       timeManager, storage, capacity);

    int prospectingIndex = FindProspectingModule(*unit);
    if (prospectingIndex >= 0)
    {
        unit->ActivateModule(prospectingIndex);
        unit->SetSelectedModuleIndex(prospectingIndex);
        unit->SetIsInModuleView(true);
    }
    unit->PublicShowMessage("[PLAYTEST] T: tier up  R: reset  ESC: quit");
    return unit;
}

int main(int argc, char** argv)
{
    // Hidden smoke-test mode: render N frames, export a screenshot, exit.
    // Used by CI/headless checks; players never need it.
    const char* shotPath = nullptr;
    for (int i = 1; i < argc - 1; i++)
    {
        if (std::string(argv[i]) == "--shot") shotPath = argv[i + 1];
    }

    const int screenWidth = 1280;
    const int screenHeight = 720;

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(screenWidth, screenHeight, "Colony - Prospecting Playtest");
    SetTargetFPS(60);

    int exitCode = 0;
    {
        RenderManager renderManager(screenWidth, screenHeight);
        renderManager.LoadFonts();

        ResourceManager resourceManager(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
        TimeManager timeManager;

        std::map<ResourceType, float> storage;
        std::map<ResourceType, float> capacity;

        std::unique_ptr<Unit> unit = MakeUnit(resourceManager, timeManager, storage, capacity);

        int frame = 0;
        while (!WindowShouldClose())
        {
            float deltaTime = GetFrameTime();
            timeManager.Update(deltaTime);
            unit->Update(deltaTime);

            if (IsKeyPressed(KEY_T))
            {
                int idx = FindProspectingModule(*unit);
                if (idx >= 0) unit->DebugUpgradeModuleTier(idx);
            }
            if (IsKeyPressed(KEY_R))
            {
                storage.clear();
                capacity.clear();
                unit = MakeUnit(resourceManager, timeManager, storage, capacity);
                unit->PublicShowMessage("[PLAYTEST] Run reset - fresh grid at tier 0");
            }

            BeginDrawing();
            ClearBackground(BLACK);
            renderManager.DrawUnitView(unit.get(), timeManager);

            // Control hints chip, tucked into the top bar next to the menu icon
            const char* hints = "T tier up   R reset   ESC quit";
            int hintW = MeasureText(hints, 10);
            int hintX = screenWidth - hintW - 300;
            DrawRectangle(hintX, 18, hintW + 20, 20, {10, 15, 28, 220});
            DrawRectangleLines(hintX, 18, hintW + 20, 20, {36, 62, 92, 255});
            DrawText(hints, hintX + 10, 23, 10, {120, 138, 165, 255});

            EndDrawing();
            frame++;

            if (shotPath && frame >= 40)
            {
                Image shot = LoadImageFromScreen();
                if (!ExportImage(shot, shotPath)) exitCode = 1;
                UnloadImage(shot);
                break;
            }
        }
    }

    CloseWindow();
    return exitCode;
}
