#ifndef TERRAIN_GPU_H
#define TERRAIN_GPU_H

#include "raylib.h"
#include "terrain_synthesis.h"

// GPU terrain synthesis: the real-imagery amplification chain of
// terrain_synthesis.cpp run as fragment-shader passes, for the targets
// that have a GPU to spare -- which is every target except a software
// GL stack (WSL without /dev/dri, a headless CI box).
//
// The two paths are picked between once, by GetTerrainPath(): an
// override (COLONY_TERRAIN=cpu|gpu; in a browser, ?terrain=cpu|gpu),
// else a timed probe on every platform -- a software rasterizer takes
// hundreds of milliseconds for what a GPU does in a few, so the
// threshold is not delicate, and a browser's WebGL can be one.
// Everything that draws terrain asks TerrainChainOnGpu(), never the
// platform.

enum TerrainPath
{
    TERRAIN_PATH_CPU = 0,   // GenerateTerrainChain on worker threads
    TERRAIN_PATH_GPU = 1    // GenerateTerrainChainGPU on the main thread
};

// Needs a live GL context on the first call. Cached afterwards.
TerrainPath GetTerrainPath();

// True when this device's shaders can run the world-anchored regolith.
// False on GLSL ES 1.00 (WebGL1), where the CPU path must be used instead.
bool TerrainGpuCanSubFloor();
const char* GetTerrainPathName();

// The chain resolution each path can afford: 512 on the CPU (the
// pyramid blur made 1024 cost 2.4 s per cell), 1024 on a desktop GPU,
// 512 on the web where the 9-cell cache would otherwise be 113 MB of
// phone memory.
int GetTerrainPathResolution();


// How long one CPU chain at `res` would take here, in milliseconds.
//
// Exists because "can this machine afford the site layer" and "which path
// builds it" drifted apart: on the web the shaders cannot run the regolith
// (GLSL ES 1.00), so the layer is always built on the CPU -- while the gate
// deciding whether to build it was reading a GPU measurement. The browser
// was being asked a 1523 px CPU chain on the strength of a 12 ms GPU probe.
//
// Measured once, at 256, the first time it is asked; scaled from there.
// The chain is superlinear in resolution -- more octaves fit under the
// data floor as pixels shrink, and the crater and clast populations grow
// with area -- so this scales by a measured exponent, not by res^2. From
// one browser's ?chainbench=1 on a software rasteriser:
//
//   256 px   620 ms      512 -> 256 ratio 5.72  (exponent 2.52)
//   512 px  3546 ms     1024 -> 512 ratio 6.35  (exponent 2.67)
//  1024 px 22532 ms
//
// Returns 0 when the mosaic is not loaded and there is nothing to measure.
double TerrainCpuChainMs(int res);

// The largest resolution whose CPU chain fits `budgetMs`, or 0 if even the
// floor (256) does not. Use it to SIZE the layer rather than to veto it:
// a smaller chain resampled up is worth far more than no chain at all.
int TerrainCpuChainResFor(double budgetMs);

// What one blocking CPU build may cost when a view arrives: a pause, not
// frames, and the result is cached after.
const double TERRAIN_CHAIN_BUDGET_MS = 1200.0;

// Who builds a chain: the GPU when the probe chose it AND this device's
// shaders can run the regolith, the CPU otherwise. On WebGL1 the shaders
// compile the regolith to a stub, so a GPU-built chain there is the vague
// picture with no craters in it.
//
// Every consumer asks this one question -- the game's terrain cache and
// lunar_map's layer alike -- so the cost model and the builder cannot
// disagree about who builds. They did on 2026-09-23, twice: lunar_map's
// site layer came up grey in the browser, and the game's site level,
// once the ladder was wired in, came up without its craters on any
// device with a real GPU.
bool TerrainChainOnGpu();

// One generated chain: PLANET (100 km), COLONY (25 km), SECT (5 km),
// each a res x res colour render target. Caller owns all three.
struct TerrainGpuChain
{
    RenderTexture2D color[TERRAIN_CHAIN_MAX_LEVELS] = {};
    int levels = TERRAIN_CHAIN_MAX_LEVELS;   // how many of them are real
};

// Same contract as GenerateTerrainChain, same registration between
// levels, same seed. Must be called on the main thread with a GL
// context; safe to call mid-frame, inside a camera or another render
// texture -- the GL state it disturbs is put back before returning.
// Returns false if the shaders failed to build (caller uses the CPU).
// spans == nullptr walks the game's own 100 / 25 / 5; anything else
// walks the ladder it is given, exactly as the CPU path does. The last
// real level is out->color[out->levels - 1].
bool GenerateTerrainChainGPU(double latDeg, double lonDeg, int res,
                             TerrainGpuChain* out,
                             const TerrainSiteDisturbance* site = nullptr,
                             const TerrainChainSpans* spans = nullptr);

void UnloadTerrainGpuChain(TerrainGpuChain* chain);

// The chain's two unlit fields on the GPU -- the same contract as
// GenerateTerrainFields (terrain_synthesis.h), and the same fields, from
// the shader passes instead of the CPU. Needs a live GL context and the
// main thread, like the rest of this file. Returns false if the shaders
// are unavailable, in which case the caller uses the CPU function.
//
// The height comes back 16-bit and the albedo 8-bit, which is what
// survives the caller's high-pass; heightScaleM is filled in the same
// way the CPU fills it.
bool GenerateTerrainFieldsGPU(double latDeg, double lonDeg, int res,
                              double spanKm, TerrainChainFields* out,
                              const TerrainSiteDisturbance* site = nullptr,
                              double dataFloorKm = 0.0);

// Shaders and scratch targets. Call once at shutdown.
void UnloadTerrainGpu();

#endif // TERRAIN_GPU_H
