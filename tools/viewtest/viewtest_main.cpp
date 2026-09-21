// View-ladder playtest: Orbital -> Colony -> Sect.
//
// Walks the game's real geographic views with the real RenderManager, and
// overlays the KNOWN ISSUES for whichever view you are looking at. The
// annotations are playtest-only commentary — nothing here ships in the game.
//
// Controls
//   click / tap, or Down arrow   descend one view
//   Esc / right-click / Up arrow ascend one view
//   1 2 3                        jump to Orbital / Colony / Sect
//   I                            toggle the issue overlay
//   R                            move the colony to the next real place
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
#include "region_identity.h"
#include "lunar_dem_shared.h"
#include "game_constants.h"
#include "game_enums.h"
#include "game_structs.h"
#include "terrain_synthesis.h"
#include "lunar_frame.h"
#include "lunar_globe.h"

#include <cstdlib>
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
            "ORBITAL VIEW", "whole moon, ~3,476 km sphere",
            {
                {"OK",   "Real LROC WAC imagery, wrapped to a sphere."},
                {"OK",   "MOVE over the moon: a ghost box tracks the"},
                {"OK",   "  cursor and names the region under it."},
                {"OK",   "CLICK to propose a site; YES founds the colony"},
                {"OK",   "  there and descends. NO / right-click cancels."},
                {"OK",   "Colonies are marked at their real places;"},
                {"OK",   "  clicking a marker opens that colony."},
                {"OK",   "DRAG turns the globe and the WHEEL zooms, so"},
                {"OK",   "  the far side is reachable too. Turning it is"},
                {"OK",   "  not a click: a drag never proposes a site."},
                {"TODO", "This click is the founding STUB: the survey"},
                {"TODO", "  descent (district, site, cards, verdict)"},
                {"TODO", "  replaces it - site-selection plan, Part B."},
                {"GAP",  "Zoom stops at x8 - past that the WAC mosaic"},
                {"GAP",  "  (~1.3 km/px) has nothing left to resolve."},
            }};
    case 1:
        return {
            "COLONY VIEW", "25 km window on the colony's centre",
            {
                {"OK",   "GENERATED ground - level 1 of the chain at the"},
                {"OK",   "  colony's real place, one zoom step above"},
                {"OK",   "  the sect."},
                {"OK",   "Drawn in the colony's own frame: origin at its"},
                {"OK",   "  centre, 1 unit = 50 m, so sects sit on the"},
                {"OK",   "  ground they actually occupy."},
                {"OK",   "Each sect's chain is prefetched, so opening"},
                {"OK",   "  one does not hitch."},
                {"OK",   "Ctrl+click founds a sect: 5 km spacing, inside"},
                {"OK",   "  the window, not in another colony's land."},
                {"TODO", "No district rung above this yet: the globe"},
                {"TODO", "  cuts straight to 25 km. Part B adds it."},
            }};
    default:
        return {
            "SECT VIEW", "5 km footprint - the ground you build on",
            {
                {"OK",   "GENERATED ground - level 2, from real WAC"},
                {"OK",   "  pixels at the sect's true coordinates."},
                {"OK",   "Deterministic: same place = same ground, always."},
                {"OK",   "Press R to move the colony and watch it change."},
                {"GAP",  "Dome and units are flat screen-space discs;"},
                {"GAP",  "  they sit ON the image, not IN the terrain."},
                {"OK",   "Ground around the site is levelled off and"},
                {"OK",   "  worked: undulations plus alterations at"},
                {"OK",   "  each dome. --nodisturb shows it untouched."},
                {"GAP",  "No vehicle tracks between the units yet."},
                {"TODO", "LOLA DEM is 1.9 km/px, so a 5 km footprint"},
                {"TODO", "  spans ~2.6 DEM pixels: slopes are coarse."},
            }};
    }
}

// ---------------------------------------------------------------------------

struct ViewTestContext
{
    RenderManager* renderManager = nullptr;
    TimeManager* timeManager = nullptr;
    InputManager* inputManager = nullptr;
    Planet* planet = nullptr;
    Colony* colony = nullptr;
    Sect* sect = nullptr;
    std::vector<Colony*> colonies;

    int level = 0;              // 0 orbital, 1 colony, 2 sect
    bool showIssues = true;
    int placeStep = 0;
    bool picked = false;         // has the player chosen a place from orbit?
    bool pickPending = false;    // clicked a spot, waiting on confirmation
    double pendingLat = 0.0;     // the spot awaiting confirmation
    double pendingLon = 0.0;
    double pickLat = 9.6;        // Copernicus - reads at every scale
    double pickLon = -20.0;
    Camera2D camera = {0};
    bool headless = false;
    std::string shotPrefix;
};

static ViewTestContext g_ctx;

// Real places R cycles through, so the ground visibly changes.
struct Place { const char* name; double lat, lon; };
static const Place PLACES[] = {
    { "Copernicus",     9.6,  -20.0 },
    { "Mare Imbrium",  32.8,  -15.6 },
    { "Tycho",        -43.3,  -11.4 },
    { "Aristarchus",   23.7,  -47.4 },
    { "Tsiolkovskiy", -21.2,  128.9 },
    { "Shackleton",   -89.6,    0.0 },
};
static const int PLACE_COUNT = (int)(sizeof(PLACES) / sizeof(PLACES[0]));

// Turn the globe to face the chosen place, as the game does on the way
// back up from a colony, so its marker is in view.
static void FaceGlobeAt(double latDeg, double lonDeg)
{
    OrbitalCamera cam = GetOrbitalCamera();
    cam.subLatDeg = latDeg;
    cam.subLonDeg = lonDeg;
    SetOrbitalCamera(cam);
}

static void RebuildSect(ViewTestContext& ctx)
{
    // The colony owns whatever it is given: Colony::~Colony deletes its
    // sects. So the sect is built once, handed over, and moved after
    // that - never deleted here, and never replaced behind the colony's
    // back. Deleting it here left the colony holding a freed pointer,
    // which it then freed again on the way out.
    LunarPoint here;
    here.latDeg = ctx.pickLat;
    here.lonDeg = ctx.pickLon;
    if (ctx.sect == nullptr)
    {
        ctx.sect = new Sect(here, ctx.planet->GetResourceManager(), *ctx.timeManager);
        if (ctx.colony)
        {
            ctx.colony->AddSect(ctx.sect);
        }
    }
    else
    {
        ctx.sect->SetPoint(here);
    }
    // The colony's window follows its one sect, so the colony view is
    // centred on the same ground the sect view shows.
    if (ctx.colony) ctx.colony->SetCentre(here);
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
    const char* names[3] = {"ORBITAL", "COLONY", "SECT"};
    const char* kms[3] = {"3,476 km", "25 km", "5 km"};

    int barH = 44;
    int y = VT_HEIGHT - barH;
    DrawRectangle(0, y, VT_WIDTH, barH, Color{8, 10, 16, 232});
    DrawRectangleLines(0, y, VT_WIDTH, barH, Color{60, 75, 105, 255});

    int x = 12;
    for (int i = 0; i < 3; i++)
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

    DrawText("click/tap = zoom in   Esc = out   I = notes   R = next place",
             x + 14, y + 16, 14, Color{140, 155, 180, 255});
}

// The colony's 25 km window, in pixels on the globe as drawn now.
static float OrbitalBoxRadius()
{
    float discR = (float)OrbitalDiscRadiusPx(VT_WIDTH, VT_HEIGHT);
    float boxR = (float)(COLONY_WINDOW_KM / 3476.0) * discR * 2.0f;
    return (boxR < 7.0f) ? 7.0f : boxR;
}

static void DrawWindowBox(float px, float py, Color line, float thick,
                          Color fill)
{
    float r = OrbitalBoxRadius();
    Rectangle box = {px - r, py - r, r * 2, r * 2};
    if (fill.a > 0) DrawRectangleRec(box, fill);
    DrawRectangleLinesEx(box, thick, line);
    // Tick marks either side, so the box reads as a reticle.
    DrawLineEx(Vector2{px - r - 12, py}, Vector2{px - r - 3, py}, thick, line);
    DrawLineEx(Vector2{px + r + 3, py}, Vector2{px + r + 12, py}, thick, line);
}

// Geometry of the confirmation prompt, shared by drawing and hit-testing
// so the buttons cannot drift apart from where they are drawn.
struct PickPrompt
{
    Rectangle panel;
    Rectangle yes;
    Rectangle no;
};

static PickPrompt ComputePickPrompt(float px, float py)
{
    const float w = 330.0f, h = 124.0f;
    float r = OrbitalBoxRadius();
    float x = px + r + 18.0f;
    float y = py - h / 2.0f;
    // Flip to the other side / clamp so the prompt always stays on screen.
    if (x + w > VT_WIDTH - 12.0f) x = px - r - 18.0f - w;
    if (x < 12.0f) x = 12.0f;
    if (y < 12.0f) y = 12.0f;
    if (y + h > VT_HEIGHT - 56.0f) y = VT_HEIGHT - 56.0f - h;

    const float bw = 124.0f, bh = 34.0f;
    PickPrompt p;
    p.panel = Rectangle{x, y, w, h};
    // Buttons sit above the hint line, which is inside the panel so it
    // stays readable over bright terrain.
    p.yes = Rectangle{x + 14.0f, y + h - bh - 30.0f, bw, bh};
    p.no = Rectangle{x + w - bw - 14.0f, y + h - bh - 30.0f, bw, bh};
    return p;
}

// Orbital site selection (the harness's own two-step version of the
// game's founding stub):
//   hover  - a translucent box tracks the cursor over the moon
//   click  - the box goes solid and a confirmation prompt appears
//   yes    - the colony is founded there and we descend
//   no / right-click - back to hovering
static void DrawOrbitalPickMarker(const ViewTestContext& ctx)
{
    if (ctx.level != 0) return;

    const Color gold = Color{255, 200, 100, 255};
    const Color ghost = Color{255, 220, 150, 150};

    if (!ctx.pickPending)
    {
        // Hovering: a translucent window box follows the cursor wherever
        // it is over the moon, naming the ground under it.
        Vector2 m = GetMousePosition();
        double lat, lon;
        if (OrbitalPickToLatLon(m.x, m.y, VT_WIDTH, VT_HEIGHT, &lat, &lon))
        {
            DrawWindowBox(m.x, m.y, ghost, 1.5f, Color{255, 220, 150, 28});
        }
        DrawText("move over the moon, click to choose where to found the colony",
                 20, VT_HEIGHT - 116, 17, Color{200, 210, 230, 255});
        return;
    }

    // Pending: the box is solid at the chosen spot, and the prompt asks
    // for confirmation right beside it.
    float px, py;
    if (!OrbitalLatLonToScreen(ctx.pendingLat, ctx.pendingLon,
                               VT_WIDTH, VT_HEIGHT, &px, &py))
    {
        return;
    }
    DrawWindowBox(px, py, gold, 3.0f, Color{255, 210, 130, 46});

    PickPrompt pr = ComputePickPrompt(px, py);

    // The cursor keeps its ghost box while the prompt is open, so it
    // reads as "you can still point somewhere else".
    Vector2 hm = GetMousePosition();
    if (!CheckCollisionPointRec(hm, pr.panel))
    {
        double hlat, hlon;
        if (OrbitalPickToLatLon(hm.x, hm.y, VT_WIDTH, VT_HEIGHT,
                                &hlat, &hlon))
        {
            DrawWindowBox(hm.x, hm.y, ghost, 1.5f, Color{255, 220, 150, 22});
        }
    }
    RegionIdentity region = IdentifyRegion(GetLunarDem(), ctx.pendingLat, ctx.pendingLon);
    DrawRectangleRec(pr.panel, Color{10, 12, 20, 238});
    DrawRectangleLinesEx(pr.panel, 2.0f, gold);
    DrawText("Found the colony here?",
             (int)pr.panel.x + 14, (int)pr.panel.y + 12, 18, RAYWHITE);
    DrawText(TextFormat("%+.2f, %+.2f   -   %s",
                        ctx.pendingLat, ctx.pendingLon, region.name),
             (int)pr.panel.x + 14, (int)pr.panel.y + 36, 14,
             Color{170, 190, 215, 255});

    Vector2 m = GetMousePosition();
    bool overYes = CheckCollisionPointRec(m, pr.yes);
    bool overNo = CheckCollisionPointRec(m, pr.no);

    DrawRectangleRec(pr.yes, overYes ? Color{40, 120, 70, 255}
                                     : Color{26, 60, 40, 255});
    DrawRectangleLinesEx(pr.yes, 1.5f, Color{110, 220, 140, 255});
    DrawText("YES", (int)pr.yes.x + 44, (int)pr.yes.y + 9, 17,
             Color{190, 245, 205, 255});

    DrawRectangleRec(pr.no, overNo ? Color{120, 45, 45, 255}
                                   : Color{56, 26, 26, 255});
    DrawRectangleLinesEx(pr.no, 1.5f, Color{255, 130, 120, 255});
    DrawText("NO", (int)pr.no.x + 50, (int)pr.no.y + 9, 17,
             Color{255, 195, 190, 255});

    DrawText("or click elsewhere to move it  -  right-click cancels",
             (int)pr.panel.x + 14,
             (int)(pr.panel.y + pr.panel.height) - 22, 13,
             Color{165, 180, 205, 255});
}

static void DrawSiteBadge(const ViewTestContext& ctx)
{
    if (ctx.level != 2) return;
    RegionIdentity region = IdentifyRegion(GetLunarDem(), ctx.pickLat, ctx.pickLon);
    int w = 360, h = 46;
    int x = 14, y = VT_HEIGHT - 44 - h - 10;
    DrawRectangle(x, y, w, h, Color{10, 12, 20, 225});
    DrawRectangleLines(x, y, w, h, Color{90, 110, 150, 255});
    DrawText(TextFormat("%s   generated terrain", region.name),
             x + 10, y + 7, 15, RAYWHITE);
    DrawText(TextFormat("real coords  %+.3f deg,  %+.3f deg", ctx.pickLat, ctx.pickLon),
             x + 10, y + 26, 13, Color{150, 200, 235, 255});
}

static void HandleInput(ViewTestContext& ctx)
{
    if (IsKeyPressed(KEY_ONE)) ctx.level = 0;
    if (IsKeyPressed(KEY_TWO)) ctx.level = 1;
    if (IsKeyPressed(KEY_THREE)) ctx.level = 2;
    if (IsKeyPressed(KEY_I)) ctx.showIssues = !ctx.showIssues;

    if (IsKeyPressed(KEY_R))
    {
        // Walk a set of real places so the terrain visibly changes.
        ctx.placeStep = (ctx.placeStep + 1) % PLACE_COUNT;
        ctx.pickLat = PLACES[ctx.placeStep].lat;
        ctx.pickLon = PLACES[ctx.placeStep].lon;
        ctx.picked = true;
        RebuildSect(ctx);
        FaceGlobeAt(ctx.pickLat, ctx.pickLon);
    }

    // In orbit the moon is a globe you can spin, so a press is not yet a
    // choice: commit on release, and only if the press did not turn it.
    // Below orbit a press is still the fastest thing to react to.
    bool descend = (ctx.level == 0
                    ? (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)
                       && !LunarGlobeWasDragged())
                    : IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
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

    // Orbital site selection is a two-step commit: the first click
    // proposes a spot, the prompt confirms it.
    if (ctx.level == 0)
    {
        if (ctx.pickPending)
        {
            float px, py;
            bool onDisc = OrbitalLatLonToScreen(ctx.pendingLat, ctx.pendingLon,
                                                VT_WIDTH, VT_HEIGHT, &px, &py);
            PickPrompt pr = ComputePickPrompt(px, py);

            bool cancel = ascend || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

            if (descend)
            {
                if (onDisc && CheckCollisionPointRec(m, pr.yes))
                {
                    ctx.pickLat = ctx.pendingLat;
                    ctx.pickLon = ctx.pendingLon;
                    ctx.picked = true;
                    ctx.pickPending = false;
                    RebuildSect(ctx);
                    ctx.level = 1;               // descend into it
                }
                else if (CheckCollisionPointRec(m, pr.no))
                {
                    cancel = true;
                }
                else if (CheckCollisionPointRec(m, pr.panel))
                {
                    // Clicking the prompt's own body does nothing, so a
                    // near-miss on a button is not read as a decision.
                }
                else
                {
                    // Changed your mind: a click anywhere else on the moon
                    // simply moves the proposal there — no need to answer
                    // the prompt first. Off the moon, it cancels.
                    double lat, lon;
                    if (OrbitalPickToLatLon(m.x, m.y, VT_WIDTH, VT_HEIGHT,
                                            &lat, &lon))
                    {
                        ctx.pendingLat = lat;
                        ctx.pendingLon = lon;
                    }
                    else
                    {
                        cancel = true;
                    }
                }
            }

            if (cancel) ctx.pickPending = false;
            descend = false;
            ascend = false;                      // prompt owns both clicks
        }
        else if (descend)
        {
            double lat, lon;
            if (OrbitalPickToLatLon(m.x, m.y, VT_WIDTH, VT_HEIGHT,
                                    &lat, &lon))
            {
                ctx.pendingLat = lat;
                ctx.pendingLon = lon;
                ctx.pickPending = true;          // ask before committing
            }
            descend = false;                     // a miss stays in orbit
        }
    }

    if (descend && ctx.level < 2) ctx.level++;
    if (ascend && ctx.level > 0)
    {
        ctx.level--;
        if (ctx.level == 0 && ctx.picked) FaceGlobeAt(ctx.pickLat, ctx.pickLon);
    }
}

// The colony view draws in the colony's own frame: origin at its centre,
// which RebuildSect keeps on the sect. Fit the 25 km window across the
// width so the ladder reads as a ladder.
static void ApplyViewCamera(ViewTestContext& ctx)
{
    if (ctx.level == 1)
    {
        ctx.camera.target = Vector2{0.0f, 0.0f};
        ctx.camera.zoom = VT_WIDTH / (float)(COLONY_WINDOW_KM * LOCAL_UNITS_PER_KM);
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
        // Once a place is chosen the colony stands there; before that the
        // harness draws its own reticle instead of a marker.
        if (ctx.picked)
            ctx.renderManager->DrawOrbitalView(ctx.colonies, ctx.colony);
        else
            ctx.renderManager->DrawOrbitalView();
        break;
    case 1:
        ctx.renderManager->DrawColonyView(ctx.camera, ctx.colony, ctx.planet,
                                          ctx.colonies, *ctx.inputManager,
                                          *ctx.timeManager);
        break;
    default:
        ctx.renderManager->DrawSectView(ctx.sect, *ctx.timeManager);
        break;
    }

    DrawOrbitalPickMarker(ctx);
    DrawSiteBadge(ctx);
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
        else if (a == "--nodisturb")
        {
            SetSiteDisturbanceEnabled(false);
        }
        else if (a == "--subfloor")
        {
            SetSubFloorEnabled(true);
        }
        else if (a == "--pick" && i + 1 < argc)
        {
            // --pick LAT,LON: land anywhere on the moon without clicking.
            std::string v = argv[++i];
            size_t sep = v.find(',');
            if (sep != std::string::npos)
            {
                g_ctx.pickLat = atof(v.substr(0, sep).c_str());
                g_ctx.pickLon = atof(v.substr(sep + 1).c_str());
            }
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
        Planet planet;
        planet.GenerateMap(20260813u);      // a fixed Moon: same ground every run
        Colony colony;

        g_ctx.renderManager = &renderManager;
        g_ctx.timeManager = &timeManager;
        g_ctx.inputManager = &inputManager;
        g_ctx.planet = &planet;
        g_ctx.colony = &colony;
        g_ctx.colonies.push_back(&colony);

        g_ctx.camera.target = {0.0f, 0.0f};
        g_ctx.camera.offset = {VT_WIDTH / 2.0f, VT_HEIGHT / 2.0f};
        g_ctx.camera.rotation = 0.0f;
        g_ctx.camera.zoom = 1.0f;

        RebuildSect(g_ctx);

        if (g_ctx.headless)
        {
            // One screenshot per view, for reviewing without a display.
            // The place is chosen (--pick or the default) so the
            // screenshots show the ladder from an actual selection.
            g_ctx.picked = true;
            RebuildSect(g_ctx);
            FaceGlobeAt(g_ctx.pickLat, g_ctx.pickLon);

            for (int lvl = 0; lvl < 3; lvl++)
            {
                g_ctx.level = lvl;
                DrawFrame(g_ctx);      // settle fonts/textures
                DrawFrame(g_ctx);
                Image shot = LoadImageFromScreen();
                const char* names[3] = {"orbital", "colony", "sect"};
                std::string path = g_ctx.shotPrefix + "_" + names[lvl] + ".png";
                ExportImage(shot, path.c_str());
                UnloadImage(shot);
                TraceLog(LOG_WARNING, "wrote %s", path.c_str());
            }

            // Two extra frames capturing the orbital selection states,
            // which need a cursor position and so cannot be reached by
            // the plain per-view sweep above.
            {
                g_ctx.level = 0;
                float hx, hy;
                OrbitalLatLonToScreen(g_ctx.pickLat, g_ctx.pickLon,
                                      VT_WIDTH, VT_HEIGHT, &hx, &hy);

                struct Demo { const char* name; bool pending; float mx, my; };
                // Hover: cursor out over the mare, box tracking it.
                // Confirm: spot proposed, cursor resting on YES.
                PickPrompt pr = ComputePickPrompt(hx, hy);
                Demo demos[2] = {
                    {"orbital_hover", false, hx + 150.0f, hy + 90.0f},
                    {"orbital_confirm", true, hx - 190.0f, hy + 130.0f},
                };

                for (const Demo& d : demos)
                {
                    g_ctx.pickPending = d.pending;
                    g_ctx.pendingLat = g_ctx.pickLat;
                    g_ctx.pendingLon = g_ctx.pickLon;
                    SetMousePosition((int)d.mx, (int)d.my);
                    DrawFrame(g_ctx);
                    SetMousePosition((int)d.mx, (int)d.my);
                    DrawFrame(g_ctx);
                    Image sh = LoadImageFromScreen();
                    std::string pth = g_ctx.shotPrefix + "_" + d.name + ".png";
                    ExportImage(sh, pth.c_str());
                    UnloadImage(sh);
                    TraceLog(LOG_WARNING, "wrote %s", pth.c_str());
                }
                g_ctx.pickPending = false;
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

        // The colony frees its sects; this pointer is only a handle.
        g_ctx.sect = nullptr;
    }

    CloseWindow();
    return status;
}
