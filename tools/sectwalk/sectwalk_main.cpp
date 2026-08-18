// Sect walkthrough playtest.
//
// Boots straight into the Sect view so every unit and every module can be
// inspected before merging. No planet view, no colony building, no menu --
// just the sect, its Core dome, and its ring of production units.
//
// Scope is deliberately narrow: this exists to answer "how do all 40 modules
// actually look", which no other harness covers. tools/preview renders one
// panel to a PNG; this one lets you walk the whole tree by hand.
//
// Controls (also shown on screen):
//   Click a unit / the Core dome  - open that unit's view
//   Click a module in the left list - open that module's panel
//   BACK button, ESC or S        - return to the sect view
//   BUILD ALL button or B        - build every module on every unit
//   TIER + / TIER - or +/-       - step every module's tier together
//   R                            - reset the sect
//
// Build & run (native):
//   cmake -B build && cmake --build build --target colony_sectwalk
//   ./build/src/colony_sectwalk
//
// Headless (renders N frames, writes a PNG, exits):
//   ./build/src/colony_sectwalk --shot out.png
//
// The Web build (PLATFORM=Web) is playable on phone/tablet; taps map to clicks.

#include "raylib.h"

#include "rendermanager.h"
#include "sect.h"
#include "unit.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "game_constants.h"

#include <memory>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Fixed seed so the sect is the same every run and screenshots are comparable.
static const unsigned int SECTWALK_SEED = 20260813u;

// Keep every unit supplied so nothing stalls for reasons unrelated to the UI.
static const float SECTWALK_TOPUP_TARGET = 5000.0f;

struct WalkContext
{
    RenderManager* renderManager = nullptr;
    ResourceManager* resourceManager = nullptr;
    TimeManager* timeManager = nullptr;
    std::unique_ptr<Sect> sect;

    Unit* current = nullptr;   // null = sect view
    int screenWidth = 1280;
    int screenHeight = 720;
    int frame = 0;
    const char* shotPath = nullptr;
    bool done = false;
    std::string toast;
    float toastTimer = 0.0f;
};

static void Toast(WalkContext& ctx, const std::string& text)
{
    ctx.toast = text;
    ctx.toastTimer = 2.5f;
}

// Touch-friendly button; returns true on click.
static bool WalkButton(Rectangle r, const char* label, Color accent)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, r);

    DrawRectangleRounded(r, 0.3f, 4, hover ? Color{20, 56, 96, 255} : Color{14, 30, 52, 255});
    DrawRectangleRoundedLinesEx(r, 0.3f, 4, 1.0f, accent);

    int labelW = MeasureText(label, 11);
    DrawText(label, static_cast<int>(r.x + (r.width - labelW) / 2.0f),
             static_cast<int>(r.y + (r.height - 11.0f) / 2.0f), 11, accent);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Every unit's stores topped up, so panels show live numbers rather than
// zeroes and BUILD/UPGRADE stay affordable while walking the tree.
static void TopUpUnits(Sect& sect)
{
    const ResourceType keep[] = {
        ResourceType::ENERGY, ResourceType::WATER, ResourceType::FOOD, ResourceType::O2,
        ResourceType::H2, ResourceType::C, ResourceType::Fe, ResourceType::Si,
        ResourceType::Ti, ResourceType::Al, ResourceType::Ca,
        ResourceType::MACHINERY, ResourceType::ELECTRONICS, ResourceType::ALLOYS,
        ResourceType::CONSTRUCTION_MATERIALS
    };

    for (Unit* unit : sect.GetUnits())
    {
        if (!unit) continue;
        for (ResourceType type : keep)
        {
            float have = unit->GetStoredResource(type);
            if (have < SECTWALK_TOPUP_TARGET)
            {
                unit->AddResource(type, SECTWALK_TOPUP_TARGET - have);
            }
        }
    }
}

static void BuildEverything(Sect& sect)
{
    for (Unit* unit : sect.GetUnits())
    {
        if (!unit) continue;
        for (size_t i = 0; i < unit->GetModules().size(); i++)
        {
            if (!unit->GetModules()[i].isBuilt) unit->PublicBuildModule(static_cast<int>(i));
            unit->ActivateModule(static_cast<int>(i));
        }
    }
}

static void StepTier(Sect& sect, int delta)
{
    for (Unit* unit : sect.GetUnits())
    {
        if (!unit) continue;
        for (size_t i = 0; i < unit->GetModules().size(); i++)
        {
            const auto& mod = unit->GetModules()[i];
            if (!mod.isBuilt) continue;
            if (delta > 0) unit->DebugUpgradeModuleTier(static_cast<int>(i));
            // Tier-down has no engine path; stepping up only.
        }
    }
}

static std::unique_ptr<Sect> MakeSect(WalkContext& ctx)
{
    Vector2 position = {SECT_CORE_RADIUS * 2.0f * 5.0f, SECT_CORE_RADIUS * 2.0f * 5.0f};
    auto sect = std::make_unique<Sect>(position, *ctx.resourceManager, *ctx.timeManager);
    TopUpUnits(*sect);
    return sect;
}

// Sect-view hit test. Positions are written by Sect::DrawInSectView, so this
// must run after the draw call for the current frame.
static Unit* UnitUnderMouse(Sect& sect, Vector2 mouse)
{
    for (Unit* unit : sect.GetUnits())
    {
        if (!unit) continue;
        Vector2 pos = unit->GetUnitPosInSectView();
        float radius = unit->GetUnitRadiusInSectView();
        if (radius > 0.0f && CheckCollisionPointCircle(mouse, pos, radius)) return unit;
    }
    return nullptr;
}

static void DrawHud(WalkContext& ctx)
{
    const Color cyan = {80, 225, 255, 255};
    const Color gold = {255, 200, 80, 255};
    const Color green = {80, 230, 150, 255};
    const Color dim = {120, 138, 165, 255};

    float y = 14.0f;
    float x = ctx.screenWidth - 402.0f;

    bool wantBuild = WalkButton({x, y, 84.0f, 28.0f}, "BUILD ALL", green);
    bool wantTierUp = WalkButton({x + 92.0f, y, 74.0f, 28.0f}, "TIER +", cyan);
    bool wantReset = WalkButton({x + 174.0f, y, 66.0f, 28.0f}, "RESET", gold);
    bool wantBack = false;
    if (ctx.current)
    {
        wantBack = WalkButton({x + 248.0f, y, 66.0f, 28.0f}, "BACK", dim);
    }

    wantBuild |= IsKeyPressed(KEY_B);
    wantTierUp |= IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD);
    wantReset |= IsKeyPressed(KEY_R);
    wantBack |= IsKeyPressed(KEY_S) || IsKeyPressed(KEY_ESCAPE);

    if (wantBuild)
    {
        BuildEverything(*ctx.sect);
        Toast(ctx, "All modules built and activated");
    }
    if (wantTierUp)
    {
        StepTier(*ctx.sect, +1);
        Toast(ctx, "Every built module stepped up one tier");
    }
    if (wantReset)
    {
        ctx.current = nullptr;
        ctx.sect = MakeSect(ctx);
        Toast(ctx, "Sect reset");
    }
    if (wantBack && ctx.current)
    {
        ctx.current = nullptr;
        Toast(ctx, "Back to sect view");
    }

    // Hint line, bottom-left, out of the panels' way
    const char* hint = ctx.current
        ? "Click a module in the left list  |  BACK / S / ESC returns to the sect"
        : "Click a unit or the CORE dome to open it  |  B build all  |  + tier up";
    DrawText(hint, 12, ctx.screenHeight - 22, 11, dim);

    if (ctx.toastTimer > 0.0f)
    {
        int w = MeasureText(ctx.toast.c_str(), 12);
        DrawRectangle(10, ctx.screenHeight - 48, w + 18, 20, Color{10, 15, 28, 220});
        DrawRectangleLines(10, ctx.screenHeight - 48, w + 18, 20, Color{36, 62, 92, 255});
        DrawText(ctx.toast.c_str(), 19, ctx.screenHeight - 44, 12, cyan);
    }
}

static void UpdateDrawFrame(void* arg)
{
    WalkContext& ctx = *static_cast<WalkContext*>(arg);

    float deltaTime = GetFrameTime();
    ctx.timeManager->Update(deltaTime);
    ctx.sect->Update(deltaTime);
    TopUpUnits(*ctx.sect);

    if (ctx.toastTimer > 0.0f) ctx.toastTimer -= deltaTime;

    BeginDrawing();
    ClearBackground(BLACK);

    if (ctx.current)
    {
        ctx.renderManager->DrawUnitView(ctx.current, *ctx.timeManager);
    }
    else
    {
        ctx.renderManager->DrawSectView(ctx.sect.get(), *ctx.timeManager);
    }

    DrawHud(ctx);
    EndDrawing();

    // Selection runs after the draw, because Sect::DrawInSectView is what
    // writes each unit's screen position and radius.
    if (!ctx.current && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        Unit* hit = UnitUnderMouse(*ctx.sect, GetMousePosition());
        if (hit)
        {
            ctx.current = hit;
            hit->SetIsInModuleView(false);
            hit->SetSelectedModuleIndex(0);
            Toast(ctx, hit->GetUnitType() + " unit opened");
        }
    }

    ctx.frame++;

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
    WalkContext ctx;

    for (int i = 1; i < argc - 1; i++)
    {
        if (std::string(argv[i]) == "--shot") ctx.shotPath = argv[i + 1];
    }

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(ctx.screenWidth, ctx.screenHeight, "Colony - Sect Walkthrough");
    SetTargetFPS(60);

    {
        RenderManager renderManager(ctx.screenWidth, ctx.screenHeight);
        renderManager.LoadFonts();

        // Fixed seed: the same sect every run, so a visual regression is real
        // rather than a different map.
        ResourceManager resourceManager(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
        resourceManager.GenerateResourceMap(SECTWALK_SEED);
        TimeManager timeManager;

        ctx.renderManager = &renderManager;
        ctx.resourceManager = &resourceManager;
        ctx.timeManager = &timeManager;
        ctx.sect = MakeSect(ctx);

#ifdef __EMSCRIPTEN__
        emscripten_set_main_loop_arg(UpdateDrawFrame, &ctx, 0, 1);
#else
        while (!WindowShouldClose() && !ctx.done)
        {
            UpdateDrawFrame(&ctx);
        }
#endif

        // Sect owns Units, which hold GPU-facing state; destroy while GL lives.
        ctx.sect.reset();
    }

    CloseWindow();
    return 0;
}
