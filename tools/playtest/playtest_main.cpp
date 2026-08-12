// Prospecting playtest sandbox.
//
// Boots straight into the extraction unit's prospecting module with a real
// game loop and live input -- no menu, no colony building, no navigation.
// Sweep the grid, drill samples, run the lab, and watch survey progress and
// extraction efficiency respond.
//
// Controls (also shown in-game):
//   Mouse/touch - operate the panel (tabs, grid cells, bands, tools)
//   TIER UP button or T - upgrade prospecting tier (0 -> 3)
//   RESET button or R   - reset the run (fresh grid, tier 0)
//   ESC                 - quit (native build)
//
// Build & run (native):
//   cmake -B build && cmake --build build --target colony_playtest
//   ./build/src/colony_playtest
//
// The Web build (PLATFORM=Web) deploys via .github/workflows/deploy-web.yml
// and is playable on phone/tablet -- taps map to clicks.

#include "raylib.h"

#include "rendermanager.h"
#include "unit.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "game_constants.h"

#include <map>
#include <memory>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static int FindProspectingModule(Unit& unit)
{
    const auto& modules = unit.GetModules();
    for (size_t i = 0; i < modules.size(); i++)
    {
        if (modules[i].moduleType == "PROSPECTING") return static_cast<int>(i);
    }
    return -1;
}

// Everything the frame callback needs; kept alive for the whole session.
struct PlaytestContext
{
    RenderManager* renderManager = nullptr;
    ResourceManager* resourceManager = nullptr;
    TimeManager* timeManager = nullptr;
    std::map<ResourceType, float> storage;
    std::map<ResourceType, float> capacity;
    std::unique_ptr<Unit> unit;

    int screenWidth = 1280;
    int screenHeight = 720;
    int frame = 0;
    const char* shotPath = nullptr;
    bool done = false;
};

static std::unique_ptr<Unit> MakeUnit(PlaytestContext& ctx)
{
    // Mid-grid position so the unit sits on a populated resource cell
    Vector2 position = {SECT_CORE_RADIUS * 2.0f * 5.0f, SECT_CORE_RADIUS * 2.0f * 5.0f};

    auto unit = std::make_unique<Unit>("Extraction", position, *ctx.resourceManager,
                                       *ctx.timeManager, ctx.storage, ctx.capacity);

    int prospectingIndex = FindProspectingModule(*unit);
    if (prospectingIndex >= 0)
    {
        unit->ActivateModule(prospectingIndex);
        unit->SetSelectedModuleIndex(prospectingIndex);
        unit->SetIsInModuleView(true);
    }
    unit->PublicShowMessage("[PLAYTEST] Tap cells to survey. TIER UP / RESET top right.");
    return unit;
}

// Small clickable chip; returns true when clicked/tapped this frame.
static bool PlaytestButton(Rectangle r, const char* label, Color accent)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, r);

    DrawRectangleRounded(r, 0.35f, 4, hover ? Color{20, 56, 96, 255} : Color{14, 30, 52, 255});
    DrawRectangleRoundedLinesEx(r, 0.35f, 4, 1.0f, accent);

    int labelW = MeasureText(label, 10);
    DrawText(label, static_cast<int>(r.x + (r.width - labelW) / 2.0f),
             static_cast<int>(r.y + (r.height - 10.0f) / 2.0f), 10, accent);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void UpdateDrawFrame(void* arg)
{
    PlaytestContext& ctx = *static_cast<PlaytestContext*>(arg);

    float deltaTime = GetFrameTime();
    ctx.timeManager->Update(deltaTime);
    ctx.unit->Update(deltaTime);

    bool wantTierUp = IsKeyPressed(KEY_T);
    bool wantReset = IsKeyPressed(KEY_R);

    BeginDrawing();
    ClearBackground(BLACK);
    ctx.renderManager->DrawUnitView(ctx.unit.get(), *ctx.timeManager);

    // On-screen controls (touch-friendly), tucked into the top bar
    float bx = ctx.screenWidth - 460.0f;
    wantTierUp |= PlaytestButton({bx, 14.0f, 80.0f, 28.0f}, "TIER UP", {80, 225, 255, 255});
    wantReset |= PlaytestButton({bx + 88.0f, 14.0f, 70.0f, 28.0f}, "RESET", {255, 200, 80, 255});

    EndDrawing();
    ctx.frame++;

    if (wantTierUp)
    {
        int idx = FindProspectingModule(*ctx.unit);
        if (idx >= 0) ctx.unit->DebugUpgradeModuleTier(idx);
    }
    if (wantReset)
    {
        ctx.storage.clear();
        ctx.capacity.clear();
        ctx.unit = MakeUnit(ctx);
        ctx.unit->PublicShowMessage("[PLAYTEST] Run reset - fresh grid at tier 0");
    }

    if (ctx.shotPath && ctx.frame >= 40)
    {
        Image shot = LoadImageFromScreen();
        ExportImage(shot, ctx.shotPath);
        UnloadImage(shot);
        ctx.done = true;
    }
}

int main(int argc, char** argv)
{
    PlaytestContext ctx;

    // Hidden smoke-test mode: render N frames, export a screenshot, exit.
    for (int i = 1; i < argc - 1; i++)
    {
        if (std::string(argv[i]) == "--shot") ctx.shotPath = argv[i + 1];
    }

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(ctx.screenWidth, ctx.screenHeight, "Colony - Prospecting Playtest");
    SetTargetFPS(60);

    {
        RenderManager renderManager(ctx.screenWidth, ctx.screenHeight);
        renderManager.LoadFonts();

        ResourceManager resourceManager(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
        TimeManager timeManager;

        ctx.renderManager = &renderManager;
        ctx.resourceManager = &resourceManager;
        ctx.timeManager = &timeManager;
        ctx.unit = MakeUnit(ctx);

#ifdef __EMSCRIPTEN__
        emscripten_set_main_loop_arg(UpdateDrawFrame, &ctx, 0, 1);
#else
        while (!WindowShouldClose() && !ctx.done)
        {
            UpdateDrawFrame(&ctx);
        }
#endif

        // Unit (and its GPU-facing state) must die while the GL context lives
        ctx.unit.reset();
    }

    CloseWindow();
    return 0;
}
