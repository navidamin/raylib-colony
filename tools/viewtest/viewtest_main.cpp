// View-ladder playtest: Orbital -> Planet -> Colony -> Sect.
//
// Walks the game's real geographic views with the real RenderManager, and
// overlays the KNOWN ISSUES for whichever view you are looking at. The
// annotations are playtest-only commentary — nothing here ships in the game.
//
// Controls
//   click / tap, or Down arrow   descend one view
//   Esc / right-click / Up arrow ascend one view
//   1 2 3 4                      jump to Orbital / Planet / Colony / Sect
//   I                            toggle the issue overlay
//   R                            re-roll the sect's grid cell (new terrain)
//
// Build:  cmake --build build --target colony_viewtest
// Run:    tools/viewtest/viewtest.sh          (headless screenshots)
//         ./build/src/colony_viewtest         (interactive)

#include "raylib.h"

#include "rendermanager.h"
#include "planet.h"
#include "colony.h"
#include "sect.h"
#include "time_manager.h"
#include "inputmanager.h"
#include "resource_manager.h"
#include "game_constants.h"
#include "game_enums.h"
#include "terrain_synthesis.h"

#include <string>
#include <vector>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static const int VT_WIDTH = 1280;
static const int VT_HEIGHT = 720;

// ---------------------------------------------------------------------------
// The commentary. One block per view: what is real, and what is still wrong.
// ---------------------------------------------------------------------------

struct IssueNote
{
    const char* severity;   // "OK" | "GAP" | "TODO"
    const char* text;
};

struct ViewNotes
{
    const char* title;
    const char* scale;
    std::vector<IssueNote> notes;
};

static ViewNotes NotesForView(int level)
{
    switch (level)
    {
    case 0:
        return {
            "ORBITAL VIEW", "whole moon, ~3,476 km disc",
            {
                {"OK",   "Real LROC WAC imagery, wrapped to a sphere."},
                {"GAP",  "Static disc: no rotation, no drag-to-spin."},
                {"GAP",  "Not clickable - you cannot pick a landing"},
                {"GAP",  "  region here; descending is hardcoded."},
                {"TODO", "12 pre-baked rotation frames exist unused in"},
                {"TODO", "  src/assets/planet/orbital_rotation/."},
            }};
    case 1:
        return {
            "PLANET VIEW", "100 km, 20x20 cells of 5 km",
            {
                {"GAP",  "THE BIG ONE: this draws 3 shuffled tile"},
                {"GAP",  "  textures, NOT generated terrain."},
                {"GAP",  "Zooming in teleports you to unrelated ground -"},
                {"GAP",  "  nothing here matches the sect surface."},
                {"TODO", "Level 0 of the terrain chain IS this window"},
                {"TODO", "  (100 km) and is already computed, then"},
                {"TODO", "  thrown away. Reusing it costs zero compute."},
                {"TODO", "Needs world-space draw across 2000 units, and"},
                {"TODO", "  ~1024px res (512 = 195 m/px here)."},
                {"GAP",  "ViewManager sets NO zoom on view switch, so in"},
                {"GAP",  "  the real game Planet and Colony look identical"},
                {"GAP",  "  until you wheel. This test forces 100/25 km."},
            }};
    case 2:
        return {
            "COLONY VIEW", "25 km, 5x5 cells",
            {
                {"GAP",  "Same gap: no generated backdrop, so the"},
                {"GAP",  "  ground has no relation to the sect below."},
                {"TODO", "Level 1 of the chain IS this window (25 km),"},
                {"TODO", "  also computed and discarded."},
                {"TODO", "Must be centred on the COLONY, not the sect -"},
                {"TODO", "  a sect's chain is centred slightly off."},
                {"OK",   "Sect placement and roads work on the grid."},
            }};
    default:
        return {
            "SECT VIEW", "5 km cell - the one that works",
            {
                {"OK",   "Ground is GENERATED from real WAC pixels at"},
                {"OK",   "  this cell's true lunar coordinates."},
                {"OK",   "Deterministic: same cell = same ground, always."},
                {"OK",   "Press R to hop cells and watch terrain change."},
                {"GAP",  "Dome and units are flat screen-space discs;"},
                {"GAP",  "  they sit ON the image, not IN the terrain."},
                {"GAP",  "No vehicle tracks, no disturbed-regolith halo"},
                {"GAP",  "  (the concept-art look is not built)."},
                {"TODO", "LOLA DEM is 1.9 km/px, so a 5 km cell spans"},
                {"TODO", "  ~2.6 DEM pixels: slopes here are coarse."},
                {"TODO", "First entry costs a ~100 ms generation hitch."},
            }};
    }
}

// ---------------------------------------------------------------------------

struct ViewTestContext
{
    RenderManager* renderManager = nullptr;
    TimeManager* timeManager = nullptr;
    InputManager* inputManager = nullptr;
    ResourceManager* resourceManager = nullptr;
    Planet* planet = nullptr;
    Colony* colony = nullptr;
    Sect* sect = nullptr;
    std::vector<Colony*> colonies;

    int level = 0;              // 0 orbital, 1 planet, 2 colony, 3 sect
    bool showIssues = true;
    int cellX = 10;
    int cellY = 10;
    int cellStep = 0;
    Camera2D camera = {0};
    bool headless = false;
    std::string shotPrefix;
};

static ViewTestContext g_ctx;

static void RebuildSect(ViewTestContext& ctx)
{
    delete ctx.sect;
    Vector2 pos = {(ctx.cellX + 0.5f) * SECT_CORE_RADIUS * 2.0f,
                   (ctx.cellY + 0.5f) * SECT_CORE_RADIUS * 2.0f};
    ctx.sect = new Sect(pos, *ctx.resourceManager, *ctx.timeManager);
    if (ctx.colony && ctx.colony->GetSects().empty())
    {
        ctx.colony->AddSect(ctx.sect);
    }
}

static void DrawIssuePanel(const ViewTestContext& ctx)
{
    ViewNotes vn = NotesForView(ctx.level);

    const int pad = 12;
    const int lineH = 18;
    const int panelW = 470;
    int panelH = 78 + (int)vn.notes.size() * lineH + pad;
    int x = VT_WIDTH - panelW - 14;
    int y = 14;

    DrawRectangle(x, y, panelW, panelH, Color{10, 12, 20, 232});
    DrawRectangleLines(x, y, panelW, panelH, Color{90, 110, 150, 255});

    DrawRectangle(x, y, panelW, 26, Color{28, 36, 58, 255});
    DrawText("PLAYTEST NOTES  -  not shipped in game", x + pad, y + 6, 14,
             Color{150, 170, 210, 255});

    DrawText(vn.title, x + pad, y + 34, 20, RAYWHITE);
    DrawText(vn.scale, x + pad, y + 58, 14, Color{150, 170, 210, 255});

    int ly = y + 82;
    for (const IssueNote& n : vn.notes)
    {
        Color c = Color{200, 200, 210, 255};
        if (TextIsEqual(n.severity, "OK")) c = Color{110, 220, 140, 255};
        else if (TextIsEqual(n.severity, "GAP")) c = Color{255, 130, 120, 255};
        else if (TextIsEqual(n.severity, "TODO")) c = Color{240, 200, 110, 255};

        // Continuation lines (starting with spaces) get no badge.
        if (n.text[0] != ' ')
        {
            DrawText(n.severity, x + pad, ly, 13, c);
        }
        DrawText(n.text, x + pad + 44, ly, 14, Color{225, 228, 235, 255});
        ly += lineH;
    }
}

static void DrawNavBar(const ViewTestContext& ctx)
{
    const char* names[4] = {"ORBITAL", "PLANET", "COLONY", "SECT"};
    const char* kms[4] = {"3,476 km", "100 km", "25 km", "5 km"};

    int barH = 44;
    int y = VT_HEIGHT - barH;
    DrawRectangle(0, y, VT_WIDTH, barH, Color{8, 10, 16, 232});
    DrawRectangleLines(0, y, VT_WIDTH, barH, Color{60, 75, 105, 255});

    int x = 12;
    for (int i = 0; i < 4; i++)
    {
        bool active = (i == ctx.level);
        int w = 150;
        DrawRectangle(x, y + 7, w, barH - 14,
                      active ? Color{40, 90, 130, 255} : Color{22, 26, 38, 255});
        DrawRectangleLines(x, y + 7, w, barH - 14,
                           active ? Color{120, 200, 235, 255}
                                  : Color{60, 70, 95, 255});
        DrawText(TextFormat("%d %s", i + 1, names[i]), x + 10, y + 12, 15,
                 active ? RAYWHITE : Color{150, 160, 180, 255});
        DrawText(kms[i], x + 10, y + 27, 11,
                 active ? Color{170, 220, 245, 255} : Color{110, 120, 140, 255});
        x += w + 8;
    }

    DrawText("click/tap = zoom in   Esc = out   I = notes   R = new cell",
             x + 14, y + 16, 14, Color{140, 155, 180, 255});
}

static void DrawSectCellBadge(const ViewTestContext& ctx)
{
    if (ctx.level != 3) return;
    double lat, lon;
    TerrainGridCellToLatLon(ctx.cellX, ctx.cellY, &lat, &lon);
    int w = 330, h = 46;
    int x = 14, y = VT_HEIGHT - 44 - h - 10;
    DrawRectangle(x, y, w, h, Color{10, 12, 20, 225});
    DrawRectangleLines(x, y, w, h, Color{90, 110, 150, 255});
    DrawText(TextFormat("CELL (%d, %d)   generated terrain", ctx.cellX,
                        ctx.cellY), x + 10, y + 7, 15, RAYWHITE);
    DrawText(TextFormat("real coords  %+.3f deg,  %+.3f deg", lat, lon),
             x + 10, y + 26, 13, Color{150, 200, 235, 255});
}

static void HandleInput(ViewTestContext& ctx)
{
    if (IsKeyPressed(KEY_ONE)) ctx.level = 0;
    if (IsKeyPressed(KEY_TWO)) ctx.level = 1;
    if (IsKeyPressed(KEY_THREE)) ctx.level = 2;
    if (IsKeyPressed(KEY_FOUR)) ctx.level = 3;
    if (IsKeyPressed(KEY_I)) ctx.showIssues = !ctx.showIssues;

    if (IsKeyPressed(KEY_R))
    {
        // Walk a diagonal of distinct cells so the terrain visibly changes.
        static const int cells[6][2] = {{10, 10}, {6, 6}, {14, 5},
                                        {3, 12}, {17, 16}, {8, 15}};
        ctx.cellStep = (ctx.cellStep + 1) % 6;
        ctx.cellX = cells[ctx.cellStep][0];
        ctx.cellY = cells[ctx.cellStep][1];
        RebuildSect(ctx);
    }

    bool descend = IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
                   || IsKeyPressed(KEY_DOWN);
    bool ascend = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_UP)
                  || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    // Clicks inside the note panel or nav bar should not navigate.
    Vector2 m = GetMousePosition();
    if (descend && (m.y > VT_HEIGHT - 44
                    || (ctx.showIssues && m.x > VT_WIDTH - 500 && m.y < 320)))
    {
        descend = false;
    }

    if (descend && ctx.level < 3) ctx.level++;
    if (ascend && ctx.level > 0) ctx.level--;
}

// The game's ViewManager does NOT set a zoom when switching views, so
// Planet and Colony render at identical scale until the player wheels.
// Here we set the documented scales so the ladder reads as a ladder:
// PLANET shows the whole 20x20 grid (100 km), COLONY a 5x5 block (25 km).
static void ApplyViewCamera(ViewTestContext& ctx)
{
    float cellUnits = SECT_CORE_RADIUS * 2.0f;             // 100 units = 5 km
    if (ctx.level == 1)
    {
        ctx.camera.target = {PLANET_WIDTH / 2.0f, PLANET_HEIGHT / 2.0f};
        ctx.camera.zoom = VT_HEIGHT / (PLANET_SIZE * cellUnits);   // 100 km
    }
    else if (ctx.level == 2)
    {
        Vector2 p = ctx.sect ? ctx.sect->GetPosition()
                             : Vector2{PLANET_WIDTH / 2.0f, PLANET_HEIGHT / 2.0f};
        ctx.camera.target = p;
        ctx.camera.zoom = VT_HEIGHT / (5.0f * cellUnits);          // 25 km
    }
    ctx.camera.offset = {VT_WIDTH / 2.0f, VT_HEIGHT / 2.0f};
}

static void DrawFrame(ViewTestContext& ctx)
{
    ApplyViewCamera(ctx);
    BeginDrawing();
    ClearBackground(BLACK);

    switch (ctx.level)
    {
    case 0:
        ctx.renderManager->DrawOrbitalView();
        break;
    case 1:
        ctx.renderManager->DrawPlanetView(ctx.camera, ctx.planet, ctx.colonies,
                                          *ctx.inputManager, *ctx.timeManager);
        break;
    case 2:
        ctx.renderManager->DrawColonyView(ctx.camera, ctx.colony, ctx.planet,
                                          ctx.colonies, *ctx.inputManager,
                                          *ctx.timeManager);
        break;
    default:
        ctx.renderManager->DrawSectView(ctx.sect, *ctx.timeManager);
        break;
    }

    DrawSectCellBadge(ctx);
    if (ctx.showIssues) DrawIssuePanel(ctx);
    DrawNavBar(ctx);

    EndDrawing();
}

static void UpdateFrame(void* arg)
{
    ViewTestContext& ctx = *(ViewTestContext*)arg;
    HandleInput(ctx);
    ctx.timeManager->Update(GetFrameTime());
    DrawFrame(ctx);
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        if (a == "--shots" && i + 1 < argc)
        {
            g_ctx.headless = true;
            g_ctx.shotPrefix = argv[++i];
        }
    }

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(VT_WIDTH, VT_HEIGHT, "Colony - View Ladder Playtest");

    int status = 0;
    {
        RenderManager renderManager(VT_WIDTH, VT_HEIGHT);
        renderManager.LoadFonts();
        TimeManager timeManager;
        InputManager inputManager;
        ResourceManager resourceManager(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
        resourceManager.GenerateResourceMap(20260813u);
        Planet planet;
        Colony colony;

        g_ctx.renderManager = &renderManager;
        g_ctx.timeManager = &timeManager;
        g_ctx.inputManager = &inputManager;
        g_ctx.resourceManager = &resourceManager;
        g_ctx.planet = &planet;
        g_ctx.colony = &colony;
        g_ctx.colonies.push_back(&colony);

        g_ctx.camera.target = {PLANET_WIDTH / 2.0f, PLANET_HEIGHT / 2.0f};
        g_ctx.camera.offset = {VT_WIDTH / 2.0f, VT_HEIGHT / 2.0f};
        g_ctx.camera.rotation = 0.0f;
        g_ctx.camera.zoom = 1.0f;

        RebuildSect(g_ctx);

        if (g_ctx.headless)
        {
            // One screenshot per view, for reviewing without a display.
            for (int lvl = 0; lvl < 4; lvl++)
            {
                g_ctx.level = lvl;
                DrawFrame(g_ctx);      // settle fonts/textures
                DrawFrame(g_ctx);
                Image shot = LoadImageFromScreen();
                const char* names[4] = {"orbital", "planet", "colony", "sect"};
                std::string path = g_ctx.shotPrefix + "_" + names[lvl] + ".png";
                ExportImage(shot, path.c_str());
                UnloadImage(shot);
                TraceLog(LOG_WARNING, "wrote %s", path.c_str());
            }
        }
        else
        {
#if defined(PLATFORM_WEB)
            emscripten_set_main_loop_arg(UpdateFrame, &g_ctx, 0, 1);
#else
            SetTargetFPS(60);
            while (!WindowShouldClose())
            {
                UpdateFrame(&g_ctx);
            }
#endif
        }

        delete g_ctx.sect;
        g_ctx.sect = nullptr;
    }

    CloseWindow();
    return status;
}
