/* c2dtest — the phase-1 acceptance tests from docs/PORT_PROMPTS.md.
 *
 * Each step of the shim had to pass its test before the next was started.
 * Run headless:  tools/c2dtest/c2dtest.sh
 */
#include "raylib.h"
#include "rlgl.h"
#include "c2d.h"
#include "drill_sim.h"
#include "dash_knowledge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

    /* ================================================================
       The ToolRack extensions. Six gaps the Holo3D port never exercised,
       each tested before the module was allowed to use it.
       ================================================================ */

    printf("\n== 9. rpoly: quadratic corner rounding ==\n");
    {
        const C2DCorner sharp[4] = {{100,100,0},{300,100,0},{300,240,0},{100,240,0}};
        const C2DCorner round4[4] = {{100,100,40},{300,100,40},{300,240,40},{100,240,40}};
        Vector2 a[256], b[256];
        const int na = c2d_rpoly_pts(sharp, 4, 0, 0, a, 256);
        const int nb = c2d_rpoly_pts(round4, 4, 0, 0, b, 256);
        CHECK(na == 4, "radius 0 emits the corner untouched");
        CHECK(nb > 30, "a rounded corner is flattened into an arc");
        /* the square corner (100,100) must be OUTSIDE the rounded polygon,
           and every emitted point must stay within the original box */
        CHECK(!inside_poly((Vector2){101.0f, 101.0f}, b, nb),
              "the corner is actually cut away, not just re-pointed");
        bool inBox = true;
        for (int i = 0; i < nb; i++)
            if (b[i].x < 99.5f || b[i].x > 300.5f || b[i].y < 99.5f || b[i].y > 240.5f) inBox = false;
        CHECK(inBox, "the arc bulges inward, never outside the hull");
        /* the (dx,dy) offset slab depends on */
        Vector2 c[256];
        const int nc = c2d_rpoly_pts(round4, 4, -5.0f, -3.0f, c, 256);
        CHECK(nc == nb && fabsf((c[0].x - b[0].x) + 5.0f) < 0.01f
                       && fabsf((c[0].y - b[0].y) + 3.0f) < 0.01f,
              "(dx,dy) shifts every vertex and nothing else");
    }

    printf("\n== 10. nested clip, and restore closing both ==\n");
    {
        /* slab's pattern: clip to A, clip to B, fill huge, one restore */
        RenderTexture2D rt = LoadRenderTexture(400, 300);
        BeginTextureMode(rt); ClearBackground(BLACK); EndTextureMode();
        c2d_begin(&s, BLACK);
        const Vector2 A[4] = {{50,50},{250,50},{250,250},{50,250}};
        const Vector2 B[4] = {{150,20},{350,20},{350,220},{150,220}};
        c2d_save();
        c2d_clip_poly_begin(A, 4, false);
        c2d_clip_poly_begin(B, 4, false);
        c2d_rect(0, 0, 900, 900, WHITE);
        c2d_restore();                       /* must close BOTH */
        c2d_rect(400, 400, 40, 40, RED);     /* proves we are back on the surface */
        c2d_end();
        Image img = LoadImageFromTexture(s.tex.texture); ImageFlipVertical(&img);
        const int inBoth = GetImageColor(img, 200, 150).r;
        const int inAonly = GetImageColor(img, 100, 150).r;
        const int inBonly = GetImageColor(img, 300, 100).r;
        printf("    A and B %d, A only %d, B only %d\n", inBoth, inAonly, inBonly);
        CHECK(inBoth > 200, "the intersection is filled");
        CHECK(inAonly < 30 && inBonly < 30, "neither clip alone survives -- they intersect");
        CHECK(GetImageColor(img, 410, 410).r > 200, "restore returned drawing to the surface");
        UnloadImage(img); UnloadRenderTexture(rt);
    }

    printf("\n== 11. gradient axis ==\n");
    {
        C2DGradient gx = c2d_gradient_linear_x(100.0f, 300.0f);
        c2d_gradient_stop(&gx, 0.0f, (Color){0,0,0,255});
        c2d_gradient_stop(&gx, 1.0f, (Color){255,255,255,255});
        c2d_begin(&s, BLACK);
        const Vector2 q[4] = {{100,100},{300,100},{300,200},{100,200}};
        c2d_fill_poly_gradient(q, 4, &gx);
        c2d_end();
        Image img = LoadImageFromTexture(s.tex.texture); ImageFlipVertical(&img);
        const int l = GetImageColor(img, 110, 150).r, r = GetImageColor(img, 290, 150).r;
        const int t = GetImageColor(img, 200, 110).r, bm = GetImageColor(img, 200, 190).r;
        printf("    horizontal: left %d right %d ; top %d bottom %d\n", l, r, t, bm);
        CHECK(r > l + 150, "a horizontal gradient varies along x");
        CHECK(abs(t - bm) < 12, "and NOT along y");
        UnloadImage(img);
    }

    printf("\n== 12. transform stack ==\n");
    {
        c2d_begin(&s, BLACK);
        c2d_save();
        c2d_translate(200.0f, 100.0f);
        c2d_scale(2.0f, 1.0f);
        c2d_rect(0.0f, 0.0f, 50.0f, 40.0f, WHITE);   /* -> 200..300 x 100..140 */
        c2d_restore();
        c2d_rect(0.0f, 0.0f, 10.0f, 10.0f, RED);     /* transform popped */
        /* a clip inside a transform must not be double-transformed */
        c2d_save();
        c2d_translate(0.0f, 300.0f);
        const Vector2 cp[4] = {{100,0},{200,0},{200,80},{100,80}};
        c2d_clip_poly_begin(cp, 4, false);
        c2d_rect(0, 0, 900, 900, GREEN);
        c2d_clip_end();
        c2d_restore();
        c2d_end();
        Image img = LoadImageFromTexture(s.tex.texture); ImageFlipVertical(&img);
        printf("    scaled rect: (250,120)=%d  (310,120)=%d\n",
               GetImageColor(img, 250, 120).r, GetImageColor(img, 310, 120).r);
        CHECK(GetImageColor(img, 250, 120).r > 200, "translate+scale places the rect");
        CHECK(GetImageColor(img, 310, 120).r < 30, "and scales its width, not past it");
        CHECK(GetImageColor(img, 5, 5).r > 200, "restore pops the transform");
        const int gIn = GetImageColor(img, 150, 340).g, gOut = GetImageColor(img, 150, 250).g;
        printf("    clipped-in-transform: inside %d outside %d\n", gIn, gOut);
        CHECK(gIn > 200, "a clip inside a transform lands where the transform puts it");
        CHECK(gOut < 30, "and is composited once, not transformed twice");
        UnloadImage(img);
    }

    printf("\n== 13. lineDashOffset ==\n");
    {
        const Vector2 ln[2] = {{50.0f, 100.0f}, {450.0f, 100.0f}};
        c2d_begin(&s, BLACK);
        c2d_dashed_polyline_phase(ln, 2, 10.0f, 10.0f, 0.0f, WHITE, 4.0f);
        const Vector2 ln2[2] = {{50.0f, 200.0f}, {450.0f, 200.0f}};
        c2d_dashed_polyline_phase(ln2, 2, 10.0f, 10.0f, 10.0f, WHITE, 4.0f);
        c2d_end();
        Image img = LoadImageFromTexture(s.tex.texture); ImageFlipVertical(&img);
        /* offset 10 on a 10/10 pattern is exactly antiphase */
        const int a0 = GetImageColor(img, 55, 100).r, b0 = GetImageColor(img, 55, 200).r;
        const int a1 = GetImageColor(img, 65, 100).r, b1 = GetImageColor(img, 65, 200).r;
        printf("    x=55: phase0 %d phase10 %d ; x=65: phase0 %d phase10 %d\n", a0, b0, a1, b1);
        CHECK(a0 > 200 && b0 < 30, "offset moves the pattern along the path");
        CHECK(a1 < 30 && b1 > 200, "a half-period offset is exactly antiphase");
        UnloadImage(img);
    }

    printf("\n== 14. glow on a FILL ==\n");
    {
        const Vector2 sq[4] = {{200,150},{280,150},{280,230},{200,230}};
        c2d_begin(&s, BLACK);
        c2d_glow_fill(sq, 4, (Color){0,255,255,255}, 12.0f);
        const Vector2 sq2[4] = {{200,350},{280,350},{280,430},{200,430}};
        c2d_fill_poly(sq2, 4, (Color){0,255,255,255});
        c2d_end();
        Image img = LoadImageFromTexture(s.tex.texture); ImageFlipVertical(&img);
        const int core = GetImageColor(img, 240, 190).g;
        const int just = GetImageColor(img, 240, 234).g;   /* 4px outside */
        const int mid  = GetImageColor(img, 240, 240).g;   /* 10px outside */
        const int far  = GetImageColor(img, 240, 260).g;   /* 30px outside */
        const int flat = GetImageColor(img, 240, 434).g;   /* no glow, 4px out */
        printf("    fill glow: core %d, +4px %d, +10px %d, +30px %d ; unglowed +4px %d\n",
               core, just, mid, far, flat);
        CHECK(core > 240, "the shape itself stays solid -- not ringed");
        CHECK(just > flat + 40, "there is a halo outside the edge");
        CHECK(just > mid && mid > far, "and it decays with distance");
        CHECK(far < 20, "reaching about blur and no further");
        UnloadImage(img);
    }

    printf("\n== 15. the drill: clicking drives it, heat follows the spindle ==\n");
    {
        /* The behaviour ported from redline.html, and the only part of the
           drill bar that is not a picture. No GL involved. */
        DrillSim d;
        DrillSim_Reset(&d);

        /* THE STRING TURNS ONLY WHEN STARTED: a planned hole is not a
           running drill, and a click before the start is not a kick. */
        const float rested = d.rpm;
        DrillSim_Bite(&d);
        CHECK(d.rpm == rested, "a click before the start does nothing");
        DrillSim_SetTarget(&d, 30.0f);
        for (int i = 0; i < 600; i++) DrillSim_Step(&d, 1.0f / 60.0f);
        CHECK(d.depthM == 0.0f, "a planned, unstarted drill does not move");
        CHECK(DrillSim_Start(&d) && d.running, "the start runs it");

        /* free drilling from here on: the whole column, started */
        DrillSim_Reset(&d);
        DrillSim_SetTarget(&d, DRILL_TARGET_M);
        DrillSim_Start(&d);
        for (int i = 0; i < 200; i++) DrillSim_Step(&d, 1.0f / 60.0f);
        const float idle = d.rpm;

        DrillSim_Bite(&d);
        CHECK(d.rpm > idle + 0.2f, "a click kicks the spindle up");

        /* left alone it decays back toward idle */
        for (int i = 0; i < 200; i++) DrillSim_Step(&d, 1.0f / 60.0f);
        printf("    after 3.3s idle: rpm %.3f (idle %.3f) heat %.3f\n", d.rpm, idle, d.heat);
        CHECK(d.rpm < idle + 0.02f, "and it decays back to idle when you stop");
        CHECK(d.heat < 0.02f, "heat bleeds away with it");

        /* held at a rhythm, heat climbs and the hole deepens */
        DrillSim_Reset(&d);
        DrillSim_SetTarget(&d, DRILL_TARGET_M);
        DrillSim_Start(&d);
        float peakHeat = 0.0f;
        for (int i = 0; i < 600; i++)
        {
            if (i % 12 == 0) DrillSim_Bite(&d);      /* ~5 clicks a second */
            DrillSim_Step(&d, 1.0f / 60.0f);
            if (d.heat > peakHeat) peakHeat = d.heat;
        }
        printf("    after 10s of clicking: rpm %.2f heat %.2f depth %.1f m wear %.2f\n",
               d.rpm, d.heat, d.depthM, d.wear);
        CHECK(peakHeat > 0.3f, "sustained clicking heats the bit");
        CHECK(d.depthM > 5.0f, "and cuts");

        /* the coupling itself: more spindle, more heat, all else equal */
        DrillSim slow, fast;
        DrillSim_Reset(&slow); DrillSim_Reset(&fast);
        DrillSim_SetTarget(&slow, DRILL_TARGET_M); DrillSim_Start(&slow);
        DrillSim_SetTarget(&fast, DRILL_TARGET_M); DrillSim_Start(&fast);
        for (int i = 0; i < 240; i++)
        {
            if (i % 24 == 0) DrillSim_Bite(&slow);
            if (i % 6  == 0) DrillSim_Bite(&fast);
            DrillSim_Step(&slow, 1.0f / 60.0f);
            DrillSim_Step(&fast, 1.0f / 60.0f);
        }
        printf("    slow rpm %.2f heat %.2f | fast rpm %.2f heat %.2f\n",
               slow.rpm, slow.heat, fast.rpm, fast.heat);
        CHECK(fast.rpm > slow.rpm, "clicking faster spins faster");
        CHECK(fast.heat > slow.heat + 0.1f, "and runs hotter -- the coupling");

        /* the band moves with the rock, which is the thing worth reading */
        DrillSim probe;
        DrillSim_Reset(&probe);
        probe.rpm = 0.5f;
        probe.depthM = 5.0f;                  /* regolith, band .26-.60 */
        const int inRegolith = DrillSim_BandState(&probe);
        probe.depthM = 100.0f;                /* basalt, band .68-1.18  */
        const int inBasalt = DrillSim_BandState(&probe);
        printf("    rpm 0.50 reads %d in regolith, %d in basalt\n", inRegolith, inBasalt);
        CHECK(inRegolith == 0 && inBasalt < 0,
              "the same spindle is in band in soft rock and rubbing in hard");
    }

    printf("\n== 16. delineation: what the console knows ==\n");
    {
        /* The claims are the design's own (survey_knowledge.h): about seven
           well-spread full-depth holes clear 95%, a 3x3 grid reaches 99.8%,
           and shallow holes alone plateau. */
        DashKnowledge k;
        DashKnow_Clear(&k);
        CHECK(DashKnow_Delineation(&k, DK_LATTICE, DRILL_TARGET_M) == 0.0f,
              "an undrilled block is known not at all");
        CHECK(!DashKnow_IsMeasured(&k, DK_LATTICE, DRILL_TARGET_M),
              "and isolate is locked");

        /* one hole in the middle, full depth */
        DashKnow_Add(&k, DK_LATTICE * 0.5f, DK_LATTICE * 0.5f, DRILL_TARGET_M);
        const float one = DashKnow_Delineation(&k, DK_LATTICE, DRILL_TARGET_M);
        printf("    1 hole:  %.3f (%s)\n", one, DashKnow_Tier(&k, DK_LATTICE, DRILL_TARGET_M));
        CHECK(one > 0.1f && one < DK_DELIN_GATE,
              "one hole says something, and nothing like enough");

        /* a 3x3 grid, well spread */
        DashKnow_Clear(&k);
        for (int a = 0; a < 3; a++)
            for (int b = 0; b < 3; b++)
                DashKnow_Add(&k, (a + 0.5f) / 3.0f * DK_LATTICE,
                                 (b + 0.5f) / 3.0f * DK_LATTICE, DRILL_TARGET_M);
        const float grid = DashKnow_Delineation(&k, DK_LATTICE, DRILL_TARGET_M);
        printf("    3x3 grid: %.3f (%s)\n", grid, DashKnow_Tier(&k, DK_LATTICE, DRILL_TARGET_M));
        CHECK(grid > 0.99f, "a 3x3 grid all but settles the block");
        CHECK(DashKnow_IsMeasured(&k, DK_LATTICE, DRILL_TARGET_M), "and unlocks isolate");

        /* DEPTH MATTERS: the same five holes, shallow against full-depth.
           (Enough shallow holes will still settle it -- misses multiply, so
           sixteen of anything gets there. The claim the model makes is that
           depth is worth something, not that shallow drilling is worthless.) */
        DashKnowledge sh, dp;
        DashKnow_Clear(&sh); DashKnow_Clear(&dp);
        for (int a = 0; a < 5; a++)
        {
            const float i = (a % 3 + 0.5f) / 3.0f * DK_LATTICE;
            const float j = (a / 3 + 0.5f) / 3.0f * DK_LATTICE;
            DashKnow_Add(&sh, i, j, DRILL_TARGET_M * 0.12f);
            DashKnow_Add(&dp, i, j, DRILL_TARGET_M);
        }
        const float shallow = DashKnow_Delineation(&sh, DK_LATTICE, DRILL_TARGET_M);
        const float deep    = DashKnow_Delineation(&dp, DK_LATTICE, DRILL_TARGET_M);
        printf("    5 shallow %.3f (%s) vs 5 full-depth %.3f (%s)\n",
               shallow, DashKnow_Tier(&sh, DK_LATTICE, DRILL_TARGET_M),
               deep, DashKnow_Tier(&dp, DK_LATTICE, DRILL_TARGET_M));
        /* Only just, and that is a SCALE MISMATCH worth seeing in the output
           rather than hiding behind a threshold: DK_K_SKIRT_M is 230 m, a
           physical statement about how far below its bottom a hole still
           constrains the beds, and it was set against the game's ~2 km
           column. Over redline's 120 m column a 14 m hole is already inside
           the skirt of the whole thing, so depth hardly matters. Resolved
           when the console is fed the real column -- see the C5-C7 notes in
           survey-dashboard-implementation.md. */
        CHECK(deep > shallow, "the same holes drilled deeper know more");
        CHECK(!DashKnow_IsMeasured(&sh, DK_LATTICE, DRILL_TARGET_M),
              "five shallow holes do not settle the column");

        /* three mediocre holes beat one good one: misses multiply */
        DashKnowledge a1, a3;
        DashKnow_Clear(&a1); DashKnow_Clear(&a3);
        DashKnow_Add(&a1, 14.0f, 14.0f, DRILL_TARGET_M);
        DashKnow_Add(&a3, 7.0f, 7.0f, DRILL_TARGET_M);
        DashKnow_Add(&a3, 21.0f, 7.0f, DRILL_TARGET_M);
        DashKnow_Add(&a3, 14.0f, 21.0f, DRILL_TARGET_M);
        printf("    1 central %.3f vs 3 spread %.3f\n",
               DashKnow_Delineation(&a1, DK_LATTICE, DRILL_TARGET_M),
               DashKnow_Delineation(&a3, DK_LATTICE, DRILL_TARGET_M));
        CHECK(DashKnow_Delineation(&a3, DK_LATTICE, DRILL_TARGET_M) >
              DashKnow_Delineation(&a1, DK_LATTICE, DRILL_TARGET_M),
              "three spread holes are worth more than one central one");
    }

    printf("\n== 17. the borehole bar owns the depth ==\n");
    {
        DrillSim d;
        DrillSim_Reset(&d);
        DrillSim_SetTarget(&d, 20.0f);
        DrillSim_Start(&d);
        int completions = 0;
        for (int i = 0; i < 3000; i++)
        {
            if (i % 8 == 0) DrillSim_Bite(&d);
            DrillSim_Step(&d, 1.0f / 60.0f);
            if (d.completed) completions++;
        }
        printf("    target 20 m -> stopped at %.2f m, completed %d time(s)\n",
               d.depthM, completions);
        CHECK(d.depthM <= 20.05f, "the string stops AT the depth asked for");
        CHECK(d.depthM >= 19.95f, "and reaches it");
        CHECK(completions == 1, "completion fires once, not every frame");
        CHECK(!d.running, "landing the target stops the string");

        /* and it goes on when a deeper depth is asked for -- and started */
        DrillSim_SetTarget(&d, 45.0f);
        CHECK(DrillSim_Start(&d), "a deeper plan can be started");
        for (int i = 0; i < 3000; i++)
        {
            if (i % 8 == 0) DrillSim_Bite(&d);
            DrillSim_Step(&d, 1.0f / 60.0f);
        }
        printf("    then target 45 m -> %.2f m\n", d.depthM);
        CHECK(d.depthM > 44.9f && d.depthM < 45.1f, "a deeper target feeds it on");
    }

    printf("\n== 18. the dig profile: a reading every 0.5 m ==\n");
    {
        DrillSim d;
        DrillProfile p;
        DrillSim_Reset(&d);
        DrillProfile_Clear(&p);
        DrillSim_SetTarget(&d, 30.0f);
        DrillProfile_Plan(&p, 3.0f, 4.0f, 30.0f, d.t);
        CHECK(p.open && p.count == 0, "the plan opens it, empty");
        for (int i = 0; i < 120; i++) { DrillSim_Step(&d, 0.05f); DrillProfile_Record(&p, &d); }
        CHECK(p.count == 0 && p.startedT < 0.0f, "nothing is recorded before the start");

        /* long, uneven frames: a bin must never be skipped */
        DrillSim_Start(&d);
        for (int i = 0; i < 20000 && d.running; i++)
        {
            if (i % 6 == 0) DrillSim_Bite(&d);
            DrillSim_Step(&d, (i % 3 == 0) ? 0.6f : 0.016f);
            DrillProfile_Record(&p, &d);
        }
        bool exact = true;
        for (int i = 0; i < p.count; i++)
            if (p.sample[i].depthM != DRILL_PROFILE_STEP_M * (float)(i + 1)) exact = false;
        printf("    30 m hole -> %d readings, finished at %.1f s\n", p.count, p.finishedT);
        CHECK(p.count == 60, "one reading per 0.5 m, 60 for 30 m");
        CHECK(exact, "each at its own bin depth, none skipped by a long frame");
        CHECK(p.finishedT >= 0.0f, "the hole's landing is stamped");
        CHECK(DrillSim_Strata()[p.sample[23].stratum].name[0] == 'R' &&
              DrillSim_Strata()[p.sample[24].stratum].name[0] == 'M',
              "12.0 m reads regolith, 12.5 m megaregolith");

        /* deeper is the same hole */
        DrillSim_SetTarget(&d, 40.0f);
        DrillProfile_Plan(&p, 3.0f, 4.0f, 40.0f, d.t);
        DrillSim_Start(&d);
        for (int i = 0; i < 20000 && d.running; i++)
        {
            if (i % 6 == 0) DrillSim_Bite(&d);
            DrillSim_Step(&d, 0.03f);
            DrillProfile_Record(&p, &d);
        }
        CHECK(p.count == 80, "deepening to 40 m keeps the first 60 and adds 20");

        /* ABORT: the string stops where it is, the hole is what was drilled */
        DrillSim_SetTarget(&d, 90.0f);
        DrillProfile_Plan(&p, 3.0f, 4.0f, 90.0f, d.t);
        DrillSim_Start(&d);
        for (int i = 0; i < 400; i++)
        {
            if (i % 6 == 0) DrillSim_Bite(&d);
            DrillSim_Step(&d, 0.03f);
            DrillProfile_Record(&p, &d);
        }
        const float at = d.depthM;
        const int kept = p.count;
        CHECK(at > 40.0f && at < 90.0f, "mid-hole when aborted");
        CHECK(DrillSim_Abort(&d) && !d.running, "abort stops the string");
        DrillProfile_Abort(&p, &d);
        for (int i = 0; i < 300; i++) { DrillSim_Step(&d, 0.03f); DrillProfile_Record(&p, &d); }
        CHECK(d.depthM == at && d.targetM == at, "and the hole stays at the depth reached");
        CHECK(p.aborted && p.count == kept, "the profile ends there, keeping what it read");
        CHECK(!DrillSim_Abort(&d), "nothing to abort once stopped");
    }

    c2d_fonts_unload();
    c2d_surface_destroy(&s);
    CloseWindow();
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
