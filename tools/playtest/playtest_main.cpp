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
//   DIG SPOT button or D - dig the selected cell at the selected depth
//   RESET button or R   - reset the run (fresh grid, tier 0)
//   ESC                 - quit (native build)
//
// The RESOURCE STATEMENT panel (bottom left) is the point of the sandbox: it
// shows, per element, how much tonnage is Measured / Indicated / Inferred at
// the current tier. Sweeping and sampling move tonnage leftward along that
// bar, and DIG SPOT converts one spot outright. Watching that bar move is
// how you feel whether surveying is worth its cost.
//
// Build & run (native):
//   cmake -B build && cmake --build build --target colony_playtest
//   ./build/src/colony_playtest
//
// The Web build (PLATFORM=Web) deploys via .github/workflows/deploy-web.yml
// and is playable on phone/tablet -- taps map to clicks.

#include "raylib.h"
#include "rlgl.h"
#include "web_mouse.h"

#include "rendermanager.h"
#include "unit.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "game_constants.h"
#include "prospecting_system.h"
#include "prospecting_grid.h"
#include "resource_types.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
    // Web supersampling: on a screen larger than the layout the buffer is
    // renderScale x bigger and every frame draws through a matrix scale, so
    // the browser only ever DOWNSCALES -- sharp at any fraction, where
    // stretching the 1280 buffer smeared every small glyph. Layout, input
    // and all game code stay in 1280x720 logical space.
    int renderScale = 1;
    int frame = 0;
    const char* shotPath = nullptr;
    bool done = false;
};

// In the real game the unit is fed by its sect/colony. The sandbox has
// neither, so supply a steady trickle (and a cap) to keep prospecting
// playable without making energy free.
static const float PLAYTEST_ENERGY_PER_SECOND = 30.0f;
static const float PLAYTEST_ENERGY_CAP = 1500.0f;

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
    Vector2 mouse = ColonyGetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, r);

    DrawRectangleRounded(r, 0.35f, 4, hover ? Color{20, 56, 96, 255} : Color{14, 30, 52, 255});
    DrawRectangleRoundedLinesEx(r, 0.35f, 4, 1.0f, accent);

    int labelW = MeasureText(label, 10);
    DrawText(label, static_cast<int>(r.x + (r.width - labelW) / 2.0f),
             static_cast<int>(r.y + (r.height - 10.0f) / 2.0f), 10, accent);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// The three named classes, in the colour key the panel and the design docs
// both use. Duplicated here rather than exported from the renderer because
// the playtest is a harness, not part of the game's UI.
static Color PlaytestClassColor(ResourceClass cls)
{
    switch (cls)
    {
        case ResourceClass::MEASURED:  return Color{ 80, 230, 150, 255};
        case ResourceClass::INDICATED: return Color{255, 200,  80, 255};
        case ResourceClass::INFERRED:  return Color{124, 143, 214, 255};
        default:                       return Color{ 70,  84, 104, 255};
    }
}

// Resource statement: per element, tonnage split by how well it is known.
//
// This is GetClassSplit() made visible. Without it the classification work is
// engine-implemented but not player-reachable -- you could not tell from the
// panel alone whether a sweep had actually converted anything.
static void PlaytestDrawStatement(Unit& unit, float x, float y, float w, float h)
{
    const ProspectingSystem* ps = unit.GetProspectingSystem();
    if (!ps) return;

    const ProspectingGrid& grid = ps->GetGrid();
    const SampleTray& tray = ps->GetTray();
    int tier = ps->GetTier();

    DrawRectangleRounded({x, y, w, h}, 0.06f, 4, Color{10, 14, 26, 235});
    DrawRectangleRoundedLinesEx({x, y, w, h}, 0.06f, 4, 1.0f, Color{30, 44, 66, 255});
    DrawText("RESOURCE STATEMENT", static_cast<int>(x + 10), static_cast<int>(y + 8),
             10, Color{80, 225, 255, 255});

    // Which elements this parent cell actually has. Centre of the lattice is
    // always in reach, so it is a safe probe at any tier.
    std::map<ResourceType, float> present =
        grid.GetGroundTruth(PROSPECTING_GRID_SIZE / 2, PROSPECTING_GRID_SIZE / 2,
                            DepthLayer::SURFACE);

    struct Row { ResourceType type; ClassSplit split; float total; };
    std::vector<Row> rows;
    for (const auto& kv : present)
    {
        ClassSplit split = GetClassSplit(grid, tray, kv.first, tier);
        float total = split.Total();
        if (total <= 0.0f) continue;
        rows.push_back({kv.first, split, total});
    }

    // Biggest deposits first -- the statement should lead with what matters.
    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) { return a.total > b.total; });

    // Only so many rows fit. Say how many were dropped rather than silently
    // truncating, which would read as "these are all of them".
    const size_t MAX_ROWS = 4;
    size_t hidden = rows.size() > MAX_ROWS ? rows.size() - MAX_ROWS : 0;
    if (rows.size() > MAX_ROWS) rows.resize(MAX_ROWS);

    float maxTotal = 0.0f;
    for (const Row& r : rows) maxTotal = std::max(maxTotal, r.total);
    if (maxTotal <= 0.0f) return;

    float rowY = y + 21.0f;
    const float rowH = 11.0f;
    const float labelW = 34.0f;
    const float barX = x + 10.0f + labelW;
    const float barMaxW = w - labelW - 76.0f;

    for (const Row& r : rows)
    {
        DrawText(ResourceTypeToString(r.type), static_cast<int>(x + 10),
                 static_cast<int>(rowY + 1), 10, Color{180, 198, 220, 255});

        // Bar length is tonnage, so a small deposit cannot look like a big one
        // just because it happens to be well surveyed.
        float barW = barMaxW * (r.total / maxTotal);
        float segX = barX;
        const ResourceClass order[4] = { ResourceClass::MEASURED, ResourceClass::INDICATED,
                                         ResourceClass::INFERRED, ResourceClass::UNCLASSIFIED };
        for (ResourceClass cls : order)
        {
            float segW = barW * (r.split.Get(cls) / r.total);
            if (segW <= 0.0f) continue;
            DrawRectangleRec({segX, rowY, segW, 7.0f}, PlaytestClassColor(cls));
            segX += segW;
        }
        DrawRectangleLinesEx({barX, rowY, barW, 7.0f}, 1.0f, Color{26, 34, 52, 255});

        // The number the player is actually trying to grow.
        float committablePct = 100.0f * r.split.Committable() / r.total;
        DrawText(TextFormat("%3.0f%%", committablePct),
                 static_cast<int>(barX + barMaxW + 8.0f), static_cast<int>(rowY - 1), 10,
                 committablePct > 0.5f ? Color{80, 230, 150, 255} : Color{70, 84, 104, 255});

        rowY += rowH;
    }

    DrawText(hidden > 0 ? TextFormat("%% = measured + indicated   (+%d more)",
                                     static_cast<int>(hidden))
                        : "% = measured + indicated",
             static_cast<int>(x + 10), static_cast<int>(y + h - 12.0f), 9,
             Color{90, 106, 130, 255});
}

static void UpdateDrawFrame(void* arg)
{
    PlaytestContext& ctx = *static_cast<PlaytestContext*>(arg);

    float deltaTime = GetFrameTime();
    ctx.timeManager->Update(deltaTime);
    ctx.unit->Update(deltaTime);

    // Sandbox energy supply, capped
    if (ctx.unit->GetStoredResource(ResourceType::ENERGY) < PLAYTEST_ENERGY_CAP)
    {
        ctx.unit->AddResource(ResourceType::ENERGY, PLAYTEST_ENERGY_PER_SECOND * deltaTime);
    }

    bool wantTierUp = IsKeyPressed(KEY_T);
    bool wantReset = IsKeyPressed(KEY_R);
    bool wantDig = IsKeyPressed(KEY_D);

    BeginDrawing();
    ClearBackground(BLACK);
    // Everything draws in 1280x720 logical space; the matrix carries it into
    // the (possibly supersampled) buffer. Scissors don't ride the matrix --
    // RenderManager scales those itself via SetPixelScale.
    rlPushMatrix();
    if (ctx.renderScale != 1)
    {
        rlScalef(static_cast<float>(ctx.renderScale),
                 static_cast<float>(ctx.renderScale), 1.0f);
    }
    ctx.renderManager->DrawUnitView(ctx.unit.get(), *ctx.timeManager);

    // On-screen controls (touch-friendly), tucked into the top bar
    float bx = ctx.screenWidth - 460.0f;
    wantTierUp |= PlaytestButton({bx, 14.0f, 80.0f, 28.0f}, "TIER UP", {80, 225, 255, 255});
    wantReset |= PlaytestButton({bx + 88.0f, 14.0f, 70.0f, 28.0f}, "RESET", {255, 200, 80, 255});
    wantDig |= PlaytestButton({bx + 166.0f, 14.0f, 80.0f, 28.0f}, "DIG SPOT",
                              {80, 230, 150, 255});

    // Drawn after the panel, in the empty strip below the module list. Sized
    // to clear the DIRECTIVES card above it and the panel border below.
    PlaytestDrawStatement(*ctx.unit, 18.0f, 497.0f, 250.0f, 80.0f);

    rlPopMatrix();
    EndDrawing();
    ctx.frame++;

    if (wantTierUp)
    {
        int idx = FindProspectingModule(*ctx.unit);
        if (idx >= 0) ctx.unit->DebugUpgradeModuleTier(idx);
    }
    // Sandbox shortcut for what the excavation module will do properly: dig
    // the selected spot so its class flips to MEASURED, and watch the
    // statement bar move. Digging is direct observation, so it is the fastest
    // way to feel the difference between knowing and guessing.
    if (wantDig)
    {
        ProspectingSystem* ps = ctx.unit->GetProspectingSystem();
        if (ps && ps->selectedCellX >= 0 && ps->selectedCellY >= 0)
        {
            ProspectingGrid& grid = ps->GetGrid();
            if (grid.IsInReach(ps->selectedCellX, ps->selectedCellY))
            {
                grid.RecordExcavation(ps->selectedCellX, ps->selectedCellY,
                                      ps->selectedDepth, 1.0f);
                ctx.unit->PublicShowMessage(
                    TextFormat("[PLAYTEST] Dug (%d,%d) - that layer is now MEASURED",
                               ps->selectedCellX, ps->selectedCellY));
            }
            else
            {
                ctx.unit->PublicShowMessage("[PLAYTEST] That spot is out of reach");
            }
        }
        else
        {
            ctx.unit->PublicShowMessage("[PLAYTEST] Select a grid cell first");
        }
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
#ifdef __EMSCRIPTEN__
    // Choose the supersample before the window exists, and tell the shell
    // (SHELL v6 reads __colonyBufW/H to pin the framebuffer, and
    // __colonyLogicalW/H to keep pointer coordinates in layout space).
    {
        int devW = EM_ASM_INT({
            return Math.round(document.documentElement.clientWidth
                              * (window.devicePixelRatio || 1));
        });
        int devH = EM_ASM_INT({
            return Math.round(document.documentElement.clientHeight
                              * (window.devicePixelRatio || 1));
        });
        float fit = std::min(devW / static_cast<float>(ctx.screenWidth),
                             devH / static_cast<float>(ctx.screenHeight));
        ctx.renderScale = fit > 1.05f ? 2 : 1;
        EM_ASM({
            window.__colonyBufW = $0; window.__colonyBufH = $1;
            window.__colonyLogicalW = $2; window.__colonyLogicalH = $3;
        }, ctx.screenWidth * ctx.renderScale, ctx.screenHeight * ctx.renderScale,
           ctx.screenWidth, ctx.screenHeight);
    }
#endif
    InitWindow(ctx.screenWidth * ctx.renderScale,
               ctx.screenHeight * ctx.renderScale,
               "Colony - Prospecting Playtest");
    SetTargetFPS(60);

    {
        RenderManager renderManager(ctx.screenWidth, ctx.screenHeight);
        renderManager.SetPixelScale(static_cast<float>(ctx.renderScale));
        renderManager.LoadFonts();

        // The constructor only allocates the grids; Planet normally calls this
        // to populate them. Without it the whole map is empty and every sample
        // reads 0% richness.
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

        // Unit (and its GPU-facing state) must die while the GL context lives
        ctx.unit.reset();
    }

    CloseWindow();
    return 0;
}
