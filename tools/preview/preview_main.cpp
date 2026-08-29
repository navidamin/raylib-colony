// Offscreen UI preview tool.
//
// Renders a single unit module panel to a PNG without running the game loop,
// so UI work can be reviewed headlessly (CI, containers, Claude Code sessions).
// It drives the real RenderManager against a real Unit, so what it exports is
// what the game draws -- there is no second implementation to drift.
//
// See tools/preview/README.md for usage.

#include "raylib.h"

#include "rendermanager.h"
#include "unit.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "game_constants.h"
#include "game_enums.h"

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
    std::string module = "prospecting";
    std::string tab = "sweep";
    std::string state = "analyzed";
    int tier = 2;
    int width = 1280;
    int height = 720;
    int spriteSize = 4;
    int spriteGlow = 3;
    float energy = -1.0f;   // <0 = leave the unit's default
    int bench = 0;          // >0 = time this many frames, print ms/frame
    std::string outPath = "preview.png";
};

static void PrintUsage()
{
    std::cout
        << "Usage: colony_preview [options]\n"
        << "\n"
        << "  --module <name>   prospecting | excavation | beneficiation | operations |\n"
        << "                    directives | overview | sprites (default: prospecting)\n"
        << "  --sprite-size <n> crystal sprite size variant     (sprites only, default: 4)\n"
        << "  --sprite-glow <n> crystal sprite glow variant     (sprites only, default: 3)\n"
        << "  --tab <name>      sweep | samples | lab          (prospecting only)\n"
        << "  --state <name>    empty | swept | sampled | analyzed | line | line-done\n"
        << "  --tier <0-3>      module tier to preview         (default: 2)\n"
        << "  --energy <n>      override stored energy (tests cost gating)\n"
        << "  --size <WxH>      output resolution              (default: 1280x720)\n"
        << "  --out <path>      output PNG path                (default: preview.png)\n"
        << "  --help            show this message\n";
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
        else if (arg == "--bench" && hasNext)
        {
            options.bench = TextToInteger(argv[++i]);
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
static std::string ModuleTypeFromName(const std::string& name)
{
    if (name == "prospecting") return "PROSPECTING";
    if (name == "excavation") return "EXCAVATION";
    if (name == "beneficiation") return "BENEFICIATION";
    if (name == "operations") return "OPERATIONS";
    if (name == "directives") return "DIRECTIVES";
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

    // "line": the prescribed line, mid-drill -- collar C6, aimed across the
    // shoot, string in the fractured zone. "line-done": the same hole
    // finished, specimen shelved.
    if (state == "line" || state == "line-done")
    {
        system.StartAim(2, 5);
        system.AimAt(3, 5, 2);
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

int main(int argc, char** argv)
{
    PreviewOptions options;
    if (!ParseArgs(argc, argv, options)) return 0;

    if (options.module == "sprites") return RenderSpriteSheet(options);

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

    Unit unit("Extraction", unitPosition, resourceManager, timeManager, storage, capacity);

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

            // Select a cell inside instrument reach, so the cell readout shows
            // real data rather than an out-of-range cell.
            int centre = system->GetGrid().GetGridSize() / 2;
            system->selectedCellX = centre;
            system->selectedCellY = centre;
            if (system->GetTray().GetCount() > 0) system->selectedSampleIndex = 0;
        }
        }
    }

    if (status == 0)
    {
        // Draw twice: the first frame lets fonts and textures settle before capture.
        for (int frame = 0; frame < 2; frame++)
        {
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
                      << " (module=" << options.module
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
