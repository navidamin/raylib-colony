/* survey_dash.c — see survey_dash.h. */
#include "survey_dash.h"

#include "c2d.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

/* THREE PANES, which is a layout decision and not the reference's. The JS
 * dashboard has four blocks -- rack + tool stats down the left, block + log in
 * the middle, drill bar + drill stats down the right. This console is three:
 *
 *      left    the tool rack
 *      middle  the block, with the message log beneath it
 *      right   the drill bar
 *
 * The two stats blocks have no pane, so they are not drawn. Placement inside
 * each pane still comes from the reference (rack scale, block camera), so the
 * modules themselves are untouched -- only where the console puts them.
 *
 * 1536 = 20 + 350 + 16 + 744 + 16 + 370 + 20. */
#define PANE_M   20.0f
#define PANE_G   16.0f
#define PANE_TOP 20.0f
#define PANE_H   (SURVEY_DASH_DESIGN_H - PANE_TOP * 2.0f)

#define LEFT_X    PANE_M
#define LEFT_W    350.0f
#define MID_X     (LEFT_X + LEFT_W + PANE_G)
#define MID_W     744.0f
#define RIGHT_X   (MID_X + MID_W + PANE_G)
#define RIGHT_W   370.0f

/* THE SIDE COLUMNS ARE TWO PANELS EACH: the instrument, and its stats under
 * it. That is the reference's own arrangement (dashboard.html's layout.stats
 * and layout.dstats) and the three-pane port had dropped it for want of
 * room. The stats are what a tool or a running drill reports, so they belong
 * under the thing they describe. */
#define STATS_H   168.0f
#define STATS_GAP  12.0f
#define MAIN_H    (PANE_H - STATS_H - STATS_GAP)
#define STATS_Y   (PANE_TOP + MAIN_H + STATS_GAP)

/* THE RACK SCALES UNIFORMLY. It was 0.845 across and 0.925 down -- a squash
 * nobody chose, left over from filling a taller pane. One factor, fitted to
 * whichever of the two axes runs out first, and centred in what is left. */
#define RACK_FIT_W  (LEFT_W - 16.0f)
#define RACK_FIT_H  (MAIN_H - 30.0f)   /* the panel's bracket needs air above and below */
#define DASH_RACK_SCALE \
    ((RACK_FIT_W / TR_W) < (RACK_FIT_H / TR_H) ? (RACK_FIT_W / TR_W) : (RACK_FIT_H / TR_H))
#define DASH_RACK_SCALE_Y DASH_RACK_SCALE
#define DASH_RACK_X       (LEFT_X + (LEFT_W - TR_W * DASH_RACK_SCALE) * 0.5f)
#define DASH_RACK_Y       (PANE_TOP + (MAIN_H - TR_H * DASH_RACK_SCALE) * 0.5f)

/* the block, the confidence bar, then the log, down the middle pane */
#define CONF_H         76.0f
#define LOG_H         196.0f
#define LOG_X         (MID_X + 16.0f)
#define LOG_Y         (PANE_TOP + PANE_H - LOG_H - 16.0f)
#define CONF_Y        (LOG_Y - CONF_H - 12.0f)
#define LOG_W         (MID_W - 32.0f)
#define DASH_BLOCK_CX (MID_X + MID_W * 0.5f)
#define DASH_BLOCK_CY (PANE_TOP + (CONF_Y - PANE_TOP) * 0.50f)

/* The block was drawn at a fifth of its size in a pane seven hundred units
 * wide -- the reference's zoom, carried over without re-fitting it to a pane
 * that is a different shape. At 0.42 it fills the space it has. The player
 * can move it: see DASH_ZOOM_MIN/MAX. */
#define DASH_BLOCK_ZOOM 0.37f
#define DASH_ZOOM_MIN   0.15f
#define DASH_ZOOM_MAX   1.30f
#define DASH_ZOOM_STEP  1.12f

/* the rect inside which a drag rotates the block */
#define DASH_BLOCK_X0 (MID_X + 20.0f)
#define DASH_BLOCK_Y0 (PANE_TOP + 20.0f)
#define DASH_BLOCK_X1 (MID_X + MID_W - 20.0f)
#define DASH_BLOCK_Y1 (CONF_Y - 8.0f)

/* The console ships supersampled: Canvas antialiases coverage and raylib does
 * not, and 2 is where that stops paying (docs/design/prospecting/
 * holo3d-inventory.md).
 *
 * NOT ON THE WEB. Supersampling multiplies EVERY offscreen target the shim
 * owns. At 2 a design-sized target is 3072x1536 -- 18 MB apiece -- and seven
 * of them are design-sized: the surface, four clip layers and Holo3D's two
 * ghost buffers. That is 126 MB, plus a depth renderbuffer on each and the
 * downsampled blur pairs on top. A browser tab refuses somewhere in there,
 * and raylib answers a refused LoadRenderTexture with id 0. Binding id 0 is
 * binding the CANVAS -- at the viewport the texture asked for. That is
 * exactly "drawing to a destination rect smaller than the viewport rect",
 * and why the page came up broken with no error in it.
 *
 * At 1 the same seven come to 31 MB. The visual diff gate is measured at 2
 * and is a desktop and CI concern; the phone gets a console instead of a
 * blank screen, which is the better trade. */
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
  #define DASH_SS 1
#else
  #define DASH_SS 2
#endif

/* THE ONLY FILE STATICS LEFT, and they are the process's, not a console's:
 * one render surface, one set of fonts, one block geometry, shared by every
 * console that draws. Everything a console remembers is in SurveyDashState. */
static bool         g_resReady = false;
static C2DSurface   g_surf;
static Holo3DModel *g_model = NULL;

/* What ground the ONE model is currently carrying. It is the model's
 * property, not a console's: two prospecting units have different ground and
 * whichever is on screen has to re-apply its own. */
static void *g_groundCtx = NULL;
static int   g_groundRev = -1;

static float Clampf01v(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/* ---- the log ---------------------------------------------------------- */

static void DashLog_Push(SurveyDashState *s, float t, const char *text, const char *tag)
{
    SurveyDashLog *L = &s->log;
    for (int i = (L->count < SURVEY_DASH_LOG_MAX ? L->count : SURVEY_DASH_LOG_MAX - 1); i > 0; i--)
    {
        memcpy(L->time[i], L->time[i - 1], sizeof(L->time[0]));
        memcpy(L->text[i], L->text[i - 1], sizeof(L->text[0]));
        memcpy(L->tag [i], L->tag [i - 1], sizeof(L->tag [0]));
    }
    snprintf(L->time[0], sizeof(L->time[0]), "%02d:%02d",
             (int)(t / 60.0f) % 100, (int)t % 60);
    snprintf(L->text[0], sizeof(L->text[0]), "%s", text);
    snprintf(L->tag [0], sizeof(L->tag [0]), "%s", tag ? tag : "");
    if (L->count < SURVEY_DASH_LOG_MAX) L->count++;
}

/* Point the entries at this struct's own strings. Done on the way to the
 * draw, never on push, so a copied state is repaired rather than left with
 * pointers into the state it was copied from. */
static void DashLog_Bind(SurveyDashLog *L)
{
    for (int i = 0; i < L->count; i++)
    {
        L->entry[i].time = L->time[i];
        L->entry[i].parts[0] = (DashLogPart){L->text[i], false};
        L->entry[i].partCount = 1;
        if (L->tag[i][0])
        {
            L->entry[i].parts[1] = (DashLogPart){L->tag[i], true};
            L->entry[i].partCount = 2;
        }
        L->entry[i].chipCount = 0;
    }
}

/* The ruler's graduations, from the ground itself rather than from a table:
 * the collar, then the base of each stratum, then the bottom of the column.
 * Those bases ARE the game's four depth layers (LAYER_THICKNESS_M), so the
 * ruler, the bands beside it, the energy a hole costs and the layer it cores
 * are finally all reading the same numbers.
 *
 * The strata are named inside their own bands; what the ruler marks is the
 * HOLE's landmarks. */
#define DASH_RULER_TICKS (DRILL_STRATA_COUNT + 1)

static const DashDepth *SurveyDash_Ruler(void)
{
    static DashDepth d[DASH_RULER_TICKS];
    static char      label[DASH_RULER_TICKS][12];
    static bool      built = false;
    if (!built)
    {
        const DrillStratum *S = DrillSim_Strata();
        snprintf(label[0], sizeof(label[0]), "%d m", (int)(S[0].top + 0.5f));
        d[0] = (DashDepth){S[0].top, label[0], "SURFACE"};
        for (int i = 0; i < DRILL_STRATA_COUNT; i++)
        {
            snprintf(label[i + 1], sizeof(label[i + 1]), "%d m", (int)(S[i].bot + 0.5f));
            d[i + 1] = (DashDepth){S[i].bot, label[i + 1],
                                   (i == DRILL_STRATA_COUNT - 1) ? "TARGET" : NULL};
        }
        built = true;
    }
    return d;
}

/* ---- the feed ---------------------------------------------------------
 *
 * An UNBUILT tool is an EMPTY BAY, not a dim one. The rack already draws a
 * null entry as an empty machined socket -- the JS data has one -- and "a bay
 * you have no tool for yet" is exactly what that reads as. The alternative,
 * drawing it present but inactive, collides with the state a BUILT tool sits
 * in when it is simply not running, and the player would have no way to tell
 * a tool they can use from one they cannot.
 *
 * What it costs: the rack no longer names what is coming. Nothing else in the
 * game advertises unbuilt tools either, so inventing that here would be
 * designing rather than integrating. Recorded in
 * docs/design/prospecting/console-real-data.md. */
void SurveyDash_Feed(SurveyDashState *s, const SurveyDashFeed *feed)
{
    if (!s || !feed) return;
    if (!s->started) SurveyDash_Reset(s);

    ToolRackData *r = &s->rack;
    r->header = "SURVEY TOOLS";
    r->slots = feed->toolCount > TR_SLOTS_MAX ? TR_SLOTS_MAX : feed->toolCount;

    for (int i = 0; i < TR_SLOTS_MAX; i++)
    {
        ToolRackTool *t = &r->tools[i];
        memset(t, 0, sizeof(*t));
        t->fade = -1.0f;                 /* no transition in flight */
        if (i >= r->slots) continue;

        const SurveyDashTool *g = &feed->tool[i];
        if (!g->built) continue;         /* the empty socket */

        t->present  = true;
        t->name     = g->name;
        t->type     = g->kind;
        t->icon     = g->icon;
        t->active   = (i == feed->selectedTool);
        t->selected = t->active;
    }

    /* One model for the block and the drill. Repointed every frame, so a
     * copied state stops pointing into the state it was copied from. */
    s->know = feed->knowledge ? feed->knowledge : &s->own;

    /* The ground, only when it has actually moved. Holo3D holds one point
     * grid for the process, so the check is against what the MODEL carries,
     * not against what this console last asked for. */
    if (g_model && feed->groundAt &&
        (g_groundCtx != feed->groundCtx || g_groundRev != feed->groundRevision))
    {
        Holo3D_SetGround(g_model, feed->bedCount, feed->groundAt, feed->groundCtx,
                         feed->bedText);
        g_groundCtx = feed->groundCtx;
        g_groundRev = feed->groundRevision;
    }
}

int SurveyDash_TakeToolPick(SurveyDashState *s)
{
    if (!s) return -1;
    const int p = s->toolPick;
    s->toolPick = -1;
    return p;
}

bool SurveyDash_Init(void)
{
    if (g_resReady) return true;

    c2d_set_supersample(DASH_SS);
    c2d_fonts_load("src/assets/fonts/JetBrainsMono-Medium.ttf",
                   "src/assets/fonts/JetBrainsMono-SemiBold.ttf",
                   "src/assets/fonts/JetBrainsMono-Bold.ttf");
    g_surf = c2d_surface_create(SURVEY_DASH_DESIGN_W, SURVEY_DASH_DESIGN_H);
    if (g_surf.tex.id == 0)
    {
        TraceLog(LOG_WARNING,
                 "SURVEY CONSOLE: no %dx%d render target (supersample %d) -- "
                 "the console cannot draw",
                 SURVEY_DASH_DESIGN_W * DASH_SS, SURVEY_DASH_DESIGN_H * DASH_SS, DASH_SS);
        return false;
    }

    H3DBuildOpts opts = {0};
    opts.surfaceW = SURVEY_DASH_DESIGN_W;
    opts.surfaceH = SURVEY_DASH_DESIGN_H;
    g_model = Holo3D_Build(&opts);
    if (!g_model) { c2d_surface_destroy(&g_surf); return false; }

    g_resReady = true;
    return true;
}

void SurveyDash_Shutdown(void)
{
    if (!g_resReady) return;
    Holo3D_Free(g_model);
    g_model = NULL;
    g_groundCtx = NULL;
    g_groundRev = -1;
    c2d_surface_destroy(&g_surf);
    c2d_fonts_unload();
    g_resReady = false;
}

void SurveyDash_Reset(SurveyDashState *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));

    s->block.yaw = -0.1f;
    s->block.pitch = 0.42f;
    s->block.selected = -1;

    s->view.cx = DASH_BLOCK_CX;
    s->view.cy = DASH_BLOCK_CY;
    s->view.zoom = DASH_BLOCK_ZOOM;

    s->rack = ToolRack_Demo();
    DrillSim_Reset(&s->drill);
    DashKnow_Clear(&s->own);

    s->siteI = DK_LATTICE * 0.5f;
    s->siteJ = DK_LATTICE * 0.5f;
    s->toolPick = -1;
    s->know = &s->own;

    DashLog_Push(s, 0.0f, "Console online. Tap the cap to set a site, the ruler to set a depth.", NULL);
    s->started = true;
}

void SurveyDash_Draw(SurveyDashState *s, Rectangle region, float dt)
{
    if (!s) return;
    if (!SurveyDash_Init())
    {
        /* A blank rectangle is indistinguishable from a hung game. Say what
         * happened, on the screen, where the console would have been. */
        DrawRectangleRec(region, DashC_Bg());
        const char *msg = "SURVEY CONSOLE UNAVAILABLE - no render target";
        const int tw = MeasureText(msg, 20);
        DrawText(msg, (int)(region.x + (region.width - tw) * 0.5f),
                 (int)(region.y + region.height * 0.5f - 10.0f), 20,
                 (Color){0xff, 0x8a, 0x5a, 255});
        return;
    }
    if (!s->started) SurveyDash_Reset(s);
    if (!s->know) s->know = &s->own;

    Holo3D_Tick(&s->block, dt, (float)GetTime());
    s->block.time = (float)GetTime();

    c2d_begin(&g_surf, DashC_Bg());

    Dash_Panel(LEFT_X, PANE_TOP, LEFT_W, MAIN_H,  (Color){0, 0, 0, 0}, 12.0f, 26.0f);
    Dash_Panel(LEFT_X, STATS_Y,  LEFT_W, STATS_H, (Color){0, 0, 0, 0}, 12.0f, 22.0f);
    Dash_Panel(MID_X,  PANE_TOP, MID_W,  PANE_H,  (Color){0, 0, 0, 0}, 12.0f, 26.0f);
    Dash_Panel(RIGHT_X, STATS_Y, RIGHT_W, STATS_H, (Color){0, 0, 0, 0}, 12.0f, 22.0f);

    ToolRackOpts ro = {0};
    ro.level = 6;
    ro.grain = false;      /* the grain tile is not wired to a seed yet */
    c2d_save();
    c2d_translate(DASH_RACK_X, DASH_RACK_Y);
    c2d_scale(DASH_RACK_SCALE, DASH_RACK_SCALE_Y);
    ToolRack_DrawB(&s->rack, 0.0f, 0.0f, &ro);
    c2d_restore();

    /* drawBlock3D (1611): callouts, brackets and the base ring are OFF in the
     * dashboard -- they belong to the block's own full-screen view. */
    H3DHud hud = {0};
    hud.reticle = true;
    Holo3D_Render(g_model, &s->block, &s->view);
    Holo3D_DrawHud(g_model, &s->block, &s->view, &hud);

    Dash_Confidence(LOG_X, CONF_Y, LOG_W, CONF_H,
                    DashKnow_Delineation(s->know, DK_LATTICE, DRILL_TARGET_M),
                    DashKnow_Tier(s->know, DK_LATTICE, DRILL_TARGET_M),
                    DashKnow_IsMeasured(s->know, DK_LATTICE, DRILL_TARGET_M));
    DashLog_Bind(&s->log);
    Dash_Log(LOG_X, LOG_Y, LOG_W, LOG_H, s->log.entry, s->log.count);

    DrillSim_Step(&s->drill, dt);
    if (s->drill.completed)
    {
        /* C5+C6 meet here: a finished hole is what the model learns from. */
        DashKnow_Add(s->know, s->siteI, s->siteJ, s->drill.completedAtM);
        char msg[96];
        snprintf(msg, sizeof(msg), "Hole to %d m logged at site %d/%d in ",
                 (int)(s->drill.completedAtM + 0.5f), (int)s->siteI, (int)s->siteJ);
        DashLog_Push(s, s->drill.t, msg, DrillSim_At(s->drill.completedAtM)->name);
        /* announced ONCE -- it fires on a completion, and every later hole
         * is also a completion with the model still measured */
        if (!s->saidMeasured && DashKnow_IsMeasured(s->know, DK_LATTICE, DRILL_TARGET_M))
        {
            DashLog_Push(s, s->drill.t, "Model MEASURED. Isolate unlocked.", NULL);
            s->saidMeasured = true;
        }
    }
    Dash_DrillBar(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, "DRILL BAR",
                  SurveyDash_Ruler(), DASH_RULER_TICKS, &s->drill, dt);

    c2d_end();

    c2d_present_into(&g_surf, region);
}

Vector2 SurveyDash_ToDesign(Rectangle region, Vector2 p)
{
    (void)region;
    return c2d_to_design(&g_surf, p);
}

void SurveyDash_Press(SurveyDashState *s, Rectangle region, Vector2 screenPt)
{
    if (!s || !s->started) return;
    const Vector2 d = SurveyDash_ToDesign(region, screenPt);
    s->down = true;
    s->moved = false;
    s->downPt = d;
    s->onBlock = (d.x >= DASH_BLOCK_X0 && d.x <= DASH_BLOCK_X1 &&
                  d.y >= DASH_BLOCK_Y0 && d.y <= DASH_BLOCK_Y1);

    /* C6: the ruler is the depth control. Checked before the face, because
     * it sits inside the bar and a click on it must arm rather than drill. */
    const float pick = Dash_DrillBarPickDepth(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, d.x, d.y);
    if (pick >= 0.0f)
    {
        DrillSim_SetTarget(&s->drill, pick);
        char msg[96];
        snprintf(msg, sizeof(msg), "String set to %d m. Tap the hole to feed it down.",
                 (int)(pick + 0.5f));
        DashLog_Push(s, s->drill.t, msg, NULL);
        return;
    }

    /* The drill takes its input on PRESS, not release: the whole loop is a
     * rhythm the player keeps, and waiting for the button to come up puts a
     * lag between the tap and the kick. */
    float fx, fy, fw, fh;
    Dash_DrillBarFace(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, &fx, &fy, &fw, &fh);
    if (d.x >= fx && d.x <= fx + fw && d.y >= fy && d.y <= fy + fh)
        DrillSim_Bite(&s->drill);
}

void SurveyDash_Zoom(SurveyDashState *s, Rectangle region, Vector2 screenPt, float steps)
{
    if (!s || !s->started || steps == 0.0f) return;
    const Vector2 d = SurveyDash_ToDesign(region, screenPt);
    /* Only over the block. The rack and the drill bar are lists, and a wheel
     * over a list should never move something else. */
    if (d.x < DASH_BLOCK_X0 || d.x > DASH_BLOCK_X1 ||
        d.y < DASH_BLOCK_Y0 || d.y > DASH_BLOCK_Y1) return;

    float z = s->view.zoom * powf(DASH_ZOOM_STEP, steps);
    if (z < DASH_ZOOM_MIN) z = DASH_ZOOM_MIN;
    if (z > DASH_ZOOM_MAX) z = DASH_ZOOM_MAX;
    s->view.zoom = z;
}

void SurveyDash_Drag(SurveyDashState *s, Rectangle region, Vector2 delta)
{
    if (!s || !s->started || !s->down || !s->onBlock) return;
    /* the letterbox scale, so a drag turns the block by the same amount
     * whatever the window size */
    const float k = (g_surf.dst.width > 0.0f)
                  ? (float)g_surf.w / g_surf.dst.width : 1.0f;
    (void)region;
    if (fabsf(delta.x) + fabsf(delta.y) > 0.0f) s->moved = true;
    s->block.yaw   += delta.x * k * 0.006f;
    s->block.pitch += delta.y * k * 0.004f;
    if (s->block.pitch < 0.05f) s->block.pitch = 0.05f;
    if (s->block.pitch > 1.30f) s->block.pitch = 1.30f;
    s->block.fast = true;
}

void SurveyDash_Release(SurveyDashState *s, Rectangle region, Vector2 screenPt)
{
    if (!s || !s->started) return;
    const Vector2 d = SurveyDash_ToDesign(region, screenPt);
    s->block.fast = false;
    if (s->down && !s->moved)
    {
        if (s->onBlock)
        {
            const int bed = Holo3D_Hit(g_model, d.x, d.y);
            /* C5: isolate is GATED. Until the model is MEASURED a tap on the
             * block moves the drill site instead of peeling a bed -- the
             * control the player has before they have earned the other one. */
            if (DashKnow_IsMeasured(s->know, DK_LATTICE, DRILL_TARGET_M))
            {
                if (bed >= 0) Holo3D_Select(&s->block, bed);
            }
            else
            {
                /* the site, from where the tap landed across the block */
                const float u = Clampf01v((d.x - DASH_BLOCK_X0) / (DASH_BLOCK_X1 - DASH_BLOCK_X0));
                const float v = Clampf01v((d.y - DASH_BLOCK_Y0) / (DASH_BLOCK_Y1 - DASH_BLOCK_Y0));
                s->siteI = u * (float)DK_LATTICE;
                s->siteJ = v * (float)DK_LATTICE;
                s->drill.depthM = 0.0f;      /* a new site is a new hole */
                s->drill.lift = 0.0f;
                s->drill.done = false;
                char msg[96];
                snprintf(msg, sizeof(msg), "Site moved to %d/%d. Collared.",
                         (int)s->siteI, (int)s->siteJ);
                DashLog_Push(s, s->drill.t, msg, NULL);
            }
        }
        else
        {
            /* rack-local units: undo the translate and the scale, in that
               order, exactly as the draw applied them */
            const float rx = (d.x - DASH_RACK_X) / DASH_RACK_SCALE;
            const float ry = (d.y - DASH_RACK_Y) / DASH_RACK_SCALE_Y;
            const int slot = ToolRack_HitTestB(rx, ry, s->rack.slots);
            if (slot >= 0 && s->rack.tools[slot].present)
            {
                /* Report it, do not act on it. The game owns the selection
                 * and hands it back on the next feed, so the rack can never
                 * disagree with SurveyConsole::SelectedTool. A harness with no
                 * game behind it takes the pick and feeds it straight back --
                 * the same handshake, not a second code path. */
                s->toolPick = slot;
            }
        }
    }
    s->down = false;
    s->moved = false;
}
