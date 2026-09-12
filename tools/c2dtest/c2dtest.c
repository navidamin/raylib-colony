/* c2dtest — the phase-1 acceptance tests from docs/PORT_PROMPTS.md.
 *
 * Each step of the shim had to pass its test before the next was started.
 * Run headless:  tools/c2dtest/c2dtest.sh
 */
#include "raylib.h"
#include "rlgl.h"
#include "c2d.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_fail = 0, g_pass = 0;

static void CHECK(bool ok, const char *what)
{
    if (ok) { g_pass++; printf("  pass  %s\n", what); }
    else    { g_fail++; printf("  FAIL  %s\n", what); }
}

/* ---- 3. ear clipping ------------------------------------------------ */
/* The spec's test: a deliberately concave 20-point sawtooth, the shape the
 * layer walls actually are. The distinguishing assertion is NOT "every
 * vertex is an input vertex" -- a fan passes that -- it is that every
 * triangle lies INSIDE the polygon, which a fan on a concave shape does
 * not, and that the triangles sum to the polygon's own area. */
static bool inside_poly(Vector2 p, const Vector2 *poly, int n)
{
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
            p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            in = !in;
    return in;
}
static float area_poly(const Vector2 *p, int n)
{
    float a = 0.0f;
    for (int i = 0, j = n - 1; i < n; j = i++) a += p[j].x * p[i].y - p[i].x * p[j].y;
    return fabsf(a) * 0.5f;
}

/* c2d.c keeps the triangulator private, so the test drives it the way a
 * call site does -- through a fill -- and reads the pixels back. That is a
 * stronger test than poking at indices: it catches winding culling too,
 * which is the bug that ate every wall fill in the first port. */
static void SawtoothWall(Vector2 *out, int *n)
{
    /* top edge left->right with a rough height field, bottom edge back */
    const int N = 10;
    int k = 0;
    for (int i = 0; i < N; i++)
    {
        const float x = 60.0f + i * 40.0f;
        out[k++] = (Vector2){x, 120.0f + ((i % 2) ? 46.0f : 0.0f)};
    }
    for (int i = N - 1; i >= 0; i--)
    {
        const float x = 60.0f + i * 40.0f;
        out[k++] = (Vector2){x, 300.0f - ((i % 3) ? 30.0f : 0.0f)};
    }
    *n = k;
}

int main(int argc, char **argv)
{
    const char *outPng = (argc > 1) ? argv[1] : "build/preview/c2dtest.png";
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1024, 768, "c2dtest");
    SetTargetFPS(60);

    printf("\n== 1. surface, letterbox, to_design ==\n");
    C2DSurface s = c2d_surface_create(1536, 1024);
    {
        /* present once per window size so dst is computed, then round trip */
        const int sizes[3][2] = {{800, 600}, {1920, 1080}, {1024, 1366}};
        for (int i = 0; i < 3; i++)
        {
            SetWindowSize(sizes[i][0], sizes[i][1]);
            BeginDrawing(); ClearBackground(BLACK); c2d_present(&s); EndDrawing();
            const Vector2 corners[4] = {{0,0},{1536,0},{0,1024},{1536,1024}};
            float worst = 0.0f;
            for (int c = 0; c < 4; c++)
            {
                const Vector2 scr = {s.dst.x + corners[c].x / 1536.0f * s.dst.width,
                                     s.dst.y + corners[c].y / 1024.0f * s.dst.height};
                const Vector2 back = c2d_to_design(&s, scr);
                const float e = fmaxf(fabsf(back.x - corners[c].x), fabsf(back.y - corners[c].y));
                if (e > worst) worst = e;
            }
            printf("    %dx%d -> dst %.1f,%.1f %.1fx%.1f  worst round trip %.3f px\n",
                   sizes[i][0], sizes[i][1], s.dst.x, s.dst.y, s.dst.width, s.dst.height, worst);
            CHECK(worst < 1.0f, "corners round trip within 1px");
            CHECK(s.dst.width <= sizes[i][0] + 0.5f && s.dst.height <= sizes[i][1] + 0.5f,
                  "fit-contain stays inside the window");
        }
        SetWindowSize(1024, 768);
    }

    printf("\n== 2. alpha stack ==\n");
    {
        CHECK(fabsf(c2d_alpha() - 1.0f) < 1e-6f, "starts at 1");
        c2d_push_alpha(0.5f);
        c2d_push_alpha(0.6f);
        CHECK(fabsf(c2d_alpha() - 0.3f) < 1e-6f, "0.5 then 0.6 multiplies to 0.3");
        const Color t = c2d_tint((Color){200, 100, 50, 255});
        CHECK(t.a == 76 || t.a == 77, "tint applies the stack to alpha");
        c2d_pop_alpha(); c2d_pop_alpha();
        CHECK(fabsf(c2d_alpha() - 1.0f) < 1e-6f, "pop restores exactly");
        c2d_save(); c2d_push_alpha(0.25f); c2d_restore();
        CHECK(fabsf(c2d_alpha() - 1.0f) < 1e-6f, "restore unwinds an unbalanced push");
    }

    printf("\n== 9. hash against the JS ==\n");
    {
        /* computed from js/holo3d.js hash() in node */
        const int    tri[10][3] = {{1,7,65},{2,7,65},{3,8,66},{16,12,67},{0,0,0},
                                   {5,5,5},{13,3,68},{9,11,193},{4,6,99},{12,2,102}};
        const double js[10] = {0.608505772,0.191437126,0.843887689,0.355802444,0.000000000,
                               0.667254385,0.130225261,0.657684631,0.823403532,0.127861752};
        double worst = 0.0;
        for (int i = 0; i < 10; i++)
        {
            const double d = fabs((double)c2d_hash(tri[i][0], tri[i][1], tri[i][2]) - js[i]);
            if (d > worst) worst = d;
        }
        printf("    worst disagreement with JS: %.3e\n", worst);
        CHECK(worst < 1e-6, "matches the JS to 1e-6");
        /* the property that actually matters: no sample in the domain the
           wall speckle uses lands within the disagreement of a threshold */
        int close = 0;
        for (int k = 0; k < 5; k++)
            for (int i = 0; i < 40; i++)
                for (int m = 0; m < 40; m++)
                    for (int wname = 65; wname <= 68; wname++)
                    {
                        const float h = c2d_hash(i + 1, m + 7, k * 4 + wname);
                        if (fabsf(h - 0.18f) < 1e-5f || fabsf(h - 0.14f) < 1e-5f) close++;
                    }
        printf("    samples within 1e-5 of a threshold: %d\n", close);
        CHECK(close == 0, "no speckle sample straddles a threshold");
    }

    printf("\n== 6. text baseline ==\n");
    {
        c2d_fonts_load("src/assets/fonts/JetBrainsMono-Medium.ttf",
                       "src/assets/fonts/JetBrainsMono-SemiBold.ttf",
                       "src/assets/fonts/JetBrainsMono-Bold.ttf");
        const float size = 40.0f;
        const float asc = c2d_ascender(C2D_W500, size);
        const float cap = c2d_cap_height(C2D_W500, size);
        printf("    ascender %.2f  cap %.2f  at size %.0f\n", asc, cap, size);
        CHECK(asc > cap && asc < size * 1.2f, "ascender is above cap height and plausible");
        /* draw 'H' with the baseline at y = 300 and find the ink */
        RenderTexture2D rt = LoadRenderTexture(200, 400);
        BeginTextureMode(rt); ClearBackground(BLACK);
        c2d_text(C2D_W500, size, "H", 20.0f, 300.0f, WHITE,
                 C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
        EndTextureMode();
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);          /* render textures come back flipped */
        int top = -1, bot = -1;
        for (int y = 0; y < img.height; y++)
            for (int x = 0; x < img.width; x++)
                if (GetImageColor(img, x, y).r > 90) { if (top < 0) top = y; bot = y; }
        printf("    'H' ink spans y %d..%d, baseline asked for 300\n", top, bot);
        CHECK(bot >= 0 && fabsf((float)bot - 299.0f) <= 2.0f, "baseline lands within 2px of y");
        CHECK(top >= 0 && fabsf((float)(bot - top + 1) - cap) <= 2.0f, "ink height equals the cap height");
        UnloadImage(img); UnloadRenderTexture(rt);
    }

    printf("\n== 3. ear clipping, and the winding it has to survive ==\n");
    {
        /* BOTH windings. The first version of this test used one polygon,
           it happened to wind the way raylib wanted, and the winding bug it
           was supposed to catch went straight through into the port -- where
           it culled every cell of the block's cap. */
        for (int rev = 0; rev < 2; rev++) {
        Vector2 poly[64]; int n = 0;
        SawtoothWall(poly, &n);
        if (rev) for (int i = 0; i < n / 2; i++)
                 { Vector2 t2 = poly[i]; poly[i] = poly[n-1-i]; poly[n-1-i] = t2; }
        RenderTexture2D rt = LoadRenderTexture(560, 400);
        BeginTextureMode(rt); ClearBackground(BLACK);
        c2d_fill_poly(poly, n, WHITE);
        EndTextureMode();
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        long lit = 0, outside = 0;
        for (int y = 0; y < img.height; y++)
            for (int x = 0; x < img.width; x++)
            {
                if (GetImageColor(img, x, y).r < 120) continue;
                lit++;
                if (!inside_poly((Vector2){x + 0.5f, y + 0.5f}, poly, n)) outside++;
            }
        const float want = area_poly(poly, n);
        printf("    winding %s: filled %ld px, area %.0f, spill %ld px\n",
               rev ? "reversed" : "as given", lit, want, outside);
        CHECK(lit > want * 0.92f, "the concave polygon is actually filled (not culled)");
        CHECK(outside < want * 0.02f, "no triangle spills outside -- a fan would");
        UnloadImage(img); UnloadRenderTexture(rt);
        }
    }

    printf("\n== 4. gradient: smooth, not banded ==\n");
    {
        C2DGradient g = c2d_gradient_linear(20.0f, 380.0f);
        c2d_gradient_stop(&g, 0.00f, (Color){63, 155, 212, 255});
        c2d_gradient_stop(&g, 0.20f, (Color){29,  93, 144, 255});
        c2d_gradient_stop(&g, 0.75f, (Color){12,  42,  74, 255});
        c2d_gradient_stop(&g, 1.00f, (Color){12,  42,  74, 255});
        const Vector2 quad[4] = {{20,20},{180,20},{180,380},{20,380}};
        RenderTexture2D rt = LoadRenderTexture(200, 400);
        BeginTextureMode(rt); ClearBackground(BLACK);
        c2d_fill_poly_gradient(quad, 4, &g);
        EndTextureMode();
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        int biggestJump = 0;
        for (int y = 21; y < 379; y++)
        {
            const int a = GetImageColor(img, 100, y - 1).r, b = GetImageColor(img, 100, y).r;
            const int d = abs(a - b);
            if (d > biggestJump) biggestJump = d;
        }
        printf("    largest row-to-row step in the ramp: %d\n", biggestJump);
        CHECK(biggestJump <= 3, "no per-triangle banding");
        UnloadImage(img); UnloadRenderTexture(rt);
    }

    printf("\n== 8. ellipse: partial arcs stay open ==\n");
    {
        Vector2 pts[33];
        const int n = c2d_ellipse_pts((Vector2){100, 100}, 60, 30, 0.0f, 0.0f, PI, pts, 33);
        CHECK(n == 33, "returns the requested count");
        CHECK(fabsf(pts[0].x - 160.0f) < 0.01f && fabsf(pts[0].y - 100.0f) < 0.01f, "arc starts at a0");
        CHECK(fabsf(pts[32].x - 40.0f) < 0.01f && fabsf(pts[32].y - 100.0f) < 0.01f, "arc ends at a1, not closed");
        Vector2 rot[9];
        c2d_ellipse_pts((Vector2){0, 0}, 10, 5, PI * 0.5f, 0.0f, 0.0f, rot, 9);
        CHECK(fabsf(rot[0].x) < 0.01f && fabsf(rot[0].y - 10.0f) < 0.01f, "rotation is honoured");
    }

    printf("\n== 7 + 5. radial gradient and glow, rendered ==\n");
    {
        /* judged by eye from the PNG, and by one numeric check each */
        c2d_begin(&s, (Color){2, 11, 19, 255});
        c2d_fill_radial((Vector2){260, 200}, 6.0f, 120.0f,
                        (Color){0, 251, 254, 210}, (Color){0, 251, 254, 0}, 64);
        c2d_text(C2D_W600, 22.0f, "radial  r_in 6  r_out 120", 120.0f, 360.0f,
                 (Color){163, 184, 204, 255}, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);

        const Vector2 line[2] = {{560, 120}, {900, 260}};
        c2d_glow_stroke(line, 2, (Color){230, 255, 255, 255}, 2.2f, 16.0f);
        const Vector2 flat[2] = {{560, 300}, {900, 300}};
        c2d_glow_stroke(flat, 2, (Color){230, 255, 255, 255}, 2.2f, 0.0f);
        c2d_text(C2D_W600, 22.0f, "glow blur 16  vs  blur 0", 560.0f, 360.0f,
                 (Color){163, 184, 204, 255}, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);

        Vector2 saw[64]; int sn = 0; SawtoothWall(saw, &sn);
        for (int i = 0; i < sn; i++) { saw[i].x += 60.0f; saw[i].y += 420.0f; }
        C2DGradient g = c2d_gradient_linear(540.0f, 720.0f);
        c2d_gradient_stop(&g, 0.00f, (Color){63, 155, 212, 255});
        c2d_gradient_stop(&g, 0.20f, (Color){29,  93, 144, 255});
        c2d_gradient_stop(&g, 0.75f, (Color){12,  42,  74, 255});
        c2d_gradient_stop(&g, 1.00f, (Color){12,  42,  74, 255});
        c2d_fill_poly_gradient(saw, sn, &g);
        c2d_polygon(saw, sn, (Color){207, 239, 255, 160}, 1.5f);
        c2d_text(C2D_W600, 22.0f, "concave wall, 4-stop gradient, round joins",
                 60.0f, 800.0f, (Color){163, 184, 204, 255},
                 C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);

        Vector2 ring[73];
        c2d_ellipse_pts((Vector2){1150, 600}, 200.0f, 80.0f, 0.0f, 0.0f, 2.0f * PI, ring, 73);
        c2d_dashed_polyline(ring, 73, 8.0f, 6.0f, (Color){95, 240, 255, 90}, 1.2f);
        Vector2 arc[33];
        c2d_ellipse_pts((Vector2){1150, 600}, 140.0f, 56.0f, 0.0f, 0.6f, 2.6f, arc, 33);
        c2d_glow_stroke(arc, 33, (Color){95, 240, 255, 255}, 2.5f, 12.0f);
        c2d_text(C2D_W500, 20.0f, "dashed full ellipse + partial arc, centred",
                 1150.0f, 800.0f, (Color){163, 184, 204, 255},
                 C2D_ALIGN_CENTER, C2D_BASELINE_ALPHABETIC);

        c2d_push_alpha(0.3f);
        c2d_fill_poly((Vector2[]){{1000,120},{1400,120},{1400,240},{1000,240}}, 4,
                      (Color){255, 255, 255, 255});
        c2d_pop_alpha();
        c2d_text(C2D_W700, 24.0f, "alpha 0.3", 1200.0f, 300.0f,
                 (Color){188, 210, 230, 255}, C2D_ALIGN_CENTER, C2D_BASELINE_ALPHABETIC);
        c2d_end();

        Image img = LoadImageFromTexture(s.tex.texture);
        ImageFlipVertical(&img);
        /* the radial must fall off, not step */
        const int c0 = GetImageColor(img, 260, 200).b, c1 = GetImageColor(img, 260, 260).b,
                  c2 = GetImageColor(img, 260, 315).b;
        printf("    radial blue at r=0/60/115: %d %d %d\n", c0, c1, c2);
        CHECK(c0 > c1 && c1 > c2, "radial falls off monotonically");
        /* the glowed line must light pixels well off its own axis */
        /* the halo's outer pass is w + blur*1.6 wide, so half of it reaches
           13.9px; sample inside that, and the same offset on the unblurred
           line for the comparison */
        const int onAxis = GetImageColor(img, 730, 190).r;
        const int offAxis = GetImageColor(img, 730, 199).r;
        const int flatOff = GetImageColor(img, 730, 309).r;
        printf("    glow: on-axis %d, 9px off %d; no-blur 9px off %d\n", onAxis, offAxis, flatOff);
        CHECK(offAxis > flatOff + 8, "blur 16 spreads light that blur 0 does not");
        const int farOff = GetImageColor(img, 730, 208).r;
        printf("    glow: 18px off %d (should be back to ground)\n", farOff);
        CHECK(farOff < 12, "the halo has a finite reach");
        ExportImage(img, outPng);
        UnloadImage(img);
        printf("    wrote %s\n", outPng);
    }

    c2d_fonts_unload();
    c2d_surface_destroy(&s);
    CloseWindow();
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
