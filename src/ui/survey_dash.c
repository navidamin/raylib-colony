/* survey_dash.c — see survey_dash.h. */
#include "survey_dash.h"

#include "c2d.h"
#include "holo3d.h"
#include "toolrack.h"

#include <math.h>
#include <string.h>

/* DASH.layout, js/dashboard.html:1627. Only the two entries the ported
 * modules need; the rest arrives with the Dashboard chrome. */
#define DASH_RACK_X       48.0f
#define DASH_RACK_Y       32.0f
#define DASH_RACK_SCALE   0.845f
#define DASH_RACK_SCALE_Y 0.875f
#define DASH_LAYERS_X     400.0f
#define DASH_LAYERS_Y     22.0f

/* blockView (1609): { cx: b.x + 370, cy: b.y + 345, zoom: 0.28 } */
#define DASH_BLOCK_CX   (DASH_LAYERS_X + 370.0f)
#define DASH_BLOCK_CY   (DASH_LAYERS_Y + 345.0f)
#define DASH_BLOCK_ZOOM 0.28f

/* blockRegion (1610): the rect inside which a drag rotates the block */
#define DASH_BLOCK_X0 (DASH_LAYERS_X + 130.0f)
#define DASH_BLOCK_Y0 (DASH_LAYERS_Y + 120.0f)
#define DASH_BLOCK_X1 (DASH_LAYERS_X + 610.0f)
#define DASH_BLOCK_Y1 (DASH_LAYERS_Y + 590.0f)

/* The console ships supersampled: Canvas antialiases coverage and raylib does
 * not, and 2 is where that stops paying (docs/design/prospecting/
 * holo3d-inventory.md). */
#define DASH_SS 2

static bool         g_ready = false;
static C2DSurface   g_surf;
static Holo3DModel *g_model = NULL;
static H3DState     g_block;
static H3DView      g_view;
static ToolRackData g_rack;

/* Drag state. `moved` distinguishes a rotate from a tap, the same way the
 * JS controller does -- without it one drag suppresses the next tap. */
static bool    g_down = false, g_moved = false, g_onBlock = false;
static Vector2 g_downPt = {0.0f, 0.0f};

bool SurveyDash_Init(void)
{
    if (g_ready) return true;

    c2d_set_supersample(DASH_SS);
    c2d_fonts_load("src/assets/fonts/JetBrainsMono-Medium.ttf",
                   "src/assets/fonts/JetBrainsMono-SemiBold.ttf",
                   "src/assets/fonts/JetBrainsMono-Bold.ttf");
    g_surf = c2d_surface_create(SURVEY_DASH_DESIGN_W, SURVEY_DASH_DESIGN_H);
    if (g_surf.tex.id == 0) return false;

    H3DBuildOpts opts = {0};
    opts.surfaceW = SURVEY_DASH_DESIGN_W;
    opts.surfaceH = SURVEY_DASH_DESIGN_H;
    g_model = Holo3D_Build(&opts);
    if (!g_model) { c2d_surface_destroy(&g_surf); return false; }

    memset(&g_block, 0, sizeof(g_block));
    g_block.yaw = -0.1f;
    g_block.pitch = 0.42f;
    g_block.selected = -1;
    memset(&g_view, 0, sizeof(g_view));
    g_view.cx = DASH_BLOCK_CX;
    g_view.cy = DASH_BLOCK_CY;
    g_view.zoom = DASH_BLOCK_ZOOM;

    g_rack = ToolRack_Demo();
    g_ready = true;
    return true;
}

void SurveyDash_Shutdown(void)
{
    if (!g_ready) return;
    Holo3D_Free(g_model);
    g_model = NULL;
    c2d_surface_destroy(&g_surf);
    c2d_fonts_unload();
    g_ready = false;
}

void SurveyDash_Draw(Rectangle region, float dt)
{
    if (!SurveyDash_Init()) return;

    Holo3D_Tick(&g_block, dt, (float)GetTime());
    g_block.time = (float)GetTime();

    c2d_begin(&g_surf, (Color){0x02, 0x0e, 0x17, 255});

    ToolRackOpts ro = {0};
    ro.level = 6;
    ro.grain = false;      /* the grain tile is not wired to a seed yet */
    c2d_save();
    c2d_translate(DASH_RACK_X, DASH_RACK_Y);
    c2d_scale(DASH_RACK_SCALE, DASH_RACK_SCALE_Y);
    ToolRack_DrawB(&g_rack, 0.0f, 0.0f, &ro);
    c2d_restore();

    /* drawBlock3D (1611): callouts, brackets and the base ring are OFF in the
     * dashboard -- they belong to the block's own full-screen view. */
    H3DHud hud = {0};
    hud.reticle = true;
    Holo3D_Render(g_model, &g_block, &g_view);
    Holo3D_DrawHud(g_model, &g_block, &g_view, &hud);

    c2d_end();

    c2d_present_into(&g_surf, region);
}

Vector2 SurveyDash_ToDesign(Rectangle region, Vector2 p)
{
    (void)region;
    return c2d_to_design(&g_surf, p);
}

void SurveyDash_Press(Rectangle region, Vector2 screenPt)
{
    if (!g_ready) return;
    const Vector2 d = SurveyDash_ToDesign(region, screenPt);
    g_down = true;
    g_moved = false;
    g_downPt = d;
    g_onBlock = (d.x >= DASH_BLOCK_X0 && d.x <= DASH_BLOCK_X1 &&
                 d.y >= DASH_BLOCK_Y0 && d.y <= DASH_BLOCK_Y1);
}

void SurveyDash_Drag(Rectangle region, Vector2 delta)
{
    if (!g_ready || !g_down || !g_onBlock) return;
    /* the letterbox scale, so a drag turns the block by the same amount
     * whatever the window size */
    const float k = (g_surf.dst.width > 0.0f)
                  ? (float)g_surf.w / g_surf.dst.width : 1.0f;
    (void)region;
    if (fabsf(delta.x) + fabsf(delta.y) > 0.0f) g_moved = true;
    g_block.yaw   += delta.x * k * 0.006f;
    g_block.pitch += delta.y * k * 0.004f;
    if (g_block.pitch < 0.05f) g_block.pitch = 0.05f;
    if (g_block.pitch > 1.30f) g_block.pitch = 1.30f;
    g_block.fast = true;
}

void SurveyDash_Release(Rectangle region, Vector2 screenPt)
{
    if (!g_ready) return;
    const Vector2 d = SurveyDash_ToDesign(region, screenPt);
    g_block.fast = false;
    if (g_down && !g_moved)
    {
        if (g_onBlock)
        {
            const int bed = Holo3D_Hit(g_model, d.x, d.y);
            if (bed >= 0) Holo3D_Select(&g_block, bed);
        }
        else
        {
            /* rack-local units: undo the translate and the scale, in that
               order, exactly as the draw applied them */
            const float rx = (d.x - DASH_RACK_X) / DASH_RACK_SCALE;
            const float ry = (d.y - DASH_RACK_Y) / DASH_RACK_SCALE_Y;
            const int slot = ToolRack_HitTestB(rx, ry, g_rack.slots);
            if (slot >= 0 && g_rack.tools[slot].present)
            {
                for (int i = 0; i < g_rack.slots; i++) g_rack.tools[i].selected = false;
                g_rack.tools[slot].active = !g_rack.tools[slot].active;
                g_rack.tools[slot].selected = g_rack.tools[slot].active;
            }
        }
    }
    g_down = false;
    g_moved = false;
}
