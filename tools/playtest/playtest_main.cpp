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

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "raylib.h"
#include "rlgl.h"
#include "display_scale.h"
#include "web_mouse.h"

#include "rendermanager.h"
#include "unit.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "game_constants.h"
#include "prospecting_system.h"
#include "prospecting_grid.h"
#include "resource_types.h"

// Which build is this? Set from the git short SHA at configure time (see
// src/CMakeLists.txt). Drawn in the corner so "did my change deploy?" is a
// glance, not a round trip -- a browser can serve a stale wasm while every
// CI step is green.
#ifndef COLONY_BUILD_ID
#define COLONY_BUILD_ID "dev"
#endif

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
    // The buffer is 1x-3x the layout, chosen and followed by
    // src/display_scale.h; everything here stays in 1280x720 logical space.
    int frame = 0;
    // --scale-to N: natively, change the scale at frame 20 -- the same
    // DisplayScale_Apply a web resize goes through (0 = never).
    int scaleTo = 0;
    // --holes N: start with N finished holes, drilled by the console's own
    // drill (SurveyDash_DrillNow), so the fog, barrels and logs can be
    // played with without drilling them by hand first.
    int seedHoles = 0;
    // --land-at F: at frame F a hole lands at (0.72, 0.30), 90 m, with its
    // reveal -- the model taking it in -- so the animation can be captured
    // without drilling at a software renderer's frame rate.
    int landAt = 0;
    const char* shotPath = nullptr;
    bool statementOpen = false;
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
    // The deep plates rest dim and light up under the pointer. On a phone
    // there is no pointer, and the behaviour survives only because the shell
    // keeps publishing the last touch position after the finger lifts -- so
    // a tap IS the hover. That is worth saying out loud on the one build
    // people actually use on a phone; an unreachable feature is not a
    // feature (docs/guides/feature-completeness.md).
    unit->PublicShowMessage("[PLAYTEST] Tap the block to site a hole, pull down for a depth, "
                            "then tap the drill bar to dig. RESOURCE opens the statement.");
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
    // Outside the frame: following a resize changes the buffer.
    DisplayScale_Poll(deltaTime);
    if (ctx.scaleTo > 0 && ctx.frame == 20) DisplayScale_Apply(ctx.scaleTo);
    if (ctx.landAt > 0 && ctx.frame == ctx.landAt)
    {
        SurveyDash_DrillNow(&ctx.unit->GetProspectingSystem()->Dash(), 0.72f, 0.30f, 90.0f, true);
        // in the log, so a capture can be lined up with it
        TraceLog(LOG_WARNING, "PLAYTEST: hole landed at frame %d", ctx.frame);
    }
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
    // Everything draws in 1280x720 logical space; the display scale carries
    // it into the (possibly bigger) buffer.
    DisplayScale_BeginFrame();
    ctx.renderManager->DrawUnitView(ctx.unit.get(), *ctx.timeManager);

    // On-screen controls (touch-friendly), tucked into the top bar
    float bx = ctx.screenWidth - 460.0f;
    wantTierUp |= PlaytestButton({bx, 14.0f, 80.0f, 28.0f}, "TIER UP", {80, 225, 255, 255});
    wantReset |= PlaytestButton({bx + 88.0f, 14.0f, 70.0f, 28.0f}, "RESET", {255, 200, 80, 255});
    wantDig |= PlaytestButton({bx + 166.0f, 14.0f, 80.0f, 28.0f}, "DIG SPOT",
                              {80, 230, 150, 255});

    /* DEBUG DIALS, playtest only. DRILL multiplies how fast the bit cuts:
       at x40 the start and two taps take it about half way down the 120 m
       column (measured). KNOW multiplies how much each hole teaches: at x3 a
       hole weighs as three at the same spot, so the fog clears and the
       delineation climbs faster, in the same shape. F6 / F7 cycle them too. */
    {
        static const float kSpeed[4] = {1.0f, 4.0f, 16.0f, 40.0f};
        static const float kGain[3] = {1.0f, 2.0f, 3.0f};
        const Color dbg = {230, 120, 255, 255};
        const char* sl = TextFormat("DRILL x%d", static_cast<int>(DrillSim_Speed()));
        if (PlaytestButton({bx - 196.0f, 14.0f, 88.0f, 28.0f}, sl, dbg) || IsKeyPressed(KEY_F6))
        {
            int i = 0;
            while (i < 4 && kSpeed[i] != DrillSim_Speed()) i++;
            DrillSim_SetSpeed(kSpeed[(i + 1) % 4]);
        }
        const char* gl = TextFormat("KNOW x%d", static_cast<int>(DashKnow_DebugGain()));
        if (PlaytestButton({bx - 102.0f, 14.0f, 88.0f, 28.0f}, gl, dbg) || IsKeyPressed(KEY_F7))
        {
            int i = 0;
            while (i < 3 && kGain[i] != DashKnow_DebugGain()) i++;
            DashKnow_SetDebugGain(kGain[(i + 1) % 3]);
            // the ground re-fits from the model on a revision change, so the
            // beds answer the new gain now rather than at the next hole
            if (ProspectingSystem* ps = ctx.unit->GetProspectingSystem())
                ps->Dash().own.revision++;
        }
    }

    /* THE RESOURCE STATEMENT, FOLDED. It used to sit in the strip below the
       module list, which the survey console does not have -- the console is
       full width now, so the old slot lands on top of the tool rack's bays.
       This is the decision the console itself already made and wrote down
       (SurveyConsole::resourceOverlay): the statement is the module's SCORE,
       read BETWEEN decisions rather than during one, so it folds behind a
       chip and opens over the console when you ask for it.

       The chip lives in the letterbox bar, which is the one part of the
       screen the console does not use. */
    if (PlaytestButton({10.0f, 300.0f, 116.0f, 26.0f}, "RESOURCE",
                       ctx.statementOpen ? Color{80, 230, 150, 255}
                                         : Color{80, 225, 255, 255}))
    {
        ctx.statementOpen = !ctx.statementOpen;
    }
    if (ctx.statementOpen)
        PlaytestDrawStatement(*ctx.unit, 18.0f, 430.0f, 250.0f, 96.0f);

    // Build stamp, bottom-right: the git SHA this binary was configured
    // from, plus the live framebuffer, so a screenshot answers both "which
    // build is this?" and "did the canvas fit as intended?".
    {
        const char* stamp = TextFormat("BUILD %s   %dx%d", COLONY_BUILD_ID,
                                       GetScreenWidth(), GetScreenHeight());
        int sw = MeasureText(stamp, 10);
        DrawText(stamp, ctx.screenWidth - sw - 10, ctx.screenHeight - 15, 10,
                 Color{90, 110, 130, 255});
    }

    DisplayScale_EndFrame();
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
        if (std::string(argv[i]) == "--scale-to") ctx.scaleTo = std::atoi(argv[i + 1]);
        if (std::string(argv[i]) == "--holes") ctx.seedHoles = std::atoi(argv[i + 1]);
        if (std::string(argv[i]) == "--land-at") ctx.landAt = std::atoi(argv[i + 1]);
    }

    SetTraceLogLevel(LOG_WARNING);
    // Web: the step comes from the display; natively from --scale N.
    DisplayScale_Init(ctx.screenWidth, ctx.screenHeight, argc, argv);
    InitWindow(DisplayScale_BufferW(), DisplayScale_BufferH(),
               "Colony - Prospecting Playtest");
    DisplayScale_AfterWindow();
    SetTargetFPS(60);

    {
        RenderManager renderManager(ctx.screenWidth, ctx.screenHeight);
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
        if (ctx.seedHoles > 0)
        {
            // spread over the block, at varied depths, as the preview seeds
            static const float kDepth[9] = {120.0f, 70.0f, 100.0f, 45.0f, 120.0f,
                                            85.0f, 30.0f, 110.0f, 60.0f};
            const int side = ctx.seedHoles <= 1 ? 1 : (ctx.seedHoles <= 4 ? 2 : 3);
            int placed = 0;
            for (int a = 0; a < side && placed < ctx.seedHoles; a++)
                for (int b = 0; b < side && placed < ctx.seedHoles; b++, placed++)
                    SurveyDash_DrillNow(&ctx.unit->GetProspectingSystem()->Dash(),
                                        (a + 0.5f) / side, (b + 0.5f) / side, kDepth[placed % 9], false);
        }

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
