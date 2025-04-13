#ifndef ENGINE_H
#define ENGINE_H

#include "raylib.h"
#include "raymath.h"
#include "inputmanager.h"
#include "viewmanager.h"
#include "gamemanager.h"
#include "rendermanager.h"
#include "game_constants.h"
#include "game_enums.h"
#include "time_manager.h"

class Engine {
public:
    Engine(int screenWidth, int screenHeight, const char* title);
    ~Engine();

    void InitGame();
    void Run();

private:
    int screenWidth;
    int screenHeight;

    // Manager instances
    InputManager inputManager;
    ViewManager viewManager;
    GameManager gameManager;
    RenderManager renderManager;

    void HandleInput();
    void Update();
    void Draw();
};

#endif // ENGINE_H
