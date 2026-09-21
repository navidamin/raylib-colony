#include "lunar_dem_shared.h"

#include "raylib.h"

#include <string>

namespace
{

// The global model ships with the game's assets (every build that
// copies assets carries it, the web preload included). The optional
// high-resolution SLDEM overlays stay with the prototypes: 51 MB that
// only a desktop asks for.
const char* DEFAULT_PATH = "src/assets/planet/lola/ldem_16_uint.tif";
const char* OVERLAY_DIR = "prototypes/planet_visuals/data/lola";

std::string g_path = DEFAULT_PATH;
LolaDem g_dem;
bool g_tried = false;

} // namespace

const char* LunarDemDefaultPath()
{
    return DEFAULT_PATH;
}

void SetLunarDemPath(const char* path)
{
    if (path == nullptr || path[0] == '\0') return;
    if (g_tried)
    {
        TraceLog(LOG_WARNING, "DEM: path set to %s after the model was "
                              "already loaded from %s; ignored",
                 path, g_path.c_str());
        return;
    }
    g_path = path;
}

const LolaDem* GetLunarDem()
{
    if (!g_tried)
    {
        g_tried = true;
        if (g_dem.Load(g_path))
        {
            // Overlays beside the model (a --dem elsewhere brings its
            // own), and the prototypes' folder where fetch-dem puts them.
            size_t slash = g_path.find_last_of("/\\");
            std::string dir = (slash == std::string::npos) ? "."
                                                            : g_path.substr(0, slash);
            int n = g_dem.LoadOverlays(dir);
            if (dir != OVERLAY_DIR) n += g_dem.LoadOverlays(OVERLAY_DIR);
            if (n > 0)
            {
                TraceLog(LOG_INFO, "DEM: %d high-resolution overlay(s) active", n);
            }
        }
        else
        {
            TraceLog(LOG_WARNING, "DEM: %s not loaded; terrain rows and the "
                                  "site verdict fall back", g_path.c_str());
        }
    }
    return g_dem.IsLoaded() ? &g_dem : nullptr;
}
