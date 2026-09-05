#ifndef TERRAIN_GPU_H
#define TERRAIN_GPU_H

#include "raylib.h"
#include "terrain_synthesis.h"

// GPU terrain synthesis: the real-imagery amplification chain of
// terrain_synthesis.cpp run as fragment-shader passes, for the targets
// that have a GPU to spare -- which is every target except a software
// GL stack (WSL without /dev/dri, a headless CI box).
//
// The two paths are picked between once, by GetTerrainPath(): an env
// override (COLONY_TERRAIN=cpu|gpu), else always GPU in the browser
// (no worker threads there, and WebGL is always a real GPU), else a
// timed probe -- a software rasterizer takes hundreds of milliseconds
// for what a GPU does in a few, so the threshold is not delicate.
// Everything that draws terrain asks the path, never the platform.

enum TerrainPath
{
    TERRAIN_PATH_CPU = 0,   // GenerateTerrainChain on worker threads
    TERRAIN_PATH_GPU = 1    // GenerateTerrainChainGPU on the main thread
};

// Needs a live GL context on the first call. Cached afterwards.
TerrainPath GetTerrainPath();
const char* GetTerrainPathName();

// The chain resolution each path can afford: 512 on the CPU (the
// pyramid blur made 1024 cost 2.4 s per cell), 1024 on a desktop GPU,
// 512 on the web where the 9-cell cache would otherwise be 113 MB of
// phone memory.
int GetTerrainPathResolution();

// Whether this machine should build the site-level chain layer at all,
// and the one-line reason. Decided by the same probe that picks the path:
// a GPU path can afford it, a CPU path can where there is a thread to put
// it on, and a browser falling back to a software rasteriser should not
// have it -- that is the case this exists for, where "WebGL is a real
// GPU" used to send the layer into a 44 second build.
bool TerrainLayerAffordable();
const char* TerrainLayerWhy();

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
                              const TerrainSiteDisturbance* site = nullptr);

// Shaders and scratch targets. Call once at shutdown.
void UnloadTerrainGpu();

#endif // TERRAIN_GPU_H
