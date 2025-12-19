#include "Engine.h"
#include <ctime>

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
}

Engine::~Engine() {
    CloseWindow();
}

void Engine::InitGame() {
    gameManager.InitGame();

    // Initialize camera to focus on the first sect
    Vector2 initialPosition;
    if (!gameManager.GetColonies().empty() && !gameManager.GetCurrentColony()->GetSects().empty()) {
        initialPosition = gameManager.GetCurrentColony()->GetSects()[0]->GetPosition();
    } else {
        initialPosition = {PLANET_WIDTH / 2, PLANET_HEIGHT / 2};
    }

    viewManager.GetCamera().target = initialPosition;
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

    switch (viewManager.GetCurrentView()) {
        case View::Menu:
            if (IsKeyPressed(KEY_ENTER)) {
                viewManager.SwitchToColonyView(gameManager.GetCurrentColony());
                viewManager.ResetCameraForCurrentView(viewManager.GetCurrentView(),
                                                     gameManager.GetColonies(),
                                                     gameManager.GetCurrentColony(),
                                                     gameManager.GetPlanet());
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
                                       gameManager.GetTimeManager());
            break;
        case View::Sect:
            renderManager.DrawSectView(gameManager.GetCurrentSect(),
                                     gameManager.GetTimeManager());
            break;
        case View::Unit:
            renderManager.DrawUnitView(gameManager.GetCurrentUnit());
            break;
    }

    renderManager.EndDraw();
}
