// Offscreen preview tool.
//
// Renders either a single unit module panel (--module) or a whole game view
// (--view) to a PNG without running the game loop, so UI and terrain work can
// be reviewed headlessly (CI, containers, Claude Code sessions). It drives the
// real RenderManager against real game objects, so what it exports is what the
// game draws -- there is no second implementation to drift.
//
// See tools/preview/README.md for usage.

#include "raylib.h"

#include "rendermanager.h"
#include "unit.h"
#include "planet.h"
#include "colony.h"
#include "sect.h"
#include "inputmanager.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "terrain_synthesis.h"
#include "game_constants.h"
#include "game_enums.h"
#include "rock_texture.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Fixed world seed: reproducible screenshots across runs and machines.
static const unsigned int PREVIEW_MAP_SEED = 20260813u;

struct PreviewOptions
{
    std::string unitType = "Extraction";
    // Panel mode (--module) is the default; passing --view switches to
    // whole-view mode and the module options are ignored.
    std::string module = "prospecting";
    int depth = -1;              // excavation: which layer the shaft works
    std::string tab = "sweep";
    std::string state = "analyzed";
    int tier = 2;
    int width = 1280;
    int height = 720;
    int spriteSize = 4;
    int spriteGlow = 3;
    float energy = -1.0f;   // <0 = leave the unit's default
    int bench = 0;
    int hover = -1;
    int mouseX = -1, mouseY = -1;          // >0 = time this many frames, print ms/frame
    std::string outPath = "preview.png";

    // View mode (--view): empty means panel mode
    std::string view;
    int cellX = 10;    // planet grid cell for --view sect
    int cellY = 10;
    std::string tune;  // named terrain tuning preset (sect view)
};

static void PrintUsage()
{
    std::cout
        << "Usage: colony_preview [options]\n"
        << "\n"
        << "  --unit <type>     Extraction | Farming | Energy | Manufacture | Research |\n"
        << "                    Construction | Transport | Communication | Core\n"
        << "                    (default: Extraction)\n"
        << "  --module <name>   Extraction:  prospecting | excavation | beneficiation |\n"
        << "                                 operations | directives\n"
        << "                    Farming:     irrigation | greenhouse | hydroponics |\n"
        << "                                 harvest | storage\n"
        << "                    Energy:      solar | battery | nuclear | grid | emergency\n"
        << "                    Manufacture: fabrication | assembly | quality |\n"
        << "                                 logistics | automation\n"
        << "                    Research:    laboratory | analysis | simulation |\n"
        << "                                 archive | publication\n"
        << "                    Construction: siteprep | foundation | structures |\n"
        << "                                 fitout | maintenance\n"
        << "                    Transport:   fleet | routing | depot | servicing |\n"
        << "                                 dispatch\n"
        << "                    Communication: antenna | relay | telemetry |\n"
        << "                                 encryption | network\n"
        << "                    Core:        lifesupport | roster | command |\n"
        << "                                 monitoring | safety\n"
        << "                    Also: overview | sprites | strata (default: prospecting)\n"
        << "  --sprite-size <n> crystal sprite size variant     (sprites only, default: 4)\n"
        << "  --sprite-glow <n> crystal sprite glow variant     (sprites only, default: 3)\n"
        << "  --tab <name>      sweep | samples | lab          (prospecting only)\n"
        << "  --state <name>    empty | swept | sampled | analyzed | line | line-early |\n                    line-pull | line-done | trip\n"
        << "  --tier <0-3>      module tier to preview         (default: 2)\n"
        << "  --energy <n>      override stored energy (tests cost gating)\n"
        << "  --hover <0-3>     light a plate as if hovered (headless: no pointer)\n"
        << "  --depth <0-3>     excavation: the depth layer the shaft works\n"
        << "  --mouse <X,Y>     park the real pointer here -- exercises the true\n"
        << "                    hover path (pick, cursors, ground readout)\n"
        << "  --size <WxH>      output resolution              (default: 1280x720)\n"
        << "  --out <path>      output PNG path                (default: preview.png)\n"
        << "  --help            show this message\n"
        << "\n"
        << "View mode (renders a whole game view instead of a module panel):\n"
        << "\n"
        << "  --view <name>     orbital | planet | sect\n"
        << "  --cell <X,Y>      planet grid cell for sect view (default: 10,10)\n"
        << "  --tune <name>     terrain preset: baseline|silky|rough|rolling|\n"
        << "                    boulders|dramatic              (sect view)\n";
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
        else if (arg == "--unit" && hasNext)
        {
            options.unitType = argv[++i];
        }
        else if (arg == "--module" && hasNext)
        {
            options.module = argv[++i];
        }
        else if (arg == "--tab" && hasNext)
        {
            options.tab = argv[++i];
        }
        else if (arg == "--state" && hasNext)
        {
            options.state = argv[++i];
        }
        else if (arg == "--tier" && hasNext)
        {
            options.tier = TextToInteger(argv[++i]);
        }
        else if (arg == "--sprite-size" && hasNext)
        {
            options.spriteSize = TextToInteger(argv[++i]);
        }
        else if (arg == "--sprite-glow" && hasNext)
        {
            options.spriteGlow = TextToInteger(argv[++i]);
        }
        else if (arg == "--energy" && hasNext)
        {
            options.energy = static_cast<float>(TextToInteger(argv[++i]));
        }
        else if (arg == "--mouse" && hasNext)
        {
            std::string v = argv[++i];
            size_t comma = v.find(',');
            if (comma != std::string::npos)
            {
                options.mouseX = TextToInteger(v.substr(0, comma).c_str());
                options.mouseY = TextToInteger(v.substr(comma + 1).c_str());
            }
        }
        else if (arg == "--hover" && hasNext)
        {
            options.hover = TextToInteger(argv[++i]);
        }
        else if (arg == "--depth" && hasNext)
        {
            options.depth = TextToInteger(argv[++i]);
        }
        else if (arg == "--bench" && hasNext)
        {
            options.bench = TextToInteger(argv[++i]);
        }
        else if (arg == "--view" && hasNext)
        {
            options.view = argv[++i];
        }
        else if (arg == "--tune" && hasNext)
        {
            options.tune = argv[++i];
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
        else if (arg == "--out" && hasNext)
        {
            options.outPath = argv[++i];
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

// Maps a --module name onto the moduleType string used by the Unit module list.
// Names are unique across every unit type, so --unit only selects which unit is
// constructed; it does not disambiguate the module name.
static std::string ModuleTypeFromName(const std::string& name)
{
    // Extraction
    if (name == "prospecting") return "PROSPECTING";
    if (name == "excavation") return "EXCAVATION";
    if (name == "beneficiation") return "BENEFICIATION";
    if (name == "operations") return "OPERATIONS";
    if (name == "directives") return "DIRECTIVES";

    // Farming
    if (name == "irrigation") return "IRRIGATION";
    if (name == "greenhouse") return "GREENHOUSE";
    if (name == "hydroponics") return "HYDROPONICS";
    if (name == "harvest") return "HARVEST";
    if (name == "storage") return "STORAGE";

    // Energy
    if (name == "solar") return "SOLAR_ARRAY";
    if (name == "battery") return "BATTERY";
    if (name == "nuclear") return "NUCLEAR";
    if (name == "grid") return "GRID";
    if (name == "emergency") return "EMERGENCY";

    // Manufacture
    if (name == "fabrication") return "FABRICATION";
    if (name == "assembly") return "ASSEMBLY";
    if (name == "quality") return "QUALITY";
    if (name == "logistics") return "LOGISTICS";
    if (name == "automation") return "AUTOMATION";

    // Research
    if (name == "laboratory") return "LABORATORY";
    if (name == "analysis") return "ANALYSIS";
    if (name == "simulation") return "SIMULATION";
    if (name == "archive") return "ARCHIVE";
    if (name == "publication") return "PUBLICATION";

    // Construction
    if (name == "siteprep") return "SITE_PREP";
    if (name == "foundation") return "FOUNDATION";
    if (name == "structures") return "STRUCTURES";
    if (name == "fitout") return "FITOUT";
    if (name == "maintenance") return "MAINTENANCE";

    // Transport
    if (name == "fleet") return "FLEET";
    if (name == "routing") return "ROUTING";
    if (name == "depot") return "DEPOT";
    if (name == "servicing") return "SERVICING";
    if (name == "dispatch") return "DISPATCH";

    // Communication
    if (name == "antenna") return "ANTENNA";
    if (name == "relay") return "RELAY";
    if (name == "telemetry") return "TELEMETRY";
    if (name == "encryption") return "ENCRYPTION";
    if (name == "network") return "NETWORK";

    // Core
    if (name == "lifesupport") return "LIFE_SUPPORT";
    if (name == "roster") return "ROSTER";
    if (name == "command") return "COMMAND";
    if (name == "monitoring") return "MONITORING";
    if (name == "safety") return "SAFETY";

    return "";
}

static int FindModuleIndex(Unit& unit, const std::string& moduleType)
{
    const auto& modules = unit.GetModules();
    for (size_t i = 0; i < modules.size(); i++)
    {
        if (modules[i].moduleType == moduleType) return static_cast<int>(i);
    }
    return -1;
}

static ProspectingTab TabFromName(const std::string& name)
{
    if (name == "samples") return ProspectingTab::SAMPLES;
    if (name == "lab") return ProspectingTab::LAB;
    return ProspectingTab::SWEEP;
}

// Drives the prospecting system into a representative state so panels have
// something to draw. Each state builds on the previous one.
static void ApplyProspectingState(ProspectingSystem& system, const std::string& state)
{
    if (state == "empty") return;

    ProspectingGrid& grid = system.GetGrid();
    SampleTray& tray = system.GetTray();
    float gameTime = system.gameTime;

    // "worked": ground excavation has dug, at varying depths. Distinct from
    // surveyed ground -- a dug spot is known for certain AND partly emptied,
    // and the panel must show those as different things.
    if (state == "worked")
    {
        int size = grid.GetGridSize();
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                if (!IsSubCellInReach(x, y, 3)) continue;

                // A spread: some spots barely scratched, some worked out down
                // the column, most untouched.
                int layers = (x * 3 + y * 5) % 7;
                if (layers > 4) layers = 0;
                for (int d = 0; d < layers && d < 4; d++)
                {
                    grid.RecordExcavation(x, y, static_cast<DepthLayer>(d),
                                          0.3f + 0.2f * ((x + y) % 4));
                }
            }
        }
        return;
    }

    // "swept" and beyond: run the LIBS sweep (single band -- GPR is gone) so
    // the surface layer's prior carries the lateral chemistry pattern.
    if (system.GetSweep().CanSweep(grid, 0))
    {
        system.GetSweep().ExecuteSweep(grid, 0, gameTime);
    }

    if (state == "swept") return;

    // "trip": the same hole driven FLAT OUT until the bit fractures, captured
    // mid-trip -- string partway out of the hole, wear bar full-red history,
    // the core log holding what was already cut.
    if (state == "trip")
    {
        system.StartAim(4, 10);
        system.AimAt(3, 10, 4);
        system.CommitHole();
        for (int i = 0; i < 6000 && !system.lineHole.tripping; i++)
        {
            system.KickString();
            system.UpdateLineHole(0.1f);
        }
        float hold = system.lineHole.tripDur * 0.4f;
        for (float t = 0.0f; t < hold; t += 0.1f)
        {
            system.UpdateLineHole(0.1f);
        }
        return;
    }

    // "line": the prescribed line, mid-drill -- collar C6, aimed across the
    // shoot, string in the fractured zone. "line-done": the same hole
    // finished, specimen shelved.
    // "line-early": the bit still in the REGOLITH plate, which carries the
    // shoot's relief -- the case that proves the active-plate rim hugs a
    // LIFTED surface, not just a flat one.
    if (state == "line-early")
    {
        system.StartAim(4, 10);
        system.AimAt(3, 10, 4);
        system.CommitHole();
        int step = 0;
        for (float t = 0.0f; t < 4.0f; t += 0.1f)
        {
            if (step++ % 3 == 0) system.KickString();
            system.UpdateLineHole(0.1f);
        }
        return;
    }

    // "line-pull": the hole is finished and the string is coming back out --
    // caught mid-hoist, the one frame where the line over the plates is still
    // drawn but is retreating up it. "line-done" is the same hole a few
    // seconds later, string racked and the line gone from the block model.
    if (state == "line-pull")
    {
        system.StartAim(4, 10);
        system.AimAt(3, 10, 4);
        system.CommitHole();
        int step = 0;
        for (int i = 0; i < 20000 &&
                        system.lineHole.state == LineHoleState::DRILLING; i++)
        {
            if (step++ % 3 == 0) system.KickString();
            system.UpdateLineHole(0.1f);
        }
        float hold = system.lineHole.pullDur * 0.45f;
        for (float t = 0.0f; t < hold; t += 0.1f) system.UpdateLineHole(0.1f);
        return;
    }

    if (state == "line" || state == "line-done")
    {
        system.StartAim(4, 10);
        system.AimAt(3, 10, 4);
        system.CommitHole();
        // Drive like an engaged player: idle is a bare crawl by design, so
        // kick the string at ~4 clicks/s while it runs.
        float total = (state == "line") ? 30.0f : 240.0f;
        int step = 0;
        for (float t = 0.0f; t < total; t += 0.1f)
        {
            if (step++ % 3 == 0) system.KickString();
            system.UpdateLineHole(0.1f);
        }
        return;
    }


    // "sampled": the first two holes of a campaign. Vertical auger columns --
    // each cores everything from the surface down to its target, so a MID
    // hole classifies three points of its column at once and an INDICATED
    // halo grows around each hole.
    system.GetSampler().CollectSample(grid, tray, 2, 2, DepthLayer::MID);
    system.GetSampler().CollectSample(grid, tray, 5, 4, DepthLayer::SHALLOW);

    if (state == "sampled") return;

    // "analyzed": a drilled-out campaign -- the state the whole design aims
    // at. Step-out holes at varied depths: Measured columns, Indicated halos
    // merging between neighbouring holes, Inferred fringes, and deep ground
    // still a bet where nothing reached it. (The name is kept so preview.sh
    // and its callers need no change; the lab this state once drove is gone.)
    system.GetSampler().CollectSample(grid, tray, 3, 3, DepthLayer::DEEP);
    system.GetSampler().CollectSample(grid, tray, 5, 2, DepthLayer::MID);
    system.GetSampler().CollectSample(grid, tray, 1, 5, DepthLayer::SHALLOW);
    system.GetSampler().CollectSample(grid, tray, 6, 6, DepthLayer::MID);
    system.GetSampler().CollectSample(grid, tray, 0, 1, DepthLayer::SURFACE);
}

static int RenderSpriteSheet(const PreviewOptions& options)
{
    const char* spriteRoot = "src/assets/sprites/samples";

    if (!DirectoryExists(spriteRoot))
    {
        std::cout << "No sprite directory at " << spriteRoot << "\n";
        return 1;
    }

    // Collect family -> shape directories.
    std::vector<std::string> families;
    FilePathList familyList = LoadDirectoryFiles(spriteRoot);
    for (unsigned int i = 0; i < familyList.count; i++)
    {
        if (!IsPathFile(familyList.paths[i])) families.push_back(familyList.paths[i]);
    }
    UnloadDirectoryFiles(familyList);
    std::sort(families.begin(), families.end());

    std::vector<std::vector<std::string>> shapesByFamily;
    size_t maxShapes = 0;
    for (const std::string& family : families)
    {
        std::vector<std::string> shapes;
        FilePathList shapeList = LoadDirectoryFiles(family.c_str());
        for (unsigned int i = 0; i < shapeList.count; i++)
        {
            if (!IsPathFile(shapeList.paths[i])) shapes.push_back(shapeList.paths[i]);
        }
        UnloadDirectoryFiles(shapeList);
        std::sort(shapes.begin(), shapes.end());
        maxShapes = std::max(maxShapes, shapes.size());
        shapesByFamily.push_back(shapes);
    }

    if (families.empty() || maxShapes == 0)
    {
        std::cout << "No sprite families found under " << spriteRoot << "\n";
        return 1;
    }

    const int cellSize = 150;
    const int labelHeight = 18;
    const int margin = 20;
    const int headerHeight = 40;

    int sheetWidth = margin * 2 + static_cast<int>(maxShapes) * cellSize;
    int sheetHeight = headerHeight + margin +
                      static_cast<int>(families.size()) * (cellSize + labelHeight);

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(sheetWidth, sheetHeight, "Crystal Sprite Sheet");

    int status = 0;
    {
        // Load the requested variant from every shape directory.
        std::string variant = TextFormat("size_%d_glow_%d.png",
                                          options.spriteSize, options.spriteGlow);

        // Load every sprite up front. Textures must stay alive until the render
        // batch is flushed at EndTextureMode(), so they cannot be unloaded
        // inline -- doing so draws whatever texture is still resident instead.
        struct SheetEntry
        {
            Texture2D texture;
            std::string label;
            int cellX;
            int cellY;
        };

        std::vector<SheetEntry> entries;

        for (size_t f = 0; f < families.size(); f++)
        {
            int rowY = headerHeight + margin + static_cast<int>(f) * (cellSize + labelHeight);

            for (size_t s = 0; s < shapesByFamily[f].size(); s++)
            {
                std::string file = shapesByFamily[f][s] + "/" + variant;
                if (!FileExists(file.c_str())) continue;

                SheetEntry entry;
                entry.texture = LoadTexture(file.c_str());
                entry.label = GetFileName(shapesByFamily[f][s].c_str());
                entry.cellX = margin + static_cast<int>(s) * cellSize;
                entry.cellY = rowY;
                entries.push_back(entry);
            }
        }

        RenderTexture2D target = LoadRenderTexture(sheetWidth, sheetHeight);

        BeginTextureMode(target);
        ClearBackground({18, 18, 30, 255});

        DrawText(TextFormat("Crystal sample sprites  -  %s", variant.c_str()),
                 margin, 14, 18, {200, 220, 255, 255});

        for (const SheetEntry& entry : entries)
        {
            // Fit the sprite inside the cell, preserving aspect ratio.
            float scale = std::min(static_cast<float>(cellSize - 10) / entry.texture.width,
                                   static_cast<float>(cellSize - 10) / entry.texture.height);
            float drawW = entry.texture.width * scale;
            float drawH = entry.texture.height * scale;

            DrawTextureEx(entry.texture,
                          {entry.cellX + (cellSize - drawW) / 2.0f,
                           entry.cellY + (cellSize - drawH) / 2.0f},
                          0.0f, scale, WHITE);

            DrawText(entry.label.c_str(), entry.cellX + 6, entry.cellY + cellSize, 11,
                     {150, 165, 190, 255});
        }

        EndTextureMode();

        for (SheetEntry& entry : entries) UnloadTexture(entry.texture);

        Image sheet = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&sheet);  // render textures are stored bottom-up
        bool exported = ExportImage(sheet, options.outPath.c_str());
        UnloadImage(sheet);
        UnloadRenderTexture(target);

        if (exported)
        {
            std::cout << "Wrote " << options.outPath << " (" << families.size()
                      << " families, variant " << variant << ")\n";
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

// --- Just the drill bar --------------------------------------------------
// The four strata textures as the borehole strip wears them, plus each tile
// raw at 1:1 so the grain can be judged at the size it was generated. These
// are the ACTUAL pixels the game draws with -- same generator, same tint law
// (x2 against a mean of 128), same near-1:1 tiling scale as the dock.
static int RenderStrataSheet(const PreviewOptions& options)
{
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(options.width, options.height, "Strata textures");

    int status = 0;
    {
        const Color rock[4] = {{58,52,43,255},{69,62,52,255},{57,66,77,255},{39,42,48,255}};
        const Color edge[4] = {{25,21,16,255},{28,23,18,255},{22,28,35,255},{16,18,22,255}};
        const char* names[4]  = {"REGOLITH", "MEGAREGOLITH", "FRACTURED", "BASALT"};
        const char* depths[4] = {"0 - 12 m", "12 - 34 m", "34 - 68 m", "68 - 120 m"};
        const char* note[4]   = {"impact-gardened soil: fine grain, broad mottle, angular grit",
                                 "coarse breccia: poorly sorted blocks, dark seams between",
                                 "fractured rock: calm slabs cut by fractures, some ice-filled",
                                 "dense lava: near-uniform, vesicles, faint columnar joints"};

        Texture2D tex[4] = {};
        for (int L = 0; L < 4; L++)
        {
            std::vector<unsigned char> px = RockTexture::Generate(L, RockTexture::SIZE);
            Image img = { px.data(), RockTexture::SIZE, RockTexture::SIZE, 1,
                          PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
            tex[L] = LoadTextureFromImage(img);
            SetTextureFilter(tex[L], TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(tex[L], TEXTURE_WRAP_REPEAT);
        }

        for (int frame = 0; frame < 2; frame++)
        {
            BeginDrawing();
            ClearBackground({10, 13, 20, 255});
            DrawText("BOREHOLE - four generated strata", 40, 24, 20, {200, 220, 245, 255});
            DrawText("one texture per rock; the block model's plates wear the same four",
                     40, 50, 13, {120, 140, 165, 255});

            const float barX = 44.0f, barW = 150.0f, barTop = 84.0f;
            const float bandH = 142.0f;
            const float k = static_cast<float>(RockTexture::SIZE) / 118.0f;

            // sky above the collar, as the dock has it
            DrawRectangleRec({barX, barTop - 22.0f, barW, 22.0f}, {10, 16, 24, 255});

            for (int L = 0; L < 4; L++)
            {
                float y0 = barTop + L * bandH;
                Color tint = { static_cast<unsigned char>(std::min(255, rock[L].r * 2)),
                               static_cast<unsigned char>(std::min(255, rock[L].g * 2)),
                               static_cast<unsigned char>(std::min(255, rock[L].b * 2)), 255 };
                DrawTexturePro(tex[L], {0.0f, L * 41.0f, barW * k, bandH * k},
                               {barX, y0, barW, bandH}, {0.0f, 0.0f}, 0.0f, tint);
                DrawRectangleRec({barX, y0, barW, 2.0f}, edge[L]);

                float tx = barX + barW + 26.0f;
                DrawText(names[L], static_cast<int>(tx), static_cast<int>(y0 + 6.0f), 17,
                         {214, 226, 240, 255});
                DrawText(depths[L], static_cast<int>(tx), static_cast<int>(y0 + 28.0f), 13,
                         {120, 200, 235, 255});
                DrawText(note[L], static_cast<int>(tx), static_cast<int>(y0 + 48.0f), 12,
                         {120, 140, 165, 255});

                // the raw tile at 1:1, untinted, so the generated grain is
                // visible without the stratum colour over it
                float rx = static_cast<float>(options.width) - RockTexture::SIZE - 40.0f;
                DrawTextureRec(tex[L], {0.0f, 0.0f, static_cast<float>(RockTexture::SIZE),
                                        static_cast<float>(RockTexture::SIZE)},
                               {rx, y0 + 4.0f}, WHITE);
                DrawRectangleLines(static_cast<int>(rx), static_cast<int>(y0 + 4.0f),
                                   RockTexture::SIZE, RockTexture::SIZE, {60, 72, 88, 255});
            }
            DrawText("128 x 128, tileable", static_cast<int>(options.width) - RockTexture::SIZE - 40,
                     static_cast<int>(barTop - 18.0f), 12, {120, 140, 165, 255});
            EndDrawing();
        }

        Image shot = LoadImageFromScreen();
        bool exported = ExportImage(shot, options.outPath.c_str());
        UnloadImage(shot);
        for (int L = 0; L < 4; L++) UnloadTexture(tex[L]);

        if (exported) std::cout << "Wrote " << options.outPath << " (strata textures)\n";
        else { std::cout << "Failed to write " << options.outPath << "\n"; status = 1; }
    }

    CloseWindow();
    return status;
}

// Renders a whole game view (--view) rather than a single module panel.
static int RenderGameView(const PreviewOptions& options)
{
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
        ResourceManager resourceManager(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
        Sect* sect = nullptr;
        if (options.view == "sect")
        {
            Vector2 sectPos = {
                (options.cellX + 0.5f) * SECT_CORE_RADIUS * 2.0f,
                (options.cellY + 0.5f) * SECT_CORE_RADIUS * 2.0f};
            sect = new Sect(sectPos, resourceManager, timeManager);
            double lat, lon;
            TerrainGridCellToLatLon(options.cellX, options.cellY, &lat, &lon);
            std::cout << "Sect on cell (" << options.cellX << ","
                      << options.cellY << ") -> lat " << lat
                      << ", lon " << lon << "\n";
            // Raw terrain dump alongside the composed view, for style
            // comparison. Named presets vary the non-crater surface layers.
            TerrainTuning tune;
            if (options.tune == "silky")
            {
                tune.grain = 0.5f; tune.undulation = 0.6f;
                tune.boulders = 0.0f; tune.speckle = 0.5f;
                tune.relWeight = 0.30f; tune.lightWeight = 0.45f;
                tune.sCurve = 0.12f;
            }
            else if (options.tune == "rough")
            {
                tune.grain = 2.2f; tune.undulation = 1.2f;
                tune.boulders = 1.5f; tune.boulderAmp = 1.2f;
                tune.speckle = 1.6f;
            }
            else if (options.tune == "rolling")
            {
                tune.grain = 0.7f; tune.undulation = 2.8f;
                tune.boulders = 0.4f; tune.relWeight = 0.50f;
                tune.speckle = 0.8f;
            }
            else if (options.tune == "boulders")
            {
                tune.grain = 0.9f; tune.undulation = 0.8f;
                tune.boulders = 4.0f; tune.boulderAmp = 1.6f;
                tune.speckle = 1.1f;
            }
            else if (options.tune == "dramatic")
            {
                tune.grain = 1.4f; tune.undulation = 1.6f;
                tune.formRelief = 1.5f; tune.relWeight = 0.55f;
                tune.lightWeight = 0.75f; tune.sCurve = 0.40f;
                tune.boulders = 1.0f; tune.speckle = 1.2f;
            }
            Image ground = GenerateSectTerrain(lat, lon, 512, &tune);
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

int main(int argc, char** argv)
{
    PreviewOptions options;
    if (!ParseArgs(argc, argv, options)) return 0;

    if (!options.view.empty()) return RenderGameView(options);

    if (options.module == "sprites") return RenderSpriteSheet(options);
    if (options.module == "strata") return RenderStrataSheet(options);

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(options.width, options.height, "Colony UI Preview");

    // Everything holding GPU resources lives in this scope so it is destroyed
    // while the GL context is still alive. Destructing after CloseWindow()
    // unloads fonts and textures against a dead context and segfaults.
    int status = 0;
    {
    RenderManager renderManager(options.width, options.height);
    renderManager.LoadFonts();

    // The constructor only allocates the grids; Planet normally calls this to
    // populate them. Without it the whole map is empty and every sample reads
    // 0% richness. A fixed seed keeps previews reproducible -- the default
    // seed (0) uses random_device, which would make every screenshot show a
    // different planet and defeat visual comparison.
    ResourceManager resourceManager(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    resourceManager.GenerateResourceMap(PREVIEW_MAP_SEED);
    TimeManager timeManager;

    // Place the unit mid-grid so it samples a populated resource cell.
    Vector2 unitPosition = {
        SECT_CORE_RADIUS * 2.0f * 5.0f,
        SECT_CORE_RADIUS * 2.0f * 5.0f
    };

    std::map<ResourceType, float> storage;
    std::map<ResourceType, float> capacity;

    Unit unit(options.unitType, unitPosition, resourceManager, timeManager, storage, capacity);

    // Stock the build materials a module needs, so --tier reaches built and
    // upgraded states. Without this every preview would show NOT BUILT with an
    // unaffordable cost list, which is not what most previews are checking.
    for (ResourceType material : {ResourceType::CONSTRUCTION_MATERIALS, ResourceType::MACHINERY,
                                  ResourceType::ELECTRONICS, ResourceType::ALLOYS,
                                  ResourceType::Fe, ResourceType::Si, ResourceType::WATER})
    {
        unit.AddResource(material, 5000.0f);
    }

    if (options.energy >= 0.0f)
    {
        storage[ResourceType::ENERGY] = options.energy;
    }

    std::string moduleType = ModuleTypeFromName(options.module);

    if (moduleType.empty())
    {
        // "overview" (or an unrecognised name) renders the unit resource overview.
        unit.SetIsInModuleView(false);
    }
    else
    {
        int moduleIndex = FindModuleIndex(unit, moduleType);
        if (moduleIndex < 0)
        {
            std::cout << "No module of type " << moduleType << " on this unit.\n";
            status = 1;
        }
        else
        {

        // Modules 3 and 4 of each unit start unbuilt. Build before upgrading so
        // the preview shows a coherent state -- otherwise the panel reports a
        // tier-2 energy draw next to a NOT BUILT badge and a locked tier arc.
        // Requesting --tier 0 leaves an unbuilt module alone, which is how the
        // not-built state is previewed.
        if (options.tier > 0 && !unit.GetModules()[moduleIndex].isBuilt)
        {
            unit.PublicBuildModule(moduleIndex);
        }

        for (int t = 0; t < options.tier; t++)
        {
            unit.DebugUpgradeModuleTier(moduleIndex);
        }

        unit.ActivateModule(moduleIndex);
        unit.SetSelectedModuleIndex(moduleIndex);
        unit.SetIsInModuleView(true);

        // Ground state is a property of the WORLD, not of whichever panel is
        // being previewed. Excavation reads the same grid prospecting writes,
        // so its panel needs the state applied too -- surveyed ground is the
        // whole point of comparing it against unsurveyed.
        if (unit.HasProspectingSystem())
        {
            ProspectingSystem* system = unit.GetProspectingSystem();

            // The prospecting rig needs its own tier to survey deeply, and in
            // an excavation preview nothing else raises it.
            if (moduleType != "PROSPECTING")
            {
                for (size_t i = 0; i < unit.GetModules().size(); i++)
                {
                    if (unit.GetModules()[i].moduleType != "PROSPECTING") continue;
                    for (int t = 0; t < options.tier; t++)
                    {
                        unit.DebugUpgradeModuleTier(static_cast<int>(i));
                    }
                    break;
                }
            }

            system->activeTab = TabFromName(options.tab);
            ApplyProspectingState(*system, options.state);
            // Headless: there is no pointer to put on a plate, so the hover
            // is handed to the panel directly.
            system->previewHoverLayer = options.hover;

            // Select a cell inside instrument reach, so the cell readout shows
            // real data rather than an out-of-range cell.
            int centre = system->GetGrid().GetGridSize() / 2;
            system->selectedCellX = centre;
            system->selectedCellY = centre;
            if (system->GetTray().GetCount() > 0) system->selectedSampleIndex = 0;
        }

        // Excavation's own state. Depth is what the shaft dock draws against,
        // so it needs a headless setter for the same reason the block model
        // needed previewHoverLayer: there is no pointer to click a depth with.
        if (unit.HasExcavationSystem() && options.depth >= 0)
        {
            unit.GetExcavationSystem()->selectedDepth =
                static_cast<DepthLayer>(std::clamp(options.depth, 0, 3));
        }

        // Run the module for a few ticks so the panel shows a rig that is
        // WORKING rather than one that has never been asked to. lastResult is
        // what drives the bit's spin, its heat and the readout, and it is empty
        // until something digs -- so an excavation preview was previously a
        // still life of an idle machine, which is not the state worth judging.
        if (moduleType == "EXCAVATION" && unit.HasExcavationSystem() &&
            unit.HasProspectingSystem())
        {
            ExcavationSystem* exc = unit.GetExcavationSystem();
            for (int t = 0; t < 6; t++)
            {
                exc->Dig(*unit.GetProspectingSystem(), 1, 1.0f, 0.5f);
            }
        }
        }
    }

    if (status == 0)
    {
        // Draw twice: the first frame lets fonts and textures settle before capture.
        for (int frame = 0; frame < 2; frame++)
        {
            // Park the pointer before the frame reads it, so the hover paths
            // run exactly as they do under a hand.
            if (options.mouseX >= 0) SetMousePosition(options.mouseX, options.mouseY);
            BeginDrawing();
            ClearBackground(BLACK);
            renderManager.DrawUnitView(&unit, timeManager);
            EndDrawing();
        }

        // Frame-cost instrument: the panel math runs on the CPU, so ms/frame
        // here tracks what the wasm build feels like (times a wasm penalty).
        if (options.bench > 0)
        {
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < options.bench; i++)
            {
                BeginDrawing();
                ClearBackground(BLACK);
                renderManager.DrawUnitView(&unit, timeManager);
                EndDrawing();
            }
            double ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - t0).count();
            std::cout << "bench: " << options.bench << " frames, "
                      << ms / options.bench << " ms/frame\n";
        }

        Image screenshot = LoadImageFromScreen();
        bool exported = ExportImage(screenshot, options.outPath.c_str());
        UnloadImage(screenshot);

        if (exported)
        {
            std::cout << "Wrote " << options.outPath
                      << " (unit=" << options.unitType
                      << " module=" << options.module
                      << " tab=" << options.tab
                      << " state=" << options.state
                      << " tier=" << options.tier << ")\n";
        }
        else
        {
            std::cout << "Failed to write " << options.outPath << "\n";
            status = 1;
        }
    }
    }

    CloseWindow();
    return status;
}
