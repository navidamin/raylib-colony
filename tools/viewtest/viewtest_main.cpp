// Game walk: the level ladder as the game runs it (Globe -> District ->
// Site), then the founded colony's own views (Colony -> Sect). Only the
// first three are levels; Colony and Sect are where a colony is managed.
//
// Walks the game's real geographic views with the real RenderManager and
// the real descent (SurveyFlow), and overlays the KNOWN ISSUES for
// whichever view you are looking at. The annotations are playtest-only
// commentary — nothing here ships in the game.
//
// Controls
//   the descent's own: hover names a region, click claims / descends /
//   founds, Esc / right-click / BACK goes up a rung; right-click AT the
//   globe turns its spin on or off
//   click / Down arrow            colony -> sect
//   Esc / right-click / Up arrow  sect -> colony -> up the descent
//   1 3 4                         jump to the globe / the colony / the sect
//   I                             toggle the issue overlay
//   R                             turn the globe to the next real place
//
// Build:  cmake --build build --target colony_viewtest
// Run:    tools/viewtest/viewtest.sh          (headless screenshots)
//         ./build/src/colony_viewtest         (interactive)

#include "relief.h"
#include "raylib.h"

#include "rendermanager.h"
#include "gamemanager.h"
#include "planet.h"
#include "colony.h"
#include "sect.h"
#include "time_manager.h"
#include "inputmanager.h"
#include "region_identity.h"
#include "lunar_dem_shared.h"
#include "game_constants.h"
#include "game_enums.h"
#include "game_structs.h"
#include "terrain_synthesis.h"
#include "lunar_frame.h"
#include "lunar_globe.h"
#include "survey_flow.h"
#include "survey_script.h"
#include "build_stamp.h"

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

static ViewNotes NotesForView(int level, bool hasColony)
{
    switch (level)
    {
    case 0:
        return {
            "ORBITAL VIEW", "whole moon, ~3,476 km sphere",
            {
                {"OK",   "Real LROC WAC imagery, wrapped to a sphere."},
                {"OK",   "MOVE over the moon: the region under the"},
                {"OK",   "  cursor lights up and names itself; the"},
                {"OK",   "  region card previews its economy."},
                {"OK",   "CLICK to claim it and fly to its district."},
                {"OK",   "Colonies are marked at their real places;"},
                {"OK",   "  clicking a marker opens that colony."},
                {"OK",   "DRAG turns the globe and the WHEEL zooms; a"},
                {"OK",   "  drag never claims."},
                {"OK",   "RIGHT-CLICK spins it on / off. It starts still,"},
                {"OK",   "  and comes back still and zoomed out (x1.3)."},
                {"OK",   "Claiming flies: the globe turns and zooms onto"},
                {"OK",   "  the district (--shots keeps 25/50/80 % frames)."},
                {"GAP",  "Zoom stops at x8 - past that the WAC mosaic"},
                {"GAP",  "  (~1.3 km/px) has nothing left to resolve."},
            }};
    case 1:
        if (GetDistrictStyle() == DistrictStyle::SUPERSAMPLED)
            return {
                "DISTRICT", "200 km window, 25 km cursor - supersampled",
                {
                    {"OK",   "The synthesizer, built at twice the screen's"},
                    {"OK",   "  width and averaged down - the same ground the"},
                    {"OK",   "  site level continues (?district=super)."},
                    {"OK",   "The cursor is the next rung's window: it"},
                    {"OK",   "  snaps to a 25 km grid; click descends."},
                    {"OK",   "Level card: the cursor's mean slope,"},
                    {"OK",   "  buildable fraction and relief from LOLA."},
                    {"OK",   "DRAG moves the window over the moon; the"},
                    {"OK",   "  ground is rebuilt there on release."},
                    {"GAP",  "Four times the build: about a second on a"},
                    {"GAP",  "  laptop GPU at each claim and drag release."},
                }};
        return {
            "DISTRICT", "200 km window, 25 km cursor",
            {
                {"OK",   "The moon's real relief (Kaguya stereo, 237 m)"},
                {"OK",   "  lit by the game's sun - WHICH MIX of ground."},
                {"OK",   "  No synthesized detail at this scale."},
                {"OK",   "The cursor is the next rung's window: it"},
                {"OK",   "  snaps to a 25 km grid; click descends."},
                {"OK",   "Level card: the cursor's mean slope,"},
                {"OK",   "  buildable fraction and relief from LOLA."},
                {"OK",   "DRAG moves the window over the moon; the"},
                {"OK",   "  ground is rebuilt there on release, and the"},
                {"OK",   "  region card re-labels if a border is crossed."},
                {"TODO", "Named-feature arcs are drawn at the rung's"},
                {"TODO", "  framing; zooming moves the ground, not them."},
            }};
    case 2:
        if (hasColony)
            return {
                "COLONY VIEW", "25 km window on the colony's centre",
                {
                    {"OK",   "The same 25 km texture the site rung showed:"},
                    {"OK",   "  nothing changed at the click."},
                    {"OK",   "Drawn in the colony's own frame: origin at"},
                    {"OK",   "  its centre, 1 unit = 50 m."},
                    {"OK",   "Ctrl+click founds a sect: 5 km spacing, inside"},
                    {"OK",   "  the window, not in another colony's land."},
                    {"OK",   "Esc goes back up the descent it came by."},
                }};
        return {
            "SITE RUNG", "25 km window, 1.5 km cursor = the base",
            {
                {"OK",   "WHICH GROUND: the cursor is the base's own"},
                {"OK",   "  footprint; green founds, red refuses and"},
                {"OK",   "  names the limit that failed."},
                {"OK",   "Verdict from real LOLA slope, roughness,"},
                {"OK",   "  relief and permanent shadow."},
                {"OK",   "DRAG moves the window; Esc comes back up to"},
                {"OK",   "  a district centred where you now are."},
                {"OK",   "Colonies already here are marked; landing"},
                {"OK",   "  on one opens it instead."},
                {"GAP",  "Past 80 deg the window smears (D7, measured"},
                {"GAP",  "  at Shackleton); claims there are refused."},
            }};
    default:
        return {
            "SECT VIEW", "5 km footprint - the ground you build on",
            {
                {"OK",   "GENERATED ground - level 2, from real WAC"},
                {"OK",   "  pixels at the sect's true coordinates."},
                {"OK",   "Deterministic: same place = same ground, always."},
                {"GAP",  "Dome and units are flat screen-space discs;"},
                {"GAP",  "  they sit ON the image, not IN the terrain."},
                {"OK",   "Ground around the site is levelled off and"},
                {"OK",   "  worked: undulations plus alterations at"},
                {"OK",   "  each dome. --nodisturb shows it untouched."},
                {"GAP",  "No vehicle tracks between the units yet."},
            }};
    }
}

// ---------------------------------------------------------------------------

struct ViewTestContext
{
    RenderManager* renderManager = nullptr;
    InputManager* inputManager = nullptr;
    GameManager* game = nullptr;
    SurveyFlow flow;

    int level = 0;              // 0 orbital, 1 district, 2 colony/site, 3 sect
    bool showIssues = true;
    int placeStep = 0;
    double pickLat = 32.8;      // Mare Imbrium: flat, green, reads at every scale
    double pickLon = -15.6;
    double aimDxKm = 30.0;      // where the cursor is aimed below the globe
    double aimDyKm = -20.0;
    Camera2D camera = {0};
    bool headless = false;
    bool surveyFrame = false;
    std::string shotPrefix;
};

static ViewTestContext g_ctx;

// Real places R turns the globe to.
struct Place { const char* name; double lat, lon; };
static const Place PLACES[] = {
    { "Mare Imbrium",  32.8,  -15.6 },
    { "Copernicus",     9.6,  -20.0 },
    { "Tycho",        -43.3,  -11.4 },
    { "Aristarchus",   23.7,  -47.4 },
    { "Tsiolkovskiy", -21.2,  128.9 },
    { "Shackleton",   -89.6,    0.0 },
};
static const int PLACE_COUNT = (int)(sizeof(PLACES) / sizeof(PLACES[0]));

static Rectangle NotesPanelRect(const ViewTestContext& ctx)
{
    ViewNotes vn = NotesForView(ctx.level, ctx.game->GetCurrentColony() != nullptr);
    const int pad = 12, lineH = 18, panelW = 470;
    int panelH = 78 + (int)vn.notes.size() * lineH + pad;
    // Bottom right, above the descent's prompt strip and below its level
    // card, so the commentary covers neither.
    return Rectangle{ (float)(VT_WIDTH - panelW - 14), (float)(VT_HEIGHT - 44 - panelH - 10),
                      (float)panelW, (float)panelH };
}

static void DrawIssuePanel(const ViewTestContext& ctx)
{
    ViewNotes vn = NotesForView(ctx.level, ctx.game->GetCurrentColony() != nullptr);
    Rectangle r = NotesPanelRect(ctx);
    const int pad = 12, lineH = 18;
    int x = (int)r.x, y = (int)r.y, panelW = (int)r.width, panelH = (int)r.height;

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
        if (n.text[0] != ' ') DrawText(n.severity, x + pad, ly, 13, c);
        DrawText(n.text, x + pad + 44, ly, 14, Color{225, 228, 235, 255});
        ly += lineH;
    }
}

// The rung bar, top centre: clear of the descent's header (left), its
// cards (both sides) and its prompt strip (bottom).
static Rectangle NavBarRect()
{
    return Rectangle{ 380.0f, 6.0f, 4 * 128.0f + 3 * 6.0f, 44.0f };
}

static void DrawNavBar(const ViewTestContext& ctx)
{
    const char* names[4] = {"ORBITAL", "DISTRICT", "COLONY", "SECT"};
    const char* kms[4] = {"3,476 km", "200 km", "25 km", "5 km"};
    Rectangle bar = NavBarRect();
    int x = (int)bar.x, y = (int)bar.y;
    for (int i = 0; i < 4; i++)
    {
        bool active = (i == ctx.level);
        int w = 128;
        DrawRectangle(x, y, w, (int)bar.height,
                      active ? Color{40, 90, 130, 235} : Color{22, 26, 38, 215});
        DrawRectangleLines(x, y, w, (int)bar.height,
                           active ? Color{120, 200, 235, 255} : Color{60, 70, 95, 255});
        DrawText(TextFormat("%d %s", i + 1, names[i]), x + 10, y + 7, 14,
                 active ? RAYWHITE : Color{150, 160, 180, 255});
        DrawText(kms[i], x + 10, y + 25, 11,
                 active ? Color{170, 220, 245, 255} : Color{110, 120, 140, 255});
        x += w + 6;
    }
}

static void DrawSiteBadge(const ViewTestContext& ctx)
{
    if (ctx.level != 3 || !ctx.game->GetCurrentSect()) return;
    const LunarPoint& p = ctx.game->GetCurrentSect()->GetPoint();
    RegionIdentity region = IdentifyRegion(GetLunarDem(), p.latDeg, p.lonDeg);
    int w = 360, h = 46;
    int x = 14, y = VT_HEIGHT - h - 10;
    DrawRectangle(x, y, w, h, Color{10, 12, 20, 225});
    DrawRectangleLines(x, y, w, h, Color{90, 110, 150, 255});
    DrawText(TextFormat("%s   generated terrain", region.name), x + 10, y + 7, 15, RAYWHITE);
    DrawText(TextFormat("real coords  %+.3f deg,  %+.3f deg", p.latDeg, p.lonDeg),
             x + 10, y + 26, 13, Color{150, 200, 235, 255});
}

// ---------------------------------------------------------------------------

static void FaceGlobeAt(double latDeg, double lonDeg)
{
    SurveyScript::FaceGlobe(latDeg, lonDeg);
}

static bool SurveyOwnsLevel(const ViewTestContext& ctx)
{
    return ctx.level <= 1 || (ctx.level == 2 && ctx.game->GetCurrentColony() == nullptr);
}

// The view follows the rung, as Engine::SyncViewToRung does.
static void SyncLevel(ViewTestContext& ctx)
{
    const SiteSelectionController& ctl = ctx.flow.Controller();
    switch (ctl.Level())
    {
    case 0: ctx.level = 0; break;
    case 1: ctx.level = 1; break;
    default:
    {
        Colony* here = ctx.game->ColonyInWindow(ctx.flow.WindowCentre(), ctx.flow.WindowSpanKm());
        ctx.game->SetCurrentColony(here);
        ctx.level = 2;
        break;
    }
    }
}

static void ApplyFrame(ViewTestContext& ctx, const SurveyFlow::Frame& f)
{
    if (f.openedColony)
    {
        ctx.game->SetCurrentColony(f.openedColony);
        ctx.level = 2;
        return;
    }
    if (f.founded)
    {
        Colony* c = ctx.game->FoundColony(f.foundPoint, f.windowCentre, &f.region);
        if (c) { ctx.level = 2; }
        else   { ctx.level = 0; FaceGlobeAt(f.foundPoint.latDeg, f.foundPoint.lonDeg); }
        return;
    }
    if (f.rungChanged) SyncLevel(ctx);
}

static void HandleInput(ViewTestContext& ctx)
{
    if (IsKeyPressed(KEY_I)) ctx.showIssues = !ctx.showIssues;
    if (IsKeyPressed(KEY_ONE)) { ctx.flow.Reset(); ctx.level = 0; }
    if (IsKeyPressed(KEY_THREE) && ctx.game->GetCurrentColony()) ctx.level = 2;
    if (IsKeyPressed(KEY_FOUR) && ctx.game->GetCurrentSect()) ctx.level = 3;
    if (IsKeyPressed(KEY_R))
    {
        ctx.placeStep = (ctx.placeStep + 1) % PLACE_COUNT;
        ctx.flow.Reset();
        ctx.level = 0;
        FaceGlobeAt(PLACES[ctx.placeStep].lat, PLACES[ctx.placeStep].lon);
    }

    Vector2 m = GetMousePosition();
    bool onHarnessUi = CheckCollisionPointRec(m, NavBarRect())
                    || (ctx.showIssues && CheckCollisionPointRec(m, NotesPanelRect(ctx)));

    ctx.surveyFrame = false;
    if (SurveyOwnsLevel(ctx))
    {
        SurveyInput in = ctx.inputManager->Survey(GetFrameTime());
        if (onHarnessUi) { in.click = false; in.uiConsumedClick = true; }
        ctx.flow.BeginFrame(in, VT_WIDTH, VT_HEIGHT, ctx.game->GetColonies());
        ctx.surveyFrame = true;
        return;
    }

    bool descend = (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !onHarnessUi)
                   || IsKeyPressed(KEY_DOWN);
    bool ascend = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_UP)
                  || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (ctx.level == 2)
    {
        if (descend && ctx.game->GetCurrentSect()) ctx.level = 3;
        if (ascend)
        {
            // Up the descent it came by, or to the globe facing it.
            if (ctx.flow.Controller().Level() > 0 && ctx.flow.Escape(VT_WIDTH, VT_HEIGHT))
            {
                SyncLevel(ctx);
            }
            else
            {
                Colony* c = ctx.game->GetCurrentColony();
                ctx.flow.Reset();
                ctx.level = 0;
                if (c) FaceGlobeAt(c->GetCentre().latDeg, c->GetCentre().lonDeg);
            }
        }
    }
    else if (ctx.level == 3)
    {
        if (ascend) ctx.level = 2;
    }
}

// The colony view draws in the colony's own frame: origin at its centre,
// the 25 km window filling the height -- the site rung's framing, as the
// game's ViewManager sets it.
static void ApplyViewCamera(ViewTestContext& ctx)
{
    ctx.camera.target = Vector2{0.0f, 0.0f};
    ctx.camera.zoom = VT_HEIGHT / (float)(COLONY_WINDOW_KM * LOCAL_UNITS_PER_KM);
    ctx.camera.offset = {VT_WIDTH / 2.0f, VT_HEIGHT / 2.0f};
}

static void DrawFrame(ViewTestContext& ctx)
{
    ApplyViewCamera(ctx);
    GameManager& gm = *ctx.game;
    BeginDrawing();
    ClearBackground(BLACK);

    switch (ctx.level)
    {
    case 0:
    case 1:
        ctx.flow.Draw(*ctx.renderManager, gm.GetPlanet(), gm.GetColonies(), gm.GetCurrentColony());
        break;
    case 2:
        if (gm.GetCurrentColony())
            ctx.renderManager->DrawColonyView(ctx.camera, gm.GetCurrentColony(), gm.GetPlanet(),
                                              gm.GetColonies(), *ctx.inputManager,
                                              gm.GetTimeManager());
        else
            ctx.flow.Draw(*ctx.renderManager, gm.GetPlanet(), gm.GetColonies(), nullptr);
        break;
    default:
        ctx.renderManager->DrawSectView(gm.GetCurrentSect(), gm.GetTimeManager());
        break;
    }

    DrawSiteBadge(ctx);
    if (ctx.showIssues) DrawIssuePanel(ctx);
    DrawNavBar(ctx);

    EndDrawing();

    if (ctx.surveyFrame)
    {
        ApplyFrame(ctx, ctx.flow.EndFrame(*ctx.renderManager));
        ctx.surveyFrame = false;
    }
}

static void UpdateFrame(void* arg)
{
    ViewTestContext& ctx = *(ViewTestContext*)arg;
    HandleInput(ctx);
    ctx.game->Update(GetFrameTime());
    DrawFrame(ctx);
}

// ---------------------------------------------------------------------------
// Headless: the whole descent, scripted through the same flow.

static void Shot(ViewTestContext& ctx, const char* name)
{
    DrawFrame(ctx);      // settle fonts/textures
    DrawFrame(ctx);
    Image shot = LoadImageFromScreen();
    std::string path = ctx.shotPrefix + "_" + name + ".png";
    ExportImage(shot, path.c_str());
    UnloadImage(shot);
    TraceLog(LOG_WARNING, "wrote %s", path.c_str());
}

// One scripted frame of the descent: the pointer rests here, and clicks
// or not. Goes through BeginFrame / Draw / EndFrame like a live frame.
// instant lands a transition the same frame; a live click starts the
// flight the game would fly.
static void Step(ViewTestContext& ctx, Vector2 pointer, bool click, bool instant = true)
{
    SurveyInput in = SurveyScript::Base(pointer);
    in.click = click;
    in.instant = instant;
    in.dt = 1.0f / 60.0f;
    ctx.flow.BeginFrame(in, VT_WIDTH, VT_HEIGHT, ctx.game->GetColonies());
    ctx.surveyFrame = true;
    DrawFrame(ctx);
}

static bool StepAt(ViewTestContext& ctx, double lat, double lon, bool click, bool instant = true)
{
    Vector2 p;
    if (!SurveyScript::PointerFor(ctx.flow.Controller(), VT_WIDTH, VT_HEIGHT, lat, lon, &p))
    {
        TraceLog(LOG_WARNING, "viewtest: %.2f,%.2f is not on the visible globe", lat, lon);
        return false;
    }
    Step(ctx, p, click, instant);
    return true;
}

// Fly the flight the last click began, at 60 Hz, shooting frames at 25,
// 50 and 80 % so the approach can be judged: the target should fall
// straight to the centre, not swing out and back.
static void Fly(ViewTestContext& ctx, const char* tag, bool shots)
{
    const SiteSelectionController& ctl = ctx.flow.Controller();
    const float marks[3] = { 0.25f, 0.5f, 0.8f };
    int next = 0;
    int guard = 0;
    while (ctl.FlightActive() && guard++ < 600)
    {
        Step(ctx, Vector2{ VT_WIDTH * 0.5f, VT_HEIGHT * 0.5f }, false, false);
        if (shots && next < 3 && ctl.FlightActive() && ctl.FlightT() >= marks[next])
        {
            // A copy: TextFormat's buffer is rewritten by the frame's own
            // text before the file is named otherwise.
            std::string name = TextFormat("%s_%02d", tag, (int)(marks[next] * 100.0f + 0.5f));
            Shot(ctx, name.c_str());
            next++;
        }
    }
}

// Claim a place, descend into it and found there: the whole ladder,
// with a screenshot at each rung when `shots` is set.
static bool ScriptedFounding(ViewTestContext& ctx, double lat, double lon, bool shots)
{
    ctx.flow.Reset();
    ctx.level = 0;
    FaceGlobeAt(lat, lon);
    LunarPoint pick; pick.latDeg = lat; pick.lonDeg = lon;
    LunarPoint target = LunarOffsetPoint(pick, ctx.aimDxKm, ctx.aimDyKm);

    if (!StepAt(ctx, lat, lon, false)) return false;
    if (shots) Shot(ctx, "orbital");
    StepAt(ctx, lat, lon, true, false);                     // claim: a live flight
    Fly(ctx, "flight1", shots);
    if (ctx.flow.Controller().Level() != 1) return false;

    StepAt(ctx, target.latDeg, target.lonDeg, false);
    if (shots) Shot(ctx, "district");
    StepAt(ctx, target.latDeg, target.lonDeg, true, false); // descend: a live dive
    Fly(ctx, "flight2", shots);
    if (ctx.flow.Controller().Level() != 2) return false;

    StepAt(ctx, target.latDeg, target.lonDeg, false);
    if (shots) Shot(ctx, "site");
    bool green = ctx.flow.Controller().HaveVerdict() && ctx.flow.Controller().Verdict().allowed;
    StepAt(ctx, target.latDeg, target.lonDeg, true);        // found
    if (!green || ctx.game->GetCurrentColony() == nullptr)
    {
        TraceLog(LOG_WARNING, "viewtest: the site at %.2f,%.2f was refused", target.latDeg, target.lonDeg);
        return false;
    }
    if (shots)
    {
        Shot(ctx, "colony");
        ctx.level = 3;
        ctx.game->SelectDefaultUnit();
        Shot(ctx, "sect");
    }
    return true;
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
            // --pick LAT,LON: the region the scripted descent claims.
            std::string v = argv[++i];
            size_t sep = v.find(',');
            if (sep != std::string::npos)
            {
                g_ctx.pickLat = atof(v.substr(0, sep).c_str());
                g_ctx.pickLon = atof(v.substr(sep + 1).c_str());
            }
        }
        else if (a == "--aim" && i + 1 < argc)
        {
            // --aim DX,DY: km east/north of the pick the cursor is aimed at.
            std::string v = argv[++i];
            size_t sep = v.find(',');
            if (sep != std::string::npos)
            {
                g_ctx.aimDxKm = atof(v.substr(0, sep).c_str());
                g_ctx.aimDyKm = atof(v.substr(sep + 1).c_str());
            }
        }
    }

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(VT_WIDTH, VT_HEIGHT, StampedTitle("Colony - Game Walk"));

    InputManager::FixWebPointerUnits();
    int status = 0;
    {
        RenderManager renderManager(VT_WIDTH, VT_HEIGHT);
        renderManager.LoadFonts();
        InputManager inputManager;
        GameManager game;
        game.InitGame();
        game.GetPlanet()->GenerateMap(20260813u);   // a fixed Moon: same ground every run
        TerrainWarmMosaic();
        SetLunarGlobeSpin(0.0);

        g_ctx.renderManager = &renderManager;
        g_ctx.inputManager = &inputManager;
        g_ctx.game = &game;
        g_ctx.camera.offset = {VT_WIDTH / 2.0f, VT_HEIGHT / 2.0f};
        g_ctx.camera.rotation = 0.0f;
        g_ctx.camera.zoom = 1.0f;
        FaceGlobeAt(g_ctx.pickLat, g_ctx.pickLon);

        if (g_ctx.headless)
        {
            // The whole ladder at the picked place, then a second colony
            // on the far side, then the globe with both marked.
            if (!ScriptedFounding(g_ctx, g_ctx.pickLat, g_ctx.pickLon, true)) status = 1;
            ScriptedFounding(g_ctx, -21.2, 128.9, false);        // Tsiolkovskiy
            g_ctx.flow.Reset();
            g_ctx.level = 0;
            FaceGlobeAt(-10.0, 100.0);
            Step(g_ctx, Vector2{ VT_WIDTH * 0.5f + 180.0f, VT_HEIGHT * 0.5f + 60.0f }, false);
            Shot(g_ctx, "orbital_two");
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
    }

    CloseWindow();
    return status;
}
