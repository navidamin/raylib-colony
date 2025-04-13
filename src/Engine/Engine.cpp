#include "Engine.h"

// Include all implementation files directly
#include "inputmanager.cpp"
#include "viewmanager.cpp"
#include "gamemanager.cpp"
#include "rendermanager.cpp"

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
            }
            if (IsKeyPressed(KEY_P)) {
                viewManager.SwitchToPlanetView(gameManager.GetCurrentColony());
                viewManager.ResetCameraForCurrentView(viewManager.GetCurrentView(),
                                                     gameManager.GetColonies(),
                                                     gameManager.GetCurrentColony(),
                                                     gameManager.GetPlanet());
            }
            break;
        case View::Sect:
            if (IsKeyPressed(KEY_U)) {
                viewManager.SwitchToUnitView(gameManager.GetCurrentColony(),
                                           gameManager.GetCurrentSect(),
                                           gameManager.GetCurrentUnit());
            }
            if (IsKeyPressed(KEY_C)) {
                viewManager.SwitchToColonyView(gameManager.GetCurrentColony());
                viewManager.ResetCameraForCurrentView(viewManager.GetCurrentView(),
                                                     gameManager.GetColonies(),
                                                     gameManager.GetCurrentColony(),
                                                     gameManager.GetPlanet());
            }
            break;
        case View::Unit:
            if (IsKeyPressed(KEY_S)) {
                viewManager.SwitchToSectView(gameManager.GetCurrentColony(), gameManager.GetCurrentSect());
            }
            break;
    }

    // Handle double-click selection of specific colonies, sects, and units
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && inputManager.IsDoubleClick()) {
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
                gameManager.SelectSect(mousePosition, viewManager.GetCamera());
                viewManager.SwitchToSectView(gameManager.GetCurrentColony(), gameManager.GetCurrentSect());
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
