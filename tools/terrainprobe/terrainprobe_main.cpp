// terrain_probe: build one location's terrain chain both ways and put
// the results side by side.
//
// Prints which path GetTerrainPath() would pick here and why, times
// the GPU chain (with the read-back that makes the GPU actually finish)
// and the CPU chain, writes every level of both as PNG, and reports
// per-level luminance statistics with the mean absolute difference
// between the two -- the number to watch when the shader drifts from
// the synthesizer it mirrors. Needs a GL context, so it opens a hidden
// window; run it from the repo root so src/assets resolves.
//
//   terrain_probe [--lat L] [--lon L] [--res N] [--out DIR]
//                 [--site 0|1] [--path cpu|gpu|both]

#include "raylib.h"
#include "rlgl.h"
#include "terrain_synthesis.h"
#include "terrain_gpu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{

struct Stats { double mean, stdev; };

Stats Luminance(const Image& img)
{
    Image g = ImageCopy(img);
    ImageFormat(&g, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const unsigned char* px = (const unsigned char*)g.data;
    size_t n = (size_t)g.width * g.height;
    double sum = 0.0, sq = 0.0;
    for (size_t i = 0; i < n; i++)
    {
        double l = (px[i * 4] + px[i * 4 + 1] + px[i * 4 + 2]) / (3.0 * 255.0);
        sum += l;
        sq += l * l;
    }
    UnloadImage(g);
    double mean = sum / n;
    return {mean, std::sqrt(std::max(0.0, sq / n - mean * mean))};
}

// Mean absolute luminance difference, both images resized to the
// smaller one so a 1024 GPU chain can be held against a 512 CPU one.
double MeanAbsDiff(const Image& a, const Image& b)
{
    int w = std::min(a.width, b.width);
    Image ra = ImageCopy(a), rb = ImageCopy(b);
    ImageFormat(&ra, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageFormat(&rb, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageResize(&ra, w, w);
    ImageResize(&rb, w, w);
    const unsigned char* pa = (const unsigned char*)ra.data;
    const unsigned char* pb = (const unsigned char*)rb.data;
    double acc = 0.0;
    size_t n = (size_t)w * w;
    for (size_t i = 0; i < n; i++)
    {
        double la = (pa[i * 4] + pa[i * 4 + 1] + pa[i * 4 + 2]) / 3.0;
        double lb = (pb[i * 4] + pb[i * 4 + 1] + pb[i * 4 + 2]) / 3.0;
        acc += std::fabs(la - lb);
    }
    UnloadImage(ra);
    UnloadImage(rb);
    return acc / n;
}

} // namespace

int main(int argc, char** argv)
{
    double lat = TERRAIN_ANCHOR_LAT, lon = TERRAIN_ANCHOR_LON;
    int res = 0;
    std::string out = "build/terrain_probe";
    std::string path = "both";
    bool site = true;
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        bool next = (i + 1 < argc);
        if (a == "--lat" && next) lat = std::atof(argv[++i]);
        else if (a == "--lon" && next) lon = std::atof(argv[++i]);
        else if (a == "--res" && next) res = std::atoi(argv[++i]);
        else if (a == "--out" && next) out = argv[++i];
        else if (a == "--path" && next) path = argv[++i];
        else if (a == "--site" && next) site = std::atoi(argv[++i]) != 0;
        else
        {
            std::printf("usage: terrain_probe [--lat L] [--lon L] [--res N] "
                        "[--out DIR] [--site 0|1] [--path cpu|gpu|both]\n");
            return (a == "--help" || a == "-h") ? 0 : 1;
        }
    }

    SetTraceLogLevel(LOG_INFO);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 240, "terrain_probe");

    TerrainSiteDisturbance dist;
    dist.enabled = site;
    const TerrainSiteDisturbance* sitePtr = site ? &dist : nullptr;

    std::printf("path here: %s\n", GetTerrainPathName());
    if (res <= 0) res = GetTerrainPathResolution();
    std::printf("location %.3f, %.3f  res %d  site %s  out %s/\n",
                lat, lon, res, site ? "on" : "off", out.c_str());

    Image gpu[3] = {}, cpu[3] = {};
    bool haveGpu = false, haveCpu = false;

    if (path == "gpu" || path == "both")
    {
        TerrainGpuChain chain;
        double t0 = GetTime();
        if (GenerateTerrainChainGPU(lat, lon, res, &chain, sitePtr))
        {
            double tSubmit = (GetTime() - t0) * 1000.0;
            for (int i = 0; i < 3; i++)
            {
                // The colour targets are in image orientation already.
                gpu[i] = LoadImageFromTexture(chain.color[i].texture);
            }
            double tDone = (GetTime() - t0) * 1000.0;
            std::printf("GPU chain: submitted in %.1f ms, finished (read back) in %.1f ms\n",
                        tSubmit, tDone);
            UnloadTerrainGpuChain(&chain);
            haveGpu = true;
        }
        else std::printf("GPU chain: unavailable\n");
    }
    if (path == "cpu" || path == "both")
    {
        double t0 = GetTime();
        GenerateTerrainChain(lat, lon, res, cpu, sitePtr);
        std::printf("CPU chain: %.1f ms\n", (GetTime() - t0) * 1000.0);
        haveCpu = true;
    }

    const char* names[3] = {"planet_100km", "colony_25km", "sect_5km"};
    for (int i = 0; i < 3; i++)
    {
        if (haveGpu)
        {
            std::string f = out + "/gpu_" + names[i] + ".png";
            ExportImage(gpu[i], f.c_str());
            Stats s = Luminance(gpu[i]);
            std::printf("%-13s GPU  mean %.3f  std %.3f", names[i], s.mean, s.stdev);
            if (haveCpu) std::printf("   |diff| vs CPU %.1f / 255", MeanAbsDiff(gpu[i], cpu[i]));
            std::printf("\n");
        }
        if (haveCpu)
        {
            std::string f = out + "/cpu_" + names[i] + ".png";
            ExportImage(cpu[i], f.c_str());
            Stats s = Luminance(cpu[i]);
            std::printf("%-13s CPU  mean %.3f  std %.3f\n", names[i], s.mean, s.stdev);
        }
    }
    for (int i = 0; i < 3; i++)
    {
        if (haveGpu) UnloadImage(gpu[i]);
        if (haveCpu) UnloadImage(cpu[i]);
    }
    UnloadTerrainGpu();
    CloseWindow();
    return 0;
}
