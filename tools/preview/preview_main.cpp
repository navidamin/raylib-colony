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
        << "  --state <name>    empty | swept | sampled | analyzed\n"
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

    // "swept" and beyond: run GPR sweeps so the heat map has signal.
    // Band 0 is left unswept so the RUN SWEEP button previews in its
    // enabled state.
    int bandCount = SWEEP_FREQUENCY_BANDS;
    for (int band = 1; band < bandCount; band++)
    {
        if (system.GetSweep().CanSweep(grid, band))
        {
            system.GetSweep().ExecuteSweep(grid, band, gameTime);
        }
    }

    if (state == "swept") return;

    // "sampled" and beyond: collect a spread of samples across cells and depths.
    const DepthLayer depths[] = {
        DepthLayer::SURFACE, DepthLayer::SHALLOW, DepthLayer::MID, DepthLayer::DEEP
    };

    int gridSize = grid.GetGridSize();
    int collected = 0;
    for (int y = 0; y < gridSize && !tray.IsFull(); y++)
    {
        for (int x = 0; x < gridSize && !tray.IsFull(); x++)
        {
            DepthLayer depth = depths[collected % 4];
            if (!system.GetSampler().CanDrill(depth)) depth = DepthLayer::SURFACE;

            if (system.GetSampler().CollectSample(grid, tray, x, y, depth))
            {
                collected++;
            }
        }
    }

    if (state == "sampled") return;

    // "analyzed": run a thorough lab workup on every tray sample -- every
    // available tool, ending with the destructive Fire Assay.
    const AnalysisTool toolOrder[] = {
        AnalysisTool::VISUAL_INSPECTION,
        AnalysisTool::OPTICAL_MICROSCOPY,
        AnalysisTool::MAGNETIC_SUSCEPTIBILITY,
        AnalysisTool::XRF,
        AnalysisTool::LIBS_PULSE,
        AnalysisTool::FIRE_ASSAY,
    };

    std::vector<Sample>& samples = tray.GetSamples();
    for (Sample& sample : samples)
    {
        for (AnalysisTool tool : toolOrder)
        {
            if (system.GetLab().CanApplyTool(sample, tool))
            {
                system.GetLab().ApplyTool(sample, tool, gameTime);
            }
        }
    }
}

// Renders a contact sheet of the pre-rendered crystal sample sprites, one row
// per shape family. These assets live in src/assets/sprites/samples/ but are
// not currently drawn by the game, so this is the only way to review them.
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
