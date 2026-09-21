#include "lunar_dem_shared.h"

#include "raylib.h"

#include <string>

namespace
{

const char* DEFAULT_PATH = "prototypes/planet_visuals/data/lola/ldem_16_uint.tif";

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
            // Overlays live beside the global model. A path with no
            // directory part means the working directory.
            size_t slash = g_path.find_last_of("/\\");
            std::string dir = (slash == std::string::npos) ? "."
                                                            : g_path.substr(0, slash);
            int n = g_dem.LoadOverlays(dir);
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
