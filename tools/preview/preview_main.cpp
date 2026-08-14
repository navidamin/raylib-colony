// Offscreen view preview tool.
//
// Renders a game view to a PNG with no display, driving the real
// RenderManager so what it exports is what the game draws. Ported from the
// module-panel preview tool on claude/game-status-remaining-z2u35f and
// pointed at this branch's planet/orbital work.
//
// Usage (from the repo root, so relative asset paths resolve):
//   cmake --build build --target colony_preview
//   tools/preview/preview.sh --view orbital
//   tools/preview/preview.sh --view planet --out build/preview/planet.png
//   tools/preview/preview.sh --all

#include "raylib.h"

#include "rendermanager.h"
#include "planet.h"
#include "colony.h"
#include "sect.h"
#include "time_manager.h"
#include "inputmanager.h"
#include "game_constants.h"
#include "resource_manager.h"
#include "terrain_synthesis.h"

#include <iostream>
#include <string>
#include <vector>

// Fixed world seed: reproducible screenshots across runs and machines.
static const unsigned int PREVIEW_MAP_SEED = 20260813u;

struct PreviewOptions
{
    std::string view = "orbital";
    int width = 1280;
    int height = 720;
    std::string outPath = "preview.png";
    int cellX = 10;    // planet grid cell for --view sect
    int cellY = 10;
};

static void PrintUsage()
{
    std::cout
        << "Usage: colony_preview [options]\n"
        << "\n"
        << "  --view <name>   orbital | planet | sect (default: orbital)\n"
        << "  --cell <X,Y>    planet grid cell for sect view (default: 10,10)\n"
        << "  --size <WxH>    output resolution       (default: 1280x720)\n"
        << "  --out <path>    output PNG path         (default: preview.png)\n"
        << "  --help          show this message\n";
}

static bool ParseArgs(int argc, char** argv, PreviewOptions& options)
{
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        bool hasNext = (i + 1) < argc;

        if (arg == "--help" || arg == "-h")
        {
            PrintUsage();
            return false;
        }
        else if (arg == "--view" && hasNext)
        {
            options.view = argv[++i];
        }
        else if (arg == "--out" && hasNext)
        {
            options.outPath = argv[++i];
        }
        else if (arg == "--cell" && hasNext)
        {
            std::string value = argv[++i];
            size_t sep = value.find(',');
            if (sep != std::string::npos)
            {
                options.cellX = TextToInteger(value.substr(0, sep).c_str());
                options.cellY = TextToInteger(value.substr(sep + 1).c_str());
            }
        }
        else if (arg == "--size" && hasNext)
        {
            std::string value = argv[++i];
            size_t sep = value.find('x');
            if (sep != std::string::npos)
            {
                options.width = TextToInteger(value.substr(0, sep).c_str());
                options.height = TextToInteger(value.substr(sep + 1).c_str());
            }
        }
        else
        {
            std::cout << "Unknown or incomplete option: " << arg << "\n\n";
            PrintUsage();
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv)
{
    PreviewOptions options;
    if (!ParseArgs(argc, argv, options)) return 0;

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(options.width, options.height, "Colony View Preview");

    int status = 0;
    {
        // GPU-owning objects live in this scope so they are destroyed while
        // the GL context is alive; destructing after CloseWindow() segfaults.
        RenderManager renderManager(options.width, options.height);
        renderManager.LoadFonts();

        TimeManager timeManager;
        InputManager inputManager;

        Planet planet;
        std::vector<Colony*> colonies;

        // Sect standing on its real grid cell (sect view only)
        ResourceManager resourceManager(PLANET_SIZE,
                                        SECT_CORE_RADIUS * 2.0f);
        Sect* sect = nullptr;
        if (options.view == "sect")
        {
            Vector2 sectPos = {
                (options.cellX + 0.5f) * SECT_CORE_RADIUS * 2.0f,
                (options.cellY + 0.5f) * SECT_CORE_RADIUS * 2.0f};
            sect = new Sect(sectPos, resourceManager, timeManager);
            double lat, lon;
            TerrainGridCellToLatLon(options.cellX, options.cellY,
                                    &lat, &lon);
            std::cout << "Sect on cell (" << options.cellX << ","
                      << options.cellY << ") -> lat " << lat
                      << ", lon " << lon << "\n";
            // Raw terrain dump alongside the composed view, for
            // comparing against the Python prototype's output.
            Image ground = GenerateSectTerrain(lat, lon);
            std::string groundPath = options.outPath + ".ground.png";
            ExportImage(ground, groundPath.c_str());
            UnloadImage(ground);
        }

        Camera2D camera = {0};
        camera.target = {PLANET_WIDTH / 2.0f, PLANET_HEIGHT / 2.0f};
        camera.offset = {options.width / 2.0f, options.height / 2.0f};
        camera.rotation = 0.0f;
        camera.zoom = 1.0f;

        // Draw twice: the first frame lets fonts and textures settle.
        for (int frame = 0; frame < 2; frame++)
        {
            BeginDrawing();
            ClearBackground(BLACK);

            if (options.view == "planet")
            {
                renderManager.DrawPlanetView(camera, &planet, colonies,
                                              inputManager, timeManager);
            }
            else if (options.view == "sect")
            {
                renderManager.DrawSectView(sect, timeManager);
            }
            else
            {
                renderManager.DrawOrbitalView();
            }

            EndDrawing();
        }

        delete sect;

        Image screenshot = LoadImageFromScreen();
        bool exported = ExportImage(screenshot, options.outPath.c_str());
        UnloadImage(screenshot);

        if (exported)
        {
            std::cout << "Wrote " << options.outPath
                      << " (view=" << options.view << ")\n";
        }
        else
        {
            std::cout << "Failed to write " << options.outPath << "\n";
            status = 1;
        }
    }

    CloseWindow();
    return status;
}
