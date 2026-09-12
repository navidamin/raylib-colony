/* The port half of the visual diff (spec 5). One frame, the same fixed
 * state as ref.html, into a design-size surface, exported as a PNG. */
#include "raylib.h"
#include "c2d.h"
#include "holo3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float argf(int c, char **v, const char *k, float d)
{
    for (int i = 1; i < c - 1; i++) if (!strcmp(v[i], k)) return (float)atof(v[i + 1]);
    return d;
}
static const char *args(int c, char **v, const char *k, const char *d)
{
    for (int i = 1; i < c - 1; i++) if (!strcmp(v[i], k)) return v[i + 1];
    return d;
}

int main(int argc, char **argv)
{
    const int W = 1600, H = 1300;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(320, 240, "holo3d visdiff");

    c2d_fonts_load("src/assets/fonts/JetBrainsMono-Medium.ttf",
                   "src/assets/fonts/JetBrainsMono-SemiBold.ttf",
                   "src/assets/fonts/JetBrainsMono-Bold.ttf");

    const int ss = (int)argf(argc, argv, "--ss", 1.0f);
    c2d_set_supersample(ss);
    C2DSurface surf = c2d_surface_create(W, H);

    H3DBuildOpts opts = {0};
    opts.surfaceW = W; opts.surfaceH = H;
    Holo3DModel *m = Holo3D_Build(&opts);

    H3DState st = {0};
    st.yaw     = argf(argc, argv, "--yaw", -0.1f);
    st.pitch   = argf(argc, argv, "--pitch", 0.42f);
    st.explode = argf(argc, argv, "--explode", 0.0f);
    st.selected = (int)argf(argc, argv, "--sel", -1.0f);
    st.time    = argf(argc, argv, "--t", 3.0f);
    st.fast    = argf(argc, argv, "--fast", 0.0f) > 0.5f;

    H3DView view = {0};
    view.cx = argf(argc, argv, "--cx", 640.0f);
    view.cy = argf(argc, argv, "--cy", 650.0f);
    view.zoom = argf(argc, argv, "--zoom", 0.9f);

    const bool hud = argf(argc, argv, "--hud", 1.0f) > 0.5f;

    /* two passes: the ghost buffers and the model's own bounds are filled by
     * the first render, and the HUD reads both */
    for (int pass = 0; pass < 2; pass++)
    {
        c2d_begin(&surf, (Color){0x01, 0x09, 0x10, 255});
        Holo3D_Render(m, &st, &view);
        if (hud) Holo3D_DrawHud(m, &st, &view, NULL);
        c2d_end();
    }

    Image img = LoadImageFromTexture(surf.tex.texture);
    ImageFlipVertical(&img);
    /* Resolve. A plain N x N box average, which is what the GPU's bilinear
     * minification does at an exact integer reduction -- so the PNG is what
     * c2d_present would put on the screen, not a sharper stand-in. */
    if (c2d_supersample() > 1)
    {
        const int n = c2d_supersample();
        Color *src = LoadImageColors(img);
        Image out = GenImageColor(W, H, BLANK);
        Color *dst = LoadImageColors(out);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
            {
                int r = 0, g = 0, b = 0, a = 0;
                for (int sy = 0; sy < n; sy++)
                    for (int sx = 0; sx < n; sx++)
                    {
                        const Color c = src[(y * n + sy) * (W * n) + (x * n + sx)];
                        r += c.r; g += c.g; b += c.b; a += c.a;
                    }
                const int k = n * n;
                dst[y * W + x] = (Color){(unsigned char)(r / k), (unsigned char)(g / k),
                                         (unsigned char)(b / k), (unsigned char)(a / k)};
            }
        UnloadImageColors(src);
        UnloadImage(out);
        UnloadImage(img);
        img = (Image){dst, W, H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    }
    ExportImage(img, args(argc, argv, "--out", "/tmp/port.png"));
    printf("port -> %s\n", args(argc, argv, "--out", "/tmp/port.png"));
    UnloadImage(img);

    Holo3D_Free(m);
    c2d_surface_destroy(&surf);
    c2d_fonts_unload();
    CloseWindow();
    return 0;
}
