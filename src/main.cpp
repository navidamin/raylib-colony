#include "Engine/Engine.h"
#include "display_scale.h"

// Screen dimensions
const int screenWidth = 1280;
const int screenHeight = 720;

int main(int argc, char** argv) {
    // Before the Engine: it creates the window, at the buffer size this picks.
    DisplayScale_Init(screenWidth, screenHeight, argc, argv);
    Engine engine(screenWidth, screenHeight, "Colony - Planet Colonization Game");
    engine.InitGame();
    engine.Run();
    return 0;
}
