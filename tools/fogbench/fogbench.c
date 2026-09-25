/* fogbench: time the console block with and without the fog, headless. */
#include "raylib.h"
#include "rlgl.h"
#include "c2d.h"
#include "holo3d.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static double Now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static float Beds(void *c, int b, float u, float v) { (void)c; return (float)b / 5.0f + 0.03f * u * v; }
/* half the block known: coverage falls with depth and across u */
static float Fog(void *c, float u, float v, float d) { (void)c; (void)v; float k = 1.0f - d * 1.6f + (u - 0.5f) * 0.4f; return k < 0 ? 0 : (k > 1 ? 1 : k); }
static float FogAll(void *c, float u, float v, float d) { (void)c; (void)u; (void)v; (void)d; return 0.0f; }

int main(int argc, char **argv)
{
    const int ss = argc > 1 ? atoi(argv[1]) : 2;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 720, "fogbench");
    c2d_set_supersample(ss);
    const int W = 1536, H = 768;
    C2DSurface surf = c2d_surface_create(W, H);
    H3DBuildOpts opts = {0}; opts.surfaceW = W; opts.surfaceH = H;
    Holo3DModel *m = Holo3D_Build(&opts);
    Holo3D_SetGround(m, 4, Beds, NULL, NULL);
    H3DState st = {0}; st.yaw = -0.1f; st.pitch = 0.42f; st.time = 3.0f;
    H3DView view = {0}; view.cx = 740.0f; view.cy = 330.0f; view.zoom = 0.34f;
    const int N = 30;
    for (int mode = 0; mode < 3; mode++)
    {
        /* 0: fully known (no fog)   1: fog, half known   2: fog, nothing known */
        Holo3D_SetFog(m, mode == 0 ? NULL : (mode == 1 ? Fog : FogAll), NULL);
        for (int i = 0; i < 3; i++) { c2d_begin(&surf, BLACK); Holo3D_Render(m, &st, &view); c2d_end(); }
        rlDrawRenderBatchActive();
        double t0 = Now();
        for (int i = 0; i < N; i++)
        {
            st.time += 1.0f / 60.0f;
            c2d_begin(&surf, BLACK); Holo3D_Render(m, &st, &view); c2d_end();
        }
        rlDrawRenderBatchActive();
        Image img = LoadImageFromTexture(surf.tex.texture);   /* forces the GPU to finish */
        UnloadImage(img);
        const double ms = (Now() - t0) * 1000.0 / N;
        printf("mode %d (%s): %.2f ms per block render (ss %d)\n", mode,
               mode == 0 ? "no fog, all known" : (mode == 1 ? "fog, half known" : "fog, nothing known"), ms, ss);
    }
    CloseWindow();
    return 0;
}
