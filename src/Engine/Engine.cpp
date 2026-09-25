#include "sect_art.h"
#include "Engine.h"
#include "region_identity.h"
#include "site_selection_constants.h"
#include "terrain_synthesis.h"
#include "lunar_globe.h"
#include <ctime>
#include <cmath>
#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

Engine::Engine(int screenWidth, int screenHeight, const char* title)
    : screenWidth(screenWidth),
      screenHeight(screenHeight),
      inputManager(),
      viewManager(screenWidth, screenHeight),
      gameManager(),
      renderManager(screenWidth, screenHeight),
      survey(),
      surveyFrame(false),
      pointerDiag(false),
      mouseScale(1.0f)
{
    InitWindow(screenWidth, screenHeight, title);
    InputManager::FixWebPointerUnits();
    SetTargetFPS(60);
    // A compositor that magnifies the window but hands the pointer over in
    // magnified pixels (WSLg and some fractional-scaling desktops) puts the
    // game's pointer past the real one by the same factor. Until the
    // platform is fixed, COLONY_MOUSE_SCALE=0.8 undoes a 125 % display;
    // F11 shows what the game receives so the factor can be read off.
    if (const char* env = std::getenv("COLONY_MOUSE_SCALE")) {
        float k = static_cast<float>(std::atof(env));
        if (k > 0.0f) {
            mouseScale = k;
            SetMouseScale(k, k);
        }
    }
    renderManager.LoadFonts();
}

Engine::~Engine() {
    CloseWindow();
}

void Engine::InitGame() {
    gameManager.InitGame();

    // The Colony view's camera lives in the colony's own frame, whose
    // origin is the colony's centre.
    viewManager.GetCamera().target = {0.0f, 0.0f};
    viewManager.GetCamera().offset = {static_cast<float>(screenWidth)/2, static_cast<float>(screenHeight)/2};
    viewManager.GetCamera().rotation = 0.0f;
    viewManager.GetCamera().zoom = 1.0f;

    // Decode the mosaic where a pause is expected, not in the middle of
    // the first descent.
    TerrainWarmMosaic();

    // Set initial view to Menu
    viewManager.SetCurrentView(View::Menu);
}

void Engine::UpdateFrame() {
    HandleInput();
    Update();
    Draw();
}

void Engine::Run() {
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(
        [](void* arg) { static_cast<Engine*>(arg)->UpdateFrame(); },
        this, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateFrame();
    }
#endif
}

// The descent runs the Orbital and District views, and the Colony view
// while no colony stands under it -- that is the site rung.
bool Engine::SurveyActive() const {
    View v = viewManager.GetCurrentView();
    if (v == View::Orbital || v == View::District) return true;
    return v == View::Colony && gameManager.GetCurrentColony() == nullptr;
}

void Engine::HandleInput() {
    inputManager.Update();
    surveyFrame = false;
    float dt = GetFrameTime();

    // Screenshot functionality (F12) - works in all views
    if (IsKeyPressed(KEY_F12)) {
        // Generate timestamp-based filename
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        char filename[128];
        strftime(filename, sizeof(filename), "screenshots/screenshot_%Y%m%d_%H%M%S.png", timeinfo);
        TakeScreenshot(filename);
        std::cout << "[SCREENSHOT] Saved: " << filename << std::endl;
    }

    // F9: pointer diagnostic overlay, all views (F11 is the browser's
    // fullscreen key).
    if (IsKeyPressed(KEY_F9)) {
        pointerDiag = !pointerDiag;
    }

    // DEBUG: F5 - Cycle through tech unlocks
    if (IsKeyPressed(KEY_F5)) {
        auto& registry = UnlockRegistry::Instance();
        const auto& techs = UnlockRegistry::GetAvailableTechs();
        bool unlocked = false;
        for (const auto& tech : techs) {
            if (!registry.IsUnlocked(tech)) {
                registry.Unlock(tech);
                unlocked = true;
                break;
            }
        }
        if (!unlocked) {
            std::cout << "All technologies already unlocked!" << std::endl;
        }
        registry.PrintStatus();
    }

    // DEBUG: F6 - Print the orbital survey of the ground under the cursor
    // (on the globe, or in the colony's window).
    if (IsKeyPressed(KEY_F6)) {
        Planet* planet = gameManager.GetPlanet();
        LunarPoint point;
        bool have = false;
        if (viewManager.GetCurrentView() == View::Orbital) {
            Vector2 m = GetMousePosition();
            have = OrbitalPickToLatLon(m.x, m.y, GetScreenWidth(), GetScreenHeight(),
                                       &point.latDeg, &point.lonDeg);
        } else if (viewManager.GetCurrentView() == View::Colony && gameManager.GetCurrentColony()) {
            point = gameManager.GetCurrentColony()->GetFrame().FromLocal(viewManager.GetWorldMousePosition());
            have = true;
        } else if (survey.Controller().Level() > 0) {
            point.latDeg = survey.Controller().HoverLat();
            point.lonDeg = survey.Controller().HoverLon();
            have = true;
        }
        if (planet && have) {
            ResourceManager& rm = planet->GetResourceManager();
            auto surveyData = rm.SurveyAt(point);
            const RegionIdentity& region = rm.GroundAt(point).region;

            std::cout << "\n=== ORBITAL SURVEY at " << point.latDeg << ", " << point.lonDeg
                      << " (" << (region.name[0] ? region.name : "unnamed ground") << ") ===" << std::endl;
            std::cout << "  Fe: " << (surveyData.fePercent * 100.0f) << "%" << std::endl;
            std::cout << "  Ti: " << (surveyData.tiPercent * 100.0f) << "%" << std::endl;
            std::cout << "  Si: " << (surveyData.siPercent * 100.0f) << "%" << std::endl;
            std::cout << "  Al: " << (surveyData.alPercent * 100.0f) << "%" << std::endl;
            std::cout << "  Ca: " << (surveyData.caPercent * 100.0f) << "%" << std::endl;
            std::cout << "  Th: " << surveyData.thPpm << " ppm" << std::endl;
            std::cout << "  K:  " << surveyData.kPpm << " ppm" << std::endl;
            std::cout << "  H signal: " << (surveyData.hydrogenSignal * 100.0f) << "%" << std::endl;
            std::cout << "  Solar: " << (surveyData.solarIllumination * 100.0f) << "%" << std::endl;
            std::cout << "  Slope: " << surveyData.terrainSlope << " deg" << std::endl;
            std::cout << "  Earth vis: " << (surveyData.earthVisibility * 100.0f) << "%" << std::endl;
            std::cout << "  Archetype: " << GetSiteArchetypeDescriptor(region.archetype).name << std::endl;
            std::cout << "================================\n" << std::endl;
        }
    }

    // DEBUG: F7 - Force upgrade selected module tier (bypasses tech/cost checks)
    if (IsKeyPressed(KEY_F7) && viewManager.GetCurrentView() == View::Unit)
    {
        Unit* unit = gameManager.GetCurrentUnit();
        if (unit)
        {
            int modIdx = unit->GetSelectedModuleIndex();
            unit->DebugUpgradeModuleTier(modIdx);
        }
    }

    // ---------- the descent ----------
    if (SurveyActive()) {
        const SiteSelectionController& ctl = survey.Controller();
        SurveyInput in = inputManager.Survey(dt);
        bool atGlobe = (ctl.Level() == 0) && !ctl.FlightActive();
        if (atGlobe && IsKeyPressed(KEY_ENTER)) {
            survey.ClaimAtCentre(GetScreenWidth(), GetScreenHeight());
        } else {
            survey.BeginFrame(in, GetScreenWidth(), GetScreenHeight(), gameManager.GetColonies());
        }
        surveyFrame = true;
        // At the globe there is nowhere to back out to but the menu.
        if (atGlobe && IsKeyPressed(KEY_ESCAPE)) {
            viewManager.SetCurrentView(View::Menu);
        }
        viewManager.UpdateCamera(inputManager);
        return;
    }

    switch (viewManager.GetCurrentView()) {
        case View::Menu:
            if (IsKeyPressed(KEY_ENTER)) {
                viewManager.SwitchToOrbitalView();
            }
            break;
        case View::Colony:
            if (IsKeyPressed(KEY_S)) {
                viewManager.SwitchToSectView(gameManager.GetCurrentColony(), gameManager.GetCurrentSect());
                gameManager.SelectDefaultUnit();  // Auto-select default unit
            }
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
                // Up one rung: the district the descent came through, or
                // the globe turned to face this colony.
                if (survey.Controller().Level() > 0
                    && survey.Escape(GetScreenWidth(), GetScreenHeight())) {
                    SyncViewToRung();
                } else {
                    Colony* colony = gameManager.GetCurrentColony();
                    viewManager.SwitchToOrbitalView(colony ? &colony->GetCentre() : nullptr);
                }
            }
            // TEST: Press R to build roads between all sects
            if (IsKeyPressed(KEY_R)) {
                gameManager.BuildAllRoads();
            }
            // TEST: Press T to cycle transport mode on selected/first road
            if (IsKeyPressed(KEY_T)) {
                gameManager.CycleTransportModes();
            }
            // TEST INFRASTRUCTURE (Phase 2.5)
            // KEY_0: Print transport state
            if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0)) {
                gameManager.PrintTransportState();
            }
            // KEY_1: Test road construction
            if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) {
                gameManager.TestRoadConstruction();
            }
            // KEY_2: Select nearest road at cursor
            if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) {
                gameManager.SelectNearestRoad(viewManager.GetWorldMousePosition());
            }
            // KEY_3: Run integration test
            if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) {
                gameManager.RunTransportIntegrationTest();
            }
            // KEY_B: Toggle road build mode
            if (IsKeyPressed(KEY_B)) {
                gameManager.ToggleBuildRoadMode();
            }
            // In build mode, left click selects sect
            if (gameManager.IsBuildRoadMode() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                gameManager.SelectSectForRoadBuild(viewManager.GetWorldMousePosition());
            }
            break;
        case View::Sect:
            if (IsKeyPressed(KEY_U)) {
                viewManager.SwitchToUnitView(gameManager.GetCurrentColony(),
                                           gameManager.GetCurrentSect(),
                                           gameManager.GetCurrentUnit());
            }
            if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_ESCAPE)) {
                viewManager.SwitchToColonyView(gameManager.GetCurrentColony());
            }
            break;
        case View::Unit:
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_ESCAPE)) {
                viewManager.SwitchToSectView(gameManager.GetCurrentColony(), gameManager.GetCurrentSect());
            }
            break;
        default:
            break;
    }

    // Handle double-click selection of sects and units
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (inputManager.IsDoubleClick()) {
            Vector2 mousePosition = inputManager.GetMousePosition();
            switch (viewManager.GetCurrentView()) {
                case View::Colony:
                    gameManager.SelectSect(mousePosition, viewManager.GetCamera());
                    viewManager.SwitchToSectView(gameManager.GetCurrentColony(), gameManager.GetCurrentSect());
                    gameManager.SelectDefaultUnit();  // Auto-select default unit
                    break;
                case View::Sect:
                    gameManager.SelectUnit(mousePosition);
                    viewManager.SwitchToUnitView(gameManager.GetCurrentColony(),
                                               gameManager.GetCurrentSect(),
                                               gameManager.GetCurrentUnit());
                    break;
                default:
                    break;
            }
        }
    }

    // Ctrl+click in the Colony view founds a sect at the pointer.
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inputManager.IsCommandPressed()
        && viewManager.GetCurrentView() == View::Colony) {
        gameManager.BuildNewSect(viewManager.GetWorldMousePosition());
    }

    // Update camera based on input
    viewManager.UpdateCamera(inputManager);
}

void Engine::Update() {
    float deltaTime = GetFrameTime();
    gameManager.Update(deltaTime);
    // The sect view's base art bakes a slice per frame from the first frame,
    // so it is ready long before the player reaches a sect (sect_art.h).
    SectArt::Update(6.0);
}

void Engine::Draw() {
    renderManager.BeginDraw();

    switch (viewManager.GetCurrentView()) {
        case View::Menu:
            renderManager.DrawMenuView();
            break;
        case View::Orbital:
        case View::District:
            survey.Draw(renderManager, gameManager.GetPlanet(), gameManager.GetColonies(),
                        gameManager.GetCurrentColony());
            break;
        case View::Colony:
            if (gameManager.GetCurrentColony()) {
                renderManager.DrawColonyView(viewManager.GetCamera(),
                                           gameManager.GetCurrentColony(),
                                           gameManager.GetPlanet(),
                                           gameManager.GetColonies(),
                                           inputManager,
                                           gameManager.GetTimeManager(),
                                           gameManager.GetSelectedRoad(),
                                           gameManager.IsBuildRoadMode(),
                                           gameManager.GetRoadBuildStartSect());
            } else {
                // The site rung: this window, no colony under it yet.
                survey.Draw(renderManager, gameManager.GetPlanet(), gameManager.GetColonies(),
                            nullptr);
            }
            break;
        case View::Sect:
            renderManager.DrawSectView(gameManager.GetCurrentSect(),
                                     gameManager.GetTimeManager());
            break;
        case View::Unit:
            renderManager.DrawUnitView(gameManager.GetCurrentUnit(), gameManager.GetTimeManager());
            break;
    }

    if (pointerDiag) {
        DrawPointerDiagnostic();
    }
    renderManager.EndDraw();

    // The frame above was drawn against the state the input was judged
    // in; now the click or escape it decided happens.
    if (surveyFrame) {
        ApplySurveyFrame(survey.EndFrame(renderManager));
        surveyFrame = false;
    }
}

// Where the game thinks the pointer is, and the numbers behind it. A
// screenshot with the OS cursor visible then says whether the platform
// scales the pointer differently from the window.
void Engine::DrawPointerDiagnostic() {
    Vector2 m = GetMousePosition();
    Vector2 dpi = GetWindowScaleDPI();
    Color ink = Color{ 80, 255, 120, 255 };
    DrawCircleLines(static_cast<int>(m.x), static_cast<int>(m.y), 14.0f, ink);
    DrawLine(static_cast<int>(m.x) - 22, static_cast<int>(m.y), static_cast<int>(m.x) + 22, static_cast<int>(m.y), ink);
    DrawLine(static_cast<int>(m.x), static_cast<int>(m.y) - 22, static_cast<int>(m.x), static_cast<int>(m.y) + 22, ink);

    int x = GetScreenWidth() - 300, y = 12, line = 16;
    DrawRectangle(x - 8, y - 6, 296, line * 7 + 12, Color{ 0, 0, 0, 190 });
    DrawText("POINTER DIAGNOSTIC  (F9)", x, y, 13, ink);
    DrawText(TextFormat("mouse    %.0f, %.0f   (ring)", m.x, m.y), x, y + line, 13, RAYWHITE);
    DrawText(TextFormat("screen   %d x %d", GetScreenWidth(), GetScreenHeight()), x, y + line * 2, 13, RAYWHITE);
    DrawText(TextFormat("render   %d x %d", GetRenderWidth(), GetRenderHeight()), x, y + line * 3, 13, RAYWHITE);
    DrawText(TextFormat("dpi      %.2f, %.2f", dpi.x, dpi.y), x, y + line * 4, 13, RAYWHITE);
    DrawText(TextFormat("scale    %.2f  (COLONY_MOUSE_SCALE)", mouseScale), x, y + line * 5, 13, RAYWHITE);
#ifdef __EMSCRIPTEN__
    double cssW = 0.0, cssH = 0.0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    DrawText(TextFormat("css box  %.0f x %.0f", cssW, cssH), x, y + line * 6, 13, RAYWHITE);
#endif
}

void Engine::ApplySurveyFrame(const SurveyFlow::Frame& frame) {
    if (frame.openedColony) {
        gameManager.SetCurrentColony(frame.openedColony);
        viewManager.SwitchToColonyView(frame.openedColony);
        return;
    }
    if (frame.founded) {
        Colony* colony = gameManager.FoundColony(frame.foundPoint, frame.windowCentre, &frame.region);
        if (colony) {
            viewManager.SwitchToColonyView(colony);
        } else {
            viewManager.SwitchToOrbitalView(&frame.foundPoint);
        }
        return;
    }
    if (frame.rungChanged) SyncViewToRung();
}

// The view follows the rung: the globe, the district, and at the site
// rung either the colony that already stands in this window or the
// window itself, waiting to be founded on.
void Engine::SyncViewToRung() {
    const SiteSelectionController& ctl = survey.Controller();
    switch (ctl.Level()) {
        case 0:
            viewManager.SetCurrentView(View::Orbital);
            break;
        case 1:
            viewManager.SetCurrentView(View::District);
            break;
        default: {
            Colony* here = gameManager.ColonyInWindow(survey.WindowCentre(), survey.WindowSpanKm());
            if (here) {
                gameManager.SetCurrentColony(here);
                viewManager.SwitchToColonyView(here);
            } else {
                gameManager.SetCurrentColony(nullptr);
                viewManager.SetCurrentView(View::Colony);
            }
            break;
        }
    }
}
