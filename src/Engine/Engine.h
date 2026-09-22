#ifndef ENGINE_H
#define ENGINE_H

#include "raylib.h"
#include "raymath.h"
#include "inputmanager.h"
#include "viewmanager.h"
#include "gamemanager.h"
#include "rendermanager.h"
#include "survey_flow.h"
#include "game_constants.h"
#include "game_enums.h"
#include "time_manager.h"
#include "unlock_registry.h"

class Engine {
public:
    Engine(int screenWidth, int screenHeight, const char* title);
    ~Engine();

    void InitGame();
    void Run();
    void UpdateFrame();

private:
    int screenWidth;
    int screenHeight;

    // Manager instances
    InputManager inputManager;
    ViewManager viewManager;
    GameManager gameManager;
    RenderManager renderManager;

    // The site-selection descent: the Orbital and District views, and
    // the Colony view while no colony stands under it (the site rung).
    SurveyFlow survey;
    bool surveyFrame;            // this frame ran the descent
    bool pointerDiag;            // F11: draw where the game thinks the pointer is
    float mouseScale;            // COLONY_MOUSE_SCALE, 1 unless the platform lies

    void HandleInput();
    void Update();
    void Draw();

    bool SurveyActive() const;
    void ApplySurveyFrame(const SurveyFlow::Frame& frame);
    void SyncViewToRung();
    void DrawPointerDiagnostic();
};

#endif // ENGINE_H
