// Interactive sandbox for the WHOLE extraction unit.
//
// The prospecting playtest (playtest_main.cpp) boots into one module. This
// boots into the unit with prospecting, excavation and beneficiation all
// running, so the chain a player actually experiences is testable end to end:
//
//     survey the ground -> pick where to dig -> watch what arrives
//
// Controls (all also on-screen, because this is played on a phone):
//
//     click a module card    switch modules
//     1 / 2 / 3              time x1, x5, x20
//     P / E                  tier up prospecting / excavation
//     S                      instant survey (sweep + cores + lab)
//     R                      reset to a fresh unit at tier 0
//
// Time acceleration is the important one. A face takes game days to work out
// and a survey takes a day to pay for itself, so at 1x you cannot see the
// module's central claim inside a play session. At x20 a spot depletes in a
// few seconds and the pit visibly advances.
//
// The instant-survey button exists for the same reason: it lets you A/B the
// same ground blind and surveyed without grinding the sweep loop twice.

#include "raylib.h"
#include "rendermanager.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "unit.h"
#include "prospecting_system.h"
#include "excavation_system.h"
#include "game_constants.h"

#include <map>
#include <memory>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// The unit is normally fed by its sect. There is none here, so supply a
// steady trickle with a cap -- free energy would hide the power cap slider
// doing its job, which is one of the things worth testing.
static const float ENERGY_PER_SECOND = 120.0f;
static const float ENERGY_CAP = 4000.0f;

struct Ctx
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
    int speed = 1;
    const char* shotPath = nullptr;
    bool done = false;
    bool showPointer = true;
};

static int FindModule(Unit& unit, const std::string& type)
{
    const auto& modules = unit.GetModules();
    for (size_t i = 0; i < modules.size(); i++)
    {
        if (modules[i].moduleType == type) return static_cast<int>(i);
    }
    return -1;
}

static std::unique_ptr<Unit> MakeUnit(Ctx& ctx)
{
    // Mid-grid, so the unit sits on a populated resource cell.
    Vector2 position = {SECT_CORE_RADIUS * 2.0f * 5.0f, SECT_CORE_RADIUS * 2.0f * 5.0f};

    auto unit = std::make_unique<Unit>("Extraction", position, *ctx.resourceManager,
                                       *ctx.timeManager, ctx.storage, ctx.capacity);

    // Everything on: the point is the whole chain, not one stage of it.
    for (const char* type : {"PROSPECTING", "EXCAVATION", "BENEFICIATION"})
    {
        int index = FindModule(*unit, type);
        if (index >= 0) unit->ActivateModule(index);
    }

    int excavation = FindModule(*unit, "EXCAVATION");
    if (excavation >= 0)
    {
        unit->SetSelectedModuleIndex(excavation);
        unit->SetIsInModuleView(true);
    }

    unit->Start();
    unit->PublicShowMessage("[SANDBOX] S surveys, E upgrades digging, 1/2/3 sets speed.");
    return unit;
}

// Runs a full survey instantly, so blind and surveyed ground can be compared
// on the same seed without grinding the sweep loop by hand.
static void InstantSurvey(Unit& unit)
{
    ProspectingSystem* ps = unit.GetProspectingSystem();
    if (!ps) return;

    ProspectingGrid& grid = ps->GetGrid();
    SampleTray& tray = ps->GetTray();

    for (int band = 0; band < SWEEP_FREQUENCY_BANDS; band++)
    {
        if (ps->GetSweep().CanSweep(grid, band))
        {
            ps->GetSweep().ExecuteSweep(grid, band, ps->gameTime);
        }
    }

    for (int y = 0; y < grid.GetGridSize() && !tray.IsFull(); y++)
    {
        for (int x = 0; x < grid.GetGridSize() && !tray.IsFull(); x++)
        {
            if (!grid.IsInReach(x, y)) continue;
            ps->GetSampler().CollectSample(grid, tray, x, y, DepthLayer::SURFACE);
        }
    }

    for (Sample& sample : tray.GetSamples())
    {
        for (AnalysisTool tool : {AnalysisTool::VISUAL_INSPECTION, AnalysisTool::XRF,
                                  AnalysisTool::LIBS_PULSE})
        {
            if (ps->GetLab().CanApplyTool(sample, tool))
            {
                ps->GetLab().ApplyTool(sample, tool, ps->gameTime);
            }
        }
    }

    unit.PublicShowMessage("[SANDBOX] Ground surveyed - compare the spot readings now.");
}

// Small clickable chip; returns true when clicked or tapped this frame.
static bool Chip(Rectangle r, const char* label, Color accent, bool active)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, r);
    bool held = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    Color fill = active ? Fade(accent, 0.30f) : Fade(accent, held ? 0.24f : 0.10f);
    DrawRectangleRounded(r, 0.3f, 4, fill);
    DrawRectangleRoundedLinesEx(r, 0.3f, 4, 1.0f, Fade(accent, active ? 0.95f : 0.55f));

    int fontSize = 11;
    int textW = MeasureText(label, fontSize);
    DrawText(label, static_cast<int>(r.x + (r.width - textW) * 0.5f),
             static_cast<int>(r.y + (r.height - fontSize) * 0.5f), fontSize,
             active ? WHITE : Fade(WHITE, 0.75f));

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void UpdateDrawFrame(void* arg)
{
    Ctx& ctx = *static_cast<Ctx*>(arg);

    float deltaTime = GetFrameTime();

    // Time acceleration is stepped rather than scaled into one big Update:
    // the dig engine works per tick, and a single 20x step would deplete a
    // spot in one jump instead of showing it drain.
    for (int step = 0; step < ctx.speed; step++)
    {
        ctx.timeManager->Update(deltaTime);
        ctx.unit->Update(deltaTime);

        if (ctx.unit->GetStoredResource(ResourceType::ENERGY) < ENERGY_CAP)
        {
            ctx.unit->AddResource(ResourceType::ENERGY, ENERGY_PER_SECOND * deltaTime);
        }
    }

    bool wantProspectingUp = IsKeyPressed(KEY_P);
    bool wantExcavationUp = IsKeyPressed(KEY_E);
    bool wantSurvey = IsKeyPressed(KEY_S);
    bool wantReset = IsKeyPressed(KEY_R);

    if (IsKeyPressed(KEY_F9)) ctx.showPointer = !ctx.showPointer;

    if (IsKeyPressed(KEY_ONE)) ctx.speed = 1;
    if (IsKeyPressed(KEY_TWO)) ctx.speed = 5;
    if (IsKeyPressed(KEY_THREE)) ctx.speed = 20;

    BeginDrawing();
    ClearBackground(BLACK);
    ctx.renderManager->DrawUnitView(ctx.unit.get(), *ctx.timeManager);

    // Sandbox controls along the top bar. Kept clear of "Press S for Sect
    // View" and the day counter on the right -- the first layout sat the RESET
    // chip on top of them.
    float bx = 340.0f;
    float by = 14.0f;

    wantSurvey |= Chip({bx, by, 74.0f, 28.0f}, "SURVEY", {80, 225, 255, 255}, false);
    wantProspectingUp |= Chip({bx + 80.0f, by, 66.0f, 28.0f}, "PROS +", {168, 130, 255, 255}, false);
    wantExcavationUp |= Chip({bx + 152.0f, by, 66.0f, 28.0f}, "DIG +", {168, 130, 255, 255}, false);

    bool s1 = Chip({bx + 230.0f, by, 34.0f, 28.0f}, "x1", {80, 230, 150, 255}, ctx.speed == 1);
    bool s2 = Chip({bx + 268.0f, by, 34.0f, 28.0f}, "x5", {80, 230, 150, 255}, ctx.speed == 5);
    bool s3 = Chip({bx + 306.0f, by, 40.0f, 28.0f}, "x20", {80, 230, 150, 255}, ctx.speed == 20);
    if (s1) ctx.speed = 1;
    if (s2) ctx.speed = 5;
    if (s3) ctx.speed = 20;

    wantReset |= Chip({bx + 358.0f, by, 60.0f, 28.0f}, "RESET", {255, 200, 80, 255}, false);

    // Where the game believes the cursor is. On the web the canvas is CSS-
    // scaled to fit the viewport while the framebuffer stays fixed, so a
    // coordinate bug anywhere in that chain shows up as this crosshair
    // sitting away from the real pointer -- which is otherwise invisible,
    // since the game draws no cursor of its own. F9 hides it.
    if (ctx.showPointer)
    {
        Vector2 mouse = GetMousePosition();
        Color probe = {255, 60, 200, 255};
        DrawLineEx({mouse.x - 14.0f, mouse.y}, {mouse.x + 14.0f, mouse.y}, 1.0f, probe);
        DrawLineEx({mouse.x, mouse.y - 14.0f}, {mouse.x, mouse.y + 14.0f}, 1.0f, probe);
        DrawCircleLines(static_cast<int>(mouse.x), static_cast<int>(mouse.y), 5.0f, probe);

        const char* readout = TextFormat("game sees %d,%d   screen %dx%d   render %dx%d",
                                         static_cast<int>(mouse.x), static_cast<int>(mouse.y),
                                         GetScreenWidth(), GetScreenHeight(),
                                         GetRenderWidth(), GetRenderHeight());
        DrawText(readout, 8, GetScreenHeight() - 18, 12, probe);
    }

    EndDrawing();
    ctx.frame++;

    if (wantProspectingUp)
    {
        int index = FindModule(*ctx.unit, "PROSPECTING");
        if (index >= 0) ctx.unit->DebugUpgradeModuleTier(index);
    }
    if (wantExcavationUp)
    {
        int index = FindModule(*ctx.unit, "EXCAVATION");
        if (index >= 0) ctx.unit->DebugUpgradeModuleTier(index);
    }
    if (wantSurvey)
    {
        InstantSurvey(*ctx.unit);
    }
    if (wantReset)
    {
        ctx.storage.clear();
        ctx.capacity.clear();
        ctx.speed = 1;
        ctx.unit = MakeUnit(ctx);
        ctx.unit->PublicShowMessage("[SANDBOX] Reset - fresh unit, unsurveyed ground.");
    }

    // Hidden smoke-test mode: render N frames, export a screenshot, exit.
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
    Ctx ctx;

    for (int i = 1; i < argc - 1; i++)
    {
        if (std::string(argv[i]) == "--shot") ctx.shotPath = argv[i + 1];
    }

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(ctx.screenWidth, ctx.screenHeight, "Colony - Extraction Unit Playtest");
    SetTargetFPS(60);

    {
        RenderManager renderManager(ctx.screenWidth, ctx.screenHeight);
        renderManager.LoadFonts();

        // The constructor only allocates the grids; Planet normally populates
        // them. Without this the map is empty and every reading is zero.
        ResourceManager resourceManager(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
        resourceManager.GenerateResourceMap();
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

        // The unit (and its GPU-facing state) must die while the GL context lives.
        ctx.unit.reset();
    }

    CloseWindow();
    return 0;
}
