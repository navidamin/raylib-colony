#include "Engine/Engine.h"

// Screen dimensions
const int screenWidth = 1280;
const int screenHeight = 720;

int main() {
    Engine engine(screenWidth, screenHeight, "Colony - Planet Colonization Game");
    engine.InitGame();
    engine.Run();
    return 0;
}
