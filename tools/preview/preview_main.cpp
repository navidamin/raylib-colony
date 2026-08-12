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

#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct PreviewOptions
{
    std::string module = "prospecting";
    std::string tab = "sweep";
    std::string state = "analyzed";
    int tier = 2;
    int width = 1280;
    int height = 720;
    std::string outPath = "preview.png";
};

static void PrintUsage()
{
    std::cout
        << "Usage: colony_preview [options]\n"
        << "\n"
        << "  --module <name>   prospecting | excavation | beneficiation | operations |\n"
        << "                    directives | overview          (default: prospecting)\n"
        << "  --tab <name>      sweep | samples | lab          (prospecting only)\n"
        << "  --state <name>    empty | swept | sampled | analyzed\n"
        << "  --tier <0-3>      module tier to preview         (default: 2)\n"
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

    // "swept" and beyond: run GPR sweeps so the heat map has signal.
    int bandCount = SWEEP_FREQUENCY_BANDS;
    for (int band = 0; band < bandCount; band++)
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

    // "analyzed": push every tray sample through the best available lab preset.
    const std::vector<LabPreset>& presets = LabEngine::GetPresets();
    std::vector<Sample>& samples = tray.GetSamples();

    for (Sample& sample : samples)
    {
        for (int p = static_cast<int>(presets.size()) - 1; p >= 0; p--)
        {
            if (system.GetLab().CanApplyPreset(sample, p))
            {
                system.GetLab().ApplyPreset(sample, p, gameTime);
                break;
            }
        }
    }
}

int main(int argc, char** argv)
{
    PreviewOptions options;
    if (!ParseArgs(argc, argv, options)) return 0;

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(options.width, options.height, "Colony UI Preview");

    // Everything holding GPU resources lives in this scope so it is destroyed
    // while the GL context is still alive. Destructing after CloseWindow()
    // unloads fonts and textures against a dead context and segfaults.
    int status = 0;
    {
    RenderManager renderManager(options.width, options.height);
    renderManager.LoadFonts();

    ResourceManager resourceManager(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    TimeManager timeManager;

    // Place the unit mid-grid so it samples a populated resource cell.
    Vector2 unitPosition = {
        SECT_CORE_RADIUS * 2.0f * 5.0f,
        SECT_CORE_RADIUS * 2.0f * 5.0f
    };

    std::map<ResourceType, float> storage;
    std::map<ResourceType, float> capacity;

    Unit unit("Extraction", unitPosition, resourceManager, timeManager, storage, capacity);

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

        if (moduleType == "PROSPECTING" && unit.HasProspectingSystem())
        {
            ProspectingSystem* system = unit.GetProspectingSystem();
            system->activeTab = TabFromName(options.tab);

            ApplyProspectingState(*system, options.state);

            // Select the first cell and sample so detail views have a subject.
            system->selectedCellX = 0;
            system->selectedCellY = 0;
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
