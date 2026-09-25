// The port half of the DomeForge diff: renders through src/DomeForge/ with the
// same flags tools/domeforge/ref.js takes, and writes a PNG. No window needed.
//   domeforge_render --kind unit|central|roads|ground|base [--scale S] [--size N]
//                    [--color #rrggbb] [--socket-start DEG] [--socket-count N]
//                    [--set key=value ...] --out file.png
#include "domeforge.h"
#include "raylib.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv)
{
    std::string kind = "unit", out = "port.png";
    DomeForgeConfig cfg = DomeForgeDefaults();
    double scale = 1.0;
    int size = 0, socketCount = -1;
    double socketStart = -1e9;
    for (int i = 1; i + 1 < argc; i += 2)
    {
        const char* k = argv[i];
        const char* v = argv[i + 1];
        if (!strcmp(k, "--kind")) kind = v;
        else if (!strcmp(k, "--out")) out = v;
        else if (!strcmp(k, "--scale")) scale = atof(v);
        else if (!strcmp(k, "--size")) size = atoi(v);
        else if (!strcmp(k, "--color")) cfg.color = DomeForgeHex(v);
        else if (!strcmp(k, "--socket-start")) socketStart = atof(v);
        else if (!strcmp(k, "--socket-count")) socketCount = atoi(v);
        else if (!strcmp(k, "--set"))
        {
            // --set key=value, repeatable: any DomeForgeSetParam field (tuning; port only)
            const std::string kv = v;
            const size_t eq = kv.find('=');
            if (eq == std::string::npos || !DomeForgeSetParam(cfg, kv.substr(0, eq), kv.substr(eq + 1)))
                fprintf(stderr, "unknown --set %s\n", v);
        }
    }
    DomeForgeShape& shape = kind == "central" ? cfg.central : cfg.unit;
    if (size > 0) shape.size = size;
    if (socketStart > -1e8) shape.socketStart = socketStart;
    if (socketCount >= 0) shape.socketCount = socketCount;

    const auto t0 = std::chrono::steady_clock::now();
    DomeForgeImage img;
    if (kind == "unit") img = DomeForgeRender(cfg, DomeForgeKind::UNIT);
    else if (kind == "central") img = DomeForgeRender(cfg, DomeForgeKind::CENTRAL);
    else if (kind == "base") img = DomeForgeRenderBase(cfg, scale);
    else
    {
        const DomeForgeLayout lay = DomeForgeMakeLayout(cfg, scale);
        const int W = (int)(lay.A + 0.5);
        img = kind == "roads" ? DomeForgeRenderRoads(cfg, W, W, lay.prims, scale) : DomeForgeRenderGround(cfg, W, W, scale);
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    Image im = {img.rgba.data(), img.width, img.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    SetTraceLogLevel(LOG_WARNING);
    ExportImage(im, out.c_str());
    printf("port %s: %dx%d in %.0f ms -> %s\n", kind.c_str(), img.width, img.height, ms, out.c_str());
    return 0;
}
