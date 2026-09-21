#include "Engine.h"
#include "region_identity.h"
#include "site_selection_constants.h"
#include "terrain_synthesis.h"
#include "lunar_globe.h"
#include <ctime>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

Engine::Engine(int screenWidth, int screenHeight, const char* title)
    : screenWidth(screenWidth),
      screenHeight(screenHeight),
      inputManager(),
      viewManager(screenWidth, screenHeight),
      gameManager(),
      renderManager(screenWidth, screenHeight)
{
    InitWindow(screenWidth, screenHeight, title);
    SetTargetFPS(60);
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

// The colony whose globe marker is under the pointer, if any.
static Colony* ColonyMarkerAt(Vector2 screen, std::vector<Colony*>& colonies)
{
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    Colony* best = nullptr;
    float bestPx = ORBITAL_MARKER_PICK_PX;
    for (Colony* colony : colonies)
    {
        if (!colony->HasCentre()) continue;
        float x, y;
        if (!OrbitalLatLonToScreen(colony->GetCentre().latDeg, colony->GetCentre().lonDeg,
                                   w, h, &x, &y))
            continue;
        float d = Vector2Distance(screen, Vector2{x, y});
        if (d <= bestPx)
        {
            best = colony;
            bestPx = d;
        }
    }
    return best;
}

void Engine::HandleInput() {
    inputManager.Update();

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
        }
        if (planet && have) {
            ResourceManager& rm = planet->GetResourceManager();
            auto survey = rm.SurveyAt(point);
            const RegionIdentity& region = rm.GroundAt(point).region;

            std::cout << "\n=== ORBITAL SURVEY at " << point.latDeg << ", " << point.lonDeg
                      << " (" << (region.name[0] ? region.name : "unnamed ground") << ") ===" << std::endl;
            std::cout << "  Fe: " << (survey.fePercent * 100.0f) << "%" << std::endl;
            std::cout << "  Ti: " << (survey.tiPercent * 100.0f) << "%" << std::endl;
            std::cout << "  Si: " << (survey.siPercent * 100.0f) << "%" << std::endl;
            std::cout << "  Al: " << (survey.alPercent * 100.0f) << "%" << std::endl;
            std::cout << "  Ca: " << (survey.caPercent * 100.0f) << "%" << std::endl;
            std::cout << "  Th: " << survey.thPpm << " ppm" << std::endl;
            std::cout << "  K:  " << survey.kPpm << " ppm" << std::endl;
            std::cout << "  H signal: " << (survey.hydrogenSignal * 100.0f) << "%" << std::endl;
            std::cout << "  Solar: " << (survey.solarIllumination * 100.0f) << "%" << std::endl;
            std::cout << "  Slope: " << survey.terrainSlope << " deg" << std::endl;
            std::cout << "  Earth vis: " << (survey.earthVisibility * 100.0f) << "%" << std::endl;
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

    switch (viewManager.GetCurrentView()) {
        case View::Menu:
            if (IsKeyPressed(KEY_ENTER)) {
                viewManager.SwitchToOrbitalView();
            }
            break;
        case View::Orbital: {
            // The founding stub: a click that did not turn the globe
            // founds a colony at the picked point, or opens the colony
            // whose marker was clicked. The informed descent -- district,
            // site, the region cards and the verdict -- replaces this in
            // Part B of the site-selection plan.
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !LunarGlobeWasDragged()) {
                Vector2 m = GetMousePosition();
                Colony* hit = ColonyMarkerAt(m, gameManager.GetColonies());
                if (hit) {
                    gameManager.SetCurrentColony(hit);
                } else {
                    LunarPoint picked;
                    if (OrbitalPickToLatLon(m.x, m.y, GetScreenWidth(), GetScreenHeight(),
                                            &picked.latDeg, &picked.lonDeg)) {
                        hit = gameManager.FoundColony(picked);
                    }
                }
                if (hit) {
                    viewManager.SwitchToColonyView(hit);
                }
            }
            if (IsKeyPressed(KEY_ENTER) && gameManager.GetCurrentColony()) {
                viewManager.SwitchToColonyView(gameManager.GetCurrentColony());
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                viewManager.SetCurrentView(View::Menu);
            }
            break;
        }
        case View::Colony:
            if (IsKeyPressed(KEY_S)) {
                viewManager.SwitchToSectView(gameManager.GetCurrentColony(), gameManager.GetCurrentSect());
                gameManager.SelectDefaultUnit();  // Auto-select default unit
            }
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
                // Colony <- ESC <- the globe, turned to face this colony.
                Colony* colony = gameManager.GetCurrentColony();
                viewManager.SwitchToOrbitalView(colony ? &colony->GetCentre() : nullptr);
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
}

void Engine::Draw() {
    renderManager.BeginDraw();

    switch (viewManager.GetCurrentView()) {
        case View::Menu:
            renderManager.DrawMenuView();
            break;
        case View::Orbital:
            renderManager.DrawOrbitalView(gameManager.GetColonies(),
                                          gameManager.GetCurrentColony());
            break;
        case View::Colony:
            renderManager.DrawColonyView(viewManager.GetCamera(),
                                       gameManager.GetCurrentColony(),
                                       gameManager.GetPlanet(),
                                       gameManager.GetColonies(),
                                       inputManager,
                                       gameManager.GetTimeManager(),
                                       gameManager.GetSelectedRoad(),
                                       gameManager.IsBuildRoadMode(),
                                       gameManager.GetRoadBuildStartSect());
            break;
        case View::Sect:
            renderManager.DrawSectView(gameManager.GetCurrentSect(),
                                     gameManager.GetTimeManager());
            break;
        case View::Unit:
            renderManager.DrawUnitView(gameManager.GetCurrentUnit(), gameManager.GetTimeManager());
            break;
    }

    renderManager.EndDraw();
}
