/* The port half of the ToolRack visual diff (spec 5). One frame, level and
 * grain fixed to match ref_toolrack.html, exported as a PNG. */
#include "raylib.h"
#include "c2d.h"
#include "toolrack.h"

#include <stdio.h>
#include <stdlib.h>
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
    /* the rack is 395x750 design units; the harness frames it with a margin
     * so the glow and the bracket edges are not clipped by the canvas */
    const int W = 500, H = 820;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(320, 240, "toolrack visdiff");

    c2d_fonts_load("src/assets/fonts/JetBrainsMono-Medium.ttf",
                   "src/assets/fonts/JetBrainsMono-SemiBold.ttf",
                   "src/assets/fonts/JetBrainsMono-Bold.ttf");

    const int ss = (int)argf(argc, argv, "--ss", 1.0f);
    c2d_set_supersample(ss);
    C2DSurface surf = c2d_surface_create(W, H);

    ToolRackData rack = ToolRack_Demo();
    ToolRackOpts opts = {0};
    opts.level = (int)argf(argc, argv, "--level", 6.0f);
    opts.grain = argf(argc, argv, "--grain", 0.0f) > 0.5f;

    c2d_begin(&surf, (Color){0x02, 0x11, 0x1a, 255});
    ToolRack_DrawB(&rack, 40.0f, 40.0f, &opts);
    c2d_end();

    Image img = LoadImageFromTexture(surf.tex.texture);
    ImageFlipVertical(&img);
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

    c2d_surface_destroy(&surf);
    c2d_fonts_unload();
    CloseWindow();
    return 0;
}
