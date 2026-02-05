#include "Engine.h"
#include <ctime>
#include <cmath>

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

    // No colony exists at startup - center camera on planet
    viewManager.GetCamera().target = {PLANET_WIDTH / 2, PLANET_HEIGHT / 2};
    viewManager.GetCamera().offset = {static_cast<float>(screenWidth)/2, static_cast<float>(screenHeight)/2};
    viewManager.GetCamera().rotation = 0.0f;
    viewManager.GetCamera().zoom = 1.0f;

    // Set initial view to Menu
    viewManager.SetCurrentView(View::Menu);
}

void Engine::Run() {
    while (!WindowShouldClose()) {
        HandleInput();
        Update();
        Draw();
    }
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

    // DEBUG: F6 - Print orbital survey data at cursor position
    if (IsKeyPressed(KEY_F6) && viewManager.GetCurrentView() == View::Planet) {
        Vector2 worldPos = viewManager.GetWorldMousePosition();
        Planet* planet = gameManager.GetPlanet();
        if (planet) {
            Vector2 gridPos = {
                std::floor(worldPos.x / (SECT_CORE_RADIUS * 2.0f)),
                std::floor(worldPos.y / (SECT_CORE_RADIUS * 2.0f))
            };
            int gx = static_cast<int>(gridPos.x);
            int gy = static_cast<int>(gridPos.y);
            auto survey = planet->GetResourceManager().GetOrbitalSurveyAt(gx, gy);
            auto archetype = planet->GetResourceManager().GetSiteArchetype(gx, gy);

            const char* archetypeNames[] = {
                "MARE_INDUSTRIAL", "HIGHLAND_CONSTRUCTION", "POLAR_VOLATILE",
                "KREEP_SCIENTIFIC", "LAVA_TUBE", "MIXED"
            };

            std::cout << "\n=== ORBITAL SURVEY at (" << gx << "," << gy << ") ===" << std::endl;
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
            std::cout << "  Archetype: " << archetypeNames[static_cast<int>(archetype)] << std::endl;
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
                viewManager.SwitchToPlanetView(gameManager.GetCurrentColony());
                viewManager.ResetCameraForCurrentView(View::Planet,
                                                     gameManager.GetColonies(),
                                                     gameManager.GetCurrentColony(),
                                                     gameManager.GetPlanet());
            }
            break;
        case View::SITE_SELECTION:
            // Update hover position as mouse moves
            gameManager.UpdateSiteSelectionHover(viewManager.GetWorldMousePosition());

            // Click to select a cell
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                gameManager.UpdateSiteSelectionHover(viewManager.GetWorldMousePosition());
            }

            // Enter to confirm
            if (IsKeyPressed(KEY_ENTER)) {
                gameManager.ConfirmSiteSelection();
                if (!gameManager.IsInSiteSelection()) {
                    // Successfully placed - switch to colony view
                    viewManager.SwitchToColonyView(gameManager.GetCurrentColony());
                    viewManager.ResetCameraForCurrentView(viewManager.GetCurrentView(),
                                                         gameManager.GetColonies(),
                                                         gameManager.GetCurrentColony(),
                                                         gameManager.GetPlanet());
                }
            }

            // Escape to cancel
            if (IsKeyPressed(KEY_ESCAPE)) {
                gameManager.CancelSiteSelection();
                viewManager.SetCurrentView(View::Planet);
            }
            break;
        case View::Planet:
            if (IsKeyPressed(KEY_C)) {
                viewManager.SwitchToColonyView(gameManager.GetCurrentColony());
                viewManager.ResetCameraForCurrentView(viewManager.GetCurrentView(),
                                                     gameManager.GetColonies(),
                                                     gameManager.GetCurrentColony(),
                                                     gameManager.GetPlanet());
            }
            break;
        case View::Colony:
            if (IsKeyPressed(KEY_S)) {
                viewManager.SwitchToSectView(gameManager.GetCurrentColony(), gameManager.GetCurrentSect());
                gameManager.SelectDefaultUnit();  // Auto-select default unit
            }
            if (IsKeyPressed(KEY_P)) {
                viewManager.SwitchToPlanetView(gameManager.GetCurrentColony());
                viewManager.ResetCameraForCurrentView(viewManager.GetCurrentView(),
                                                     gameManager.GetColonies(),
                                                     gameManager.GetCurrentColony(),
                                                     gameManager.GetPlanet());
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
                std::cout << "KEY_U pressed in Sect view!" << std::endl;
                viewManager.SwitchToUnitView(gameManager.GetCurrentColony(),
                                           gameManager.GetCurrentSect(),
                                           gameManager.GetCurrentUnit());
            }
            if (IsKeyPressed(KEY_C)) {
                std::cout << "KEY_C pressed in Sect view!" << std::endl;
                viewManager.SwitchToColonyView(gameManager.GetCurrentColony());
                viewManager.ResetCameraForCurrentView(viewManager.GetCurrentView(),
                                                     gameManager.GetColonies(),
                                                     gameManager.GetCurrentColony(),
                                                     gameManager.GetPlanet());
            }
            // Test if mouse is working at all
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                std::cout << "LEFT MOUSE CLICKED in Sect view!" << std::endl;
            }
            break;
        case View::Unit:
            if (IsKeyPressed(KEY_S)) {
                viewManager.SwitchToSectView(gameManager.GetCurrentColony(), gameManager.GetCurrentSect());
            }
            break;
    }

    // Handle double-click selection of specific colonies, sects, and units
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        std::cout << "Mouse button pressed in view: " << static_cast<int>(viewManager.GetCurrentView()) << std::endl;
        if (inputManager.IsDoubleClick()) {
            std::cout << "Double-click confirmed!" << std::endl;
            Vector2 mousePosition = inputManager.GetMousePosition();
            switch (viewManager.GetCurrentView()) {
                case View::Planet:
                    if (!inputManager.IsCommandPressed()){
                        gameManager.SelectColony(viewManager.GetWorldMousePosition());
                        viewManager.SwitchToColonyView(gameManager.GetCurrentColony());
                        viewManager.ResetCameraForCurrentView(viewManager.GetCurrentView(),
                                                             gameManager.GetColonies(),
                                                             gameManager.GetCurrentColony(),
                                                             gameManager.GetPlanet());
                    }
                    break;
                case View::Colony:
                    std::cout << "Colony view double-click handler" << std::endl;
                    gameManager.SelectSect(mousePosition, viewManager.GetCamera());
                    viewManager.SwitchToSectView(gameManager.GetCurrentColony(), gameManager.GetCurrentSect());
                    gameManager.SelectDefaultUnit();  // Auto-select default unit
                    break;
                case View::Sect:
                    std::cout << "Sect view double-click handler" << std::endl;
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

    // Handle build commands
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inputManager.IsCommandPressed()) {
        switch (viewManager.GetCurrentView()) {
            case View::Planet:
                gameManager.BuildNewColony(viewManager.GetWorldMousePosition());
                viewManager.SetCurrentView(View::SITE_SELECTION);
                viewManager.ResetCameraForCurrentView(View::SITE_SELECTION,
                                                     gameManager.GetColonies(),
                                                     gameManager.GetCurrentColony(),
                                                     gameManager.GetPlanet());
                break;
            case View::Colony:
                gameManager.BuildNewSect(viewManager.GetWorldMousePosition());
                break;
            default:
                break;
        }
    }

    // Update camera based on input
    viewManager.UpdateCamera(inputManager, gameManager.GetColonies(), gameManager.GetPlanet());
}

void Engine::Update() {
    float deltaTime = GetFrameTime();
    gameManager.Update(deltaTime);
    gameManager.UpdatePlanetActiveArea();
}

void Engine::Draw() {
    renderManager.BeginDraw();

    switch (viewManager.GetCurrentView()) {
        case View::Menu:
            renderManager.DrawMenuView();
            break;
        case View::SITE_SELECTION:
            renderManager.DrawSiteSelectionView(
                viewManager.GetCamera(),
                gameManager.GetPlanet(),
                gameManager.GetHoveredGridPos(),
                gameManager.GetTimeManager());
            break;
        case View::Planet:
            renderManager.DrawPlanetView(viewManager.GetCamera(),
                                       gameManager.GetPlanet(),
                                       gameManager.GetColonies(),
                                       inputManager,
                                       gameManager.GetTimeManager());
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
